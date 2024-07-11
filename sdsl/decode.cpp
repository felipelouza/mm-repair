/*
 * Function to test the decompression of an int_vector
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

    string ifile = string(argv[1]);
    int_vector<> v;
    load_from_file(v, ifile);
    cout<<"v.size()="<<v.size()<<endl;
    cout<<"v.width()="<<(int)v.width()<<endl;
    cout<<"v[0]="<<v[0]<<endl;
    long sum=0;
    for (int i=0; i<v.size(); i++) {
        sum+=v[i];
    }
    cout<<"sum="<<sum<<endl;
    // TODO: save the decoded array to file
}
