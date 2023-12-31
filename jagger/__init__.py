from jagger_ext import *

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
        return self._tagger.tokenize_batch(s)

    def set_threads(self, n: int):
        return self._tagger.set_threads(n)


