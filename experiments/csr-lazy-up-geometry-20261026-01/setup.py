from pathlib import Path
import shutil,json
p=Path('work/csr-lazy-up-geometry-20261026-01');u=Path('work/csr-u-reconstruct-cache-20261026-01')
shutil.copytree(u/'src',p/'src',dirs_exist_ok=True)
shutil.copyfile('work/csr-next-opportunities-20261026-02/pEqn.pressure-geometry.H',p/'src/pEqn.H')
b=(u/'build.sh').read_text().replace(u.name,p.name)
a=b.index('cp -p "$r/build/csr-construction-profile');z=b.index('lib=$c/platforms',a)
b=b[:a]+'''cp -p "$r/build/csr-lazy-u-combined-20261026-01/lib/libdfMatrices.a" lib/libdfMatrices.a
sha256sum "$r/build/csr-lazy-u-combined-20261026-01/lib/libdfMatrices.a" > REUSED_LIBRARY.sha256
'''+b[z:]
(p/'build.sh').write_text(b,newline='\n')
for name in ['remote.py','make_link.py']:(p/name).write_text((u/name).read_text().replace(u.name,p.name),newline='\n')
src=Path('work/csr-lazy-u-combined-min0-50-20261026-01');dst=Path('work/csr-lazy-up-geometry-min0-50-20261026-01');dst.mkdir(exist_ok=True)
for name in ['remote.py','prepare.sh','submit.sh','monitor.sh','fatal_check.py','summarize.py','INPUT-controlDict50','INPUT-fvSolution','RUN_REQUEST.json']:
 s=(src/name).read_text().replace(src.name,dst.name).replace('csr-lazy-u-combined-20261026-01',p.name).replace('LNJ_LAZY_GAMG=1','LNJ_LAZY_GAMG=1 LNJ_P_RECONSTRUCT_CACHE=1').replace('lnj-50-lazy-u','lnj-50-lazy-up').replace('seq 1 60','seq 1 120')
 (dst/name).write_text(s,newline='\n')
(dst/'RUN_REQUEST.json').write_text(json.dumps({'purpose':'Combined lazy GAMG plus U and pressure reconstruct geometry reuse','config':'minIter0,nFinestSweeps2,960MPI,50steps,t0.05','queue':'q_swlidelei','baseline':8753209,'incremental_reference':8754551,'window':'2-49','correctness':'7 final internal/boundary fields, same residual and iteration logs','no_off_arm':True},indent=2))
