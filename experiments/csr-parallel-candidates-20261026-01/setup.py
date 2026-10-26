from pathlib import Path
import shutil,json,re
root=Path('work');p=root/'csr-local-reuse-20261026-01';(p/'src').mkdir(parents=True,exist_ok=True)
u=root/'csr-u-reconstruct-cache-20261026-01';shutil.copytree(u/'src',p/'src',dirs_exist_ok=True)
s=(p/'src/pEqn.H').read_text();mark='    while (pimple.correctNonOrthogonal())'
insert='''    const char* hoistEnv=std::getenv("LNJ_PRESSURE_DIV_REUSE");
    const bool hoistDiv=hoistEnv && std::string(hoistEnv)=="1";
    tmp<volScalarField> stableDiv;
    if (hoistDiv) stableDiv=fvc::div(phiHbyA);
    auto pressureDiv=[&]() -> tmp<volScalarField> {
        if (hoistDiv) return tmp<volScalarField>(stableDiv());
        return fvc::div(phiHbyA);
    };
'''
s=s.replace(mark,insert+mark).replace('fvm::laplacian(rAUf, p_rgh) == fvc::div(phiHbyA)','fvm::laplacian(rAUf, p_rgh) == pressureDiv()');(p/'src/pEqn.H').write_text(s,newline='\n')
q=root/'csr-parallel-candidates-20261026-01'
s=(q/'alphaEqn.original.H').read_text();assert s.count('volScalarField alpha10("alpha10", alpha1);')==1
s=s.replace('{\n','{\n    const char* snapshotEnv=std::getenv("LNJ_ALPHA_LAZY_SNAPSHOT");\n    const bool lazySnapshot=snapshotEnv && std::string(snapshotEnv)=="1";\n',1).replace('volScalarField alpha10("alpha10", alpha1);','tmp<volScalarField> talpha10;\n            if (!lazySnapshot || aCorr != 0)\n            {\n                talpha10 = new volScalarField("alpha10", alpha1);\n            }').replace('0.5*alpha10;','0.5*talpha10();')
(p/'src/alphaEqn.H').write_text(s,newline='\n');shutil.copyfile(q/'alphaEqnSubCycle.original.H',p/'src/alphaEqnSubCycle.H')
b=(root/'csr-lazy-up-geometry-20261026-01/build.sh').read_text().replace('csr-lazy-up-geometry-20261026-01',p.name);(p/'build.sh').write_text(b,newline='\n')
for n in ['remote.py','make_link.py']:(p/n).write_text((u/n).read_text().replace(u.name,p.name),newline='\n')
p=root/'csr-lazy-reset-combined-20261026-01';(p/'src').mkdir(parents=True,exist_ok=True)
b=(root/'csr-lazy-u-combined-20261026-01/build.sh').read_text().replace('w=$r/build/csr-lazy-u-combined-20261026-01','w=$r/build/'+p.name).replace('u=$r/build/csr-u-reconstruct-cache-20261026-01','u=$r/build/csr-lazy-u-combined-20261026-01').replace('lazy=$r/build/csr-lazy-gamg-20261026-01','lazy=$r/build/csr-reset-trim-20261026-01').replace('CSRPCG','CSRGAMGPreconditioner')
(p/'build.sh').write_text(b,newline='\n')
for n in ['remote.py','make_link.py']:(p/n).write_text((u/n).read_text().replace(u.name,p.name),newline='\n')
(p/'src/COMPOSITION.txt').write_text('8754551 combined objects/library plus existing reset-trim CSRGAMGPreconditioner.o. Default reset full; experiment minimal.\n')
variants=[('div','csr-local-reuse-20261026-01','LNJ_PRESSURE_DIV_REUSE=1'),('alpha','csr-local-reuse-20261026-01','LNJ_ALPHA_LAZY_SNAPSHOT=1'),('reset','csr-lazy-reset-combined-20261026-01','LNJ_VCYCLE_RESET=minimal'),('coarse1','csr-lazy-u-combined-20261026-01','')]
src=root/'csr-lazy-u-combined-min0-50-20261026-01'
for name,build,env in variants:
 p=root/('csr-parallel-'+name+'-min0-50-20261026-01');p.mkdir(exist_ok=True)
 for f in ['remote.py','prepare.sh','submit.sh','monitor.sh','fatal_check.py','summarize.py','INPUT-controlDict50','INPUT-fvSolution']:
  s=(src/f).read_text().replace(src.name,p.name).replace('csr-lazy-u-combined-20261026-01',build).replace('LNJ_LAZY_GAMG=1','LNJ_LAZY_GAMG=1 '+env).replace('lnj-50-lazy-u','lnj-50-'+name).replace('seq 1 60','seq 1 180')
  if f=='INPUT-fvSolution' and name=='coarse1':
   start=s.index('    p_rgh\n');end=s.index('    p_rghFinal',start);sub=s[start:end]
   for key in ['nPreSweeps','nPostSweeps','maxPreSweeps','maxPostSweeps']:sub=re.sub(r'('+key+r'\s+)2;',r'\g<1>1;',sub)
   s=s[:start]+sub+s[end:]
  (p/f).write_text(s,newline='\n')
 (p/'RUN_REQUEST.json').write_text(json.dumps({'variant':name,'base':8754551,'original_baseline':8753209,'only_change':env or 'main-pressure coarse pre/post/max sweeps2 to1;finest2 unchanged','acceptance':'exact fields for implementation variants; convergence conservation field review for coarse1','window':'2-49,50steps,t0.05','queue':'lidelei','no_off_arm':True},indent=2))
print('Prepared 4 independent arms and2 builds')
