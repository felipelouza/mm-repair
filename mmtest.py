  #!/usr/bin/env python3
import subprocess, os.path, sys, argparse, time, struct

Description = """
Tool to create a Latex table containing the results of a set of experiments"""

Files = ['susy','higgs','airline78','covtype', 'census', 'optical', 'mnist2m']
Files_prefix = 'data/'
Logfile_name = "errors.log"


Algo = ['csrmm', 'remm','ivremm','ansremm','ansivremm']

Sizes = {'covtype':(581012, 54), 'census':(2458285, 68), 'optical':(325834, 174),
         'susy':(5000000, 18), 'higgs': (11000000,  28), 'mnist2m':(2000000,784),  
         'airline78':(14462943, 29)}

# name of file containing the input/output vectors
Xvname = "x1.dbl"
Yvname = "y.dbl"
Zvname = "z.dbl"
Evname = "ein.dbl"
Timelimit = 18000


# check that the test files exist and sizes are defined
def check_testfiles(sufxs):
  ok = True
  for f in Files:
    if f not in Sizes:
      print("  Size for test file", f, "missing",file=sys.stderr)
      ok = False
    for sx in sufxs:
      path = Files_prefix + f + sx
      if not os.path.exists(path):
        print("  Test file", path, "missing",file=sys.stderr)
        ok = False
  if not ok:
    print("Check variables: Files, Files_prefix, Sizes",file=sys.stderr)
    sys.exit(1)


# write to file a vector cointaining cols copies of value
def createx(cols,value=1):
  with open(Xvname,"wb") as f:
    for i in range(cols):
      f.write(struct.pack("<d", float(value)))


# test running times and space for matrix multiplication 
def time_test(n,logfile):
  table = []   # latex table containing the results 
  for f in Files:
    name  = Files_prefix + f
    rows,cols = Sizes[f]
    createx(cols)  # create file containing x vector
    tablerow = []  # row of the results table
    for a in Algo:
      # only save the eigenvalue
      command = "./{exe} -n {num} -e {ename} -z {z} {name} {r} {c} {x}".format(ename=Evname,
                exe = a, num=n, name=name, r=rows, c=cols, x=Xvname, z=f+Zvname)
      try:
        ris = subprocess.run(command.split(),stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,timeout=Timelimit,check=True)
      except subprocess.TimeoutExpired:
        # caso time out
        print(" Test failed: no result after %d seconds" % Timelimit)
        continue
      except subprocess.CalledProcessError as ex:
        print(" Test failed: non-zero exit code ", ex.returncode)
        # write stdout/stderr to a separate logfile
        print("## Error executing:", command,file=logfile);
        print("## stdout:\n", ex.stdout ,file=logfile);
        print("## stderr:\n", ex.stderr ,file=logfile);
        sys.exit(2)
      except Exception as ex:
        print(" Test failed:", str(ex))
        sys.exit(2)
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
  s = "{name:10.9}&{col:<4}".format(name=f,col=Sizes[f][1])
  for p in a:
    s += "&{:6.2f} &{:6.0f}  {:8.3g}".format(p[1],p[2]/1000000,p[3])
  s += "\\\\\n"
  return s


def show_command_line(f):
  f.write("=== Command line: ") 
  for x in sys.argv:
     f.write(x+" ")
  f.write("\n")   

def main():
  show_command_line(sys.stderr)
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('op', help='operation to test: mm|mc', type=str)
  parser.add_argument('-n', help='number of iterations (def 3)', default=3, type=int)  
  args = parser.parse_args()
  if args.op!='mm':
    print("Unknown operation! Must be mm",file=sys.stderr)
    exit(1)
  check_testfiles([".vc",".val",".vc.R",".vc.C"])
  # run the algorithm   
  s1 = time.time()
  with open(Logfile_name,"a") as logfile:
    table = time_test(args.n,logfile)
  e1 = time.time()
  print("Elapsed time: %.3f\n" % (e1-s1),file=sys.stderr)
  for s in table:
    print(s,end="")
  exit(0)

  
if __name__ == '__main__':
  main()
