curdir=`pwd`

builddir=${curdir}/build

rm -rf ${builddir}
mkdir ${builddir}

cmake -B${builddir} -S. \
  -DSANITIZE_ADDRESS=0 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_VERBOSE_MAKEFILE=1
