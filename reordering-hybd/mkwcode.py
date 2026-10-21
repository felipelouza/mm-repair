#!/usr/bin/env python3
"""
mkwcode.py: compute file.wcode from file.A.vc and file.B.vc and rewrite
            the two .vc files with the mapped alphabet
            (same as matrepair --split --mapping, i.e. csvmat2csrv -s -m)

Usage:
    mkwcode.py [options] basename

Reads   basename.A.vc, basename.B.vc    uint32 CSRV symbols, no alphabet
                                        mapping (0 = end of row)
Writes  basename.wcode                  the alphabet in delta encoded form
        basename.A.vc, basename.B.vc    REWRITTEN IN PLACE: every symbol p is
                                        replaced by its rank in the alphabet
                                        (0 stays 0). Use --wcode-only to keep
                                        the original files.

With row blocks (-b n, n>1) the files are
        basename.n.i.A.vc, basename.n.i.B.vc    for i = 0 .. n-1
and a single basename.wcode is produced for all of them (as csvmat2csrv
does: one alphabet for all blocks).

The .wcode file is built as in csvmat2csrv (store_alphabet):
  - SIGMA = sorted set of all distinct symbols found in the files, plus 0
  - the mapped symbol is the rank of the original symbol in SIGMA,
    so that W[rank] = SIGMA[rank] (and W[0] = 0)
  - the file contains W in delta form: file[0] = W[0],
    file[i] = W[i] - W[i-1], as uint32; so W = cumsum(file)

Warning: the script cannot tell whether a .vc file has already been
mapped; running it twice on the same files maps them twice.
"""
import argparse
import os
import sys
import numpy as np


def vc_names(base, nb):
    for i in range(nb):
        for part in ("A", "B"):
            if nb == 1:
                yield f"{base}.{part}.vc"
            else:
                yield f"{base}.{nb}.{i}.{part}.vc"


def read_symbols(fname):
    if not os.path.exists(fname):
        sys.exit(f"Error: cannot find {fname}")
    if os.path.getsize(fname) % 4 != 0:
        sys.exit(f"Error: size of {fname} is not a multiple of 4 bytes")
    return np.fromfile(fname, dtype=np.uint32)


def rewrite_mapped(fname, alphabet):
    """replace every symbol by its rank in alphabet (as map_alphabet does:
    write to fname.tmp, then rename over the original)"""
    sym = read_symbols(fname)
    rank = np.searchsorted(alphabet, sym)
    # every symbol must be in the alphabet (it was built from these files)
    if len(sym) and (rank.max() >= len(alphabet)
                     or not np.array_equal(alphabet[rank], sym)):
        sys.exit(f"Error: symbol not in alphabet while mapping {fname}")
    tmp = fname + ".tmp"
    rank.astype(np.uint32).tofile(tmp)
    os.replace(tmp, fname)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("basename", help="base name of the matrix files")
    ap.add_argument("-b", type=int, default=1, metavar="n",
                    help="number of row blocks (def. 1)")
    ap.add_argument("--wcode-only", action="store_true",
                    help="write .wcode but do not rewrite the .vc files")
    ap.add_argument("-o", "--output", default=None,
                    help="output .wcode file (default basename.wcode)")
    args = ap.parse_args()
    if args.b < 1:
        sys.exit("Error: option -b must be at least 1")

    base, nb = args.basename, args.b
    files = list(vc_names(base, nb))

    # ---------- collect the alphabet (one set for all files)
    alphabet = np.array([0], dtype=np.uint32)     # 0 is always in SIGMA
    total = 0
    for fname in files:
        sym = read_symbols(fname)
        total += len(sym)
        alphabet = np.union1d(alphabet, np.unique(sym))
    alphabet = alphabet.astype(np.uint32)         # sorted, distinct

    if alphabet[-1] >= 1 << 30:
        print("Warning: largest code is >= 2**30", file=sys.stderr)

    # ---------- write .wcode: file[0]=W[0], file[i]=W[i]-W[i-1]
    a64 = alphabet.astype(np.int64)
    delta = np.diff(a64, prepend=0).astype(np.uint32)
    out = args.output or base + ".wcode"
    delta.tofile(out)
    print(f"Written {out}: {len(alphabet)} symbols (alphabet size, 0 "
          f"included), largest code {int(alphabet[-1])}; "
          f"read {total} symbols from {len(files)} file(s)", file=sys.stderr)

    # ---------- rewrite the .vc files with the mapped alphabet
    if args.wcode_only:
        return
    for fname in files:
        rewrite_mapped(fname, alphabet)
        print(f"Mapped {fname}", file=sys.stderr)
    print(f"Mapped alphabet: symbols now in [0, {len(alphabet) - 1}]",
          file=sys.stderr)


if __name__ == "__main__":
    main()
