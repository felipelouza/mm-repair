#!/usr/bin/env python3

import sys, time, argparse, subprocess, os.path, struct

Description = """
  vc ->  vc1, vc2
  vc2 -> vc2.C vc2.R
  vc1.C, vc1 -> vc.C
  vc2.R -> vc.R
"""

# executables 
split_exe = "vcsplit"
merge_exe = "vcmerge"
irepair_exe = "brepair/irepair0"


def main():
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('input', help='vc file name', type=str)
  parser.add_argument('-m', help='force repair to use at most M MBs (def. 95%% of available RAM)',default=-1, type=int)
  parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()

  


    
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
