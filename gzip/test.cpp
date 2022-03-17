#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/stream.hpp>

namespace bio = boost::iostreams;
using buffer_t = bio::filtering_streambuf<bio::input>;
using byte_t = unsigned char;

std::vector<byte_t> &fetch_data(std::string &infilepath, std::vector<byte_t> &bytes)
{
    char byte;
    bytes.clear();

    std::ifstream input_file(infilepath);
    if (!input_file.is_open()) {
        std::cerr << "Could not open the file - '"
             << infilepath << "'" << std::endl;
        exit(EXIT_FAILURE);
    }

    while (input_file.get(byte)) {
        bytes.push_back(byte);
    }
    bytes.reserve(bytes.size());
    input_file.close();

    return bytes;
}

buffer_t &build_inbuf(bio::array_source &arrs, buffer_t &inbuf) {
    inbuf.push(boost::iostreams::gzip_decompressor());
    inbuf.push(arrs);
    return inbuf;
}


template <class T>
struct gzip_reader_value {
    T val;
    size_t r, c;
};

template<class T>
class gzip_reader {
    size_t r_curr, r_fst, r_lst, c, cols;

    std::vector<byte_t> bytes;
    bio::array_source arrs;
    std::string infilepath;
    //std::ifstream file;
    buffer_t inbuf;
    std::istream *istream;

public :

    gzip_reader()
            : r_curr(-1), r_fst(-1), r_lst(-1), c(-1), cols(-1)
    {}

    gzip_reader(std::string &infilepath, const size_t fst_row, const size_t lst_row, const size_t cols)
            : 
            infilepath(infilepath),
            r_curr(fst_row),
            r_fst(fst_row),
            r_lst(lst_row),
            c(0),
            cols(cols), 
            bytes(fetch_data(infilepath, bytes)), 
            arrs{reinterpret_cast<char const*>(&bytes[0]), bytes.size()}
    {
        istream = new std::istream(&inbuf);
        //this->arrs = bio::array_source;
        //std::cout << infilepath << std::endl;
        //assert(file.is_open());

        //this->istream  = new std::istream(&inbuf);

    }

    //gzip_reader() = delete;

    gzip_reader(const gzip_reader &) = delete;

    gzip_reader(gzip_reader &&) = default;

    ~gzip_reader() {
        //if(rows != -1 and cols!= -1) delete this->istream;
        //if(rows != -1 and cols!= -1) delete this->istream;
        inbuf.auto_close();
        this->file.close();
    }

    void reset() {
        
        this->r_curr = this->r_fst;
        this->inbuf.reset();
        this->arrs = bio::array_source{reinterpret_cast<char const*>(&bytes[0]), bytes.size()};
        build_inbuf(arrs, inbuf);
        delete this->istream;
        this->istream = new std::istream(&inbuf);

        //istream(&inbuf);
        
    }

    bool has_value() {return this->r_curr < this->r_lst;}

    gzip_reader_value<T> next() {
        assert(this->r_curr < this->r_lst);
        constexpr size_t byte2read = sizeof(T);
        uint32_t v = 0;
        uint8_t b0 = this->istream->get();
        uint8_t b1 = this->istream->get();
        uint8_t b2 = this->istream->get();
        uint8_t b3 = this->istream->get();
        uint8_t b4 = this->istream->get();
        uint8_t b5 = this->istream->get();
        uint8_t b6 = this->istream->get();
        uint8_t b7 = this->istream->get();
        uint8_t b[8] = {b0,b1,b2,b3,b4,b5,b6,b7};
        T val;
        memcpy(&val, &b, sizeof(val));
        gzip_reader_value<T> grv = {val, this->r_curr, this->c };
        ++ this->c;
        if (this->c == this->cols) {
            ++ this-> r_curr;
            this->c = 0;
        }
        return grv;
    }
};

