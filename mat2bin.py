#!/usr/bin/env python3

import sys, time, argparse, subprocess, os.path, struct

Description = """
Tool to convert a matrix written in text csv format (one line per row)
into binary form, ie rows x cols doubles or floats (with option -f)
Optionally, a set of leading rows and/or columns can be removed before 
the conversion. If the number of columns to remove is negative, 
minus that number of trailing columns are removed. 

The option --strip instead of converting to binary form
simply strips trailing or leading columns, or leading rows
maintaining the csv format. This is done at the textual level so
non-stripped values are not modified by this operation (eg integers
remain integers, they are not converted to floats with a 0 fractional part). 
"""

shasum_exe = "sha1sum"

def main():
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('input', help='input file name', type=str)
  parser.add_argument('rows', help='number of rows in outfile', type=int)
  parser.add_argument('cols', help='number of columns in outfile', type=int)
  parser.add_argument('-c', help='initial columns to skip, can be negative (def. 0)',type=int,default=0 )
  parser.add_argument('-r', help='initial rows to skip (def. 0)',type=int,default=0 )
  parser.add_argument('-o', help='output file name (def. input.dbl)',type=str,default="" )
  parser.add_argument('-f', help='output values as single precision floats',action='store_true')
  parser.add_argument('--strip', help='only strip values, do not convert ',action='store_true')
  #parser.add_argument('--sum', help='compute output file shasum',action='store_true')
  #parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()
  if args.f and args.strip:
    print("Error: Options -f and --strip are mutually exclusive");
    return 
  if args.strip and args.c==0 and args.r==0:
    print("Error: strip mode with nothing to strip");
    return 
    
  # take start time 
  print("SHA1  Infile:", file_digest(args.input), args.input);
  with open(args.input,"rt") as f:
    # set output file name and output file mode
    if args.o!="": 
      outname = args.o
    elif args.strip:
      outname = args.input + ".%dx%d" %(args.rows,args.cols)
    elif args.f: outname = args.input + ".float" 
    else: outname = args.input + ".dbl"
    outmode = "w" if args.strip else "wb"  # by default output file is binary
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
            ## convert to float 
            b = [float(x) for x in a]
            for x in b:
              if x!=0:
                nonz +=1
                values.add(x)
            if args.f: g.write(struct.pack("%df" % len(b), *b)) # single precision 
            else:      g.write(struct.pack("%dd" % len(b), *b)) # double precision
          wr += 1
  print("SHA1 Outfile:", file_digest(outname), outname);
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
