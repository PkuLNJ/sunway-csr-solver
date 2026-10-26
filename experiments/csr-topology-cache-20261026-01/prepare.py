from pathlib import Path
p=Path('work/csr-topology-cache-20261026-01')
s=Path('csr-solver-versions/reference-library-source/dfMatrices/csrMatrix/csrMatrix/csrMatrix.C').read_text()
s=s.replace('#include <vector>','#include <vector>\n#include <algorithm>\n#include <cstdlib>')
helper='''
namespace {
struct LnjCsrTopology {
    label rows;
    std::vector<label> lower, upper, rowptr, columns, lowerSlot, upperSlot;
    size_t bytes() const { return sizeof(label)*(lower.size()+upper.size()+rowptr.size()+columns.size()+lowerSlot.size()+upperSlot.size()); }
};
std::vector<LnjCsrTopology>& lnjTopologyCache() {
    static std::vector<LnjCsrTopology> entries;
    return entries;
}
bool lnjTopologyEnabled() {
    static const bool enabled = []() { const char* p=std::getenv("LNJ_CSR_TOPOLOGY_CACHE"); return p && std::string(p)=="1"; }();
    return enabled;
}
}
'''
pos=s.index('csrMatrix::csrMatrix(const lduMatrix& ldu):')
s=s[:pos]+helper+s[pos:]
needle='    // fill diag value\n'
hit='''    // Exact addressing comparison is required: an AMR mesh may reuse addresses and sizes.
    // Cache owns its arrays; no lduMatrix/mesh pointers survive a constructor call.
    if (lnjTopologyEnabled()) {
        for (const auto& c : lnjTopologyCache()) {
            if (c.rows != row_ || c.lower.size()!=lduLowerAddr.size() || c.upper.size()!=lduUpperAddr.size()
                || c.lowerSlot.size()!=lduLower.size() || c.upperSlot.size()!=lduUpper.size()) continue;
            if (!std::equal(c.lower.begin(),c.lower.end(),lduLowerAddr.begin())
                || !std::equal(c.upper.begin(),c.upper.end(),lduUpperAddr.begin())) continue;
            std::copy(c.rowptr.begin(),c.rowptr.end(),off_diag_rowptr_.begin());
            std::copy(c.columns.begin(),c.columns.end(),off_diag_colidx_.begin());
            std::copy(lduDiag.begin(),lduDiag.end(),diag_value_.begin());
            for(label i=0;i<lduLower.size();++i) off_diag_value_[c.lowerSlot[i]]=lduLower[i];
            for(label i=0;i<lduUpper.size();++i) off_diag_value_[c.upperSlot[i]]=lduUpper[i];
            return;
        }
    }
'''
s=s.replace(needle,hit+needle,1)
end=s.index('\n}\n\nvoid csrMatrix::write_pattern',s.index('csrMatrix::csrMatrix(const lduMatrix&'))
store='''
    if (lnjTopologyEnabled()) {
        const size_t budget=32u*1024u*1024u;
        const size_t needed=sizeof(label)*(lduLowerAddr.size()+lduUpperAddr.size()+off_diag_rowptr_.size()+off_diag_colidx_.size()+lduLower.size()+lduUpper.size());
        if (needed<=budget) {
            auto& entries=lnjTopologyCache();
            size_t used=0;for(const auto& c:entries) used+=c.bytes();
            while(!entries.empty() && (entries.size()>=16 || used+needed>budget)) {
                used-=entries.front().bytes();entries.erase(entries.begin());
            }
            LnjCsrTopology c;c.rows=row_;
            c.lower.assign(lduLowerAddr.begin(),lduLowerAddr.end());
            c.upper.assign(lduUpperAddr.begin(),lduUpperAddr.end());
            c.rowptr.assign(off_diag_rowptr_.begin(),off_diag_rowptr_.end());
            c.columns.assign(off_diag_colidx_.begin(),off_diag_colidx_.end());
            c.lowerSlot.resize(lduLower.size());c.upperSlot.resize(lduUpper.size());
            std::vector<label> cursor(c.rowptr);
            for(label i=0;i<lduLower.size();++i) c.lowerSlot[i]=cursor[lduUpperAddr[i]]++;
            for(label i=0;i<lduUpper.size();++i) c.upperSlot[i]=cursor[lduLowerAddr[i]]++;
            entries.push_back(std::move(c));
        }
    }
'''
s=s[:end]+store+s[end:]
(p/'src/csrMatrix.C').write_text(s,newline='\n')
b=Path('work/csr-reset-trim-20261026-01/build.sh').read_text().replace('csr-reset-trim-20261026-01',p.name).replace('CSRGAMGPreconditioner','csrMatrix').replace('115da75922ad7e1ecd07c865550852f9c387fdfefccda899d223b015325e4005','9a93b980ff166655914b555f8e6789b11e2c629c690f0512e2763e2880eefe5f')
(p/'build.sh').write_text(b,newline='\n')
(p/'make_link.py').write_text(Path('work/csr-reset-trim-20261026-01/make_link.py').read_text().replace('csr-reset-trim-20261026-01',p.name),newline='\n')
(p/'remote.py').write_text(Path('work/csr-construction-profile-20261026-01/remote.py').read_text(),newline='\n')
