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
  s = "{name:9.8}&{col:<4}".format(name=f,col=Sizes[f][1])
  for p in a:
    s += "& {:.2} {:6.2f} & {:6.2f} {:8.3g}".format(p[0][:2],p[1],p[2]/1000000,p[3])
  s += "\\\\\n"
  return s

# not used!
def main():
  # show_command_line(sys.stderr)
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('op', help='operation to test: mm', type=str)
  parser.add_argument('-n', help='number of iterations (def 3)', default=3, type=int)  
  args = parser.parse_args()
  if args.op!='mm':
    print("Unknown operation! Must be mm",file=sys.stderr)
    exit(1)
  # run the algorithm   
  s1 = time.time()
  table = time_test(args.n)
  e1 = time.time()
  print("Elapsed time: %.3f\n" % (e1-s1),file=sys.stderr)
  for s in table:
    print(s,end="")
  exit(0)

  
if __name__ == '__main__':
  main()
