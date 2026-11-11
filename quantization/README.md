# Matrix Dataset Quantizer

`matrix_dataset_quantizer.py` quantizes numeric CSV matrices and converts several datasets into matrix formats for compression experiments.

## Main use: quantize an existing CSV matrix

```bash
python3 matrix_dataset_quantizer.py csv INPUT.csv OUTPUT.csv --quantize-levels 1024
```

- `INPUT.csv` is the **input numeric matrix** (headerless, comma-separated by default).
- `OUTPUT.csv` is the quantized matrix.
- `--quantize-levels 1024` maps values to 1,024 uniformly spaced levels over the matrix's global minimum and maximum.

The input must contain only numeric values, with the same number of columns in every row. For a different delimiter, use `--delimiter`. Level quantization makes two passes over the input file to determine the global range and then write the output.

To round values instead of using uniform levels:

```bash
python3 matrix_dataset_quantizer.py csv INPUT.csv OUTPUT.csv --quantize-decimals 2
```

Use either `--quantize-levels` or `--quantize-decimals`, not both.

## GloVe input (additional mode)

The `glove` command accepts the original whitespace-separated GloVe text format, with one word followed by its vector values on each line. It does **not** accept an already numeric CSV matrix.

```bash
python3 matrix_dataset_quantizer.py glove glove.6B.300d.txt glove.q1k.csv \
    --quantize-levels 1024
```

To save the vocabulary-to-row mapping:

```bash
python3 matrix_dataset_quantizer.py glove glove.6B.300d.txt glove.csv \
    --vocab-output vocab.csv
```

## Other supported modes

The script also includes `movielens`, `kddcup99`, `olmoe`, `hidden_states`, and `attention_weights` commands.

See the available options with:

```bash
python3 matrix_dataset_quantizer.py --help
python3 matrix_dataset_quantizer.py csv --help
```
