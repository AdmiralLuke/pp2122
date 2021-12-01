#!/bin/bash

#SBATCH --time=05:00:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --error=jobscript.err
#SBATCH --output=jobscript.out

make clean
. /opt/spack/pp-2021/env.sh
module load hyperfine
make
echo "warmup"
hyperfine --warmup 3 'srun -p vl-parcio -c 24 ./partdiff 24 2 4096 2 2 50'
echo "1 Thread"
hyperfine 'srun -p vl-parcio -c 1 ./partdiff 1 2 4096 2 2 50'
echo "2 Threads"
hyperfine 'srun -p vl-parcio -c 2 ./partdiff 2 2 4096 2 2 50'
echo "3 Threads"
hyperfine 'srun -p vl-parcio -c 3 ./partdiff 3 2 4096 2 2 50'
echo "6 Threads"
hyperfine 'srun -p vl-parcio -c 6 ./partdiff 6 2 4096 2 2 50'
echo "12 Threads"
hyperfine 'srun -p vl-parcio -c 12 ./partdiff 12 2 4096 2 2 50'
echo "18 Threads"
hyperfine 'srun -p vl-parcio -c 18 ./partdiff 18 2 4096 2 2 50'
echo "24 Threads"
hyperfine 'srun -p vl-parcio -c 24 ./partdiff 24 2 2096 2 2 50'
