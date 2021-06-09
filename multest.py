#!/usr/bin/env python3
import subprocess, os.path, sys, argparse, time, struct


Description = """
Tool to create a Latex table containing the results of a set of experiments"""

       
Files = ['covtype', 'census', 'optical', 'susy']
Files_prefix = 'data/'
Logfile_name = "multest.log"

Algo = ['csrmm', 'remm']

Sizes = {'covtype':(581012, 54), 'census':(2458285, 68), 'optical':(325834, 174),
         'susy':(5000000, 18)}

# name of file containing the input/output vectors
Xvname = "x1.dbl"
Yvname = "y.dbl"
Zvname = "z.dbl"
Evname = "ein.dbl"
Timelimit = 18000

def createx(cols,value=1):
  with open(Xvname,"wb") as f:
    for i in range(cols):
      f.write(struct.pack("<d", float(value)))


def time_test(n=1):
  table = []   # latex table containing the results 
  for f in Files:
    name  = Files_prefix + f
    rows,cols = Sizes[f]
    createx(cols)  # create file containing x vector
    tablerow = []  # row of the results table
    for a in Algo:
      command = "./{exe} -n {num} -e {ename} {name} {r} {c} {x} {y} {z}".format(ename=Evname,
                exe = a, num=n, name=name, r=rows, c=cols, x=Xvname, y=Yvname, z=Zvname)
      try:
        ris = subprocess.run(command.split(),stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,timeout=Timelimit,check=True)
      except subprocess.TimeoutExpired:
        # caso time out
        print(" Test failed: no result after %d seconds" % Timelimit)
        continue
      except subprocess.CalledProcessError as ex:
        print(" Test failed: non-zero exit code ", ex.returncode)
        continue
      except Exception as ex:
        print(" Test failed:", str(ex))
        continue
      #print("Out-- ", ris.stdout)
      #print("Err-- ", ris.stderr)
      elapsed = int(ris.stdout.split()[-2])
      peakmem = int(ris.stderr.split()[3])
      with open(Evname,"rb") as evf:
        e = struct.unpack("d",evf.read(8))[0]
      tablerow.append((a, elapsed/n,peakmem, e))
    # tests for current file completed
    table.append(makerow(f, tablerow))
  # all files processed
  return table

def makerow(f, a):
  s = "{name:9.8}".format(name=f)
  for p in a:
    s += "&{:.2} {:6.2f} & {:6.2f} {:8.3g}".format(p[0][:2],p[1],p[2]/1000000,p[3])
  s += "\\\\\n"
  return s

# not used!
def main():
  # show_command_line(sys.stderr)
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('files', help='test files separated by colons', type=str)
  parser.add_argument('-w', '--window_sizes', help='sizes of sliding window separated by colons (def 4:8)', default='4:8', type=str)  
  parser.add_argument('-b', '--block_sizes', help='sizes of block separated by colons (def 32:64)', default='32:64', type=str)  
  parser.add_argument('-d', '--base_dir', help='base directory (def .)', default='.', type=str)
  parser.add_argument('-e', '--encs', help='encoders separated by colons (def lsk)', default='lsk', type=str)
  parser.add_argument('-c', '--check', help='check decompression (def No)',action='store_true')
  parser.add_argument('-p', '--pythondec', help='use python slow decompressor (def No)',action='store_true')
  parser.add_argument('-s', '--strict', help='use strict mode for (de)compression (def No)',action='store_true')
  args = parser.parse_args()
  # run the algorithm   
  s1 = time.time()
  ifiles = args.files.split(":")
  wsizes = args.window_sizes.split(":")
  bsizes = args.block_sizes.split(":")
  encoders = args.encs.split(":")  
  check_files(ifiles,args.base_dir)
  check_encoders(encoders)
  table = ""
  for b in bsizes:
    for w in wsizes:
      if len(encoders)>1:
        r = mydelta_test(w,b,ifiles,args.base_dir,encoders,args.check,args.pythondec,args.strict)
      else:
        r = myalgo_test(w,b,ifiles,args.base_dir,encoders[0],args.check,args.pythondec,args.strict)
      table += r
  # the model row is the first row up to the "\\" sequence    
  model_row = table.split("\n")[0].split("\\\\")[0]
  # create row with name of the test files
  names = make_names(model_row, ifiles)
  # add rows for gzip and xz
  table = names + table + make_zip(model_row,ifiles,args.base_dir) 
  e1 = time.time()
  print("Elapsed time: %.3f\n" % (e1-s1),file=sys.stderr)
  print(table)
  exit(0)

  
if __name__ == '__main__':
  table = time_test(4)
  for s in table:
    print(s,end="")
 
