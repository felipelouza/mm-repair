// merge two files of uint32_t
// create a file with extension merged
// the first 8 bytes of the outfile is the number of input uint32_t

#include <cassert>
#include <iostream>
#include <vector>
#ifdef MALLOC_COUNT
#include "../tools/malloc_count.h"
#endif

using namespace std;

#include "util.hpp"

void run(std::vector<uint32_t> A, std::vector<uint32_t> B, std::string input_name){

  vector<uint32_t> input;

  size_t start = 0;
  size_t i=0, j=0;
  while(i<A.size()){
    if (A[i] == 0) { // end of sequence
      while(B[j]!=0){
        input.push_back(B[j]);
        j++;
      }
      j++;
      input.push_back(0);
    }
    else{
      input.push_back(A[i]);
    }
    i++;
  }

  /* 
  for(auto &v: input) cout<<v<<" ";
     cout<<endl;
  */

  // save file (overwrite)
  std:: string outfile = input_name; // + ".merged";
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
  if(argc!=3) {
    std::cerr << "Usage:\n\t"<< argv[0] << " file_1_of_u32 file_2_of_u32\n";
    exit(1);
  }

  // read file 
  std::vector<uint32_t> input_A, input_B;
  input_A = read_file_u32(argv[1]);
  input_B = read_file_u32(argv[2]);
  
  run(input_A, input_B, argv[1]); 

#ifdef MALLOC_COUNT
  fprintf(stderr,"Peak memory allocation: %zu bytes, current: %zu bytes\n", 
      malloc_count_peak(),malloc_count_current());
#endif

  return EXIT_SUCCESS;
}
