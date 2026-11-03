// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only prototype. No OpenFOAM or CPE runtime dependency.
#ifndef LNJ_LEVEL_SGS_H
#define LNJ_LEVEL_SGS_H
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace lnj {
struct LevelStats {
    std::size_t rows=0, entries=0, levels=0, maximumWidth=0;
    double meanWidth=0, rowOnlyIdealSpeedup=1, weightedIdealUpperBound=1;
    std::size_t rowSlotsAtWorkers=0;
};

template<class Label> struct LevelSchedule {
    std::vector<Label> levelOfRow, starts, rows;
    LevelStats stats;
};

template<class Label> struct LevelSgsPlan {
    LevelSchedule<Label> forward, backward;
    double constructionSeconds=0;
};

template<class Label>
void validateGraph(const std::vector<Label>& rowPtr,const std::vector<Label>& columns) {
    static_assert(std::is_integral<Label>::value && std::is_signed<Label>::value,
                  "Signed integral index required");
    if(rowPtr.empty() || rowPtr.front()!=0) throw std::invalid_argument("rowPtr must start with zero");
    const std::size_t n=rowPtr.size()-1;
    if(n>static_cast<std::size_t>(std::numeric_limits<Label>::max()) ||
       columns.size()>static_cast<std::size_t>(std::numeric_limits<Label>::max()))
        throw std::overflow_error("Graph exceeds label capacity");
    for(std::size_t i=0;i<n;++i) {
        if(rowPtr[i]<0 || rowPtr[i+1]<rowPtr[i] ||
           static_cast<std::size_t>(rowPtr[i+1])>columns.size())
            throw std::invalid_argument("Invalid CSR row pointer");
        for(Label e=rowPtr[i];e<rowPtr[i+1];++e)
            if(columns[e]<0 || static_cast<std::size_t>(columns[e])>=n ||
               static_cast<std::size_t>(columns[e])==i)
                throw std::invalid_argument("Column outside graph or diagonal in offdiag CSR");
    }
    if(static_cast<std::size_t>(rowPtr.back())!=columns.size())
        throw std::invalid_argument("rowPtr terminal differs from nnz");
}

template<class Label>
LevelSchedule<Label> makeSchedule(const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,bool reverse,std::size_t workers) {
    const std::size_t n=rowPtr.size()-1;
    LevelSchedule<Label> out;
    out.levelOfRow.assign(n,0);
    std::vector<Label> pending(n,0);
    Label count=0;
    for(std::size_t ordinal=0;ordinal<n;++ordinal) {
        const std::size_t row=reverse ? n-1-ordinal : ordinal;
        Label level=pending[row];
        // Completed dependencies, even if reciprocal structure is absent.
        for(Label e=rowPtr[row];e<rowPtr[row+1];++e) {
            const std::size_t col=static_cast<std::size_t>(columns[e]);
            if(reverse ? col>row : col<row)
                level=std::max(level,Label(out.levelOfRow[col]+1));
        }
        out.levelOfRow[row]=level;
        count=std::max(count,Label(level+1));
        // Future rows must wait for this row's read-before-write too. Merely
        // analyzing lower nonzeros would be wrong for asymmetric sparsity.
        for(Label e=rowPtr[row];e<rowPtr[row+1];++e) {
            const std::size_t col=static_cast<std::size_t>(columns[e]);
            if(reverse ? col<row : col>row)
                pending[col]=std::max(pending[col],Label(level+1));
        }
    }
    out.starts.assign(static_cast<std::size_t>(count)+1,0);
    for(auto level:out.levelOfRow) ++out.starts[static_cast<std::size_t>(level)+1];
    for(std::size_t l=1;l<out.starts.size();++l) out.starts[l]+=out.starts[l-1];
    auto next=out.starts;
    out.rows.resize(n);
    // Row ordering within a level is irrelevant mathematically, but deterministic.
    for(std::size_t ordinal=0;ordinal<n;++ordinal) {
        const std::size_t row=reverse ? n-1-ordinal : ordinal;
        out.rows[next[out.levelOfRow[row]]++]=static_cast<Label>(row);
    }
    auto& st=out.stats;
    st.rows=n;st.entries=columns.size();st.levels=static_cast<std::size_t>(count);
    double work=0, parallelLowerBound=0;
    for(std::size_t l=0;l<st.levels;++l) {
        const std::size_t width=static_cast<std::size_t>(out.starts[l+1]-out.starts[l]);
        st.maximumWidth=std::max(st.maximumWidth,width);
        st.rowSlotsAtWorkers+=width/workers+(width%workers!=0);
        double levelWork=0, biggestRow=0;
        for(Label p=out.starts[l];p<out.starts[l+1];++p) {
            const auto row=static_cast<std::size_t>(out.rows[p]);
            const double w=double(rowPtr[row+1]-rowPtr[row])+1.;
            levelWork+=w;biggestRow=std::max(biggestRow,w);
        }
        work+=levelWork;
        parallelLowerBound+=std::max(levelWork/double(workers),biggestRow);
    }
    st.meanWidth=count ? double(n)/double(count) : 0.;
    st.rowOnlyIdealSpeedup=st.rowSlotsAtWorkers ? double(n)/double(st.rowSlotsAtWorkers) : 1.;
    st.weightedIdealUpperBound=parallelLowerBound ? work/parallelLowerBound : 1.;
    return out;
}

template<class Label>
LevelSgsPlan<Label> makeLevelSgsPlan(const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,std::size_t workers=64) {
    if(!workers) throw std::invalid_argument("workers must be positive");
    const auto start=std::chrono::steady_clock::now();
    validateGraph(rowPtr,columns);
    LevelSgsPlan<Label> out;
    out.forward=makeSchedule(rowPtr,columns,false,workers);
    out.backward=makeSchedule(rowPtr,columns,true,workers);
    out.constructionSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    return out;
}

template<class Label,class Scalar>
void rowUpdate(Label row,const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,const std::vector<Scalar>& values,
    const std::vector<Scalar>& diagonal,const std::vector<Scalar>& rhs,
    std::vector<Scalar>& psi) {
    Scalar sum=rhs[row];
    for(Label e=rowPtr[row];e<rowPtr[row+1];++e) sum-=values[e]*psi[columns[e]];
    psi[row]=sum/diagonal[row];
}

template<class Label,class Scalar>
void levelSgsSweep(const LevelSgsPlan<Label>& plan,const std::vector<Label>& rowPtr,
    const std::vector<Label>& columns,const std::vector<Scalar>& values,
    const std::vector<Scalar>& diagonal,const std::vector<Scalar>& rhs,
    std::vector<Scalar>& psi,bool reverseWithinLevel=false) {
    const auto n=rowPtr.size()-1;
    if(psi.size()!=n || rhs.size()!=n || diagonal.size()!=n || values.size()!=columns.size()
       || plan.forward.rows.size()!=n || plan.backward.rows.size()!=n)
        throw std::invalid_argument("Matrix/vector/plan size mismatch");
    for(const auto d:diagonal) if(!std::isfinite(d) || d==0)
        throw std::invalid_argument("Invalid diagonal");
    for(const auto* schedule:{&plan.forward,&plan.backward})
        for(std::size_t l=0;l<schedule->stats.levels;++l) {
            const Label first=schedule->starts[l], last=schedule->starts[l+1];
            // A future parallel backend MUST synchronize between levels, and
            // MUST retain the ORIGINAL row index and row-local CSR term order.
            for(Label p=first;p<last;++p) {
                const Label pos=reverseWithinLevel ? last-1-(p-first) : p;
                rowUpdate(schedule->rows[pos],rowPtr,columns,values,diagonal,rhs,psi);
            }
        }
}
} // namespace lnj
#endif
