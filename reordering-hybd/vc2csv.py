#!/usr/bin/env python3
"""
vc2csv.py: rebuild file.csv from file.vc and file.val (optionally file.wcode)

Usage:
    vc2csv.py [options] basename rows cols

Reads   basename.vc      uint32 symbols, 0 = end of row
        basename.val     distinct nonzero matrix values (float64 by default)
        basename.wcode   only with -w: int32 array, delta encoded (W = cumsum)
Writes  basename.csv     dense matrix, one line per row, comma separated

Decoding of a symbol p > 0:
    without -w:  real = p - 1
    with    -w:  real = W[p] - 1       (same as decode_entry() in rematrix.hpp)
    then         col  = real % cols
                 val  = Mval[real // cols]
"""
import argparse
import sys
import numpy as np


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("basename", help="base name of the matrix files")
    ap.add_argument("rows", type=int, help="number of rows")
    ap.add_argument("cols", type=int, help="number of columns")
    ap.add_argument("-w", "--wcode", action="store_true",
                    help="symbols in .vc are remapped: use basename.wcode")
    ap.add_argument("--absolute", action="store_true",
                    help="with -w: .wcode already holds absolute values "
                         "(no prefix sum)")
    ap.add_argument("-i", action="store_true",
                    help="values are int32 (.ival) instead of float64 (.val)")
    ap.add_argument("-f", action="store_true",
                    help="values are float32 (.fval) instead of float64 (.val)")
    ap.add_argument("-o", "--output", default=None,
                    help="output file (default basename.csv)")
    args = ap.parse_args()

    if args.i and args.f:
        sys.exit("Error: options -i and -f are mutually exclusive")
    if args.absolute and not args.wcode:
        sys.exit("Error: --absolute requires -w")
    if args.rows < 1 or args.cols < 1:
        sys.exit("Error: rows and cols must be positive")

    base, rows, cols = args.basename, args.rows, args.cols
    if args.i:
        vext, vtype = ".ival", np.int32
    elif args.f:
        vext, vtype = ".fval", np.float32
    else:
        vext, vtype = ".val", np.float64

    # ---------- load files
    vc = np.fromfile(base + ".A.vc", dtype=np.uint32)
    vals = np.fromfile(base + vext, dtype=vtype)
    w = None
    if args.wcode:
        w = np.fromfile(base + ".wcode", dtype=np.int32).astype(np.int64)
        if not args.absolute:
            w = np.cumsum(w)      # int64, so no overflow while summing

    # ---------- split into rows
    zeros = np.flatnonzero(vc == 0)           # end-of-row markers
    if len(zeros) != rows:
        sys.exit(f"Error: found {len(zeros)} rows in .vc file, expected {rows}")
    if zeros[-1] != len(vc) - 1:
        sys.exit("Error: .vc file does not end with an end-of-row marker")

    # ---------- decode all nonzero symbols at once
    sym = vc[vc != 0].astype(np.int64)
    if w is not None:
        if len(sym) and sym.max() >= len(w):
            sys.exit(f"Error: symbol {sym.max()} outside .wcode "
                     f"(size {len(w)})")
        real = w[sym] - 1
        if len(real) and real.min() < 0:
            sys.exit("Error: negative code after applying .wcode")
    else:
        real = sym - 1
    colv = real % cols
    vid = real // cols
    if len(vid) and vid.max() >= len(vals):
        sys.exit(f"Error: value id {vid.max()} outside {vext} "
                 f"(size {len(vals)})")
    nzval = vals[vid]

    # start/end of each row inside sym/colv/nzval (markers removed)
    counts = np.diff(zeros, prepend=-1) - 1
    ends = np.cumsum(counts)
    starts = ends - counts

    # ---------- write csv
    def fmt(x):
        if x == 0:
            return "0"
        if vtype == np.int32:
            return str(int(x))
        x = float(x)
        if x.is_integer() and abs(x) < 1e15:
            return str(int(x))
        return repr(x)           # shortest string that round-trips

    out = args.output or base + ".A"
    row = np.zeros(cols, dtype=vtype)
    with open(out, "w") as f:
        for r in range(rows):
            s, e = starts[r], ends[r]
            row[:] = 0
            row[colv[s:e]] = nzval[s:e]
            f.write(",".join(map(fmt, row)) + "\n")

    print(f"Written {out}: {rows} rows x {cols} cols, "
          f"{len(sym)} nonzeros, {len(vals)} distinct values"
          f"{' (wcode)' if w is not None else ''}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
