import os,re
r='/home/export/online1/mdt00/shisuan/swpku_of/lnj'
w=r+'/build/csr-r2-miniter0-printguard-20261026-01'
c='/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7'
parent=r+'/build/amr-scalar-cache-aligned-20261024-r2'
h=c+'/src/dfMatrices/csrMatrix/preconditioners/CSRGAMGPreconditioner/CSRGAMGPreconditioner.H'
t=open(h).read();start=t.index('    virtual void print_time() const {');end=t.index('\n    }',start)+6
body=t[start:end];assert 'spmv_time / procondition_time' in body
open(w+'/ORIGINAL-CSRGAMGPreconditioner.H','w').write(t)
open(w+'/src/CSRGAMGPreconditioner.H','w').write(t[:start]+'    virtual void print_time() const;'+t[end:])
body=body.replace('    virtual void print_time() const {','void Foam::CSRGAMGPreconditioner::print_time() const {\n        if (!(procondition_time > 0.0)) {\n            Info << "CSRGAMGPreconditioner: not applied; timing percentages N/A" << endl;\n            return;\n        }',1)
open(w+'/src/ProfilingZeroGuard.C','w').write('#include "CSRGAMGPreconditioner.H"\n'+body+'\n')
old=open(parent+'/build.sh').read();link=old[old.index('mpicxx -mdynamic'):old.index("printf 'ENTRY_LINK_RC")]
link=link.replace('"$w/obj/boussinesqFoam.o"','"$parent/obj/boussinesqFoam.o" "$w/obj/ProfilingZeroGuard.o"')
open(w+'/link-only.sh','w').write(link)
