"""Python2/3 timing summary of exactly one corrected-CSR50 run."""
from __future__ import print_function
import json,os,re,sys,math,csv
from fatal_check import find_error
def parse(text):
 text=text.replace('\r','');rows=[];current=None
 for line in text.splitlines():
  m=re.match(r'^Time\s*=\s*([0-9.eE+-]+)\s*$',line)
  if m:current=float(m.group(1))
  m=re.match(r'^Loop Time\s*=\s*([0-9.eE+-]+)\s+s\s*$',line)
  if m:
   value=float(m.group(1));assert current is not None and not(math.isnan(value) or math.isinf(value))
   assert value>=0;rows.append(dict(step=len(rows)+1,time=current,loop_seconds=value))
 assert len(rows)==50,'expected50 Loop Time entries,found%d'%len(rows)
 for i,row in enumerate(rows):assert abs(row['time']-(i+1)*.001)<1e-10,'unexpected time/step sequence'
 assert re.search(r'^End\s*$',text,re.M),'normal End missing'
 assert find_error(text) is None,'substantive fatal marker in log'
 clocks=re.findall(r'ExecutionTime\s*=\s*([0-9.eE+-]+)\s+s\s+ClockTime\s*=\s*([0-9.eE+-]+)\s+s',text)
 assert clocks,'ExecutionTime/ClockTime missing'
 execution,clock=map(float,clocks[-1]);assert clock>=0 and execution>=0
 windows=[]
 for first,last in [(1,50),(2,10),(2,49),(2,50)]:
  values=[r['loop_seconds'] for r in rows[first-1:last]];total=sum(values)
  windows.append(dict(first=first,last=last,count=len(values),sum_seconds=total,mean_seconds=total/len(values),min_seconds=min(values),max_seconds=max(values)))
 return dict(status='FIFTY_STEPS_NORMAL_END_TIMING_VERIFIED',steps=rows,windows=windows,first_step_seconds=rows[0]['loop_seconds'],loop_time_sum50=sum(r['loop_seconds'] for r in rows),final_clock_seconds=clock,final_execution_seconds=execution,
  interpretation='Loop Time follows this solver source counter; end-of-step runTime.write is outside that counter. ClockTime includes startup/writes. Windows predeclared,not selected after observing speed.')
def main(d):
 job=open(d+'/JOB_ID').read().strip();text=open(d+'/solver.'+job+'.log').read();result=parse(text)
 result['job_id']=job
 missing=[]
 for rank in range(960):
  for field in ('U','p_rgh','TT','S','alpha.water','p','rhok'):
   if not os.path.isfile(d+'/processor%d/0.05/'%rank+field):missing.append([rank,field])
 result['final_fields_checked']=6720;result['missing_final_fields']=missing
 assert not missing,'final seven-field output incomplete'
 fd=os.open(d+'/TIMING_RESULT.json',os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
 with os.fdopen(fd,'w') as f:json.dump(result,f,indent=2,sort_keys=True,allow_nan=False)
 with open(d+'/TIMING_STEPS.csv','w') as f:
  f.write('step,time,loop_seconds\n')
  for row in result['steps']:f.write('%d,%.8f,%.17g\n'%(row['step'],row['time'],row['loop_seconds']))
 print(json.dumps(dict(job_id=job,first_step_seconds=result['first_step_seconds'],clock_seconds=result['final_clock_seconds'],windows=result['windows']),sort_keys=True))
if __name__=='__main__':main(sys.argv[1])
