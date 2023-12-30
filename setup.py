from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

# Should be False in the release
dev_mode = False

jagger_compile_args=[
  '-DJAGGER_DEFAULT_MODEL="/usr/local/lib/jagger/model/kwdlc"',
  '-DNUM_POS_FIELD=4',
  ]

if dev_mode:
  jagger_compile_args.append('-O0')
  jagger_compile_args.append('-g')
  jagger_compile_args.append('-fsanitize=address')

ext_modules = [
    Pybind11Extension("jagger_ext", ["jagger/python-binding-jagger.cc"],
      include_dirs=['.'],
      extra_compile_args=jagger_compile_args,
    ),
]

setup(
    name="jagger-python",
    packages=['jagger'],
    ext_modules=ext_modules,
    # NOTE: entry_points are set in pyproject.toml
    #entry_points={
    #    'console_scripts': [
    #        "jagger=jagger.main:main"
    #    ]
    #},
    license_files= ('LICENSE', 'jagger.BSD', 'jagger.GPL', 'jagger.LGPL'),
    install_requires=[])
