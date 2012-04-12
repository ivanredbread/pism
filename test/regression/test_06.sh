#!/bin/bash

PISM_PATH=$1
MPIEXEC=$2

echo "Test # 6: bootstrapping from symmetric and non-symmetric x- and y-vars."
files="foo.nc bar.nc baz.nc"

OPTS="-boot_file foo.nc -Mx 21 -My 11 -Mz 31 -Mbz 1 -Lz 4000 -y 0 -o_size small"

set -e -x

# Create a file to bootstrap from:
$PISM_PATH/pismv -test G -Lx 4000 -Ly 4000 -Mx 21 -My 21 -Mz 11 -Mbz 1 -y 0 -o foo.nc 

# Bootstrap with a symmetric range:
$PISM_PATH/pismr $OPTS -o bar.nc 
# Change the range:
ncap2 -O -s"\"x=x+1e4;y=y+1e4\"" foo.nc foo.nc
# Bootstrap with a non-symmetric range:
$PISM_PATH/pismr $OPTS -o baz.nc 

set +e

# Check:
$PISM_PATH/nccmp.py -t 1e-16 -x -v x,y bar.nc baz.nc
if [ $? != 0 ];
then
    exit 1
fi

rm -f $files; exit 0
