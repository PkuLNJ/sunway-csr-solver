r='/home/export/online1/mdt00/shisuan/swpku_of/lnj'
w=r+'/build/csr-lazy-up-geometry-20261026-01'
t=open(r+'/build/csr-r2-miniter0-printguard-20261026-01/link-only.sh').read()
t=t.replace('"$w/obj/ProfilingZeroGuard.o"','"'+r+'/build/csr-r2-miniter0-printguard-20261026-01/obj/ProfilingZeroGuard.o"')
t=t.replace('-L"$fixed/lib"','-L"$w/lib" -L"$fixed/lib"')
t=t.replace('"$parent/obj/boussinesqFoam.o"','"$w/obj/boussinesqFoam.o" "$w/obj/ConstructProfile.o"')
open(w+'/link-only.sh','w').write(t)
