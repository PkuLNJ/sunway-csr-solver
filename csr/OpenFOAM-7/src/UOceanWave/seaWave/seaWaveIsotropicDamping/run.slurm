#! /bin/bash
#SBATCH -J test
#SBATCH -N 1
#SBATCH -p cpu
#SBATCH -n 92

# Source

apptainer exec /apps/ApptainerImages/openfoam7.sif  bash -c "wclean"
apptainer exec /apps/ApptainerImages/openfoam7.sif  bash -c "wmake"


