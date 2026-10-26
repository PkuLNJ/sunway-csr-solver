from pathlib import Path
import re,json,difflib
p=Path('work/csr-min0-final-compare-20261026-01')
result={}
for j in [8749884,8753209]:
 t=(p/('solver.%d.log'%j)).read_text().replace('\r','')
 final=re.split(r'^Time = ',t,flags=re.M)[-1]
 continuity=re.findall(r'^time step continuity errors\s*:.*$',t,re.M)
 data={'steps':len(re.findall(r'^Time = ',t,re.M)),'end':bool(re.search(r'^End\s*$',t,re.M)), 'final_continuity':re.findall(r'^time step continuity errors\s*:.*$',final,re.M),'final_alpha':re.findall(r'^Phase-1 volume fraction.*$',final,re.M),'pressure_last':re.findall(r'^.*Solving for p_rgh,.*$',final,re.M)}
 result[j]=data
print(json.dumps(result,indent=2));(p/'LOG_REVIEW.json').write_text(json.dumps(result,indent=2))
a=(p/'fvSolution.8749884').read_text();b=(p/'fvSolution.8753209').read_text();diff=''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True)));print(diff);(p/'fvSolution.diff').write_text(diff)

