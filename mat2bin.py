#!/usr/bin/env python3

import sys, time, argparse, subprocess, os.path, struct

Description = """
Tool to convert a matrix written in text csv format (one line per row)
into binary form, ie rows x cols doubles 
"""

shasum_exe = "sha256sum"

def main():
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('input', help='input file name', type=str)
  parser.add_argument('rows', help='number of rows', type=int)
  parser.add_argument('cols', help='number of columns', type=int)
  parser.add_argument('-c', help='initial columns to skip (def. 0)',type=int,default=0 )
  parser.add_argument('-r', help='initial rows to skip (def. 0)',type=int,default=0 )
  parser.add_argument('--sum', help='compute output file shasum',action='store_true')
  #parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()
  start0 = start = time.time()

  
  with open(args.input,"rt") as f:
    outname = args.input + ".%dx%d.double" %(args.rows,args.cols) 
    with open(outname,"wb") as g:
      nonz = 0
      r = 0 # number of read rows
      wr = 0 # number of written rows
      values  = set()
      for line in f:
        r += 1
        if r>args.r: # skip first args.r rows
          a = line.rstrip().split(",")
          a = a[args.c:] # remove initial arg.c columns
          if len(a)!=args.cols:
            for i in range(len(a)):
              print(i,a[i])
            print("row", r,"has", len(a), "elements")
            sys.exit(1);
          ## convert to double 
          b = [float(x) for x in a]
          for x in b:
            if x!=0:
              nonz +=1
              values.add(x)
          g.write(struct.pack("%dd" % len(b), *b))
          wr += 1
  if wr!=args.rows:
    print("Warning! Written", wr, "rows instead of", args.rows)
  print("Number of nonzeros:",nonz," Nonzero ratio: %.4f" % (nonz/(wr*args.cols)))  
  print(len(values), "Distinct nonzeros values")  
  print("==== Done")


# compute hash digest for a file 
def file_digest(name):
    try:
      hash_command = "{exe} {infile}".format(exe=shasum_exe, infile=name)
      hashsum = subprocess.check_output(hash_command.split())
      hashsum = hashsum.decode("utf-8").split()[0]
    except:
      hashsum = "Error!" 
    return hashsum  

if __name__ == '__main__':
    main()
