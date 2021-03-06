#!/bin/bash
#SBATCH --exclusive
#SBATCH --output=jobscript.out

make clean
. /opt/spack/pp-2021.env.sh
module load hyperfine
make
echo "warmup"
hyperfine --warmup 3 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 836'
echo "Benchmark #1 (1,1,836)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 836'

echo "Benchmark #2 (1,2,1182)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 1182'

echo "Benchmark #3 (1,3,1448)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 1448'

echo "Benchmark #4 (1,6,2048)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 2048'

echo "Benchmark #5 (1,12,2896)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 2896'

echo "Benchmark #6 (1,24,4096)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 4096'

echo "Benchmark #7 (2,48,5793)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 5793'

echo "Benchmark #8 (4, 96, 8192)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 8192'

echo "Benchmark #9 (8, 192, 11585)"
hyperfine 'srun -p vl-parcio -n 1 -N 1 --mpi=pmi2 ./partdiff 1 1 1024 2 2 11585'
