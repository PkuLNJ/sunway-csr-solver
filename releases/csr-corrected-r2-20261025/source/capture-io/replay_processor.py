"""Python 2.7/3 independent scalar LDU arithmetic, processor interfaces only.

Array-container binding is deliberately explicit: no guessed OpenFOAM tag names.
"""
from __future__ import print_function
import argparse
import json
import os
from check_capture_py2 import finite, Norm, inventory, unpack


def scalar_ax(diag, lower, upper, lower_addr, upper_addr, x, patches, remote):
    """remote(neighbour_rank, neighbour_patch) -> (peer_facecells, peer_x).

    patches are dicts: type, facecells, boundary_coeff; processor additionally
    neighbour_rank, neighbour_patch. Face ordering is the captured processor
    interface ordering. physical patches must already be folded into diag/b.
    """
    n=len(diag)
    if len(x)!=n or not (len(lower)==len(upper)==len(lower_addr)==len(upper_addr)):
        raise ValueError('matrix/array size mismatch')
    y=[diag[i]*x[i] for i in range(n)]
    for k,(lo,up) in enumerate(zip(lower_addr,upper_addr)):
        if not (0<=lo<n and 0<=up<n): raise ValueError('local addressing out of bounds')
        y[lo]+=upper[k]*x[up]
        y[up]+=lower[k]*x[lo]
    for patch in patches:
        kind=patch['type']
        if kind=='physical': continue
        if kind!='processor': raise ValueError('unsupported coupled patch type: '+kind)
        cells,coeff=patch['facecells'],patch['boundary_coeff']
        if len(cells)!=len(coeff): raise ValueError('patch coeff/facecells mismatch')
        peer_cells,peer_x=remote(patch['neighbour_rank'],patch['neighbour_patch'])
        if len(peer_cells)!=len(cells): raise ValueError('peer processor face count mismatch')
        for i,cell in enumerate(cells):
            peer=peer_cells[i]
            if not (0<=cell<n and 0<=peer<len(peer_x)): raise ValueError('processor addressing out of bounds')
            y[cell]-=coeff[i]*peer_x[peer]
    if not all(finite(v) for v in y): raise ValueError('nonfinite replay Ax')
    return y


def compare_vector(expected, actual):
    if len(expected)!=len(actual): raise ValueError('replay vector size mismatch')
    en,dn=Norm(),Norm(); maximum=0.0; first=None
    for i,(x,y) in enumerate(zip(expected,actual)):
        if not finite(x) or not finite(y): raise ValueError('nonfinite replay comparison')
        d=x-y
        if not finite(d): raise ValueError('nonfinite replay difference')
        if d and first is None: first=i
        en.add(x);dn.add(d);maximum=max(maximum,abs(d))
    return dict(first_difference=first,maxabs=maximum,relative_l2=dn.ratio(en),difference_norm=dn.as_dict())


class Container(object):
    def __init__(self,directory,ranks,lazy=False):
        self.directory=directory; self.ranks=ranks;self.index={} if lazy else inventory(directory,ranks)
    def ensure(self,rank):
        if rank not in self.index:self.index.update(inventory(self.directory,self.ranks,selected=[rank]))
    def array(self,rank,stage,tag):
        self.ensure(rank)
        record=self.index[rank][(stage,tag)]
        with open(os.path.join(self.directory,'rank-%d.bin'%rank),'rb') as f:
            f.seek(record['offset']); raw=f.read(record['size'])
        if len(raw)!=record['size']:raise ValueError('short replay read')
        return list(unpack(raw,record))
    def text(self,rank,stage,tag):
        return bytearray(self.array(rank,stage,tag)).decode('utf-8')
    def integer(self,rank,stage,tag):
        values=self.array(rank,stage,tag)
        if len(values)!=1 or not finite(values[0]) or values[0]!=int(values[0]):raise ValueError('invalid integer metadata')
        return int(values[0])


