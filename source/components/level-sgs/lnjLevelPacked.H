// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LNJ_LEVEL_PACKED_H
#define LNJ_LEVEL_PACKED_H
#include "lnjLevelSgs.H"
namespace lnj {
template<class Label,class Scalar> struct LevelPacked {
    std::vector<Label> starts,rowPtr,columns,packedToOriginal,originalToPacked;
    std::vector<Scalar> values,diagonal,rhs,psi;
    LevelPacked(const LevelSgsPlan<Label>& plan,const std::vector<Label>& originalRowPtr,
        const std::vector<Label>& originalColumns,const std::vector<Scalar>& originalValues)
        : starts(plan.forward.starts),rowPtr(1,0),packedToOriginal(plan.forward.rows) {
        const auto n=plan.forward.rows.size();
        validateGraph(originalRowPtr,originalColumns);
        if(originalRowPtr.size()!=n+1 || originalValues.size()!=originalColumns.size())
            throw std::invalid_argument("Packed matrix size mismatch");
        originalToPacked.resize(n);diagonal.resize(n);rhs.resize(n);psi.resize(n);
        for(std::size_t p=0;p<n;++p)originalToPacked[packedToOriginal[p]]=static_cast<Label>(p);
        for(std::size_t p=0;p<n;++p){
            const auto row=static_cast<std::size_t>(packedToOriginal[p]);
            for(Label e=originalRowPtr[row];e<originalRowPtr[row+1];++e){
                columns.push_back(originalToPacked[originalColumns[e]]);
                values.push_back(originalValues[e]);
            }
            rowPtr.push_back(static_cast<Label>(columns.size()));
        }
    }
    void packInputs(const std::vector<Scalar>& originalDiagonal,
        const std::vector<Scalar>& originalRhs,const std::vector<Scalar>& originalPsi){
        const auto n=packedToOriginal.size();
        if(originalDiagonal.size()!=n || originalRhs.size()!=n || originalPsi.size()!=n)
            throw std::invalid_argument("Packed input mismatch");
        for(std::size_t p=0;p<n;++p){
            const auto row=static_cast<std::size_t>(packedToOriginal[p]);
            if(!std::isfinite(originalDiagonal[row]) || originalDiagonal[row]==0 ||
               !std::isfinite(originalRhs[row]) || !std::isfinite(originalPsi[row]))
                throw std::invalid_argument("Nonfinite packed input or zero diagonal");
            diagonal[p]=originalDiagonal[row];rhs[p]=originalRhs[row];psi[p]=originalPsi[row];
        }
    }
    std::vector<Scalar> unpackPsi() const {
        std::vector<Scalar> result(psi.size());
        for(std::size_t p=0;p<psi.size();++p)result[packedToOriginal[p]]=psi[p];
        return result;
    }
    void hostSweep(){
        const auto levels=starts.size()-1;
        for(int phase=0;phase<2;++phase)for(std::size_t ordinal=0;ordinal<levels;++ordinal){
            const auto level=phase==0?ordinal:levels-1-ordinal;
            for(Label row=starts[level];row<starts[level+1];++row)
                rowUpdate(row,rowPtr,columns,values,diagonal,rhs,psi);
        }
    }
};
}
#endif
