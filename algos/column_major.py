import os, sys
import hashlib 

Description = """
We read a matrix stored in row-major order, and we store it
using a sparse representation using a column-major order.
"""

#has a string to a 32-bit integer
def hash(str, separator='.') :
    #check whether str==0
    tokens = str.split('e') #remove what follows 'e'
    tokens = tokens[0].split('E') #remove what follows 'E'
    tokens = tokens[0].split(separator) #separate integer and decimal parts
    a = int(tokens[0])
    b = 0
    if len(tokens)>1 :
        assert(len(tokens) == 2)
        b = int(tokens[1])
    is_zero = (a==0 and b==0)
    print('q:',a,b)
    if is_zero :
        #a zero remains a zero
        return 0
    else :
        #hash
        b = bytes(str, 'utf-8')
        h = int.from_bytes(hashlib.sha256(b).digest()[:4], 'little') # 32-bit int
    return h


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
        #values = line[:-1].split(',') #assuming to deal with integers
        values = [hash(s) for s in line[:-1].split(',')]
        assert(len(values) == ncols)
        for v,f in zip(values,outfiles) :
            if v :
                s = '{}@{}\n'.format(str(v),str(i))
                f.write(s)
    for f in outfiles :
        f.flush()
        f.close()
    


