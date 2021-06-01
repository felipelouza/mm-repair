#!/usr/bin/env python3

import sys, time, argparse, subprocess, os.path, struct

Description = """
Tool to convert a matrix written in text csv format (one line per row)
into binary form, ie rows x cols floats of doubles (with option -d)

The option --strip instead of converting to binary form
simply strips trailing or leading columns, or leading rows
maintinaing the csv format. This is done at the textual level so
non-stripped values are not modified in this operation (eg integers
remain integres, they are not converted to floa with a 0 fractional part). 
"""

shasum_exe = "sha256sum"

def main():
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('input', help='input file name', type=str)
  parser.add_argument('rows', help='number of rows', type=int)
  parser.add_argument('cols', help='number of columns', type=int)
  parser.add_argument('-c', help='initial columns to skip, can be negative (def. 0)',type=int,default=0 )
  parser.add_argument('-r', help='initial rows to skip (def. 0)',type=int,default=0 )
  parser.add_argument('-d', help='output values as double precision floats',action='store_true')
  parser.add_argument('--strip', help='only strip values, do not convert ',action='store_true')
  #parser.add_argument('--sum', help='compute output file shasum',action='store_true')
  #parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()
  if args.d and args.strip:
    print("Error: Options -d and --strip are mutually exclusive");
    return 
  if args.strip and args.c==0 and args.r==0:
    print("Error: strip mode with nothing to strip");
    return 
    
  # take start time 
  start0 = start = time.time()
  outmode = "wb"  # by default ouput file is binary
  with open(args.input,"rt") as f:
    if args.strip: 
      outname = args.input + ".%dx%d" %(args.rows,args.cols)
      outmode = "w" 
    elif args.d: outname = args.input + ".%dx%d.double" %(args.rows,args.cols) 
    else: outname = args.input + ".%dx%d.float" %(args.rows,args.cols) 
    with open(outname,outmode) as g:
      nonz = 0
      r = 0 # number of read rows
      wr = 0 # number of written rows
      values  = set()
      for line in f:
        r += 1
        if r>args.r: # skip first args.r rows
          a = line.rstrip().split(",")
          if args.c>0:
            a = a[args.c:] # remove initial args.c columns
          elif args.c<0:
            a = a[:args.c] # remove final args.c columns
          if len(a)!=args.cols:
            for i in range(len(a)):
              print(i,a[i])
            print("row", r,"has", len(a), "elements")
            sys.exit(1);
          if args.strip:
            print(",".join(a),file=g)
          else:
            ## convert to double 
            b = [float(x) for x in a]
            for x in b:
              if x!=0:
                nonz +=1
                values.add(x)
            if args.d: g.write(struct.pack("%dd" % len(b), *b))
            else: g.write(struct.pack("%df" % len(b), *b))
          wr += 1
  if wr!=args.rows:
    print("Warning! Written", wr, "rows instead of", args.rows)
  if not(args.strip):  
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
