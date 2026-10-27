import re,json,sys
from pathlib import Path
p=Path(sys.argv[1]);s=p.read_text();steps=re.findall(r'^Time = (\S+)',s,re.M);c=[float(x) for x in re.findall(r'ClockTime = ([\d.]+)',s)]
assert len(steps)==50 and float(steps[-1])==.05 and re.search(r'^End\s*$',s,re.M)
assert not re.search(r'FOAM FATAL|MPI_ABORT|Segmentation fault|Floating point exception',s,re.I)
missing=[]
for rank in range(960):
 for f in ('U','p_rgh','TT','S','alpha.water','p','rhok'):
  q=p.parent/('processor%d'%rank)/'0.05'/f
  if not q.is_file() or q.stat().st_size==0:missing.append(str(q))
print(json.dumps(dict(steps=50,window2_49=c[48]-c[0],clock50=c[-1],missing=missing,fields=6720),indent=2))
assert not missing
