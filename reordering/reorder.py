#!/usr/bin/env python3
import subprocess, os.path, sys, argparse, time, struct

Description = """
Reorder a .vc file splitted into row blocks 

Currently supported algorithms:
  pc:    PathCover
  mwm:   Maximum weighted matching
  lkh:   Lin-Kernighan
  pc+:   PathCover+
Example: python3 reorder.py pc /data/mm/covtype 581012 54 -b 8 -k 16
"""

Timelimit = 180000

# return the extension for multipart file
def filext_multipart(n,i):
  assert i<n, "Illegal parameters"
  if n==1:
    return ""
  return ".{tot}.{part}".format(tot=n,part=i)


# check that the test files exist and sizes are defined
def check_testfiles(args,sufxs):
  ok = True
  for f in Files:
    if f not in Sizes:
      print("  Size for test file", f, "missing",file=sys.stderr)
      ok = False
    
    for sx in sufxs:
      path = os.path.join(args.d,f + sx)
      if not os.path.exists(path):
        print("  Test file", path, "missing",file=sys.stderr)
        ok = False
  if not ok:
    print("Check -d option and the constants Files and Sizes",file=sys.stderr)
    sys.exit(1)


# execute command: return True is everything OK, False otherwise
def execute_command(command):
  try:
    subprocess.check_call(command.split(),timeout=Timelimit)
  except subprocess.TimeoutExpired:
      # caso time out
      print("ERROR: no result after %d seconds. Command:" % Timelimit)
      print("\t"+ command)
      return False
  except subprocess.CalledProcessError:
    print("Error executing command:")
    print("\t"+ command)
    return False
  print("OK    ", command)
  return True

def execute_command_verbose(cmd, exit_code, msg='Something went wrong: please contact the maintainers') :
  if execute_command(cmd):
    print('All done.')
  else:
    print(msg)
    sys.exit(exit_code)

def check_input_files(datadir,name,n):
  fullname = os.path.join(datadir,name)
  if not os.path.isfile(fullname):
    print("ERROR File", fullname, "missing")
    return  False
  else: 
    print("OK     File",fullname)
  # test/convert vco files   
  for i in range(n):
    fex = fullname + filext_multipart(n,i)
    # if vco is there nothing to do
    if os.path.isfile(fex + ".vco"):
      print("OK     File",fex+".vco")
    # if vc and not vco copy it 
    elif os.path.isfile(fex + ".vc"):
      command = "cp -p {name}.vc {name}.vco".format(name=fex)
      if not execute_command(command):
        return False
    # if both are missing error
    else:
      print("ERROR: Files", fullname, ".vc/.vco missing")
      return  False
  return True
  

def show_command_line(f):
  f.write("=== Command line: ") 
  for x in sys.argv:
     f.write(x+" ")
  f.write("\n")   

def main():
  #args
  show_command_line(sys.stderr)
  parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('algo',  help='algorithm to test: pc|mwm|lkh|pc+', type=str)  
  parser.add_argument('input', help='matrix file name (must be in csv format))', type=str)
  parser.add_argument('rows',  help='number of rows', type=int)
  parser.add_argument('cols',  help='number of columns', type=int)  
  parser.add_argument('-b', help='number of row blocks (default: 1)', type=int, default=1)
  parser.add_argument('-k', default=16, help='pruning parameter (default: 16)', type=int)
  args = parser.parse_args()

  #params
  algomap = {'pc':'cover', 'pc+':'cover2', 'mwm':'mwm', 'lkh':'tsp'}
  if args.algo not in algomap :
    algos_s = '|'.join(algomap.keys())
    print('Unknown algorithm: must be', algos_s)
    sys.exit(1)

  absdir = os.path.abspath(os.path.dirname(args.input))
  fname = os.path.basename(args.input)
     
  if os.path.isdir(absdir):
    print("OK     Data directory", absdir)
  else:
    print("ERROR: Invalid data directory")
    sys.exit(2)
    
  #if args.b<2:
  #  print("ERROR: Invalid number of blocks (must be >1)") 
  #  sys.exit(3) 
     
  if args.rows<1 or args.cols<1:
    print("ERROR: Number of rows and columns must be positive") 
    sys.exit(4) 
     
  if not check_input_files(absdir,fname,args.b):
    sys.exit(5)

  nr_first = None if args.b==1 else (args.rows + args.b - 1) // args.b
  nr_last = None if args.b==1 else args.rows - ((args.b - 1) * nr_first)

  cr_args = {'a_descr':args.algo, 'a':algomap[args.algo], 'd':absdir, 'f':fname, 'r':args.rows, 'c':args.cols, 'b':args.b, 
      'k':args.k, 'b_lst':args.b-1, 'r_fst':nr_first, 'r_lst':nr_last,
      'lkh_version':'LKH-3.0.7', 'lkh_link':'http://webhotel4.ruc.dk/~keld/research/LKH-3/LKH-3.0.7.tgz' }

  if args.b == 1 :
    reorder_matrix(cr_args)
  else :
    reorder_matrix_blocks(cr_args)

