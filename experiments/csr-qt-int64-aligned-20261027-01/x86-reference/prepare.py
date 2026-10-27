from pathlib import Path
import os,shutil,re,json,hashlib
src=Path('/public/home/chenzhi/xyc/QT_OpenFOAM/interFoam_case/960')
root=Path('/public/home/chenzhi/lnj/qt-int64-960-50-20261027-01')
root.mkdir()
d=root/'case';d.mkdir()
for name in ('system','constant','seaWaveData'):
 if (src/name).exists(): shutil.copytree(str(src/name),str(d/name),symlinks=False)
for rank in range(960):
 s=src/('processor%d'%rank); t=d/s.name;t.mkdir();(t/'0').mkdir()
 assert (s/'constant/polyMesh/owner').is_file()
 os.symlink(str(s/'constant'),str(t/'constant'))
 for f in (s/'0').iterdir():
  if f.name in ('TT','S'):shutil.copy2(str(f),str(t/'0'/f.name))
  else:os.symlink(str(f),str(t/'0'/f.name))
 if rank%120==119:print('READY_RANKS=%d'%(rank+1),flush=True)
f=d/'system/controlDict';text=f.read_text()
for k,v in {'startFrom':'startTime','startTime':'0','endTime':'0.05','deltaT':'0.001','writeControl':'timeStep','writeInterval':'50','writePrecision':'17','runTimeModifiable':'no'}.items():
 text,n=re.subn(r'(?m)^(\s*'+k+r'\s+)[^;]+;',lambda m:m.group(1)+v+';',text,count=1);assert n==1,k
block='''functions
{
    lnjFinalSevenFields
    {
        type writeObjects;
        libs ("libutilityFunctionObjects.so");
        objects (U p_rgh TT S alpha.water p rhok);
        writeOption anyWrite;
        executeControl none;
        writeControl timeStep;
        writeInterval 50;
    }
'''
text,n=re.subn(r'functions\s*\{',lambda m:block,text,count=1);assert n==1
f.write_text(text)
of=Path('/public/home/chenzhi/xyc/QT_OpenFOAM/FP64INT64/OpenFOAM-7')
(root/'bin').mkdir();(root/'lib').mkdir()
for s,t in [(of/'platforms/linux64GccDPInt64Opt/bin/boussinesqFoam',root/'bin/boussinesqFoam'),(of/'platforms/linux64GccDPInt64Opt/lib/libseaWave.so',root/'lib/libseaWave.so')]:shutil.copy2(str(s),str(t))
for sub in ('applications/solvers/multiphase/boussinesqFoam','applications/solvers/multiphase/VoF','src/seaWave'):
 shutil.copytree(str(of/sub),str(root/'source'/sub),ignore=shutil.ignore_patterns('linux*','lnInclude','*.o','slurm*'),symlinks=False)
(root/'INPUT_READY').write_text('960 ranks; 0.001 x 50; final seven fields; TT/S private; readonly inputs linked\n')
print('INPUT_READY',flush=True)
