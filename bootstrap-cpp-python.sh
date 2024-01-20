curdir=`pwd`

builddir=${curdir}/build_python_module

rm -rf ${builddir}
mkdir ${builddir}

# set path to pybind11
# If you install pybind11 through pip, its usually installed to <site-package path>/pybind11.
pybind11_path=`python -c "import site; print (site.getsitepackages()[0])"`

CC=clang CXX=clang++ pybind11_DIR=${pybind11_path}/pybind11 cmake -B${builddir} -S. \
  -DJAGGER_WITH_PYTHON=1 \
  -DCMAKE_VERBOSE_MAKEFILE=1
