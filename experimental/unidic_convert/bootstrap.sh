rm -rf build
mkdir build

CXX=clang++ CC=clang cmake \
  -DSANITIZE_ADDRESS=0 \
  -DCMAKE_VERBOSE_MAKEFILE=1 \
  -B build
