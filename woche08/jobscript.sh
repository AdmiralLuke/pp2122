#!/bin/bash

make clean
. /opt/spack/pp-2021.env.sh
module load hyperfine
make

#SBATCH --exclusive
#SBATCH --partition=vl-parcio

#SBATCH --nodes=1

#SBATCH --ntasks=1
echo "warmup"
hyperfine --warmup 3 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 836'
echo "Benchmark #1 (1,1,836)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 836'

#SBATCH --ntasks=2
echo "Benchmark #2 (1,2,1182)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 1182'

#SBATCH --ntasks=3
echo "Benchmark #3 (1,3,1448)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 1448'

#SBATCH --ntasks=6
echo "Benchmark #4 (1,6,2048)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 2048'

#SBATCH --ntasks=12
echo "Benchmark #5 (1,12,2896)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 2896'

#SBATCH --ntasks=24
echo "Benchmark #6 (1,24,4096)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 4096'

#SBATCH --nodes=2
#SBATCH --ntasks=48
echo "Benchmark #7 (2,48,5793)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 5793'

#SBATCH --nodes=4
#SBATCH --ntasks=96
echo "Benchmark #8 (4, 96, 8192)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 8192'

#SBATCH --nodes=8
#SBATCH --ntasks=192
echo "Benchmark #9 (8, 192, 11585)"
hyperfine 'srun --mpi=pmi2 ./partdiff 1 2 1024 2 2 11585'