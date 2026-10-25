"""Lossless capture validation/comparison; standard library only, Python 3."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def inventory(directory, ranks):
    directory = Path(directory)
    expected = {'rank-%d.%s' % (r, ext) for r in range(ranks)
                for ext in ('bin', 'tsv', 'complete')}
    actual = {p.name for p in directory.glob('rank-*')}
    if actual != expected:
        raise ValueError('rank files mismatch: missing=%r extra=%r' %
                         (sorted(expected-actual), sorted(actual-expected)))
    result = {}
    for rank in range(ranks):
        prefix = directory / ('rank-%d' % rank)
        lines = prefix.with_suffix('.tsv').read_text(encoding='utf-8').splitlines()
        if not lines or lines[0] not in ('LNJ_CAPTURE_V1\tlittle', 'LNJ_CAPTURE_V1\tbig'):
            raise ValueError('invalid header rank %d' % rank)
        endian = '<' if lines[0].endswith('little') else '>'
        marker = prefix.with_suffix('.complete').read_text(encoding='ascii').strip().split('\t')
        if len(marker) != 3 or marker[0] != 'LNJ_CAPTURE_COMPLETE_V1':
            raise ValueError('invalid completion marker')
        end = 0
        records = {}
        for line in lines[1:]:
            parts = line.split('\t')
            if len(parts) != 7:
                raise ValueError('malformed index')
            stage, tag, dtype = parts[:3]
            width, count, offset, size = map(int, parts[3:])
            if not stage or not tag or dtype not in ('f', 'i', 'u', 'bytes'):
                raise ValueError('invalid metadata')
            if width <= 0 or min(count, offset, size) < 0 or size != width*count:
                raise ValueError('invalid sizes')
            if (dtype == 'f' and width not in (4, 8)) or (dtype in ('i','u') and width not in (1,2,4,8)):
                raise ValueError('unsupported dtype width')
            if offset != end:
                raise ValueError('overlap or hole in payload')
            key = (stage, tag)
            if key in records:
                raise ValueError('duplicate stage/tag')
            records[key] = dict(dtype=dtype, width=width, count=count,
                                offset=offset, size=size, endian=endian)
            end += size
        if list(map(int, marker[1:])) != [len(records), end]:
            raise ValueError('completion totals mismatch')
        if prefix.with_suffix('.bin').stat().st_size != end:
            raise ValueError('payload truncated or trailing data')
        result[rank] = records
    return result


class ScaledNorm:
    def __init__(self):
        self.scale, self.ssq = 0.0, 1.0

    def add(self, x):
        x = abs(x)
        if not x:
            return
        if self.scale < x:
            self.ssq = 1 + self.ssq*(self.scale/x)**2
            self.scale = x
        else:
            self.ssq += (x/self.scale)**2

    def value(self):
        return self.scale*math.sqrt(self.ssq)


def values(raw, record):
    dtype, width = record['dtype'], record['width']
    if dtype == 'bytes':
        return iter(raw)
    codes = {'f': {4:'f',8:'d'}, 'i':{1:'b',2:'h',4:'i',8:'q'},
             'u':{1:'B',2:'H',4:'I',8:'Q'}}
    return (x[0] for x in struct.iter_unpack(record['endian']+codes[dtype][width], raw))


def compare(left, right, ranks):
    li, ri = inventory(left, ranks), inventory(right, ranks)
    report = []
    for rank in range(ranks):
        if set(li[rank]) != set(ri[rank]):
            raise ValueError('stage/tag mismatch rank %d' % rank)
        with open(Path(left)/('rank-%d.bin'%rank),'rb') as a, open(Path(right)/('rank-%d.bin'%rank),'rb') as b:
            for key, ar in li[rank].items():
                br = ri[rank][key]
                if any(ar[k] != br[k] for k in ('dtype','width','count')):
                    raise ValueError('array schema mismatch %r'% (key,))
                a.seek(ar['offset']); b.seek(br['offset'])
                an, bn, dn = ScaledNorm(), ScaledNorm(), ScaledNorm()
                first, maxabs, finite, byte_equal = None, 0, True, True
                ha, hb = hashlib.sha256(), hashlib.sha256()
                remain, base = ar['size'], 0
                while remain:
                    # Chunk is aligned to scalar width, including arbitrary bytes records.
                    chunk = min(remain, ar['width']*131072)
                    aa, bb = a.read(chunk), b.read(chunk)
                    if len(aa)!=chunk or len(bb)!=chunk: raise ValueError('short read')
                    ha.update(aa); hb.update(bb); byte_equal &= aa == bb
                    for j,(x,y) in enumerate(zip(values(aa,ar),values(bb,br))):
                        if x != y and first is None: first = base+j
                        if not math.isfinite(x) or not math.isfinite(y):
                            finite=False; continue
                        d=x-y
                        if not math.isfinite(d): finite=False; continue
                        maxabs=max(maxabs,abs(d)); an.add(x); bn.add(y); dn.add(d)
                    base += chunk//(1 if ar['dtype']=='bytes' else ar['width'])
                    remain-=chunk
                # Matching hashes are not used instead of element comparison.
                av, bv, dv = an.value(), bn.value(), dn.value()
                report.append(dict(rank=rank,stage=key[0],tag=key[1],count=ar['count'],
                    finite=finite,byte_equal=byte_equal,first_difference=first,
                    maxabs=maxabs,l2_left=av,l2_right=bv,l2_difference=dv,
                    relative_l2=(dv/av if av else (0 if not dv else None)),
                    sha256_left=ha.hexdigest(),sha256_right=hb.hexdigest()))
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('left'); p.add_argument('right',nargs='?')
    p.add_argument('--ranks',type=int,required=True)
    args=p.parse_args()
    if args.ranks<=0: p.error('ranks must be positive')
    result = compare(args.left,args.right,args.ranks) if args.right else {'validated_ranks':len(inventory(args.left,args.ranks))}
    print(json.dumps(result,indent=2,allow_nan=False))


if __name__=='__main__': main()
