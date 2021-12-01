#!/usr/bin/env python3
import subprocess, os.path, sys, argparse, time, struct

Description = """
Tool to create a Latex table containing the results of a set of experiments

Currently supported tests:
   mc: conversion to dense matrix format and compression with gz/xz
   mz: conversion to CSRV format followed by grammar compression
   mm: test matrix-vector multiplication algorithms"""

#Files = ['susy','higgs','airline78','covtype', 'census', 'optical', 'mnist2m']
Files = ['covtype', 'census']
Files_prefix = 'data/'
Logfile_name = "errors.log"

Sizes = {'covtype':(581012, 54), 'census':(2458285, 68), 'optical':(325834, 174),
         'susy':(5000000, 18), 'higgs': (11000000,  28), 'mnist2m':(2000000,784),  
         'airline78':(14462943, 29)}


Algos = ['csrvmm', 're32mm','reivmm','reansmm']

# name of files containing the input/output vectors
# these are created by this script so you don't have to worry
Xvname = "x1.dbl"
Yvname = "y.dbl"
Zvname = "z.dbl"
Evname = "ein.dbl"
Timelimit = 18000
TmpFilename = "tmp_mmtest"


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


# convert a csv file to binary and compress it with gzip and xz
def test_zip(logfile):
  table = ["### gzip and xz size vs dense uncompressed size (absolute and percentage)\n", 
           " file     & rows &   dense size    % &&     gzip size   % &&     xz size    % &\\\\\n"]
  for f in Files:
    name  = Files_prefix + f
    rows,cols = Sizes[f]
    tablerow = []  # row of the results table
    command = "./{exe} {name} -o {temp} {r} {c}".format(
                exe = "mat2bin.py", temp=TmpFilename, name=name, r=rows, c=cols)
    command2 = "{exe} -kf {name}".format(exe = "gzip",  name=TmpFilename)            
    command3 = "{exe} -kf {name}".format(exe = "xz",  name=TmpFilename)            
    try:
      ris = subprocess.run(command.split(),stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,timeout=Timelimit,check=True)
      #print(command2)
      ris = subprocess.run(command2.split(),stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,timeout=Timelimit,check=True)
      #print(command3)
      ris = subprocess.run(command3.split(),stdout=subprocess.PIPE,
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
    tablerow.append((os.path.getsize(TmpFilename),os.path.getsize(TmpFilename+".gz"),
                     os.path.getsize(TmpFilename+".xz")))
    # elapsed = int(ris.stdout.split()[-2])
    # peakmem = int(ris.stderr.split()[3])
    # tablerow.append((a, elapsed/n,peakmem, e))
    # tests for current file completed
    table.append(makerow_mc(f, tablerow))
  # all files processed
  return table


# compress with matrepair obtaining CSRV and grammar representation
def test_compress(logfile):
  table = ["### csrv and repair size vs dense uncompressed size (precentage)\n", 
           " file     & rows &  crsv &  re32 &  reiv & reans \\\\\n"]   # latex table containing the results 
  for f in Files:
    name  = Files_prefix + f
    rows,cols = Sizes[f]
    tablerow = []  # row of the results table
    command = "./{exe} -r -y {name} {r} {c}".format(
                exe = "matrepair", name=name, r=rows, c=cols)
    try:
      ris = subprocess.run(command.split(),stdout=logfile,
                           stderr=logfile,timeout=Timelimit,check=True)
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
    v = os.path.getsize(name+".val")
    vcsize = os.path.getsize(name+".vc") 
    csize = os.path.getsize(name+".vc.C") 
    rsize = os.path.getsize(name+".vc.R") 
    csizeiv = os.path.getsize(name+".vc.C.iv") 
    rsizeiv = os.path.getsize(name+".vc.R.iv") 
    ans_csize = os.path.getsize(name+".vc.C.ansf.1")
    tablerow.append((v+vcsize,v+csize+rsize,v+csizeiv+rsizeiv,
                    v+ans_csize+rsizeiv))
    # tests for current file completed
    table.append(makerow_mz(f, tablerow))
  # all files processed
  return table




# test running times and space for matrix multiplication 
def test_time(n,logfile):
  # build latex table containing the reuslts  
  table = ["### time x iteration and peak memory usage for matrix multiplication\n", 
           " file     & rows "]
  for a in Algos:
    table[1] += "& {name:12.9}&".format(name=a)         
  table[1] += "\\\\\n" 
  for f in Files:
    name  = Files_prefix + f
    rows,cols = Sizes[f]
    createx(cols)  # create file containing x vector
    tablerow = []  # row of the results table
    for a in Algos:
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
      # a = algo, elapsed e=eigenvalue
      tablerow.append((a, elapsed/n,peakmem, e))
    # tests for current file completed
    table.append(makerow_mm(f, tablerow))
  # all files processed
  return table

def makerow_mm(f, a):
  s = "{name:10.9}& {col:<5}".format(name=f,col=Sizes[f][1])
  for p in a:
    s += "&{:6.2f} &{:4.0f}  ".format(p[1],p[2]/1000000)
  s += "\\\\\n"
  return s

def makerow_mc(f, a):
  s = "{name:10.9}& {col:<5}".format(name=f,col=Sizes[f][1])
  d = 8*Sizes[f][0]*Sizes[f][1]/100
  for p in a:
    s += "&{:11.0f} &{:6.2f} &{:11.0f} &{:6.2f} &{:11.0f} &{:6.2f}".format(
          p[0],p[0]/d,p[1],p[1]/d,p[2],p[2]/d)
  s += "\\\\\n"
  return s


def makerow_mz(f, a):
  s = "{name:10.9}& {col:<5}".format(name=f,col=Sizes[f][1])
  d = 8*Sizes[f][0]*Sizes[f][1]/100
  for p in a:
    s += "&{:6.2f} &{:6.2f} &{:6.2f} &{:6.2f} ".format(p[0]/d,p[1]/d,p[2]/d,p[3]/d)
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
  parser.add_argument('op', help='operation to test: mm|mc|mz', type=str)
  parser.add_argument('-n', help='number of iterations (def 3)', default=3, type=int)  
  args = parser.parse_args()
     
  # run the task   
  s1 = time.time()
  with open(Logfile_name,"a") as logfile:
    if args.op=='mm':    # matrix multiplication 
      check_testfiles([".vc",".val",".vc.R",".vc.C"])
      table = test_time(args.n,logfile)
    elif args.op=='mc':   # matrix conversion
      table = test_zip(logfile)
    elif args.op=='mz':   # matrix compression
      table = test_compress(logfile)
    else: 
      print("Unknown operation! Must be mc, mm, or mz",file=sys.stderr)
      exit(1)
  e1 = time.time()
  print("Elapsed time: %.3f\n" % (e1-s1),file=sys.stderr)
  if args.op=='mm' or args.op=='mc' or args.op=='mz':  
    for s in table:
      print(s,end="")
  exit(0)

  
if __name__ == '__main__':
  main()
