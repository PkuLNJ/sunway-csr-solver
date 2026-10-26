from pathlib import Path
import re,json,sys
p=Path(sys.argv[1]);s=p.read_text().replace('\r','')
rows=[]
for name,calls,mean,mx in re.findall(r'^LNJ_CONSTRUCT (\S+) calls_mean=([\d.eE+-]+) seconds_mean=([\d.eE+-]+) seconds_max=([\d.eE+-]+)',s,re.M):
 rows.append(dict(name=name,calls_mean=float(calls),seconds_mean=float(mean),seconds_max=float(mx)))
assert len(rows)==16,'16 timing groups required'
assert re.search(r'^End\s*$',s,re.M)
trace=re.findall(r'^.*Solving for (\S+), Initial residual = ([^,]+), Final residual = ([^,]+), No Iterations (\d+)',s,re.M)
r={'timing':rows,'solver_trace':trace,'status':'TIMING_CAPTURED_NUMERIC_CHECK_PENDING'}
if len(sys.argv)>2:
 ref=Path(sys.argv[2]).read_text().replace('\r','');rtrace=re.findall(r'^.*Solving for (\S+), Initial residual = ([^,]+), Final residual = ([^,]+), No Iterations (\d+)',ref,re.M)
 r['reference_trace_count']=len(rtrace);r['candidate_trace_count']=len(trace);r['trace_exact_match']=trace==rtrace
json.dump(r,sys.stdout,indent=2)
