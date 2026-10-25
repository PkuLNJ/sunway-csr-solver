"""Python 2.7/3: validate and aggregate small C++ rank stats, not payloads."""
from __future__ import print_function
import argparse
import decimal
import io
import json
import os
decimal.getcontext().prec=80
D=decimal.Decimal


def finite_decimal(s):
    v=D(s)
    if not v.is_finite():raise ValueError('nonfinite stats scalar')
    return v


def aggregate(directory,ranks):
    accum={};schema={};schema_files=0
    for rank in range(ranks):
        path=os.path.join(directory,'rank-%d.tsv'%rank)
        with io.open(path,encoding='utf-8') as f:lines=f.read().splitlines()
        if len(lines)<3 or lines[0]!='LNJ_COMPARE_RANK_V1\t%d'%rank:raise ValueError('bad stats rank/header')
        header=lines[1].split('\t');tail=lines[-1].split('\t')
        if len(tail)!=2 or tail[0]!='COMPLETE' or int(tail[1])!=len(lines)-3:raise ValueError('incomplete stats')
        schema_path=path+'.schema.tsv'
        if os.path.exists(schema_path):
            schema_files+=1
            with io.open(schema_path,encoding='utf-8') as f:sl=f.read().splitlines()
            if len(sl)<3 or sl[0]!='LNJ_COMPARE_SCHEMA_V1\t%d'%rank or sl[1]!='status\tstage\ttag\tleft_shape\tright_shape':raise ValueError('bad schema stats header')
            st=sl[-1].split('\t')
            if len(st)!=2 or st[0]!='COMPLETE' or int(st[1])!=len(sl)-3:raise ValueError('incomplete schema stats')
            ss=set()
            for line in sl[2:-1]:
                parts=line.split('\t')
                if len(parts)!=5 or parts[0] not in ('LEFT_ONLY','RIGHT_ONLY','SHAPE_MISMATCH'):raise ValueError('bad schema stats row')
                key=tuple(parts[:3])
                if key in ss:raise ValueError('duplicate schema stats')
                ss.add(key)
                if key not in schema:schema[key]=dict(status=parts[0],stage=parts[1],tag=parts[2],rank_count=0,first_rank=rank,left_shape=parts[3],right_shape=parts[4])
                schema[key]['rank_count']+=1
        seen=set()
        for line in lines[2:-1]:
            values=line.split('\t')
            if len(values)!=len(header):raise ValueError('bad stats columns')
            row=dict(zip(header,values));key=(row['stage'],row['tag'])
            if key in seen:raise ValueError('duplicate stats key')
            seen.add(key)
            if key not in accum:accum[key]=dict(stage=key[0],tag=key[1],dtype=row['dtype'],width=int(row['width']),
                count=0,ranks=0,byte_equal=True,finite=True,nonfinite_left=0,nonfinite_right=0,nonfinite_difference=0,
                first_difference=None,worst_difference=None,maxabs=D(0),left_sum=D(0),right_sum=D(0),diff_sum=D(0))
            out=accum[key]
            if row['dtype']!=out['dtype'] or int(row['width'])!=out['width']:raise ValueError('crossrank dtype mismatch')
            out['count']+=int(row['count']);out['ranks']+=1
            for flag in ('byte_equal','finite'):
                if row[flag] not in ('0','1'):raise ValueError('bad boolean stats')
                out[flag]=out[flag] and row[flag]=='1'
            for count in ('nonfinite_left','nonfinite_right','nonfinite_difference'):
                n=int(row[count])
                if n<0:raise ValueError('negative stats count')
                out[count]+=n
            if out['first_difference'] is None and row['first_index']!='-':
                out['first_difference']=dict(rank=rank,index=int(row['first_index']),left=row['first_left'],right=row['first_right'])
            magnitude=finite_decimal(row['maxabs'])
            if magnitude>out['maxabs']:
                out['maxabs']=magnitude
                out['worst_difference']=dict(rank=rank,index=int(row['max_index']),left=row['max_left'],right=row['max_right'])
            for name in ('left','right','diff'):
                scale,ssq=finite_decimal(row[name+'_scale']),finite_decimal(row[name+'_ssq'])
                if scale<0 or ssq<0:raise ValueError('invalid norm state')
                out[name+'_sum']+=scale*scale*ssq
    result=[]
    for key in sorted(accum):
        out=accum[key]
        for name in ('left','right','diff'):out[name+'_l2_decimal']=str(out.pop(name+'_sum').sqrt())
        left,diff=D(out['left_l2_decimal']),D(out['diff_l2_decimal'])
        out['relative_l2_decimal']=str(diff/left) if left else ('0' if not diff else None)
        out['maxabs_decimal']=str(out.pop('maxabs'));result.append(out)
    if schema_files not in (0,ranks):raise ValueError('missing rank schema report')
    return dict(format='LNJ_CAPTURE_GLOBAL_STATS_V1',validated_ranks=ranks,arrays=result,
        schema_reports_validated=schema_files,schema_differences=[schema[k] for k in sorted(schema)],
        numeric_encoding='decimal strings preserve long-double range')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory');p.add_argument('--ranks',type=int,required=True)
    a=p.parse_args()
    if a.ranks<1:p.error('ranks must be positive')
    print(json.dumps(aggregate(a.directory,a.ranks),indent=2,sort_keys=True,allow_nan=False))


if __name__=='__main__':main()
