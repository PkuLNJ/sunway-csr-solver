import sys,re,json
p,arm=sys.argv[1:3];j=open(p+'/JOB_ID').read().strip();t=open(p+'/solver.'+j+'.log').read();e=open(p+'/ENVIRONMENT.txt').read()
assert 'LNJ_P_RECONSTRUCT_CACHE=1' in e
assert 'LNJ_LEVEL_SGS_MODE=tilepermute' in e
assert 'mode=tilepermute' in t
assert 'LNJ_MULES_CPE='+('0' if arm.startswith('off') else '1') in e
print(json.dumps({'job':j,'arm':arm,'environment_verified':True}))