def reorder_matrix(cr_args) :
  print('Transposing the matrix...')
  cmd = 'python3 ./column_major.py {d}/{f}'.format(**cr_args)
  execute_command_verbose(cmd, 7)
##
  print('Generating the CSM...')
  cmd = './build/tsp_generator_pruned_local {d}/{f} {r} {c} {k} '.format(**cr_args)
  execute_command_verbose(cmd, 8)
##
  if cr_args['a_descr'] == 'lkh' :
    print('Checking LKH in local dir...')
    cmd = 'bash 09_get_lkh.sh {lkh_version} {lkh_link} '.format(**cr_args)
    execute_command_verbose(cmd, 9)
##
  print('Running', cr_args['a_descr'], '...')
  if False :
    pass
  elif cr_args['a_descr']=='pc' :
    cmd = 'python3 ./cover.py {d}/{f}.pruned_local_{k}.tsp '.format(**cr_args)
    execute_command_verbose(cmd, 10)
  elif cr_args['a_descr']=='pc+' :
    cmd = 'python3 ./cover2.py {d}/{f}.pruned_local_{k}.tsp '.format(**cr_args)
    execute_command_verbose(cmd, 10)
  elif cr_args['a_descr']=='mwm' :
    cmd = './build/mwm {d}/{f} pruned_local_{k} '.format(**cr_args)
    execute_command_verbose(cmd, 10)
    cmd = 'python3 ./mwm_sol_from_pairs.py {d}/{f} pruned_local_{k} '.format(**cr_args)
    execute_command_verbose(cmd, 10)
  elif cr_args['a_descr']=='lkh' :
    cmd = '{lkh_version}/LKH {d}/{f}.pruned_local_{k}.par'.format(**cr_args)
    execute_command_verbose(cmd, 10)
##
  print('Reordering the .vc file...')
  cmd = 'ln -sf {d}/{f}.vco {d}/{f}.vc '.format(**cr_args)
  execute_command_verbose(cmd, 11)
  cmd = './vc_reorder.x {d}/{f} {r} {c} {d}/{f}.pruned_local_{k}.{a}.solution '.format(**cr_args)
  execute_command_verbose(cmd, 11)
  cmd = 'ln -sf {d}/{f}.pruned_local_{k}.{a}.solution.vc {d}/{f}.vc '.format(**cr_args)
  execute_command_verbose(cmd, 11) 
##
  print('Cleaning...')
  cmd = 'rm -r '
  cmd += '{d}/{f}_cols '.format(**cr_args)
  cmd += '{d}/{f}.pruned_local_{k}.par '.format(**cr_args)
  cmd += '{d}/{f}.pruned_local_{k}.tsp '.format(**cr_args)
  cmd += '{d}/{f}.pruned_local_{k}.{a}.solution '.format(**cr_args)
  #if cr_args['a_descr'] == 'lkh' :
  #  cmd += '{d}/{f}.pruned_local_{k}.log '.format(**cr_args)
  
  execute_command_verbose(cmd, 12)
  return   

def reorder_matrix_blocks(cr_args) :
  print('Dividing into row chunks...')
  cmd ='python3 csv_splitter.py {d}/{f} {r} {b} '.format(**cr_args)
  execute_command_verbose(cmd, 6)
##
  print('Transposing each row block...')
  cmd = 'bash 07_transpose.sh {d}/{f} {b} '.format(**cr_args)
  execute_command_verbose(cmd, 7)
##
  print('Generating the CSM for each row block...')
  cmd = 'bash 08_generate_csm.sh {d}/{f} {r} {c} {b} {k}'.format(**cr_args)
  execute_command_verbose(cmd, 8)
##
  if cr_args['a_descr'] == 'lkh' :
    print('Checking LKH in local dir...')
    cmd = 'bash 09_get_lkh.sh {lkh_version} {lkh_link} '.format(**cr_args)
    execute_command_verbose(cmd, 9)
##
  print('Running', cr_args['a_descr'], 'upon each row block...')
  cmd = 'bash 10_run_{a}.sh {d}/{f} {b} {k} {lkh_version} '.format(**cr_args)
  execute_command_verbose(cmd, 10)
##
  print('Reordering each .vc file...')
  cmd = 'bash 11_reorder.sh {d}/{f} {r} {c} {b} {k} {a}'.format(**cr_args)
  execute_command_verbose(cmd, 11) 
##
  print('Cleaning...')
  cmd = 'rm -r '
  for i in range(cr_args['b']) :
    cmd += '{d}/{f}.{b}.{i}_cols '.format(**cr_args, i=i)
    cmd += '{d}/{f}.{b}.{i}.pruned_local_{k}.par '.format(**cr_args, i=i)
    cmd += '{d}/{f}.{b}.{i}.pruned_local_{k}.tsp '.format(**cr_args, i=i)
    cmd += '{d}/{f}.{b}.{i}.pruned_local_{k}.{a}.solution '.format(**cr_args, i=i)
    if cr_args['a_descr'] == 'lkh' :
      cmd += '{d}/{f}.{b}.{i}.pruned_local_{k}.log '.format(**cr_args, i=i)
  
  execute_command_verbose(cmd, 12)
  return     

       
if __name__ == '__main__':
  main()
