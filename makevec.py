#!/usr/bin/env python3

import sys, time, argparse, math, os.path, struct

Description = """
Tool to create a vector and write it to a file in binary format

The default format is 32 bit floats; use -i to write 32 bit integers 
"""


def main():
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('outfile', help='output file name', type=str)
  parser.add_argument('size',    help='number of elements', type=int)
  parser.add_argument('val',      help='values to be repeated cyclically', nargs='+')
  parser.add_argument('-i', help='vector entries are integers',action='store_true')
  #parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()
  
  with open(args.outfile,"wb") as f:
    for i in range(args.size):
      x = args.val[ i%len(args.val)]
      if args.i: f.write(struct.pack("<i", int(x)))
      else:      f.write(struct.pack("<f", float(x)))
  print("==== Done")



if __name__ == '__main__':
    main()
