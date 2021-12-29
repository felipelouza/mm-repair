#!/bin/bash -l

# sample invocation:
#   chunkMWM.sh /data/mm/ covtype 581012 54 8
# data path must be absolute
# must be executed from the mm-repair/reordering directory 

PATH_CSV=$1
PATH_VC=$1
DATASET=$2
NR=$3
NC=$4
NB=$5

DATASET_CSV=$PATH_CSV/$DATASET
DATASET_VC=$PATH_VC/$DATASET
VCOEXT=o

let NR_FIRST=($NR+$NB-1)/$NB
let NR_LAST=($NB-1)*$NR_FIRST
let NR_LAST=$NR-$NR_LAST
let NB_LAST=$NB-1

echo Rows: $NR
echo Columns: $NC
echo Rows, first blocks: $NR_FIRST
echo Rows, last block: $NR_LAST

K_PAR=16
REORDERING_PATH=$PWD
REORDERING_BUILD_PATH=$REORDERING_PATH/build/

echo Dividing into row chunks...

python3 csv_splitter.py $DATASET_CSV $NR $NB &

wait

echo Dividing into row chunks...

#all blocks
for (( i=0; i+1<=$NB; ++i ))
do
    python3 $REORDERING_PATH/column_major.py $DATASET_CSV.$NB.$i &
done

wait

echo Generating the CSM for each row chunk ...

#first blocks
for (( i=0; i+1<$NB; ++i ))
do
    $REORDERING_BUILD_PATH/tsp_generator_pruned_local $DATASET_CSV.$NB.$i $NR_FIRST $NC $K_PAR &
done
#last block
$REORDERING_BUILD_PATH/tsp_generator_pruned_local $DATASET_CSV.$NB.$NB_LAST $NR_LAST $NC $K_PAR &

wait

echo Running MWM upon each row chunk ...

for (( i=0; i<$NB; ++i ))
do
    $REORDERING_BUILD_PATH/mwm $DATASET_CSV.$NB.$i $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.tsp &&
    python3 $REORDERING_PATH/mwm_sol_from_pairs.py $DATASET_CSV.$NB.$i $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.tsp & 
done

wait

echo Reordering each .vc file ...

# .vco are the original unpermuted .vc files 
for (( i=0; i+1<$NB; ++i ))
do
    ln -sf $DATASET_VC.$NB.$i.vco $DATASET_VC.$NB.$i.vc
    $REORDERING_PATH/vc_reorder.x $DATASET_VC.$NB.$i $NR_FIRST $NC $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.mwm.solution && 
    ln -sf $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.mwm.solution.vc $DATASET_VC.$NB.$i.vc &&
    echo " --- block $i done." &
done
ln -sf $DATASET_VC.$NB.$NB_LAST.vco $DATASET_VC.$NB.$NB_LAST.vc
$REORDERING_PATH/vc_reorder.x $DATASET_VC.$NB.$NB_LAST $NR_LAST $NC $DATASET_CSV.$NB.$NB_LAST.pruned_local_$K_PAR.mwm.solution &&
ln -sf $DATASET_CSV.$NB.$NB_LAST.pruned_local_$K_PAR.mwm.solution.vc $DATASET_VC.$NB.$NB_LAST.vc &&
echo " --- block $NB_LAST done." &

wait

echo " --- cleaning"
for (( i=0; i<$NB; ++i ))
do
  rm -fr $DATASET_CSV.$NB.$i\_cols &
  rm -f  $DATASET_CSV.$NB.$i 
  rm -f  $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.par
  rm -f  $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.tsp
  rm -f  $DATASET_CSV.$NB.$i.pruned_local_$K_PAR.mwm.solution
done

wait

echo Done.

