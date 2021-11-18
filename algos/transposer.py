import os, sys

Description = """
We read a matrix stored in row-major order, and we store it
using a sparse representation using a column-major order.
"""

def count_cols(infilepath) :
    infile = open(infilepath, 'r')
    line = infile.readline()
    tokens = line.split(',')
    ncols = len(tokens)
    infile.close()
    return ncols

def _readlines(infile) :
    line = infile.readline()
    n = 0
    while line :
        yield n,line
        n +=1
        line = infile.readline()

if __name__ == '__main__' :
    if len(sys.argv) != 1+1 :
        print('Usage is:',sys.argv[0],'<infilepath>')
        exit(-1)
    
    infilename = sys.argv[1]

    infilepath = infilename
    dirpath = infilename+'_cols'
    os.system('if [ -d {d} ] ; then rm -rv {d} ; fi ; '.format(d=dirpath))
    os.mkdir(dirpath)
    ncols = count_cols(infilepath)
    outfiles = [open(path,'w') for path in ['{d}/{x}.txt'.format(d=dirpath,x=str(n)) for n in range(ncols)]]

    infile = open(infilepath, 'r')
    for i,line in _readlines(infile) :
        values = line[:-1].split(',') #assuming to deal with integers
        assert(len(values) == ncols)
        for v,f in zip(values,outfiles) :
            if v :
                s = '{}@{}\n'.format(str(v),str(i))
                f.write(s)
    for f in outfiles :
        f.flush()
        f.close()
    


