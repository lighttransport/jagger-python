# Benchmark jagger-python

## Dataset 

Wiki40b

## Requirements

* Python
* Conda

## Install

```
$ python -m pip install -r requirements.txt
```

## Prepare data

We use huggingface datasets to download wiki40b.

Run `prepare_dataset.py`


## Benchmark in Jagger

Download and extract dictionary. https://github.com/lighttransport/jagger-python/releases/download/v0.1.0/model_kwdlc.tar.gz

Then,

```
$ python run-jagger.py
```

## Benchmark in Vaporetto

```
$ wget https://github.com/daac-tools/vaporetto-models/releases/download/v0.5.0/bccwj-suw+unidic_pos+pron.tar.xz
$ tar xvf bccwj-suw+unidic_pos+pron.tar.xz
```

```
$ python run-vaporetto.py
```

EoL.
