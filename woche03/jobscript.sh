#!/bin/bash
#SBATCH --time=10:00
#SBATCH --nodes=4
#SBATCH --tasks=4
#SBATCH --partition=vl-parcio
#SBATCH --output=timescript.out

srun ./timescript.sh
echo "Fertig" > jobscript.out
