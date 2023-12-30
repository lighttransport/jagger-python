from jagger_ext import *

import os
import sys
from pathlib import Path

#
# Assume default model file is placed at the location of this file(jagger/__init__.py)
#
default_model_path = os.path.join(os.path.dirname(__file__), "kwdlc/pattern")

class Jagger:
    def __init__(self, dict_path: Path = default_model_path):
        self.model_da = dict_path + ".da"
        self.model_c2i = dict_path + ".c2i"
        self.model_p2f = dict_path + ".p2f"
        self.model_fs = dict_path + ".fs"
        print(self.model_da)

        self._tagger = JaggerExt()

    def load_model(self, dict_path: Path):
        self._tagger.load_model(str(dict_path))

    def tokenize(self, s: str):
        return self._tagger.tokenize(s)

    def tokenize_batch(self, s: str):
        return self._tagger.tokenize_batch(s)


