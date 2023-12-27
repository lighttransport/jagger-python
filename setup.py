from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

ext_modules = [
    Pybind11Extension("jagger", ["python-binding.cc", "jagger.cc"])
]

setup(
    name="jagger-python",
    ext_modules=ext_modules,
    install_requires=[])
