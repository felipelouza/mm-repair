#!/usr/bin/env python3

import sys, datetime, time, argparse, subprocess, os, os.path
from psutil import virtual_memory

Description = """
  vc ->  vc1, vc2
  vc2 -> vc2.C vc2.R
  vc2.C, vc1 -> vc.C
  vc2.R -> vc.R
"""

# executables 
split_exe = "vcsplit"
merge_exe = "vcmerge"
irepair_exe = "brepair/irepair0"


def main():
  logfile = sys.stderr
  show_command_line(sys.stderr)
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('input', help='vc file name', type=str)
  parser.add_argument('-m', help='force repair to use at most M MBs (def. 95%% of available RAM)',default=-1, type=int)
  parser.add_argument('-v',  help='verbose',action='store_true')
  args = parser.parse_args()

  # if no max RAM provided on command line uses 90% of total 
  if(args.m<0):
    mem = virtual_memory().total
    args.m = max(4000,int(0.90*mem/2**20)) # assume at least 4GB in case virtual_memory() misbehaves
    print("Setting memory for repair to", args.m, "MBs")
  
  # init time counters 
  split_time = repair_time = merge_time = 0

  # get absolute path to main matrepair directory 
  args.main_dir = os.path.dirname(os.path.abspath(__file__))
  start = time.time()
  
  # --- split 
  command = "{exe} {infile}".format(
          exe = os.path.join(args.main_dir,split_exe), infile=args.input);
  print("==== Splitting vc file:\nCommand:", command)
  if(execute_command(command,logfile)!=True):
    sys.exit(2)
  split_time = time.time()-start

  # irepair
  command = "{exe} {infile}2 {mb}".format(mb=args.m, infile=args.input,
                    exe = os.path.join(args.main_dir,irepair_exe))
  print("==== irepair\nCommand:", command)
  if(execute_command(command,logfile)!=True):
    sys.exit(2)
  repair_time = time.time()-start -split_time
  
  # merge                  
  command = "{exe} {infile}2.C {infile}1 {infile}.C".format(infile=args.input,
                    exe = os.path.join(args.main_dir,merge_exe))
  print("==== merge\nCommand:", command)
  if(execute_command(command,logfile)!=True):
    sys.exit(2)
  merge_time = time.time()-start -repair_time
                    
  # rename
  print("==== rename .R file")
  os.replace(f"{args.input}2.R", f"{args.input}.R")
  print("==== Done")



def show_command_line(f):
  f.write("\n#### " + datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
  f.write("\n Command line: ") 
  for x in sys.argv:
     f.write(x+" ")
  f.write("\n")   


# execute command: return True is everything OK, False otherwise
def execute_command(command,logfile):
  try:
    subprocess.check_call(command.split(),stdout=logfile,stderr=logfile)
  except subprocess.CalledProcessError:
    print("Error executing command line:")
    print("\t"+ command)
    print("Check log file")
    return False
  return True



if __name__ == '__main__':
    main()
