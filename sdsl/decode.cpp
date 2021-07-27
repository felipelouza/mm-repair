/*
 * Transforms a sequence of 64-bit integers into a bit-compressed integer vector.
 * The first command line parameter argv[1] specifies the file, which contains the sequence
 * of integers.
 * The bit-compressed integer vector is stored in a file called `argv[1].int_vector`.
 */
#include <sdsl/util.hpp>
#include <sdsl/int_vector.hpp>
#include <iostream>
#include <string>

using namespace sdsl;
using namespace std;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " iv_file" << endl;
        return 1;
    }

    string ofile = string(argv[1]);
    int_vector<> v;
    load_from_file(v, ofile);
    cout<<"v.size()="<<v.size()<<endl;
    cout<<"v.width()="<<(int)v.width()<<endl;
    cout<<"v[0]="<<v[0]<<endl;
}
