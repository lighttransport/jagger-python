#include <cstdio>
#include <cstdlib>

#include "dictionary.h"

int main(int argc, char **argv) {

  if (argc < 2) {
    std::cout << "Need dict filename\n";
    exit(-1);
  }

  std::string dict_filename = std::string(argv[1]);

  MeCab::Dictionary dict;
  if (!dict.open(dict_filename.c_str())) {
    std::cerr << "Failed to open dictionary: " << dict_filename << "\n";
    exit(-1);
  }

  return EXIT_SUCCESS;
}
