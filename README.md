# jagger-python

<div align="center">
  <img src="jagger.png" width="300"/>
</div>


Python binding for Jagger(C++ implementation of Pattern-based Japanese Morphological Analyzer) : https://www.tkl.iis.u-tokyo.ac.jp/~ynaga/jagger/index.en.html

## Install

```
$ python -m pip install jagger
```

This does not install model files.

You can download precompiled KWDLC model from https://github.com/lighttransport/jagger-python/releases/download/v0.1.0/model_kwdlc.tar.gz
(Note that KWDLC has unclear license/TermOfUse. Use it at your own risk)

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

"""
吾輩    名詞,普通名詞,*,*,吾輩,わがはい,代表表記:我が輩/わがはい カテゴリ:人
は      助詞,副助詞,*,*,は,は,*
猫      名詞,普通名詞,*,*,猫,ねこ,*
である  判定詞,*,判定詞,デアル列基本形,だ,である,*
。      特殊,句点,*,*,。,。,*
名前    名詞,普通名詞,*,*,名前,なまえ,*
は      助詞,副助詞,*,*,は,は,*
まだ    副詞,*,*,*,まだ,まだ,*
ない    形容詞,*,イ形容詞アウオ段,基本形,ない,ない,*
。      特殊,句点,*,*,。,。,*
"""

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

```py
import jagger

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)

text = """
吾輩は猫である。
名前はまだない。
明日の天気は晴れです。
"""

# optional: set C++ threads(CPU cores) to use
# default: Use all CPU cores.
# tokenizer.set_threads(4)

toks_list = tokenizer.tokenize_batch(text)

for toks in toks_list:
    for tok in toks:
        print(tok.surface(), tok.feature())

```

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

### Releasing

* bump version in `setup.py`
* tag it: `git tag vX.Y.`
* push tag: `git push --tags`


## TODO

- [ ] Provide a model file trained from Wikipedia, UniDic, etc(clearer & permissive licencing&TermOfUse).
  - Use GiNZA for morphological analysis.
- [x] Split feature vector(CSV) considering quote char when extracting tags.
  - e.g. 'a,b,"c,d",e' => ["a", "b", "c,d", "e"]

## License

Python binding is available under 2-clause BSD licence.

Jagger and `ccedar_core.h` is licensed under GPLv2/LGPLv2.1/BSD triple licenses.

### Third party licences

* stack_container.h: BSD like license.
* nanocsv.h MIT license.

