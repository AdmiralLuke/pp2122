#!/bin/bash

chmod +x jobscript.sh
echo "----------------"
echo "@@GAUSS-SEIDEL@@"
echo "----------------"
./jobscript.sh

chmod +x jobscript-jac.sh
echo "-----------------"
echo "@@JACOBI@@"
echo "-----------------"
./jobscript-jac.sh

chmod +x jobscript-reg.sh
echo "-----------------"
echo "@@REGULAR@@"
echo "-----------------"
./jobscript-reg.sh
