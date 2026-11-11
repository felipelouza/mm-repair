#!/usr/bin/env python3
"""
dataset_to_csv.py

Convert MovieLens ratings and KDD Cup 1999 network-intrusion data into
plain numeric CSV matrices (rows x cols, comma-separated, no missing
values), suitable as input to matrix-compression tools such as the
CSRV/RePair pipeline from "Improving Matrix-vector Multiplication via
Lossless Grammar-Compressed Matrices" (Ferragina et al., VLDB 2022).

-----------------------------------------------------------------------
MovieLens
-----------------------------------------------------------------------
Input: the official ratings.csv file (columns: userId,movieId,rating,timestamp)
  Download from: https://grouplens.org/datasets/movielens/
    - 100K (small, dense CSV is fine): https://files.grouplens.org/datasets/movielens/ml-latest-small.zip
    - 1M:   https://files.grouplens.org/datasets/movielens/ml-1m.zip
    - 25M:  https://files.grouplens.org/datasets/movielens/ml-25m.zip

Output modes:
  --format dense   -> a full user x movie matrix, 0 where unrated.
                       Only practical for small datasets (e.g. 100K);
                       refuses to run on huge matrices unless --force is given.
  --format sparse  -> a 3-column "row,col,value" CSV (1-indexed dropped,
                       0-indexed used) plus a small *.shape.csv side file
                       with "rows,cols". This scales to the 25M dataset.

-----------------------------------------------------------------------
KDD Cup 1999
-----------------------------------------------------------------------
Input: the raw kddcup.data (or kddcup.data_10_percent) file, comma
separated, NO header, 41 features + 1 label column.
  Download from:
    - Full:  https://kdd.ics.uci.edu/databases/kddcup99/kddcup.data.gz
    - 10%:   https://kdd.ics.uci.edu/databases/kddcup99/kddcup.data_10_percent.gz

The 3 categorical columns (protocol_type, service, flag) and the label
are label-encoded to small integers so that every column ends up numeric,
matching the M in R^{n,m} format the paper assumes.

-----------------------------------------------------------------------
GloVe word vectors
-----------------------------------------------------------------------
Input: a GloVe .txt file, one line per word: "word v_1 v_2 ... v_d"
(space separated, no header). This is an extremely TALL matrix: rows =
vocabulary size, cols = embedding dimension.
  Project page: https://nlp.stanford.edu/projects/glove/
    - glove.6B.300d:      400,000 x 300   https://nlp.stanford.edu/data/glove.6B.zip
    - glove.42B.300d:    ~1,900,000 x 300  https://nlp.stanford.edu/data/glove.42B.300d.zip
    - glove.840B.300d:   ~2,200,000 x 300  https://nlp.stanford.edu/data/glove.840B.300d.zip

The leading word token is stripped from each line (the output CSV is
purely numeric); pass --vocab-output to also save a (row_index,word)
side file so you can map matrix rows back to vocabulary.

Since raw 32-bit GloVe floats almost never repeat exactly, grammar
compression gets little traction on them as-is. Two optional, mutually
exclusive quantization modes create repeated values on purpose:
  --quantize-decimals N  round every value to N decimal places.
  --quantize-levels K    uniformly bucket every value into K levels
                          spanning the matrix's global min/max (a full
                          first pass over the file is needed to find
                          that range).

Every subcommand prints the final matrix shape (rows x cols) on exit.

-----------------------------------------------------------------------
OLMoE / Mixture-of-Experts router matrix
-----------------------------------------------------------------------
Input: any local .txt file of text. No dataset to download -- instead
this runs a sparse MoE model (default: allenai/OLMoE-1B-7B-0924, fully
open weights) over your text and extracts ONE layer's real per-token
expert-routing matrix: rows = tokens (in reading order), cols = experts.
Needs `pip install torch transformers`, and the first run needs network
access to download the model from the Hugging Face Hub (cached after).

This matrix is ARCHITECTURALLY sparse, not just from quantization: OLMoE
routes each token to its top 8 of 64 experts, so every row has exactly
8 nonzero entries and 56 exact zeros, guaranteed by construction. The
nonzero entries are continuous (renormalized) softmax weights, so you
can still test --quantize-decimals / --quantize-levels on them, on top
of the structural sparsity.

  --layer N           which transformer layer's router to use
                       (default: the middle layer)
  --seq-len / --batch-size / --max-tokens  control how much text is
                       processed and how it's chunked into forward passes
  --num-experts / --top-k / --normalize-topk  override what's normally
                       auto-detected from the model config (useful if
                       you point --model at a different MoE architecture)

Usage examples:
  python3 dataset_to_csv.py olmoe corpus.txt out_matrix.csv
  python3 dataset_to_csv.py olmoe corpus.txt out_matrix.csv --layer 4 --max-tokens 200000
  python3 dataset_to_csv.py olmoe corpus.txt out_matrix.csv --quantize-levels 16 \
      --triplet-output out_matrix.triplets.txt

-----------------------------------------------------------------------
Sparse triplet output (movielens / kddcup99 / glove / olmoe)
-----------------------------------------------------------------------
Pass --triplet-output PATH to ANY subcommand to additionally write a
plain text file, one nonzero per line:
    row<delim>col<delim>value
in strict row-major order (row ascending, then col ascending within
each row) with NO duplicate (row, col) pairs. This is independent of
whatever the primary --output format is, so you'll get both files.

  --triplet-delimiter   field separator (default: a single space)
  --on-duplicate        how to resolve a genuine duplicate (row, col)
                         seen in the source data: keep_last (default),
                         keep_first, sum, or error. Only movielens can
                         actually produce duplicates (e.g. a user rating
                         the same movie twice); kddcup99 and glove build
                         each row from a single source line, so they
                         can't.

Usage examples:
  python3 dataset_to_csv.py movielens ratings.csv out_matrix.csv --format dense
  python3 dataset_to_csv.py movielens ratings.csv out_matrix --format sparse
  python3 dataset_to_csv.py movielens ratings.csv out_matrix.csv --triplet-output out_matrix.triplets.txt
  python3 dataset_to_csv.py kddcup99 kddcup.data_10_percent out_matrix.csv
  python3 dataset_to_csv.py kddcup99 kddcup.data_10_percent out_matrix.csv --keep-label
  python3 dataset_to_csv.py glove glove.6B.300d.txt out_matrix.csv
  python3 dataset_to_csv.py glove glove.6B.300d.txt out_matrix.csv --quantize-decimals 1
  python3 dataset_to_csv.py glove glove.6B.300d.txt out_matrix.csv --quantize-levels 256 \
      --vocab-output out_vocab.csv --triplet-output out_matrix.triplets.txt
"""

import argparse
import sys
import gzip
import csv as csvmod

import numpy as np
import pandas as pd
from scipy import sparse


# ----------------------------------------------------------------------
# Shared: row-major, duplicate-free sparse triplet writer
# ----------------------------------------------------------------------
#
# Output format: a plain text file, one nonzero per line, as
#   row<delim>col<delim>value
# ordered in row-major order (row ascending, then col ascending within
# each row) with no duplicate (row, col) pairs. This is independent of
# whatever the converter's primary --output is, so it can be requested
# alongside the dense/sparse-CSV output via --triplet-output.

