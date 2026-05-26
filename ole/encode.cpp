// encode a file of uint32_t using Offset-List Encoding (OLE)
// create a file with extension ole
// the first 8 bytes of the outfile is the number of input uint32_t

#include <cassert>
#include <iostream>
#include <vector>
#ifdef MALLOC_COUNT
#include "../tools/malloc_count.h"
#endif

using namespace std;

#include "util.hpp"

void run(std::vector<uint32_t> input, std::string input_name){

  size_t start = 0;
  for (size_t i = 0; i <= input.size(); ++i) {
    if (i == input.size() || input[i] == 0) { // end of sequence
      sort(input.begin() + start, input.begin() + i);
      // replace values by gaps inside the same sequence
      for (size_t j = i - 1; j > start; --j) {
        input[j] = input[j] - input[j-1];
      }
      start = i + 1;
    }
  }

  /*
  for(auto &v: input) cout<<v<<" ";
  cout<<endl;
  */

  // save file (overwrite)
  std:: string outfile = input_name; // + ".ole";
  auto fd = fopen_or_fail(outfile, "w");
  cout<<outfile<<endl;
  // write input size in sizeof(size_t) bytes  
  size_t isize = input.size(); 
  // write transformed data
  size_t e = fwrite(input.data(),sizeof(uint32_t),isize, fd); 
  if(e!=isize) quit("Error writing compressed data");
  fclose_or_fail(fd);
}

int main(int argc, char const* argv[])
{
  if(argc!=2) {
    std::cerr << "Usage:\n\t"<< argv[0] << " file_of_u32\n";
    exit(1);
  }

  // read file 
  std::vector<uint32_t> input_u32s;
  input_u32s = read_file_u32(argv[1]);
  run(input_u32s, argv[1]); 


#ifdef MALLOC_COUNT
  fprintf(stderr,"Peak memory allocation: %zu bytes, current: %zu bytes\n", 
      malloc_count_peak(),malloc_count_current());
#endif

  return EXIT_SUCCESS;
}
