#!/bin/bash
set -euo pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
w=$r/build/alpha-mules-cpe-20261027-01-on-step1
src=$r/tests/amr-firstdiv-20261023-r1/csr-wave-sqr-scalar-aligned
d=$r/tests/alpha-mules-cpe-20261027-01-on-step1
cd "$w";mkdir PREPARE_ONCE.guard
sha256sum -c STAGED.sha256 > STAGED_VERIFIED.txt
test ! -e "$d";mkdir "$d";mkdir "$d/jobControl"
cp -aL "$src/system" "$d/system"
cp -p INPUT-controlDict50 "$d/system/controlDict"
ln -s "$src/constant" "$d/constant"
ln -s "$src/seaWaveData" "$d/seaWaveData"
copy_range(){
 local i f
 for ((i=$1;i<=$2;i++));do
  mkdir "$d/processor$i"
  ln -s "$src/processor$i/constant" "$d/processor$i/constant"
  cp -as "$src/processor$i/0" "$d/processor$i/0"
  for f in TT S;do cp -aL --remove-destination "$src/processor$i/0/$f" "$d/processor$i/0/$f";done
 done
}
copy_range 0 239 & p1=$!;copy_range 240 479 & p2=$!;copy_range 480 719 & p3=$!;copy_range 720 959 & p4=$!
for pid in "$p1" "$p2" "$p3" "$p4";do wait "$pid";done
cmp "$src/system/fvSolution" "$d/system/fvSolution"
cmp "$src/system/fvSchemes" "$d/system/fvSchemes"
cp -p INPUT-fvSolution "$d/system/fvSolution"
cmp INPUT-fvSolution "$d/system/fvSolution"
cmp INPUT-controlDict50 "$d/system/controlDict"
python - "$d" <<'PY'
import os,sys
d=sys.argv[1];count=0
for rank in range(960):
 for f in ('TT','S'):
  p=d+'/processor%d/0/'%rank+f
  assert os.path.isfile(p) and not os.path.islink(p)
  count+=1
print('PRIVATE_INITIAL_TT_S_FILES=%d'%count)
PY
sha256sum "$d/system/fvSolution" "$d/system/fvSchemes" "$d/system/controlDict" > "$d/CONFIG.sha256"
cp RUN_REQUEST.json "$d/RUN_REQUEST.json"
printf 'THIN_INPUTS_READY_NO_FULL_CASE_COPY\n' > "$d/INPUTS_READY"
echo R2_50_STEP_INPUTS_READY_NO_SUBMISSION