def _write_sorted_unique_triplets(rows, cols, vals, output_path,
                                   delimiter=" ", on_duplicate="keep_last"):
    """
    rows, cols, vals: equal-length 1D array-likes describing (possibly
    unsorted, possibly duplicated) nonzero entries of a matrix.

    Writes output_path as row-major sorted, duplicate-free triplets.
    on_duplicate controls how repeated (row, col) entries are resolved:
      "keep_last"  -> use the value of the last occurrence (default)
      "keep_first" -> use the value of the first occurrence
      "sum"        -> sum all values at that (row, col)
      "error"      -> raise if any duplicate (row, col) is found

    Returns (n_triplets_written, n_duplicate_entries_found).
    """
    if on_duplicate not in ("keep_last", "keep_first", "sum", "error"):
        raise ValueError(f"unknown on_duplicate strategy: {on_duplicate}")

    df = pd.DataFrame({"row": np.asarray(rows), "col": np.asarray(cols),
                        "val": np.asarray(vals, dtype=np.float64)})
    df = df[df["val"] != 0]  # only nonzeros belong in a sparse triplet file

    dup_mask = df.duplicated(subset=["row", "col"], keep=False)
    n_dup = int(dup_mask.sum())
    if n_dup:
        if on_duplicate == "error":
            raise ValueError(
                f"{n_dup} duplicate (row, col) entries found; pass "
                f"on_duplicate='keep_last'/'keep_first'/'sum' to resolve them"
            )
        elif on_duplicate == "sum":
            df = df.groupby(["row", "col"], as_index=False, sort=False)["val"].sum()
        else:  # keep_last / keep_first map directly to pandas' keep=
            keep = "last" if on_duplicate == "keep_last" else "first"
            df = df.drop_duplicates(subset=["row", "col"], keep=keep)

    df = df.sort_values(["row", "col"], kind="mergesort")  # stable, row-major
    df.to_csv(output_path, sep=delimiter, header=False, index=False)
    return len(df), n_dup


# ----------------------------------------------------------------------
# MovieLens
# ----------------------------------------------------------------------

def movielens_to_csv(ratings_path, output_path, fmt="sparse", force=False,
                      triplet_output=None, triplet_delimiter=" ",
                      on_duplicate="keep_last"):
    """
    Pivot a MovieLens ratings.csv into a user x movie matrix.

    fmt="dense"  -> writes output_path as a full comma-separated matrix.
    fmt="sparse" -> writes output_path as "row,col,value" triples
                    (0-indexed) and output_path + '.shape.csv' with
                    "rows,cols" on one line.

    If triplet_output is given, ALSO writes a row-major, duplicate-free
    "row<delim>col<delim>value" text file there (see
    _write_sorted_unique_triplets), independent of fmt.
    """
    df = pd.read_csv(ratings_path)
    required = {"userId", "movieId", "rating"}
    if not required.issubset(df.columns):
        raise ValueError(f"Expected columns {required}, got {list(df.columns)}")

    # Map raw IDs to dense 0-indexed row/col indices.
    user_idx, user_levels = pd.factorize(df["userId"], sort=True)
    movie_idx, movie_levels = pd.factorize(df["movieId"], sort=True)
    n_rows, n_cols = len(user_levels), len(movie_levels)
    values = df["rating"].to_numpy(dtype=np.float64)

    print(f"[movielens] {n_rows} users x {n_cols} movies, "
          f"{len(df)} ratings ({100.0 * len(df) / (n_rows * n_cols):.3f}% dense)")

    if triplet_output is not None:
        n_written, n_dup = _write_sorted_unique_triplets(
            user_idx, movie_idx, values, triplet_output,
            delimiter=triplet_delimiter, on_duplicate=on_duplicate)
        if n_dup:
            print(f"[movielens] triplet output: resolved {n_dup} duplicate "
                  f"(row,col) entries using strategy '{on_duplicate}'")
        print(f"[movielens] wrote {n_written} row-major, duplicate-free "
              f"triplets to {triplet_output}")

    if fmt == "sparse":
        with open(output_path, "w", newline="") as f:
            w = csvmod.writer(f)
            for r, c, v in zip(user_idx, movie_idx, values):
                w.writerow([int(r), int(c), v])
        with open(str(output_path) + ".shape.csv", "w", newline="") as f:
            csvmod.writer(f).writerow([n_rows, n_cols])
        print(f"[movielens] wrote sparse triples to {output_path} "
              f"(shape info in {output_path}.shape.csv)")
        return n_rows, n_cols

    if fmt == "dense":
        n_cells = n_rows * n_cols
        # Rough safety check: dense CSV of doubles printed as text is
        # roughly 8-12 bytes/cell once formatted.
        est_bytes = n_cells * 10
        if est_bytes > 2_000_000_000 and not force:
            raise ValueError(
                f"Dense matrix would be ~{est_bytes / 1e9:.1f} GB of CSV "
                f"({n_rows} x {n_cols}). Use --format sparse instead, or "
                f"pass --force to proceed anyway."
            )
        mat = sparse.coo_matrix(
            (values, (user_idx, movie_idx)), shape=(n_rows, n_cols)
        ).toarray()
        np.savetxt(output_path, mat, delimiter=",", fmt="%.4g")
        print(f"[movielens] wrote dense {n_rows}x{n_cols} matrix to {output_path}")
        return n_rows, n_cols

    raise ValueError("fmt must be 'dense' or 'sparse'")


# ----------------------------------------------------------------------
# KDD Cup 1999
# ----------------------------------------------------------------------

KDD_COLUMNS = [
    "duration", "protocol_type", "service", "flag", "src_bytes", "dst_bytes",
    "land", "wrong_fragment", "urgent", "hot", "num_failed_logins",
    "logged_in", "num_compromised", "root_shell", "su_attempted",
    "num_root", "num_file_creations", "num_shells", "num_access_files",
    "num_outbound_cmds", "is_host_login", "is_guest_login", "count",
    "srv_count", "serror_rate", "srv_serror_rate", "rerror_rate",
    "srv_rerror_rate", "same_srv_rate", "diff_srv_rate",
    "srv_diff_host_rate", "dst_host_count", "dst_host_srv_count",
    "dst_host_same_srv_rate", "dst_host_diff_srv_rate",
    "dst_host_same_src_port_rate", "dst_host_srv_diff_host_rate",
    "dst_host_serror_rate", "dst_host_srv_serror_rate",
    "dst_host_rerror_rate", "dst_host_srv_rerror_rate", "label",
]

CATEGORICAL_COLUMNS = ["protocol_type", "service", "flag"]


def _open_maybe_gzip(path):
    if str(path).endswith(".gz"):
        return gzip.open(path, "rt")
    return open(path, "rt")


