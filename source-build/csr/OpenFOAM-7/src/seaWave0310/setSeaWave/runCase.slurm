#! /bin/bash
#SBATCH -J test
#SBATCH -N 1
#SBATCH -p cpu
#SBATCH -n 1


apptainer exec /apps/ApptainerImages/OpenFoam7 bash ./run.slurm
