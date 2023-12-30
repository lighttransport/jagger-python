# jagger-python

Python binding for Jagger(C++ implementation of Pattern-based Japanese Morphological Analyzer) : https://www.tkl.iis.u-tokyo.ac.jp/~ynaga/jagger/index.en.html

## Install

```
$ python -m pip install jagger
```

This does not install model files.

## Example

```py
import jagger

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)

text = "吾輩は猫である。名前はまだない。"
toks = tokenizer.tokenize(text)

for tok in toks:
    print(tok.surface(), tok.feature())
print("EOL")

# print tags
for tok in toks:
    # print tag(split feature() by comma)
    print(tok.surface())
    for i in range(tok.n_tags()):
        print("  tag[{}] = {}".format(i, tok.tag(i)))
print("EOL")
```

## Batch processing(experimental)

`tokenize_batch` tokenizes multiple lines(delimited by newline('\n', '\r', or '\r\n')) at once.
Splitting lines is done in C++ side.

## Train a model.

Pyhthon interface for training a model is not provided yet.
For a while, you can build C++ trainer cli using CMake(Windows supported).
See `train/` for details.

## Limitation

Single line string must be less than 262,144 bytes(~= 87,000 UTF-8 Japanese chars).

## Jagger version

Jagger version used in this Python binding is

2023-02-18

## For developer

Edit `dev_mode=True` in to enable asan + debug build

Run python script with

```
$ LD_PRELOAD=$(gcc -print-file-name=libasan.so) python FILE.py

or

$ LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so) python FILE.py
```


## TODO

- [ ] Provide a model file trained from Wikipedia

## License

Python binding is available under 2-clause BSD licence.

Jagger and `ccedar_core.h` is licensed under GPLv2/LGPLv2.1/BSD triple licenses.

### Third party licences

* stack_container.h: BSD like license.

