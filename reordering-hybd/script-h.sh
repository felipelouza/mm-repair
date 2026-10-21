date >> output-h.log 2>&1 
python3 retest-h.py pc -d /mnt/data/datasets/machine-learning-matrices-reorder -b $1   >> output-h.log 2>&1
#../mmtest-all-h.py mz -b $1 -d /mnt/data/datasets/machine-learning-matrices-reorder/ --extra="--noconv --hybrid"  >> output-h.log 2>&1
python3 mmtest-h.py mm -n 500 -b $1 -d /mnt/data/datasets/machine-learning-matrices-reorder/  >> output-h.log 2>&1