int main(int argc, char** argv) {
    if (argc != 5 + 1) {
        std::cerr << "Usage: " << argv[0] << " <matrix base path> <y path> <rows> <cols> <par. degree>" << std::endl;
        exit(1);
    }
    //args
    std::string matrix_path(argv[1]), y_path(argv[2]);
    const size_t rows = atoi(argv[3]), cols = atoi(argv[4]), blocks = atoi(argv[5]);

    //params
    using T = double;
    const size_t ITERATIONS = 500;
    const size_t row_block_size = (rows + blocks - 1) / blocks;
    const size_t col_block_size = (cols + blocks - 1) / blocks;

    //initialisation
    std::vector<T> cs(cols), rs(rows, 0x0);
    {//cs
        std::ifstream y_file(y_path, std::ios_base::in | std::ios_base::binary);
        assert(y_file.is_open());
        buffer_t inbuf;
        inbuf.push(boost::iostreams::gzip_decompressor());
        inbuf.push(y_file);
        std::istream istream(&inbuf);
        for (size_t col = 0; col < cols; ++col) {
            uint8_t b0 = istream.get();
            uint8_t b1 = istream.get();
            uint8_t b2 = istream.get();
            uint8_t b3 = istream.get();
            uint8_t b4 = istream.get();
            uint8_t b5 = istream.get();
            uint8_t b6 = istream.get();
            uint8_t b7 = istream.get();
            uint8_t b[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
            T val;
            memcpy(&val, &b, sizeof(val));
            cs[col] = val;
        }
        y_file.close();
        inbuf.auto_close();
    }
    std::vector<std::vector<T>> css; //map-reduce aux vects
    css.reserve(blocks);
    for(size_t block=0; block<blocks; ++block ){
        css.emplace_back(cols, 0x0);
    }
    std::vector<T> local_reduce_maxs(blocks);
    std::vector<std::thread> threads;
    threads.resize(blocks);
    std::vector<std::string> gzip_paths;
    gzip_paths.reserve(blocks);
    for(size_t block=0; block<blocks; ++block ){
        gzip_paths.emplace_back();
        gzip_paths.back() += matrix_path;
        gzip_paths.back() += ".";
        gzip_paths.back() += std::to_string(block);
        gzip_paths.back() += ".";
        gzip_paths.back() += std::to_string(blocks);
        gzip_paths.back() += ".gz";
    }
    for(std::string &s : gzip_paths ) std::cout << s << std::endl;
    T lambda = 1;

    //business-logic functions
    auto normalise = [](std::vector<T> &cs, T lambda, size_t fst_col, size_t lst_col) {
        for(size_t c=fst_col; c<lst_col; ++c ){
            cs[c] /= lambda;
        }
    };
    auto right_mult = [](gzip_reader<T> *gr, std::vector<T> &cs, std::vector<T> &rs, size_t row_block_size, size_t bid) {
        const size_t row_bgn = bid * row_block_size;
        const size_t row_end = std::min<size_t>((bid + 1) * row_block_size, rs.size());
        for(size_t row=row_bgn; row<row_end; ++row){
            rs[row] = 0x0;
        }
        while(gr->has_value()){
            auto e = gr->next();
            assert(e.r < rs.size());
            assert(e.c < cs.size());
            rs[e.r] += e.val * cs[e.c];
        }
    };
    auto left_mult = [](gzip_reader<T> *gr, std::vector<T> &cs, std::vector<T> &rs) {
        while(gr->has_value()){
            auto e = gr->next();
            assert(e.r < rs.size());
            assert(e.c < cs.size());
            cs[e.c] += e.val * rs[e.r];
        }
    };
    auto local_reduce = [](std::vector<T> &cs, std::vector<std::vector<T>> &css, size_t fst_col, size_t lst_col, size_t blocks, T &out){
        out = std::numeric_limits<T>::min();
        for(size_t c=fst_col; c<lst_col; ++c) {
            cs[c] = 0;
            for(size_t block=0; block<blocks; ++block) {
                cs[c] += css[block][c];
                css[block][c] = 0;
            }
            if (cs[c] > out) out = cs[c];
        }
    };

    //synchro
    auto join_all = [](std::vector<std::thread> &ts) {for(auto &t : ts) t.join();};
    auto reset_all = [](std::vector<gzip_reader<T>*> &grs){
        for(auto &gr : grs) gr->reset();
    };

    std::vector<gzip_reader<T>*> grs;
    grs.reserve(blocks);
    for(size_t block=0; block<blocks; ++block) {
        const size_t fst_row = block * row_block_size;
        const size_t lst_row = std::min<T>((block + 1) * row_block_size, rows);
        grs.push_back(new gzip_reader<T>(gzip_paths[block], fst_row, lst_row, cols));
    }

    for(size_t i=0; i<ITERATIONS; ++i )
    {
        //normalisation
        for(size_t block=0; block<blocks; ++block )
        {
            const size_t fst_col = block * col_block_size;
            const size_t lst_col = std::min<T>((block+1) * col_block_size, cols);
            threads[block] = std::thread(normalise, std::ref(cs), lambda, fst_col, lst_col);
        }
        join_all(threads);

        //right multiplication
        for(size_t block=0; block<blocks; ++block )
        {
            threads[block] = std::thread(right_mult, grs[block], std::ref(cs), std::ref(rs), row_block_size, block);
        }
        join_all(threads);
        reset_all(grs);


        //left multiplication
        for(size_t block=0; block<blocks; ++block)
        {
            threads[block] = std::thread(left_mult, grs[block], std::ref(css[block]), std::ref(rs));
        }
        join_all(threads);
        reset_all(grs);

#ifdef DEBUG
        for(size_t col=0; col<cols; ++col) {
            std::cout << "css[" << col << "]: [";
            for (auto c: css[col]) {
                std::cout << c << " ";
            }
            std::cout << "]\n";
        }
#endif

        //local reduce
        for(size_t block=0; block<blocks; ++block )
        {
            const size_t fst_col = block * col_block_size;
            const size_t lst_col = std::min<T>((block+1) * col_block_size, cols);
            threads[block] = std::thread(local_reduce, std::ref(cs), std::ref(css), fst_col, lst_col, blocks, std::ref(local_reduce_maxs[block]));
        }
        join_all(threads);

        //global reduce
        lambda = std::numeric_limits<T>::min();
        for(auto lrm : local_reduce_maxs )
            if (lrm>lambda)
                lambda = lrm;
    }

    std::cout << "lambda: " << lambda << std::endl;

#ifdef DEBUG
    std::cout << "rs: [";
    for (auto r : rs) {
        std::cout << r << " ";
    }
    std::cout << "]\n";

    std::cout << "cs: [";
    for (auto c : cs) {
        std::cout << c << " ";
    }
    std::cout << "]\n";
#endif
}