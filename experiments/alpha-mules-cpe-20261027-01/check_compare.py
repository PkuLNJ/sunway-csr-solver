import sys,re,json,math
text=open(sys.argv[1]).read()
assert 'COMPARISON_COMPLETE' in text and 'job_finished' in text
rows=[];keys=set()
for line in text.splitlines():
 if not line.startswith('FIELD='):continue
 d=dict(x.split('=',1) for x in line.split());key=(d['FIELD'],d['REGION']);assert key not in keys;keys.add(key)
 assert int(d['COUNT'])>0
 for k in ['REL_L2','REL_LINF','MAX_ABS','REF_MAX','BAI_MIN','BAI_MAX']:
  v=float(d[k]);assert not(math.isnan(v) or math.isinf(v)),(key,k)
 for k in ['REL_L2','REL_LINF']:assert 0<=float(d[k])<=1e-12,(key,k,d[k])
 rows.append(d)
expected=set((f,r) for f in ['U','p_rgh','TT','S','alpha.water','p','rhok'] for r in ['internal','boundary_numeric_tokens'])
assert keys==expected
print(json.dumps({'pass':True,'threshold':1e-12,'groups':rows},sort_keys=True))