def kddcup99_to_csv(data_path, output_path, keep_label=False,
                     triplet_output=None, triplet_delimiter=" ",
                     on_duplicate="keep_last"):
    """
    Read the raw KDD Cup 1999 connection-records file (comma separated,
    no header) and write a fully-numeric CSV matrix: the 3 categorical
    columns (protocol_type, service, flag) are label-encoded to integers.
    The trailing 'label' column is dropped by default (it's the
    classification target, e.g. 'normal.', 'neptune.', ...); pass
    keep_label=True to label-encode and keep it as the last column.

    If triplet_output is given, ALSO writes a row-major, duplicate-free
    "row<delim>col<delim>value" text file of the matrix's nonzero entries.
    """
    with _open_maybe_gzip(data_path) as f:
        df = pd.read_csv(f, header=None, names=KDD_COLUMNS)

    n_rows_in = len(df)

    if not keep_label:
        df = df.drop(columns=["label"])
    else:
        df["label"], label_levels = pd.factorize(df["label"], sort=True)
        print(f"[kddcup99] encoded {len(label_levels)} label classes "
              f"(0..{len(label_levels)-1})")

    for col in CATEGORICAL_COLUMNS:
        df[col], levels = pd.factorize(df[col], sort=True)
        print(f"[kddcup99] encoded column '{col}': {len(levels)} distinct values")

    # Sanity: every column should now be numeric.
    non_numeric = [c for c in df.columns if not np.issubdtype(df[c].dtype, np.number)]
    if non_numeric:
        raise RuntimeError(f"Unexpected non-numeric columns left: {non_numeric}")

    df.to_csv(output_path, header=False, index=False)
    print(f"[kddcup99] wrote {n_rows_in} x {df.shape[1]} numeric matrix "
          f"to {output_path}")

    if triplet_output is not None:
        coo = sparse.coo_matrix(df.to_numpy(dtype=np.float64))
        n_written, n_dup = _write_sorted_unique_triplets(
            coo.row, coo.col, coo.data, triplet_output,
            delimiter=triplet_delimiter, on_duplicate=on_duplicate)
        print(f"[kddcup99] wrote {n_written} row-major, duplicate-free "
              f"triplets to {triplet_output}")

    return n_rows_in, df.shape[1]


# ----------------------------------------------------------------------
# GloVe word vectors
# ----------------------------------------------------------------------

def _parse_glove_line(line, expected_dim_holder):
    line = line.strip()
    if not line:
        return None

    # Support comma-separated CSV and whitespace-separated GloVe.
    parts = line.split(",") if "," in line else line.split()

    try:
        # Numeric CSV: all fields are vector values.
        vec = [float(x) for x in parts]
        word = None
    except ValueError:
        # Original GloVe: first field is the word.
        try:
            vec = [float(x) for x in parts[1:]]
            word = parts[0]
        except ValueError:
            return None

    if not vec:
        return None

    expected_dim = expected_dim_holder[0]

    if expected_dim is None:
        expected_dim_holder[0] = len(vec)
    elif len(vec) != expected_dim:
        return None

    return word, vec
    
def _parse_glove_line_tmp(line, expected_dim_holder):
    """
    Parse one GloVe line "word v1 v2 ... vd". expected_dim_holder is a
    single-element list used as a mutable box so the expected dimension,
    once learned from the first valid line, is shared across calls.
    Returns (word, [floats]) or None if the line is unusable.
    """
    parts = line.rstrip("\n").split(" ")
    if len(parts) < 2:
        return None

    expected_dim = expected_dim_holder[0]
    if expected_dim is None:
        expected_dim = len(parts) - 1
        expected_dim_holder[0] = expected_dim

    if len(parts) - 1 == expected_dim:
        word, vec_parts = parts[0], parts[1:]
    elif len(parts) - 1 > expected_dim:
        # Rare GloVe quirk: the "word" token itself contained a space.
        # Keep the trailing expected_dim tokens as the vector.
        word = " ".join(parts[:-expected_dim])
        vec_parts = parts[-expected_dim:]
    else:
        return None  # malformed/short line, skip it

    try:
        vec = [float(x) for x in vec_parts]
    except ValueError:
        return None
    return word, vec


def glove_to_csv(input_path, output_path, quantize_decimals=None,
                  quantize_levels=None, vocab_output=None,
                  triplet_output=None, triplet_delimiter=" "):
    """
    Convert a GloVe .txt embedding file into a purely numeric CSV matrix
    (rows = vocabulary, cols = embedding dimension), streaming line by
    line so it works on multi-gigabyte files without loading everything
    into memory at once.

    quantize_decimals: round every value to N decimal places.
    quantize_levels:   uniformly bucket every value into K levels
                        spanning the matrix's global min/max (requires a
                        first streaming pass to find that range).
    These two options are mutually exclusive.
    vocab_output: optional path to write a (row_index,word) CSV so rows
                  can be mapped back to vocabulary later.
    triplet_output: optional path to ALSO write a row-major "row<delim>
                  col<delim>value" text file of nonzero entries. Rows
                  are emitted in file order and each row's columns are
                  written 0..d-1, so the result is automatically
                  row-major and duplicate-free with no extra sorting.
    """
    if quantize_decimals is not None and quantize_levels is not None:
        raise ValueError("quantize_decimals and quantize_levels are mutually exclusive")

    vmin = vmax = None
    if quantize_levels is not None:
        if quantize_levels < 2:
            raise ValueError("quantize_levels must be >= 2")
        expected_dim_holder = [None]
        vmin, vmax = float("inf"), float("-inf")
        n_seen = 0
        with _open_maybe_gzip(input_path) as f:
            for line in f:
                parsed = _parse_glove_line(line, expected_dim_holder)
                if parsed is None:
                    continue
                _, vec = parsed
                lo, hi = min(vec), max(vec)
                vmin, vmax = min(vmin, lo), max(vmax, hi)
                n_seen += 1
        print(f"[glove] first pass: {n_seen} vectors, dim={expected_dim_holder[0]}, "
              f"range=[{vmin:.4f}, {vmax:.4f}]")

    expected_dim_holder = [None]
    n_rows = 0
    n_triplets = 0
    vocab_f = open(vocab_output, "w", newline="") if vocab_output else None
    vocab_writer = csvmod.writer(vocab_f) if vocab_f else None
    triplet_f = open(triplet_output, "w", newline="") if triplet_output else None
    skipped = 0

    try:
        with _open_maybe_gzip(input_path) as f, \
             open(output_path, "w", newline="") as out_f:
            writer = csvmod.writer(out_f)
            step = (vmax - vmin) / (quantize_levels - 1) if quantize_levels and vmax > vmin else None

            for line in f:
                parsed = _parse_glove_line(line, expected_dim_holder)
                if parsed is None:
                    skipped += 1
                    continue
                word, vec = parsed
                arr = np.asarray(vec, dtype=np.float64)

                if quantize_levels is not None and step:
                    arr = vmin + np.round((arr - vmin) / step) * step
                elif quantize_decimals is not None:
                    arr = np.round(arr, quantize_decimals)

                writer.writerow(arr.tolist())
                if vocab_writer:
                    vocab_writer.writerow([n_rows, word])
                if triplet_f:
                    for col, v in enumerate(arr):
                        if v != 0:
                            triplet_f.write(
                                f"{n_rows}{triplet_delimiter}{col}{triplet_delimiter}{v}\n"
                            )
                            n_triplets += 1
                n_rows += 1
    finally:
        if vocab_f:
            vocab_f.close()
        if triplet_f:
            triplet_f.close()

    n_cols = expected_dim_holder[0] or 0
    if skipped:
        print(f"[glove] skipped {skipped} malformed line(s)")
    qmsg = ""
    if quantize_decimals is not None:
        qmsg = f" (quantized to {quantize_decimals} decimal places)"
    elif quantize_levels is not None:
        qmsg = f" (quantized to {quantize_levels} levels)"
    print(f"[glove] wrote {n_rows} x {n_cols} matrix to {output_path}{qmsg}")
    if vocab_output:
        print(f"[glove] wrote (row_index,word) vocabulary mapping to {vocab_output}")
    if triplet_output:
        print(f"[glove] wrote {n_triplets} row-major, duplicate-free "
              f"triplets to {triplet_output}")
    return n_rows, n_cols