def replay(directory,ranks,matrix_stage,mesh_stage,equation_stage,product_index=0,only_rank=None):
    if product_index not in (0,1,2):raise ValueError('unsupported product index')
    if only_rank is not None and not 0<=only_rank<ranks:raise ValueError('rank outside range')
    selected=list(range(ranks)) if only_rank is None else [only_rank]
    c=Container(directory,ranks,lazy=only_rank is not None); patches={}
    def load_patches(rank):
        if rank in patches:return
        if c.integer(rank,mesh_stage,'replaySupported')!=1: raise ValueError('unsupported coupled mesh rank %d'%rank)
        records=[]
        for p in range(c.integer(rank,mesh_stage,'patchCount')):
            prefix='patch.%d'%p
            kind=c.text(rank,mesh_stage,prefix+'.type')
            coupled=c.integer(rank,mesh_stage,prefix+'.coupled')
            if not coupled: continue
            if kind!='processor':raise ValueError('unsupported coupled type '+kind)
            records.append(dict(index=p,type=kind,
                neighbour_rank=c.integer(rank,mesh_stage,prefix+'.neighbProcNo')))
        patches[rank]=records
    for rank in selected:load_patches(rank)
    # Do not assume patch indices match across ranks. Require unique reciprocal
    # processor patch; multiple patches per rank pair need explicit peer metadata.
    for rank in selected:
        for p in patches[rank]:
            peer=p['neighbour_rank']
            if not 0<=peer<ranks:raise ValueError('neighbour rank out of bounds')
            load_patches(peer)
            matches=[q for q in patches[peer] if q['neighbour_rank']==rank]
            if len(matches)!=1:raise ValueError('ambiguous/missing reciprocal processor patch')
            p['neighbour_patch']=matches[0]['index']
    global_expected,global_difference=Norm(),Norm(); global_max=0.0; first=None; rows=[]
    product='actual.Ax%d'%product_index
    for rank in selected:
        def get(tag):return c.array(rank,matrix_stage,tag)
        x=c.array(rank,equation_stage,product+'.input')
        if product_index==0 and x!=get('x0'):raise ValueError('actual first Ax input differs from captured x0')
        local_patches=[]
        for p in patches[rank]:
            q=dict(p); prefix='patch.%d'%p['index']
            q['facecells']=get(prefix+'.faceCells')
            q['boundary_coeff']=get(prefix+'.boundaryCoeffs')
            local_patches.append(q)
        def remote(peer,patch):
            return (c.array(peer,matrix_stage,'patch.%d.faceCells'%patch),
                    c.array(peer,equation_stage,product+'.input'))
        keys=c.index[rank]
        has=(c.integer(rank,matrix_stage,'matrix.hasLower') or c.integer(rank,matrix_stage,'matrix.hasUpper')) if (matrix_stage,'matrix.hasLower') in keys and (matrix_stage,'matrix.hasUpper') in keys else ((matrix_stage,'lower') in keys or (matrix_stage,'upper') in keys)
        y=scalar_ax(get('diag'),get('lower') if has else [],get('upper') if has else [],
                    get('lowerAddr') if has else [],get('upperAddr') if has else [],x,local_patches,remote)
        expected=c.array(rank,equation_stage,product)
        row=compare_vector(expected,y);row['rank']=rank;rows.append(row)
        for i,(a,b) in enumerate(zip(expected,y)):
            d=a-b;global_expected.add(a);global_difference.add(d);global_max=max(global_max,abs(d))
            if d and first is None:first=dict(rank=rank,index=i)
    return dict(format='LNJ_PROCESSOR_REPLAY_V1',ranks=ranks,selected_ranks=selected,loaded_index_ranks=sorted(c.index),product_index=product_index,matrix_stage=matrix_stage,
        mesh_stage=mesh_stage,equation_stage=equation_stage,maxabs=global_max,
        first_difference=first,relative_l2=global_difference.ratio(global_expected),
        expected_norm=global_expected.as_dict(),difference_norm=global_difference.as_dict(),per_rank=rows)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory');p.add_argument('--ranks',type=int,required=True)
    p.add_argument('--matrix-stage',required=True);p.add_argument('--mesh-stage',required=True)
    p.add_argument('--equation-stage',required=True)
    p.add_argument('--product-index',type=int,choices=(0,1,2),default=0)
    p.add_argument('--rank',type=int,default=None)
    a=p.parse_args()
    if a.ranks<1:p.error('ranks must be positive')
    print(json.dumps(replay(a.directory,a.ranks,a.matrix_stage,a.mesh_stage,a.equation_stage,a.product_index,a.rank),indent=2,allow_nan=False))


if __name__=='__main__':main()
