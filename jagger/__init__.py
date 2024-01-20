from jagger_ext import *

# load setptools_scm generated _version.py
try:
    from ._version import version, __version__
    from ._version import version_tuple
except:
    __version__ = version = '0.0.0.dev'
    __version_tuple__ = version_tuple = (0, 0, 0, 'dev', 'git')

import os
import sys
from pathlib import Path

class Jagger:
    def __init__(self):

        self._tagger = JaggerExt()

    def load_model(self, dict_path: Path):
        self._tagger.load_model(str(dict_path))

    def tokenize(self, s: str):
        return self._tagger.tokenize(s)

    def tokenize_batch(self, s: str):
        if isinstance(s, list):
            s = '\n'.join(s)
            # strip redundant '\n'(if input is a list of text which endswith '\n'
            s.replace('\n\n', '\n')

        return self._tagger.tokenize_batch(s)

    def set_threads(self, n: int):
        return self._tagger.set_threads(n)


