date >> output.log 2>&1 
python3 retest.py pc -d /mnt/data/datasets/machine-learning-matrices-reorder -b $1   >> output.log 2>&1
../mmtest-all.py mz -b $1 -d /mnt/data/datasets/machine-learning-matrices-reorder/ --extra="--noconv"  >> output.log 2>&1
../mmtest-all.py mm -n 500 -b $1 -d /mnt/data/datasets/machine-learning-matrices-reorder/  >> output.log 2>&1

