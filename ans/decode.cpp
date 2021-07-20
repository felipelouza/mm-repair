// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <iostream>
#include <vector>

#include "cutil.hpp"
#include "methods.hpp"
#include "util.hpp"



template <uint32_t fidelity> struct ANSf_decoder {
    static const uint32_t fold_fidelity = fidelity;

    ans_fold_decode<fidelity> ans_frame;
    const uint8_t *in_u8;    
    size_t in_u8_size;
    std::array<uint64_t, 4> states;
    size_t decoded, decodedMax; 

    void init(const uint8_t* cSrc, size_t cSrcSize,size_t originalSize) {
      in_u8 = cSrc;
      in_u8_size = cSrcSize;
      auto ans_frame = ans_fold_decode<fidelity>::load(in_u8);
      in_u8 += cSrcSize; // go to the end and proceed backward
      states[3] = ans_frame.init_state(in_u8);
      states[2] = ans_frame.init_state(in_u8);
      states[1] = ans_frame.init_state(in_u8);
      states[0] = ans_frame.init_state(in_u8);
      decoded = 0;   
      decodedMax = originalSize;
    }
    
    int decode(uint32_t* dst, size_t to_decode) {
      if(decoded+to_decode>decodedMax)
        return 1;  // illegal
        
    } 
};






template <uint32_t fidelity>
void run_ans(std::string input_name)
{

    // read compressed data file 
    std::vector<uint8_t> input_u8;    
    in_u8 = read_file_u8(input_name);
    size_t cSrcSize = input_u8.size()-sizeof(uint64_t);
    // retrieve decompressed file size
    uint64_t original_size = *((uint64_t *) (input_u8.data() + cSrcSize));
    cerr << "Original file size: " << original_size;
    
    // init decompression (from ans_fold_decompress)
    auto ans_frame = ans_fold_decode<fidelity>::load(in_u8);
    in_u8 += cSrcSize; // go to the end and proceed backward

    std::array<uint64_t, 4> states;
    states[3] = ans_frame.init_state(in_u8);
    states[2] = ans_frame.init_state(in_u8);
    states[1] = ans_frame.init_state(in_u8);
    states[0] = ans_frame.init_state(in_u8);
}

int main(int argc, char const* argv[])
{
    if(argc!=3 && argc!=2) {
      std::cerr << "Usage:\n\t"<< argv[0] << " infile bytes [fidelity]\n";
      exit(1);
    }
    int fidelity = 1; 
    if(argc==3) fidelity = atoi(argv[2]);
    if(fidelity<1 || fidelity>5) {
      std::cerr << "fidelity must be between 1 and 8\n";
      exit(1);
    }
    
    switch(fidelity) {
      case 1:  run_ansf<1>(argv[1]); break;
      case 2:  run_ansf<2>(argv[1]); break;
      case 3:  run_ansf<3>(argv[1]); break;
      case 4:  run_ansf<4>(argv[1]); break;
      case 5:  run_ansf<5>(argv[1]); break;
      default: std::cerr<<"Invalid parameter: " << fidelity << std::endl; exit(3);
    }
    return EXIT_SUCCESS;
}
