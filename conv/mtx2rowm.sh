#!/bin/bash 


# sample invocation:
#   mtx2rowm.sh  google.mtx


# check arguments
if [ $# -lt 1 ] || [ $# -gt 2 ]; 
then 
  echo Usage: $0 infile [outfile];
  exit 1
fi

# define outfile name
if [ $# -eq 2 ];
  then outfile=$2
  else outfile=$1.rowm
fi

# get current dir and execute command
exe_dir=$(dirname "$0")
$exe_dir/edge2mtx.py -cts -a -1 $1 | sort -uk1n -k2n > $outfile

# get nodes and arcs from header file and delete it 
matrix_data=(`tail -n1 $1.header | tr " " "\n"`)
echo Matrix size: ${matrix_data[0]}
echo Number of nonzeros ${matrix_data[2]}
rm -fr $1.header

echo
echo The "(transposed)" matrix in row major order ready for 
echo PageRank computation is in file $outfile 
echo The column count file required by repagerank is $1.ccount

echo To compress the matrix with matrepair use the command line:
echo "  " matrepair --bool -r $outfile ${matrix_data[0]} ${matrix_data[0]}
echo Then to compute PageRank use the command line
echo "  " repagerank $outfile ${matrix_data[0]} $1.ccount 

exit 0
