"""Python 2.7/3 streaming capture validator and global per-tag comparison."""
from __future__ import print_function
import argparse
import hashlib
import io
import json
import math
import os
import struct


def finite(x):
    return not (math.isnan(x) or math.isinf(x))


class Norm(object):
    def __init__(self): self.scale, self.ssq = 0.0, 1.0
    def add(self, x):
        x = abs(x)
        if not x: return
        if self.scale < x:
            self.ssq = 1.0 + self.ssq*(self.scale/x)**2
            self.scale = x
        else: self.ssq += (x/self.scale)**2
    def value(self):
        v = self.scale*math.sqrt(self.ssq)
        return v if finite(v) else None
    def ratio(self, other):
        if not other.scale: return 0.0 if not self.scale else None
        v = (self.scale/other.scale)*math.sqrt(self.ssq/other.ssq)
        return v if finite(v) else None
    def as_dict(self): return dict(value=self.value(),scale=self.scale,ssq=self.ssq)


def inventory(directory, ranks, selected=None):
    rank_list=range(ranks) if selected is None else list(selected)
    if any(r<0 or r>=ranks for r in rank_list):raise ValueError('selected rank outside range')
    expected = set('rank-%d.%s'%(r,e) for r in rank_list for e in ('bin','tsv','complete'))
    actual = set(n for n in os.listdir(directory) if n.startswith('rank-')) if selected is None else set(n for n in expected if os.path.isfile(os.path.join(directory,n)))
    if actual != expected: raise ValueError('rank files mismatch missing=%r extra=%r'%(sorted(expected-actual),sorted(actual-expected)))
    result = {}
    for rank in rank_list:
        prefix = os.path.join(directory,'rank-%d'%rank)
        with io.open(prefix+'.tsv',encoding='utf-8') as f: lines=f.read().splitlines()
        if not lines or lines[0] not in ('LNJ_CAPTURE_V1\tlittle','LNJ_CAPTURE_V1\tbig'): raise ValueError('bad header')
        endian = '<' if lines[0].endswith('little') else '>'
        with io.open(prefix+'.complete',encoding='ascii') as f: marker=f.read().strip().split('\t')
        if len(marker)!=3 or marker[0]!='LNJ_CAPTURE_COMPLETE_V1': raise ValueError('bad completion')
        records={}; end=0
        for line in lines[1:]:
            p=line.split('\t')
            if len(p)!=7: raise ValueError('bad index row')
            stage,tag,dtype=p[:3]; width,count,offset,size=map(int,p[3:])
            if not stage or not tag or dtype not in ('f','i','u','bytes'): raise ValueError('bad schema')
            if width<=0 or min(count,offset,size)<0 or count*width!=size: raise ValueError('bad sizes')
            if dtype=='f' and width not in (4,8): raise ValueError('bad floating width')
            if dtype in ('i','u') and width not in (1,2,4,8): raise ValueError('bad integer width')
            if offset!=end: raise ValueError('payload hole/overlap')
            key=(stage,tag)
            if key in records: raise ValueError('duplicate tag')
            records[key]=dict(dtype=dtype,width=width,count=count,offset=offset,size=size,endian=endian)
            end+=size
        if list(map(int,marker[1:]))!=[len(records),end]: raise ValueError('completion totals mismatch')
        if os.path.getsize(prefix+'.bin')!=end: raise ValueError('truncated/trailing payload')
        result[rank]=records
    return result


def unpack(raw, record):
    width=record['width']; dtype=record['dtype']
    if dtype=='bytes': return bytearray(raw)
    code={'f':{4:'f',8:'d'},'i':{1:'b',2:'h',4:'i',8:'q'},'u':{1:'B',2:'H',4:'I',8:'Q'}}[dtype][width]
    return struct.unpack(record['endian']+str(len(raw)//width)+code,raw)


def compare(left,right,ranks):
    li,ri=inventory(left,ranks),inventory(right,ranks)
    accum={}
    for rank in range(ranks):
        if set(li[rank])!=set(ri[rank]): raise ValueError('arm tag mismatch rank %d'%rank)
        with open(os.path.join(left,'rank-%d.bin'%rank),'rb') as a, open(os.path.join(right,'rank-%d.bin'%rank),'rb') as b:
            for key in sorted(li[rank]):
                ar,br=li[rank][key],ri[rank][key]
                if any(ar[k]!=br[k] for k in ('dtype','width','count')): raise ValueError('arm array schema mismatch %r'%(key,))
                if key not in accum:
                    accum[key]=dict(ranks=0,count=0,finite=True,nonfinite_left=0,nonfinite_right=0,
                        nonfinite_difference=0,byte_equal=True,first_difference=None,maxabs=0,
                        left_norm=Norm(),right_norm=Norm(),difference_norm=Norm(),dtype=ar['dtype'],width=ar['width'])
                out=accum[key]
                if out['dtype']!=ar['dtype'] or out['width']!=ar['width']: raise ValueError('cross-rank type mismatch')
                out['ranks']+=1; out['count']+=ar['count']
                a.seek(ar['offset']); b.seek(br['offset']); remaining=ar['size']; base=0
                while remaining:
                    size=min(remaining,max(1,1048576//ar['width'])*ar['width'])
                    aa,bb=a.read(size),b.read(size)
                    if len(aa)!=size or len(bb)!=size: raise ValueError('short payload read')
                    out['byte_equal'] = out['byte_equal'] and aa==bb
                    for j,(x,y) in enumerate(zip(unpack(aa,ar),unpack(bb,br))):
                        if x!=y and out['first_difference'] is None: out['first_difference']=dict(rank=rank,index=base+j)
                        xf,yf=finite(x),finite(y)
                        if not xf: out['nonfinite_left']+=1
                        if not yf: out['nonfinite_right']+=1
                        if not xf or not yf: out['finite']=False; continue
                        out['left_norm'].add(x); out['right_norm'].add(y)
                        d=x-y
                        if not finite(d): out['finite']=False; out['nonfinite_difference']+=1; continue
                        out['difference_norm'].add(d); out['maxabs']=max(out['maxabs'],abs(d))
                    base+=size//(1 if ar['dtype']=='bytes' else ar['width']); remaining-=size
    rows=[]
    for key in sorted(accum):
        out=accum[key]
        out['relative_l2']=out['difference_norm'].ratio(out['left_norm'])
        for name in ('left_norm','right_norm','difference_norm'): out[name]=out[name].as_dict()
        out['stage'],out['tag']=key; rows.append(out)
    return dict(format='LNJ_CAPTURE_COMPARE_V1',validated_ranks=ranks,arrays=rows)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('left'); p.add_argument('right',nargs='?'); p.add_argument('--ranks',type=int,required=True)
    a=p.parse_args()
    if a.ranks<1: p.error('ranks must be positive')
    result=compare(a.left,a.right,a.ranks) if a.right else dict(validated_ranks=len(inventory(a.left,a.ranks)))
    print(json.dumps(result,indent=2,sort_keys=True,allow_nan=False))


if __name__=='__main__': main()
