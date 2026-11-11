#!/usr/bin/env python3
"""
vc2csv.py: rebuild a csv matrix from a .vc file and file.val
           (optionally file.wcode), for a split part (A, B) or a plain .vc

Usage:
    vc2csv.py [options] basename rows cols

Reads   basename.<part>.vc  uint32 symbols, 0 = end of row (default part A;
                            use -p B for the B part, -p "" for basename.vc)
        basename.val        distinct nonzero matrix values (float64 by default)
        basename.wcode      only with -w: int32 array, delta encoded (W = cumsum)
Writes  basename.<part>     dense matrix, one line per row, comma separated
                            (basename.csv when part is "")

Row blocks (-b n, n>1): the matrix is split in n files
        basename.n.0.<part>.vc, ..., basename.n.(n-1).<part>.vc
each holding ceil(rows/n) rows (the last block may have fewer).
The .val and .wcode files are shared by all blocks; one csv is written.

Decoding of a symbol p > 0:
    without -w:  real = p - 1
    with    -w:  real = W[p] - 1       (same as decode_entry() in rematrix.hpp)
    then         col  = real % cols
                 val  = Mval[real // cols]
"""
import argparse
import sys
import numpy as np


def decode_block(vcname, brows, cols, vals, w, vext, vtype, fout):
    """decode one .vc file (brows rows) and append its rows to fout"""
    vc = np.fromfile(vcname, dtype=np.uint32)

    # ---------- split into rows
    zeros = np.flatnonzero(vc == 0)           # end-of-row markers
    if len(zeros) != brows:
        sys.exit(f"Error: found {len(zeros)} rows in {vcname}, "
                 f"expected {brows}")
    if zeros[-1] != len(vc) - 1:
        sys.exit(f"Error: {vcname} does not end with an end-of-row marker")

    # ---------- decode all nonzero symbols at once
    sym = vc[vc != 0].astype(np.int64)
    if w is not None:
        if len(sym) and sym.max() >= len(w):
            sys.exit(f"Error: symbol {sym.max()} in {vcname} outside "
                     f".wcode (size {len(w)})")
        real = w[sym] - 1
        if len(real) and real.min() < 0:
            sys.exit("Error: negative code after applying .wcode")
    else:
        real = sym - 1
    colv = real % cols
    vid = real // cols
    if len(vid) and vid.max() >= len(vals):
        sys.exit(f"Error: value id {vid.max()} in {vcname} outside {vext} "
                 f"(size {len(vals)})")
    nzval = vals[vid]

    # start/end of each row inside colv/nzval (markers removed)
    counts = np.diff(zeros, prepend=-1) - 1
    ends = np.cumsum(counts)
    starts = ends - counts

    # ---------- write rows
    def fmt(x):
        if x == 0:
            return "0"
        if vtype == np.int32:
            return str(int(x))
        x = float(x)
        if x.is_integer() and abs(x) < 1e15:
            return str(int(x))
        return repr(x)           # shortest string that round-trips

    row = np.zeros(cols, dtype=vtype)
    for r in range(brows):
        s, e = starts[r], ends[r]
        row[:] = 0
        row[colv[s:e]] = nzval[s:e]
        fout.write(",".join(map(fmt, row)) + "\n")
    return len(sym)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("basename", help="base name of the matrix files")
    ap.add_argument("rows", type=int, help="number of rows")
    ap.add_argument("cols", type=int, help="number of columns")
    ap.add_argument("-b", type=int, default=1, metavar="n",
                    help="number of row blocks (def. 1)")
    ap.add_argument("-p", "--part", default="A", metavar="P",
                    help='split part to decode: A (default), B, or "" for '
                         'a plain basename.vc')
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
                    help="output file (default basename.<part>, or "
                         "basename.csv for a plain .vc)")
    args = ap.parse_args()

    if args.i and args.f:
        sys.exit("Error: options -i and -f are mutually exclusive")
    if args.absolute and not args.wcode:
        sys.exit("Error: --absolute requires -w")
    if args.rows < 1 or args.cols < 1:
        sys.exit("Error: rows and cols must be positive")
    if args.b < 1:
        sys.exit("Error: option -b must be at least 1")
    if args.b > args.rows:
        sys.exit("Error: too many row blocks")

    base, rows, cols, nb = args.basename, args.rows, args.cols, args.b
    part = ("." + args.part) if args.part else ""      # ".A", ".B" or ""
    if args.i:
        vext, vtype = ".ival", np.int32
    elif args.f:
        vext, vtype = ".fval", np.float32
    else:
        vext, vtype = ".val", np.float64

    # ---------- block sizes (same rule as csvmat2csrv and remm)
    maxblock = (rows + nb - 1) // nb
    sizes, remaining = [], rows
    for i in range(nb):
        r = min(remaining, maxblock)
        if r <= 0:
            sys.exit(f"Error: with {rows} rows and {nb} blocks the last "
                     f"blocks would be empty; use a different -b")
        sizes.append(r)
        remaining -= r

    # ---------- files shared by all blocks
    vals = np.fromfile(base + vext, dtype=vtype)
    w = None
    if args.wcode:
        w = np.fromfile(base + ".wcode", dtype=np.int32).astype(np.int64)
        if not args.absolute:
            w = np.cumsum(w)      # int64, so no overflow while summing

    # ---------- decode block by block
    out = args.output or (base + (part if part else ".csv"))
    nonz = 0
    with open(out, "w") as f:
        for i, brows in enumerate(sizes):
            blk = "" if nb == 1 else f".{nb}.{i}"
            vcname = f"{base}{blk}{part}.vc"
            nonz += decode_block(vcname, brows, cols, vals, w, vext, vtype, f)

    print(f"Written {out}: {rows} rows x {cols} cols, {nonz} nonzeros, "
          f"{len(vals)} distinct values, {nb} block(s)"
          f"{' (wcode)' if w is not None else ''}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
