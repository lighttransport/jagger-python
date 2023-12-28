# train model

## Requirements

Unixish system.



## Train with KWDLC

```
$ gawk '{ printf "%s", ($1 == "EOS") ? "\n" : $1 }' model/kwdlc/train.JAG > model/kwdlc/train
$ gawk '{ printf "%s", ($1 == "EOS") ? "\n" : $1 }' model/kwdlc/dev.JAG > model/kwdlc/dev
$ gawk '{ printf "%s", ($1 == "EOS") ? "\n" : $1 }' model/kwdlc/test.JAG > model/kwdlc/test

# find /home/syoyo/work/japanese-llama-experiment/jagger/jagger-2023-02-18/mecab-jumandic-7.0-20130310 -name "*.csv" | sort | xargs cat > model/kwdlc/dict
# ./build/train_jagger -d model/kwdlc/dict model/kwdlc/train.JAG > model/kwdlc/patterns

```

## Train with Vaporetto

```
$ python -m pip install vaporetto
$ python -m pip install zstandard

# Download precompiled model
$ wget https://github.com/daac-tools/vaporetto-models/releases/download/v0.5.0/bccwj-suw+unidic_pos+pron.tar.xz
$ tar xvf bccwj-sun+unidic_pos+pron.tar.xz
```

T.B.W.


## Train with CharShu

T.B.W.