# ----------------------------------------------------------------------
# OLMoE / general Mixture-of-Experts router matrix
# ----------------------------------------------------------------------
#
# Extracts the actual per-token expert-routing matrix from a sparse MoE
# transformer (rows = tokens, cols = experts). Sparsity here is
# architectural: every row has exactly `top_k` nonzero entries, set by
# the model's top-k router, not by anything we do -- quantization only
# affects the *magnitude* of the nonzero weights, never their positions.
#
# Verified against transformers' real OlmoeTopKRouter.forward():
#   probs = softmax(router_logits, dim=-1)      # over ALL experts
#   top_vals, top_idx = topk(probs, top_k)
#   if norm_topk_prob: top_vals /= top_vals.sum(-1)
# (Mixtral/Qwen-MoE use the same softmax-then-topk-then-renormalize
# pattern, just with different config attribute names; see num_experts/
# top_k overrides below.)

DEFAULT_MOE_MODEL = "allenai/OLMoE-1B-7B-0924"
DEFAULT_TRANSFORMER_MODEL = "gpt2"


def _lazy_import_torch_transformers():
    try:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as e:
        raise ImportError(
            "this subcommand needs torch and transformers installed: "
            "pip install torch transformers"
        ) from e
    return torch, AutoModelForCausalLM, AutoTokenizer


def _load_hf_model(model_name, device, dtype, tag=""):
    """
    Load a HuggingFace causal LM + tokenizer. Returns (torch, model, tokenizer).
    dtype='auto' resolves to bfloat16 on CUDA, float32 on CPU.
    """
    torch, AutoModelForCausalLM, AutoTokenizer = _lazy_import_torch_transformers()
    if device is None:
        device = "cuda" if torch.cuda.is_available() else "cpu"
    torch_dtype = (torch.bfloat16 if device == "cuda" else torch.float32) \
        if dtype == "auto" else getattr(torch, dtype)
    prefix = f"[{tag}]" if tag else "[model]"
    print(f"{prefix} loading {model_name} on {device} ({torch_dtype}) ...")
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    model = AutoModelForCausalLM.from_pretrained(model_name, torch_dtype=torch_dtype)
    model.to(device)
    model.eval()
    return torch, model, tokenizer


def _tokenize_and_chunk(text_path, tokenizer, seq_len, max_tokens, tag=""):
    """
    Read text_path, tokenize with tokenizer, split into fixed-length chunks.
    Returns (all_ids list, n_chunks int, used_tokens int).
    """
    prefix = f"[{tag}]" if tag else "[model]"
    with open(text_path, "r") as f:
        text = f.read()
    all_ids = tokenizer(text, return_tensors=None)["input_ids"]
    n_avail = len(all_ids) if max_tokens is None else min(len(all_ids), max_tokens)
    n_chunks = n_avail // seq_len
    if n_chunks == 0:
        raise ValueError(
            f"not enough tokens ({n_avail}) for even one chunk of "
            f"seq_len={seq_len}; use a longer text or smaller --seq-len"
        )
    used = n_chunks * seq_len
    print(f"{prefix} tokenized {len(all_ids)} tokens; "
          f"using {used} ({n_chunks} chunks of {seq_len}), dropping remainder")
    return all_ids, n_chunks, used


def _apply_quantization(arr_np, quantize_decimals, quantize_levels,
                         vmin=None, vmax=None, nonzero_only=False):
    """
    Apply quantize_decimals or quantize_levels to a 2-D numpy array.
    If nonzero_only=True, zeros are preserved as-is (used for sparse matrices).
    If vmin/vmax are None and quantize_levels is set, the empirical range is used.
    """
    import numpy as _np
    if quantize_decimals is not None:
        if nonzero_only:
            nz = arr_np != 0
            return _np.where(nz, _np.round(arr_np, quantize_decimals), 0.0)
        return _np.round(arr_np, quantize_decimals)
    if quantize_levels is not None:
        lo = arr_np[arr_np != 0].min() if (nonzero_only and (arr_np != 0).any()) \
            else (vmin if vmin is not None else arr_np.min())
        hi = arr_np[arr_np != 0].max() if (nonzero_only and (arr_np != 0).any()) \
            else (vmax if vmax is not None else arr_np.max())
        if hi == lo:
            return arr_np
        step = (hi - lo) / (quantize_levels - 1)
        quantized = lo + _np.round((arr_np - lo) / step) * step
        if nonzero_only:
            return _np.where(arr_np != 0, quantized, 0.0)
        return quantized
    return arr_np


def _write_rows_streaming(mat_np, n_rows_so_far, writer, triplet_f, triplet_delimiter):
    """
    Write rows of mat_np to an open csv writer, and optionally to a triplet file.
    Returns (updated_n_rows, new_triplets_written).
    """
    n_rows = n_rows_so_far
    n_triplets = 0
    for row in mat_np:
        writer.writerow(row.tolist())
        if triplet_f:
            for col, v in enumerate(row):
                if v != 0:
                    triplet_f.write(
                        f"{n_rows}{triplet_delimiter}{col}{triplet_delimiter}{v}\n"
                    )
                    n_triplets += 1
        n_rows += 1
    return n_rows, n_triplets


def _extract_topk_routing_matrix(router_logits, top_k, normalize=True):
    """
    router_logits: (n_tokens, n_experts) raw pre-softmax linear output.
    Returns a (n_tokens, n_experts) tensor that is exactly zero outside
    the top_k experts selected for each token -- this is the real
    per-token gating matrix used inside the model's forward pass.
    """
    import torch
    probs = torch.softmax(router_logits.float(), dim=-1)
    top_vals, top_idx = torch.topk(probs, top_k, dim=-1)
    if normalize:
        top_vals = top_vals / top_vals.sum(dim=-1, keepdim=True)
    n_tokens, n_experts = router_logits.shape
    full = torch.zeros((n_tokens, n_experts), dtype=top_vals.dtype)
    full.scatter_(1, top_idx, top_vals)
    return full


