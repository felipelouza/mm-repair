import struct, sys
import argparse

Description = """
Convert a CSV into a block-wise dense representation.
  
Example usage: 
  reorder.py pc /data/mm/covtype 581012 16
"""

def get_nums(infilepath, sep=',') :
    infile = open(infilepath, 'r')
    line = infile.readline()
    count = 0
    while line :
        assert(line[-1] == '\n')
        tokens = line[:-1].split(sep)
        for token in tokens :
            yield count, float(token)
        #update
        line = infile.readline()
        count += 1
    infile.close()

def to_byte(d) :
    return struct.pack('d', d)

def show_command_line(f):
  f.write("=== Command line: ") 
  for x in sys.argv:
     f.write(x+" ")
  f.write("\n")   

if __name__ == '__main__' :
    #args
    show_command_line(sys.stderr)
    parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('input', help='matrix file name (must be in csv format))', type=str)
    parser.add_argument('rows',  help='number of rows', type=int)
    parser.add_argument('blocks', help='number of row blocks', type=int)
    args = parser.parse_args()
    if len(sys.argv) != 3 + 1 :
        print('Usage is:', sys.argv[0], '<path to csv> <rows> <blocks>')
        exit(-1)
    
    infilepath = args.input 
    rows = args.rows 
    blocks = args.blocks 

    min_rows = (rows + blocks - 1 ) // blocks
    end_rows = 0
    curr_block = 0

    outfile = None
    for l,d in get_nums(infilepath) :
        if l == end_rows :
            if outfile is not None :
                outfile.close()
            outfilepath = f'{infilepath}_b.{blocks}.{curr_block}'
            outfile = open(outfilepath, 'wb')
            end_rows += min_rows
            curr_block += 1
        assert(l < end_rows)
        b = to_byte(d)
        outfile.write(b)
    outfile.close()
    print()
    assert(curr_block == blocks)
