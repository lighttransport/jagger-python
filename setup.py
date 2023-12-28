from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

jagger_compile_args=[
  '-DJAGGER_DEFAULT_MODEL="/usr/local/lib/jagger/model/kwdlc"',
  '-DNUM_POS_FIELD=4',
  ]

ext_modules = [
    Pybind11Extension("jagger", ["python-binding-jagger.cc", "jagger.cc"],
      include_dirs=['.'],
      extra_compile_args=jagger_compile_args,
    ),
]

setup(
    name="jagger-python",
    py_modules=['jagger'],
    ext_modules=ext_modules,
    license_files= ('LICENSE', 'jagger.BSD', 'jagger.GPL', 'jagger.LGPL'),
    install_requires=[])