def moe_router_to_csv(text_path, output_path, model=None, tokenizer=None,
                       model_name=DEFAULT_MOE_MODEL, layer=None, seq_len=512,
                       batch_size=4, max_tokens=None, device=None, dtype="auto",
                       num_experts=None, top_k=None, normalize_topk=None,
                       quantize_decimals=None, quantize_levels=None,
                       triplet_output=None, triplet_delimiter=" "):
    """
    Run a sparse Mixture-of-Experts causal LM (default: OLMoE-1B-7B) over
    a local text file and extract one transformer layer's per-token
    expert-routing matrix: rows = tokens in reading order, cols = experts.

    model/tokenizer: pass already-loaded objects to skip the Hub download
        (used by tests, or to reuse a model across multiple calls). If
        omitted, model_name is loaded via transformers' AutoModel/
        AutoTokenizer (needs network access the first time; cached after).

    quantize_decimals / quantize_levels: optional, mutually exclusive,
        applied ONLY to the nonzero entries (zeros from non-selected
        experts are always left untouched). quantize_levels buckets
        nonzeros uniformly over [0, 1] -- the mathematically guaranteed
        range of a (renormalized) softmax weight -- rather than the
        empirical min/max, so no separate (expensive) pass over the model
        is needed to find that range.
        Note: with very coarse quantize_levels (e.g. 4), small nonzero
        weights may snap to 0.0 -- this is intentional and increases
        sparsity further, which benefits grammar compression.

    triplet_output: optional row-major, duplicate-free "row<delim>col
        <delim>value" text file of nonzero entries (see
        _write_sorted_unique_triplets); duplicates can't occur here since
        each row is built from one token's single softmax distribution.
    """
    if quantize_decimals is not None and quantize_levels is not None:
        raise ValueError("quantize_decimals and quantize_levels are mutually exclusive")

    torch, AutoModelForCausalLM, AutoTokenizer = _lazy_import_torch_transformers()
    import numpy as _np

    if device is None:
        device = "cuda" if torch.cuda.is_available() else "cpu"

    if model is None or tokenizer is None:
        torch, model, tokenizer = _load_hf_model(model_name, device, dtype, tag="moe")
    model.eval()

    cfg = model.config
    n_layers = getattr(cfg, "num_hidden_layers")
    if layer is None:
        layer = n_layers // 2
    if not (0 <= layer < n_layers):
        raise ValueError(f"--layer must be in [0, {n_layers - 1}], got {layer}")

    if num_experts is None:
        for attr in ("num_local_experts", "num_experts"):
            if hasattr(cfg, attr):
                num_experts = getattr(cfg, attr)
                break
        if num_experts is None:
            raise ValueError("could not find num_experts in model config; pass --num-experts")
    if top_k is None:
        for attr in ("num_experts_per_tok", "num_experts_per_token"):
            if hasattr(cfg, attr):
                top_k = getattr(cfg, attr)
                break
        if top_k is None:
            raise ValueError("could not find top_k in model config; pass --top-k")
    if normalize_topk is None:
        normalize_topk = bool(getattr(cfg, "norm_topk_prob", True))

    print(f"[moe] layer={layer}/{n_layers - 1}, num_experts={num_experts}, "
          f"top_k={top_k}, normalize_topk={normalize_topk}")

    all_ids, n_chunks, used_tokens = _tokenize_and_chunk(
        text_path, tokenizer, seq_len, max_tokens, tag="moe")

    n_rows = 0
    n_triplets = 0
    triplet_f = open(triplet_output, "w", newline="") if triplet_output else None
    progress_every = max(1, n_chunks // 10)

    try:
        with open(output_path, "w", newline="") as out_f:
            writer = csvmod.writer(out_f)
            chunk_idx = 0
            while chunk_idx < n_chunks:
                b = min(batch_size, n_chunks - chunk_idx)
                batch_ids = [
                    all_ids[(chunk_idx + i) * seq_len: (chunk_idx + i + 1) * seq_len]
                    for i in range(b)
                ]
                input_ids = torch.tensor(batch_ids, dtype=torch.long, device=device)

                with torch.no_grad():
                    out = model(input_ids, output_router_logits=True)
                router_logits = out.router_logits[layer].detach().to("cpu")

                mat_np = _extract_topk_routing_matrix(
                    router_logits, top_k, normalize_topk
                ).numpy()
                mat_np = _apply_quantization(mat_np, quantize_decimals, quantize_levels,
                                              vmin=0.0, vmax=1.0, nonzero_only=True)

                n_rows, nt = _write_rows_streaming(
                    mat_np, n_rows, writer, triplet_f, triplet_delimiter)
                n_triplets += nt
                chunk_idx += b
                if (chunk_idx // b) % progress_every == 0:
                    print(f"[moe] processed {n_rows}/{used_tokens} tokens...")
    finally:
        if triplet_f:
            triplet_f.close()

    n_cols = num_experts
    qmsg = ""
    if quantize_decimals is not None:
        qmsg = f" (nonzeros quantized to {quantize_decimals} decimal places)"
    elif quantize_levels is not None:
        qmsg = f" (nonzeros quantized to {quantize_levels} levels over [0,1])"
    print(f"[moe] wrote {n_rows} x {n_cols} routing matrix to {output_path}{qmsg}")
    if triplet_output:
        print(f"[moe] wrote {n_triplets} row-major, duplicate-free "
              f"triplets to {triplet_output}")
    return n_rows, n_cols


# ----------------------------------------------------------------------
# Transformer hidden-state matrix
# ----------------------------------------------------------------------
#
# Extracts ONE layer's token-level hidden states from any HF causal LM:
#   rows = tokens (processed in reading order across the whole corpus)
#   cols = d_model  (hidden dimension, e.g. 768 for GPT-2 small)
#
# Compressibility source: dense-but-low-rank structure.  Transformer
# weight matrices converge to low-rank solutions during training, so
# the tokens' hidden-state vectors live in a low-dimensional subspace
# even though every entry is nonzero.  After quantization, tokens in
# similar syntactic / semantic contexts produce literally identical rows.
#
# First pass over the corpus is required when --quantize-levels is set,
# to find the global min/max before writing.  This doubles forward-pass
# time but avoids loading the full matrix into RAM.


def hidden_states_to_csv(text_path, output_path, model=None, tokenizer=None,
                          model_name=DEFAULT_TRANSFORMER_MODEL, layer=None,
                          seq_len=512, batch_size=4, max_tokens=None,
                          device=None, dtype="auto",
                          quantize_decimals=None, quantize_levels=None,
                          triplet_output=None, triplet_delimiter=" "):
    """
    Run a HF causal LM over a text file and save one layer's hidden-state
    matrix: rows = tokens in reading order, cols = d_model.

    layer: 0-indexed transformer layer (default: middle layer).
        Layer 0 = output of the first transformer block (not the raw
        embedding). Use negative indices to count from the end, e.g.
        layer=-1 = last transformer layer.

    quantize_levels: buckets all values into K uniform levels spanning
        the *global* [min, max] of the chosen layer's activations.
        Requires a full extra forward pass to find that range.
    """
    if quantize_decimals is not None and quantize_levels is not None:
        raise ValueError("quantize_decimals and quantize_levels are mutually exclusive")

    import numpy as _np

    torch = _lazy_import_torch_transformers()[0]

    if model is None or tokenizer is None:
        torch, model, tokenizer = _load_hf_model(
            model_name, device, dtype, tag="hidden")
    else:
        if device is None:
            device = next(model.parameters()).device
        torch = __import__("torch")

    cfg = model.config
    n_layers   = cfg.num_hidden_layers
    d_model    = cfg.hidden_size
    layer      = layer if layer is not None else n_layers // 2
    layer      = layer % n_layers     # handles negatives: -1 → last transformer layer
    layer_idx  = layer + 1            # hidden_states[0]=embed, [1..n_layers]=layers
    print(f"[hidden] layer={layer}/{n_layers - 1}, d_model={d_model}")

    all_ids, n_chunks, used_tokens = _tokenize_and_chunk(
        text_path, tokenizer, seq_len, max_tokens, tag="hidden")

    def _iter_batches():
        """Yield (batch_size×seq_len, d_model) numpy arrays for each micro-batch."""
        chunk_idx = 0
        while chunk_idx < n_chunks:
            b = min(batch_size, n_chunks - chunk_idx)
            ids = [all_ids[(chunk_idx + i) * seq_len:
                           (chunk_idx + i + 1) * seq_len] for i in range(b)]
            input_ids = torch.tensor(ids, dtype=torch.long,
                                     device=next(model.parameters()).device)
            with torch.no_grad():
                out = model(input_ids, output_hidden_states=True)
            # hidden_states[layer_idx]: (batch, seq_len, d_model)
            h = out.hidden_states[layer_idx].detach().cpu()
            yield h.reshape(-1, d_model).to(torch.float32).numpy()
            chunk_idx += b

    # --- optional first pass to find global [vmin, vmax] for quantize_levels ---
    vmin = vmax = None
    if quantize_levels is not None:
        print(f"[hidden] first pass: finding global min/max for quantize_levels={quantize_levels} ...")
        vmin, vmax = float("inf"), float("-inf")
        for arr in _iter_batches():
            vmin = min(vmin, float(arr.min()))
            vmax = max(vmax, float(arr.max()))
        print(f"[hidden] global range: [{vmin:.4f}, {vmax:.4f}]")

    # --- second (or only) pass: write CSV + optional triplet file ---
    n_rows = n_triplets = 0
    triplet_f = open(triplet_output, "w") if triplet_output else None
    progress_every = max(1, n_chunks // 10)

    try:
        with open(output_path, "w", newline="") as out_f:
            writer = csvmod.writer(out_f)
            chunk_idx_tracker = [0]

            for arr in _iter_batches():
                arr = _apply_quantization(arr, quantize_decimals, quantize_levels,
                                          vmin=vmin, vmax=vmax)
                n_rows, new_t = _write_rows_streaming(
                    arr, n_rows, writer, triplet_f, triplet_delimiter)
                n_triplets += new_t
                chunk_idx_tracker[0] += batch_size
                if chunk_idx_tracker[0] % (progress_every * batch_size) == 0:
                    print(f"[hidden] processed {n_rows}/{used_tokens} tokens ...")
    finally:
        if triplet_f:
            triplet_f.close()

    n_cols = d_model
    qmsg = ""
    if quantize_decimals is not None:
        qmsg = f" (quantized to {quantize_decimals} decimal places)"
    elif quantize_levels is not None:
        qmsg = f" (quantized to {quantize_levels} levels over [{vmin:.4f},{vmax:.4f}])"
    print(f"[hidden] wrote {n_rows} x {n_cols} hidden-state matrix to {output_path}{qmsg}")
    if triplet_output:
        print(f"[hidden] wrote {n_triplets} row-major, duplicate-free "
              f"triplets to {triplet_output}")
    return n_rows, n_cols


# ----------------------------------------------------------------------
# Transformer attention-weight matrix
# ----------------------------------------------------------------------
#
# Extracts ONE head's attention-weight matrix (post-softmax) from any HF
# causal LM, stacked across all processed sequences:
#   rows = tokens (query positions, across all sequences)
#   cols = seq_len  (key positions, fixed per chunk)
#
# For autoregressive (causal) models (GPT-2, LLaMA, ...) the upper
# triangle is EXACTLY zero by construction (causal mask), giving ~50%
# structural sparsity even before quantization.  For bidirectional models
# (BERT, RoBERTa) the matrix is fully dense.
#
# Compressibility source: head redundancy + induction heads.  Many heads
# across layers produce nearly identical diagonal or vertical patterns.
# After quantization, rows with the same dominant attention pattern become
# literally identical CSRV sequences → single grammar rules in RePair.
#
# IMPORTANT: flash-attention drops the per-token attention weights.  This
# function always sets attn_implementation="eager" in the model config to
# ensure weights are available.  This uses more memory than flash-attn
# but is the only way to obtain the actual post-softmax values.


def attention_weights_to_csv(text_path, output_path, model=None, tokenizer=None,
                              model_name=DEFAULT_TRANSFORMER_MODEL, layer=None,
                              head=0, all_heads=False,
                              seq_len=128, batch_size=4, max_tokens=None,
                              device=None, dtype="auto",
                              quantize_decimals=None, quantize_levels=None,
                              triplet_output=None, triplet_delimiter=" "):
    """
    Run a HF causal LM over a text file and save one layer's attention
    weights as a matrix.

    Single-head mode (default):
        rows = tokens × sequences, cols = seq_len
        Row r corresponds to query token r % seq_len of sequence r // seq_len.

    All-heads mode (--all-heads):
        rows = tokens × sequences × num_heads, cols = seq_len
        All heads are stacked vertically: for each sequence, the rows are
        [head_0_query_0, head_0_query_1, ..., head_H_query_L].

    layer: 0-indexed transformer layer (default: middle layer).
    head:  0-indexed attention head for single-head mode (default: 0).

    quantize_levels: values are in [0, 1] (softmax outputs), so the range
        is mathematically fixed — no first pass is needed.

    Note on seq_len: unlike hidden_states, the number of columns equals
        seq_len.  Choose seq_len to match your downstream usage (e.g.
        seq_len=128 for a 128×128 per-sequence attention matrix).
    """
    if quantize_decimals is not None and quantize_levels is not None:
        raise ValueError("quantize_decimals and quantize_levels are mutually exclusive")

    import numpy as _np

    torch = _lazy_import_torch_transformers()[0]

    if model is None or tokenizer is None:
        # Force eager attention so weights are returned
        from transformers import AutoConfig, AutoModelForCausalLM, AutoTokenizer
        if device is None:
            device = "cuda" if torch.cuda.is_available() else "cpu"
        torch_dtype = (torch.bfloat16 if device == "cuda" else torch.float32) \
            if dtype == "auto" else getattr(torch, dtype)
        print(f"[attn] loading {model_name} on {device} ({torch_dtype}, eager attn) ...")
        acfg = AutoConfig.from_pretrained(model_name,
                                           attn_implementation="eager")
        tokenizer = AutoTokenizer.from_pretrained(model_name)
        model = AutoModelForCausalLM.from_pretrained(
            model_name, config=acfg, torch_dtype=torch_dtype)
        model.to(device)
        model.eval()
    else:
        if device is None:
            device = next(model.parameters()).device
        torch = __import__("torch")

    cfg = model.config
    n_layers  = cfg.num_hidden_layers
    n_heads   = cfg.num_attention_heads
    layer     = layer if layer is not None else n_layers // 2
    layer     = layer % n_layers
    if not all_heads and not (0 <= head < n_heads):
        raise ValueError(f"--head must be in [0, {n_heads - 1}], got {head}")

    mode_str = f"all {n_heads} heads stacked" if all_heads else f"head {head}"
    print(f"[attn] layer={layer}/{n_layers - 1}, {mode_str}, seq_len={seq_len}")

    all_ids, n_chunks, used_tokens = _tokenize_and_chunk(
        text_path, tokenizer, seq_len, max_tokens, tag="attn")

    def _iter_batches():
        """Yield (b*seq_len [*n_heads], seq_len) float32 numpy arrays."""
        chunk_idx = 0
        while chunk_idx < n_chunks:
            b = min(batch_size, n_chunks - chunk_idx)
            ids = [all_ids[(chunk_idx + i) * seq_len:
                           (chunk_idx + i + 1) * seq_len] for i in range(b)]
            input_ids = torch.tensor(ids, dtype=torch.long,
                                     device=next(model.parameters()).device)
            with torch.no_grad():
                out = model(input_ids, output_attentions=True)
            # attentions[layer]: (batch, num_heads, seq_len, seq_len)
            a = out.attentions[layer].detach().cpu().to(torch.float32)
            if all_heads:
                # (batch, n_heads, seq, seq) → (batch*n_heads*seq, seq)
                b_, nh, sq, _ = a.shape
                arr = a.permute(0, 1, 2, 3).reshape(b_ * nh * sq, sq).numpy()
            else:
                # (batch, seq, seq)
                arr = a[:, head, :, :].reshape(-1, seq_len).numpy()
            yield arr
            chunk_idx += b

    # --- no first pass needed for quantize_levels: range is [0, 1] by construction ---
    vmin_attn, vmax_attn = (0.0, 1.0) if quantize_levels else (None, None)

    n_rows = n_triplets = 0
    triplet_f = open(triplet_output, "w") if triplet_output else None
    progress_every = max(1, n_chunks // 10)

    try:
        with open(output_path, "w", newline="") as out_f:
            writer = csvmod.writer(out_f)
            chunk_idx_tracker = [0]

            for arr in _iter_batches():
                arr = _apply_quantization(arr, quantize_decimals, quantize_levels,
                                          vmin=vmin_attn, vmax=vmax_attn)
                n_rows, new_t = _write_rows_streaming(
                    arr, n_rows, writer, triplet_f, triplet_delimiter)
                n_triplets += new_t
                chunk_idx_tracker[0] += batch_size
                if chunk_idx_tracker[0] % (progress_every * batch_size) == 0:
                    print(f"[attn] processed {n_rows} query-rows ...")
    finally:
        if triplet_f:
            triplet_f.close()

    n_cols = seq_len
    mult = n_heads if all_heads else 1
    qmsg = ""
    if quantize_decimals is not None:
        qmsg = f" (quantized to {quantize_decimals} decimal places)"
    elif quantize_levels is not None:
        qmsg = f" (quantized to {quantize_levels} levels over [0,1])"
    print(f"[attn] wrote {n_rows} x {n_cols} attention-weight matrix to {output_path}{qmsg}")
    if all_heads:
        print(f"[attn] rows = {n_chunks * seq_len} tokens × {n_heads} heads "
              f"= {n_rows} (all heads stacked vertically)")
    if triplet_output:
        print(f"[attn] wrote {n_triplets} row-major, duplicate-free "
              f"triplets to {triplet_output}")
    return n_rows, n_cols


# ----------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="dataset", required=True)

    pm = sub.add_parser("movielens", help="Convert MovieLens ratings.csv")
    pm.add_argument("input", help="path to ratings.csv")
    pm.add_argument("output", help="output CSV path")
    pm.add_argument("--format", choices=["dense", "sparse"], default="sparse")
    pm.add_argument("--force", action="store_true",
                     help="allow huge dense matrices")
    pm.add_argument("--triplet-output", default=None,
                     help="also write a row-major, duplicate-free "
                          "'row col value' text file here")
    pm.add_argument("--triplet-delimiter", default=" ",
                     help="delimiter for --triplet-output (default: single space)")
    pm.add_argument("--on-duplicate", choices=["keep_last", "keep_first", "sum", "error"],
                     default="keep_last",
                     help="how to resolve duplicate (row,col) entries in "
                          "--triplet-output (default: keep_last)")

    pk = sub.add_parser("kddcup99", help="Convert KDD Cup 1999 data file")
    pk.add_argument("input", help="path to kddcup.data[.gz] or kddcup.data_10_percent[.gz]")
    pk.add_argument("output", help="output CSV path")
    pk.add_argument("--keep-label", action="store_true",
                     help="label-encode and keep the attack-type label as last column")
    pk.add_argument("--triplet-output", default=None,
                     help="also write a row-major, duplicate-free "
                          "'row col value' text file here")
    pk.add_argument("--triplet-delimiter", default=" ",
                     help="delimiter for --triplet-output (default: single space)")
    pk.add_argument("--on-duplicate", choices=["keep_last", "keep_first", "sum", "error"],
                     default="keep_last",
                     help="how to resolve duplicate (row,col) entries in "
                          "--triplet-output (default: keep_last; KDD Cup 99 "
                          "rows can't actually produce duplicates, kept for "
                          "interface consistency)")

    pg = sub.add_parser("glove", help="Convert a GloVe word-vector .txt file")
    pg.add_argument("input", help="path to glove.*.txt[.gz] file")
    pg.add_argument("output", help="output CSV path (numeric matrix only)")
    pg.add_argument("--quantize-decimals", type=int, default=None,
                     help="round every value to N decimal places")
    pg.add_argument("--quantize-levels", type=int, default=None,
                     help="uniformly quantize every value into K levels "
                          "spanning the global min/max (mutually exclusive "
                          "with --quantize-decimals)")
    pg.add_argument("--vocab-output", default=None,
                     help="optional path to write a (row_index,word) CSV mapping")
    pg.add_argument("--triplet-output", default=None,
                     help="also write a row-major, duplicate-free "
                          "'row col value' text file here (note: GloVe "
                          "vectors are dense, so unless you quantize this "
                          "will list almost every cell)")
    pg.add_argument("--triplet-delimiter", default=" ",
                     help="delimiter for --triplet-output (default: single space)")

    po = sub.add_parser("olmoe", help="Extract a Mixture-of-Experts router matrix "
                                       "(default model: OLMoE-1B-7B)")
    po.add_argument("input", help="path to a local .txt file of input text")
    po.add_argument("output", help="output CSV path (tokens x experts matrix)")
    po.add_argument("--model", default=DEFAULT_MOE_MODEL,
                     help=f"HF model id (default: {DEFAULT_MOE_MODEL})")
    po.add_argument("--layer", type=int, default=None,
                     help="which transformer layer's router to extract "
                          "(default: the middle layer)")
    po.add_argument("--seq-len", type=int, default=512,
                     help="tokens per forward-pass chunk (default: 512)")
    po.add_argument("--batch-size", type=int, default=4,
                     help="chunks per forward pass (default: 4)")
    po.add_argument("--max-tokens", type=int, default=None,
                     help="cap total tokens processed (default: use whole file)")
    po.add_argument("--device", default=None, choices=["cpu", "cuda"],
                     help="default: cuda if available, else cpu")
    po.add_argument("--dtype", default="auto",
                     choices=["auto", "float32", "float16", "bfloat16"])
    po.add_argument("--num-experts", type=int, default=None,
                     help="override auto-detected expert count")
    po.add_argument("--top-k", type=int, default=None,
                     help="override auto-detected top-k routing value")
    po.add_argument("--normalize-topk", default="auto",
                     choices=["auto", "true", "false"],
                     help="override auto-detected norm_topk_prob config")
    po.add_argument("--quantize-decimals", type=int, default=None,
                     help="round nonzero routing weights to N decimal places")
    po.add_argument("--quantize-levels", type=int, default=None,
                     help="uniformly quantize nonzero routing weights into "
                          "K levels over [0,1] (mutually exclusive with "
                          "--quantize-decimals)")
    po.add_argument("--triplet-output", default=None,
                     help="also write a row-major, duplicate-free "
                          "'row col value' text file here")
    po.add_argument("--triplet-delimiter", default=" ",
                     help="delimiter for --triplet-output (default: single space)")

    # ---- shared args factory for hidden_states / attention_weights ----
    def _add_transformer_common(p_, tag):
        p_.add_argument("input",  help="path to a local .txt file of input text")
        p_.add_argument("output", help="output CSV path")
        p_.add_argument("--model", default=DEFAULT_TRANSFORMER_MODEL,
                         help=f"HF model id (default: {DEFAULT_TRANSFORMER_MODEL})")
        p_.add_argument("--layer", type=int, default=None,
                         help="0-indexed transformer layer (default: middle layer; "
                              "negative counts from end)")
        p_.add_argument("--seq-len",    type=int, default=512 if tag == "hidden" else 128,
                         help="tokens per forward-pass chunk "
                              "(default: 512 for hidden, 128 for attn)")
        p_.add_argument("--batch-size", type=int, default=4,
                         help="chunks per forward pass (default: 4)")
        p_.add_argument("--max-tokens", type=int, default=None,
                         help="cap total tokens processed (default: whole file)")
        p_.add_argument("--device", default=None, choices=["cpu", "cuda"])
        p_.add_argument("--dtype",  default="auto",
                         choices=["auto", "float32", "float16", "bfloat16"])
        p_.add_argument("--quantize-decimals", type=int, default=None,
                         help="round every value to N decimal places")
        p_.add_argument("--quantize-levels",   type=int, default=None,
                         help="uniformly quantize into K levels "
                              "(mutually exclusive with --quantize-decimals)")
        p_.add_argument("--triplet-output",    default=None,
                         help="also write a row-major 'row col value' text file")
        p_.add_argument("--triplet-delimiter", default=" ",
                         help="delimiter for --triplet-output (default: space)")

    ph = sub.add_parser("hidden_states",
                         help="Extract transformer hidden-state matrix "
                              "(rows=tokens, cols=d_model; default model: gpt2)")
    _add_transformer_common(ph, "hidden")

    pa = sub.add_parser("attention_weights",
                         help="Extract transformer attention-weight matrix "
                              "(rows=query_tokens, cols=seq_len; default model: gpt2)")
    _add_transformer_common(pa, "attn")
    pa.add_argument("--head", type=int, default=0,
                     help="which attention head to extract (default: 0)")
    pa.add_argument("--all-heads", action="store_true",
                     help="stack all heads vertically instead of picking one "
                          "(rows = tokens × num_heads, cols = seq_len)")

    args = p.parse_args()

    if args.dataset == "movielens":
        shape = movielens_to_csv(args.input, args.output, fmt=args.format, force=args.force,
                                  triplet_output=args.triplet_output,
                                  triplet_delimiter=args.triplet_delimiter,
                                  on_duplicate=args.on_duplicate)
    elif args.dataset == "kddcup99":
        shape = kddcup99_to_csv(args.input, args.output, keep_label=args.keep_label,
                                 triplet_output=args.triplet_output,
                                 triplet_delimiter=args.triplet_delimiter,
                                 on_duplicate=args.on_duplicate)
    elif args.dataset == "glove":
        if args.quantize_decimals is not None and args.quantize_levels is not None:
            p.error("--quantize-decimals and --quantize-levels are mutually exclusive")
        shape = glove_to_csv(args.input, args.output,
                              quantize_decimals=args.quantize_decimals,
                              quantize_levels=args.quantize_levels,
                              vocab_output=args.vocab_output,
                              triplet_output=args.triplet_output,
                              triplet_delimiter=args.triplet_delimiter)
    elif args.dataset == "olmoe":
        if args.quantize_decimals is not None and args.quantize_levels is not None:
            p.error("--quantize-decimals and --quantize-levels are mutually exclusive")
        normalize_topk = {"auto": None, "true": True, "false": False}[args.normalize_topk]
        shape = moe_router_to_csv(
            args.input, args.output, model_name=args.model, layer=args.layer,
            seq_len=args.seq_len, batch_size=args.batch_size, max_tokens=args.max_tokens,
            device=args.device, dtype=args.dtype, num_experts=args.num_experts,
            top_k=args.top_k, normalize_topk=normalize_topk,
            quantize_decimals=args.quantize_decimals, quantize_levels=args.quantize_levels,
            triplet_output=args.triplet_output, triplet_delimiter=args.triplet_delimiter)
    elif args.dataset == "hidden_states":
        if args.quantize_decimals is not None and args.quantize_levels is not None:
            p.error("--quantize-decimals and --quantize-levels are mutually exclusive")
        shape = hidden_states_to_csv(
            args.input, args.output, model_name=args.model, layer=args.layer,
            seq_len=args.seq_len, batch_size=args.batch_size, max_tokens=args.max_tokens,
            device=args.device, dtype=args.dtype,
            quantize_decimals=args.quantize_decimals, quantize_levels=args.quantize_levels,
            triplet_output=args.triplet_output, triplet_delimiter=args.triplet_delimiter)
    elif args.dataset == "attention_weights":
        if args.quantize_decimals is not None and args.quantize_levels is not None:
            p.error("--quantize-decimals and --quantize-levels are mutually exclusive")
        shape = attention_weights_to_csv(
            args.input, args.output, model_name=args.model, layer=args.layer,
            head=args.head, all_heads=args.all_heads,
            seq_len=args.seq_len, batch_size=args.batch_size, max_tokens=args.max_tokens,
            device=args.device, dtype=args.dtype,
            quantize_decimals=args.quantize_decimals, quantize_levels=args.quantize_levels,
            triplet_output=args.triplet_output, triplet_delimiter=args.triplet_delimiter)
    else:
        shape = None

    if shape is not None:
        rows, cols = shape
        print(f"\n=== Final CSV matrix shape: {rows} rows x {cols} columns ===")


if __name__ == "__main__":
    main()
