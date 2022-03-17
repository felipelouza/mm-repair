#include <fstream>
#include <iostream>
#include <vector>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

int main(int argc, char** argv) {
    if(argc != 4+1) {
        std::cerr << "Usage: " << argv[0] << " <csv path> <rows> <cols> <blocks>" << std::endl;
        exit(1);
    }

    //args
    std::string infilepath = argv[1];
    const size_t rows = atoi(argv[2]), cols = atoi(argv[3]), blocks = atoi(argv[4]);

    //params
    std::ifstream csv;
    csv.open(infilepath);

    const size_t row_block_size = (rows + blocks - 1) / blocks;

    //Write some test data
    std::string token;
    for(size_t block=0; block<blocks; ++block) {
        //out path
        std::string outfilepath;
        outfilepath += infilepath;
        outfilepath += ".";
        outfilepath += std::to_string(block);
        outfilepath += ".";
        outfilepath += std::to_string(blocks);
        outfilepath += ".gz";
        //prepare out file
        std::ofstream file(outfilepath, std::ios_base::binary | std::ios_base::out );
        assert(file.is_open());
        boost::iostreams::filtering_streambuf<boost::iostreams::output> outbuf;
        outbuf.push(boost::iostreams::gzip_compressor());
        outbuf.push(file);
        //Convert streambuf to ostream
        std::ostream out(&outbuf);

        const size_t rows_bgn = block * row_block_size;
        const size_t row_end = std::min<size_t>((block+1)*row_block_size, rows);

        for (size_t row = rows_bgn; row < row_end; ++row) {
            for (size_t col = 0; col < cols; ++col) {
                getline(csv, token, (col + 1 == cols) ? '\n' : ',');
                double v = stod(token);
                char *bs = reinterpret_cast<char *>(&v);
                for (auto it = bs; it < bs + 8; ++it) {
                    out << std::hex << *it;
                }
            }
        }

        //cleanup
        boost::iostreams::close(outbuf); // Don't forget this!
        file.close();
    }
    //Cleanup
    csv.close();
}