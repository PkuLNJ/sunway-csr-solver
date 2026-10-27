#!/bin/bash
#SBATCH -J lnj-qt-int64-50
#SBATCH -p kshcnormal
#SBATCH -N 30
#SBATCH --ntasks-per-node=32
#SBATCH --cpus-per-task=1
#SBATCH --exclusive
#SBATCH --time=02:00:00
#SBATCH --no-requeue
#SBATCH -o scheduler.%j.out
#SBATCH -e scheduler.%j.err
source /public/home/chenzhi/xyc/QT_OpenFOAM/FP64INT64/env.sh
set -eo pipefail
r=/public/home/chenzhi/lnj/qt-int64-960-50-20261027-01
cd "$r/case"
test "$WM_OPTIONS" = linux64GccDPInt64Opt
export LD_LIBRARY_PATH="$r/lib:$LD_LIBRARY_PATH"
sha256sum -c "$r/BINARY.sha256"
ldd "$r/bin/boussinesqFoam" > "$r/runtime-libraries.$SLURM_JOB_ID.txt"
set +e
mpirun -np 960 "$r/bin/boussinesqFoam" -parallel > "solver.$SLURM_JOB_ID.log" 2>&1
rc=$?
printf '%s\n' "$rc" > "solver.$SLURM_JOB_ID.rc"
set -e
test "$rc" = 0
/public/home/chenzhi/anaconda3/bin/python "$r/verify.py" "$r/case/solver.$SLURM_JOB_ID.log" > "$r/FINAL_CHECK.json"
