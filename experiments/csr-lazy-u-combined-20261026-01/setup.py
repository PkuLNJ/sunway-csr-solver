from pathlib import Path
p=Path('work/csr-lazy-u-combined-20261026-01');u=Path('work/csr-u-reconstruct-cache-20261026-01')
b=(u/'build.sh').read_text();b=b[:b.index('cd "$w";')].replace(u.name,p.name)+'''cd "$w";mkdir BUILD_ONCE.guard obj bin log lib
u=$r/build/csr-u-reconstruct-cache-20261026-01
lazy=$r/build/csr-lazy-gamg-20261026-01
test -f "$u/BUILD_COMPLETE";test -f "$lazy/BUILD_COMPLETE"
sha256sum "$u/obj/boussinesqFoam.o" "$u/obj/ConstructProfile.o" "$lazy/obj/CSRPCG.o" "$u/lib/libdfMatrices.a" > REUSED_INPUTS.sha256
cp -p "$u/obj/boussinesqFoam.o" "$u/obj/ConstructProfile.o" "$lazy/obj/CSRPCG.o" obj/
cp -p "$u/lib/libdfMatrices.a" lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.before
ar r lib/libdfMatrices.a obj/CSRPCG.o
ar s lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
lib=$c/platforms/$WM_OPTIONS/lib
python make_link.py
source link-only.sh
sha256sum bin/* obj/* lib/* > OUTPUTS.sha256
sha256sum -c REUSED_INPUTS.sha256 > REUSED_UNCHANGED.txt
echo COMBINED_LINK_COMPLETE > BUILD_COMPLETE
cat BUILD_COMPLETE
'''
(p/'build.sh').write_text(b,newline='\n')
(p/'make_link.py').write_text((u/'make_link.py').read_text().replace(u.name,p.name),newline='\n')
(p/'remote.py').write_text((u/'remote.py').read_text(),newline='\n')
(p/'src/COMPOSITION.txt').write_text('Reuse U geometry entry and ConstructProfile objects; replace only CSRPCG.o with existing lazy GAMG object. Both runtime switches enabled; CSR topology cache absent. Original corrected-r2 dependencies and zero-time guard retained.\n')
src=Path('work/csr-u-geometry-on-min0-50-20261026-01');case=Path('work/csr-lazy-u-combined-min0-50-20261026-01');case.mkdir(exist_ok=True)
for name in ['remote.py','prepare.sh','submit.sh','monitor.sh','fatal_check.py','summarize.py','INPUT-controlDict50','INPUT-fvSolution','RUN_REQUEST.json']:
 s=(src/name).read_text().replace(src.name,case.name).replace('csr-u-reconstruct-cache-20261026-01',p.name).replace('export LNJ_U_RECONSTRUCT_CACHE=1','export LNJ_U_RECONSTRUCT_CACHE=1 LNJ_LAZY_GAMG=1').replace('lnj-50-u-geom-on','lnj-50-lazy-u')
 (case/name).write_text(s,newline='\n')
