/*  csvmat2csrv
    Convert a matrix written in text csv format (one line per row) into the 
      CSRV format (.vc & .val files) 
    or the 
      DRV format (.dv & vald files)
    In the CSRV format we only store nonzero entries and each entry
    is represented by an id identifying the value in the .val file
    and the column number (hence the .vc extension)
    In the DRV format we store zero and nonzero entries and each entry is
    represented by an id identifying it in the .[if]vald file 
    (hence the .dv extension) 
    In both formats id's are different from zero and the zero value is used 
    to mark the end of a matrix row 
    */
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <complex>
#include <array>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <queue>
#include <sdsl/int_vector.hpp>
#include <sdsl/io.hpp>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/sd_vector.hpp>
#include <sdsl/enc_vector.hpp>

using namespace std;

// bit representing types of input/output values 
// and general options of the algorithm 
#define INT32_OUTPUT 1
#define FLOAT_OUTPUT 2
#define COMPLEX_INPUT 4
#define NO_COL_ID 8

#define SPLIT_A2 1
#define ENCODE_32 1

/* Note: the option to store the values in the .val file as floats or int32s 
 * should be used only when the values can be represented exactly in
 * those data types. If this is not the case warnings are sent to stderr, 
 * since the output will not be correct: input values are always 
 * read as doubles, and they are considered equal only if they are equal 
 * as doubles, so values that become equal after the conversion are not 
 * considered equal in the csrv representation. */ 
 
 

static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] matrix rows cols\n",name);
    fprintf(stderr,"\t\t-b num         number of row blocks, def. 1\n");    
    fprintf(stderr,"\t\t-c             input are complex numbers\n");
    fprintf(stderr,"\t\t-f             save entries as floats (debug only)\n");
    fprintf(stderr,"\t\t-i             save entries as int32s (debug only)\n");
    fprintf(stderr,"\t\t-n             don't store col id (drv format, debug only)\n");
    fprintf(stderr,"\t\t-r opt         reorder elements within each row (independently)\n");
    fprintf(stderr,"\t\t-s opt         split S into A1 and A2 (according to some criteria)\n");
    fprintf(stderr,"\t\t-m             map values in string C to [1..wcodes]\n");
    fprintf(stderr,"\t\t-d             debug prints\n");
    fprintf(stderr,"\t\t-v             verbose\n");
    fprintf(stderr,"The option -c can be combined with either -f or -i,\n");
    fprintf(stderr,"if they are not present each complex entry is represented\n");
    fprintf(stderr,"with two doubles.\n\n");
    exit(1);
}

// write error message and exit
static void quit(const char *s)
{
  if(errno==0) fprintf(stderr,"%s\n",s);
  else  perror(s);
  exit(1);
}

// number of bits to represent longs up to n
int bits (unsigned long n) {
  int b=1;
  while (n>1) { n>>=1; b++; }
  return b;
}


// write a double x as an int32/float/double in binary format  
// if there is loss of information a warning is sent to stderr
void write_bin(double x, int out_type, FILE *f)
{
  int e;
  
  if(out_type & INT32_OUTPUT) {
    int32_t y = (int32_t) lround(x);
    if(y!=x) fprintf(stderr,"Warning: loss of information in int32 conversion (%d vs %lf)\n",y,x);
    e = fwrite(&y,sizeof(y),1,f);
  }
  else if(out_type & FLOAT_OUTPUT) {
    float y = (float) x;
    if(y!=x) fprintf(stderr,"Warning: loss of information in vloat conversion (%f vs %lf)\n",y,x);
    e = fwrite(&y,sizeof(y),1,f);
  }
  else // double
    e = fwrite(&x,sizeof(x),1,f);
    
  if(e!=1) quit("Error writing to binary .val file");
} 


// provide hash function for a complex<double> 
struct ComplexHasher
{
  size_t operator()(const std::complex<double>& k) const
  {
    using std::hash;

    return ((hash<double>()(k.real())
             ^ (hash<double>()(k.imag()) << 1)) >> 1);
  }
};

// reorder elements within the same line
void reorder_line(vector<uint32_t>& row, unordered_map<uint32_t, int>& wcode_freq, int reorder_opt){

  uint32_t diff;
  switch (reorder_opt)
    {
    case 1:
        sort(row.begin(), row.end()-1);
        break;
    case 2:
        sort(row.begin(), row.end()-1, std::greater<uint32_t>());
        break;
    case 3:
        sort(row.begin(), row.end()-1, [&](uint32_t a, uint32_t b){return wcode_freq[a] < wcode_freq[b];});
        break;
    case 4:
        sort(row.begin(), row.end()-1, [&](uint32_t a, uint32_t b){return wcode_freq[a] > wcode_freq[b];});
        break;
    case 5:
        sort(row.begin(), row.end()-1);
        diff=0;
        for(int i=0; i<row.size()-1; i++){
          row[i] -= diff;
          diff += row[i];
        }
        break;
    case 6: //LMS
        vector<uint32_t> tmp(row.begin(), row.end()-1);
        sort(tmp.begin(), tmp.end());
        int n=tmp.size(), m=(n%2)?n/2+1:n/2, j=0;
        for(int i=0; i<n/2; i++){
          row[j++] = tmp[i];
          row[j++] = tmp[m++];
        }
        if(n%2!=0) row[j]=tmp[n/2];
        break;
    }

}

// PathCover: reorder row according the most frequent pairs (taking into account only topk symbols)
void reorder_row(vector<uint32_t>& row, int k, const unordered_set<uint32_t>& is_top, const map<pair<uint32_t,uint32_t>, int>& pair_freq, unordered_map<uint32_t, int> &wcode_freq){

/*
  vector<uint32_t> A, B;

  for(size_t i=0; i<row.size()-1; i++){//-1 removes zero
    auto w = row[i];
    if(is_top.count(w)) A.push_back(w); //topk
    else B.push_back(w); //others
  }

  if(A.size()<=2) return;
*/
  vector<uint32_t> A(row);

  map<pair<uint32_t,uint32_t>, int> pair_wcode;

  for(size_t i=0; i<A.size()-1; i++){ 
    for(size_t j=i+1; j<A.size()-1; j++){
      uint32_t a = A[i], b = A[j];
      if(is_top.count(a) and is_top.count(b)){
        if(a>b) swap(a, b);
        pair_wcode[{a,b}] = pair_freq.at({a,b});
      }
    }
  }

  vector<pair<pair<uint32_t,uint32_t>, int>> edges(pair_wcode.begin(), pair_wcode.end());
  sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {return a.second > b.second; });

  unordered_set<uint32_t> setA(A.begin(), A.end());
  vector<uint32_t> C(A);
  A.clear();

  int count=0;
  // PathCover
  for(const auto& [p, freq] : edges){
    uint32_t a = p.first, b = p.second;
    if(setA.count(a) and setA.count(b)){
      A.push_back(a); A.push_back(b);
      setA.erase(a); setA.erase(b);
    }
    if(count++>k) break;
    /*
       if(setA.count(a)){
       A.push_back(a); setA.erase(a); 
       }
       if(setA.count(b)){
       A.push_back(b); setA.erase(b); 
       }
       */
  }

  //for(auto &v: setA) A.push_back(v);
  for(auto &v: C)
    if(setA.count(v))A.push_back(v);

  size_t t=0;
  for(size_t i=0; i<A.size(); i++) row[t++] = A[i];
  //for(size_t i=0; i<B.size(); i++) row[t++] = B[i];

  return;
}

// compute topk most frequent symbols
vector<pair<uint32_t,int>> get_wcode_topk(unordered_map<uint32_t, int> &wcode_freq, int k){

  typedef pair<uint32_t, int> pui;

  auto cmp = [](const pui& a, const pui& b){return a.second > b.second;};
  priority_queue<pui, vector<pui>, decltype(cmp)> PQ(cmp);

  for (const auto& [wcode, freq] : wcode_freq) {
    PQ.emplace(wcode, freq);
    if(PQ.size() > k) PQ.pop();
  }

  vector<pui> result;
  while(!PQ.empty()){
    result.push_back(PQ.top());
    PQ.pop();
  }

  sort(result.begin(), result.end(), [](auto& a, auto& b) { return a.second > b.second; });

  return result;
}

// compute pair frequencies (only for the topk symbols)
map<pair<uint32_t,uint32_t>, int> get_pair_freq(char* fname, unordered_set<uint32_t> is_top){

  map<pair<uint32_t,uint32_t>, int> pair_freq;
  FILE *fvc = fopen(fname,"rb");
  if(fvc==NULL) quit("Cannot open a .vc/.dv file");

  vector<uint32_t> row;
  uint32_t value;
  while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
    row.push_back(value);
    if(value==0){
      for(size_t i=0; i<row.size()-1; i++){ //-1 removes zero
        for(size_t j=i+1; j<row.size()-1; j++){
          uint32_t a = row[i], b = row[j];
          if(is_top.count(a) and is_top.count(b)){
            if(a>b) swap(a, b);
            pair_freq[{a,b}]++;
          }
        }
      }
      row.clear();
    }
  }
  fclose(fvc);

  return pair_freq;
}

// open the string file and get the alphabet
void get_alphabet(char *fname, set<uint32_t> &SIGMA){

  FILE *fvc = fopen(fname,"r+b");
  if(fvc==NULL) quit("Cannot open a .vc/.dv file");

  uint32_t value;
  while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
    SIGMA.insert(value);
  }
  fclose(fvc);
}

// store the alphabet (using gap-encoding)
unsigned long store_alphabet(char *fname, set<uint32_t> &SIGMA){

#if ENCODE_32
  FILE *fvc = fopen(fname,"wb");
  if(fvc==NULL) quit("Cannot open a .wcode file");
  vector<uint32_t> SIGMA_v(SIGMA.size(), 0);
  unsigned long i=0, diff=0;
  for(auto &s:SIGMA){
    SIGMA_v[i] = s-diff;
    diff+=SIGMA_v[i];
    i++;
  }
  if(fwrite(SIGMA_v.data(), sizeof(uint32_t), SIGMA_v.size(), fvc)!=SIGMA_v.size())
    quit("Error writing to .wcode file");
  fclose(fvc);
#else
  sdsl::int_vector<> SIGMA_iv(SIGMA.size(), 0);
  unsigned long i=0, diff=0;
  for(auto &s:SIGMA){
    SIGMA_iv[i] = s-diff;
    diff+=SIGMA_iv[i];
    i++;
  }
  sdsl::util::bit_compress(SIGMA_iv);
  sdsl::store_to_file(SIGMA_iv, fname);
#endif

  return i; //return maxcode
}

// map alphabet [0..max(wcode)]-> [0..\sigma]
unsigned long map_alphabet(char *fname, char *fname_alpha, int debug){

  set<uint32_t> SIGMA;
  get_alphabet(fname, SIGMA);

  std::cout << "Alphabet size = " << *SIGMA.rbegin() << std::endl;
  std::cout << "(real) Alphabet size = " << SIGMA.size() << std::endl;                   

  //alphabet mapping
  uint32_t r=0;
  map<uint32_t, uint32_t> rank;
  for(auto &c:SIGMA) rank[c]=r++; 

  if(debug==2) for(auto &c:SIGMA) cout<<c<<": "<<rank[c]<<endl; 

  FILE *fvc = fopen(fname,"r+b");
  if(fvc==NULL) quit("Cannot open a .vc/.dv file");

  vector<uint32_t> row;
  uint32_t value;
  //rewrite the input string
  while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
    row.push_back(value);
    if(value==0){
      //map
      for (int i = 0; i < row.size()-1; i++) row[i] = rank[row[i]];
      fseek(fvc, (-1)*(row.size()*sizeof(uint32_t)), SEEK_CUR);
      if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
        quit("Error writing to .vc file");
      row.clear();
    }
  }
  fclose(fvc);

  unsigned long maxcode = store_alphabet(fname_alpha, SIGMA);

  return maxcode;
}

// open the string file to count frequencies of wcodes
void get_wcode_freq(char *fname, unordered_map<uint32_t, int> &wcode_freq){

  FILE *fvc = fopen(fname,"r+b");
  if(fvc==NULL) quit("Cannot open a .vc/.dv file");

  uint32_t value;
  while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
    if(wcode_freq.find(value)==wcode_freq.end()) wcode_freq[value]=1;
    else wcode_freq[value]++;
  }
  fclose(fvc);
}

// main
int main (int argc, char **argv) { 
  extern char *optarg;
  extern int optind, opterr, optopt;
  int verbose=0,vtype=0, reorder=0, reorder_pair=0, map_alpha=0, split=0, debug=0;
  int c,rows,cols,nblocks=1;
  char fname[PATH_MAX];
  time_t start_wc = time(NULL);
  uint32_t wcode;  // int written to the .vc files

  /* ------------- read options from command line ----------- */
  opterr = 0;
  while ((c=getopt(argc, argv, "b:cfir:s:mdnv")) != -1) {
    switch (c) 
    {
      case 'v':
        verbose++; break;
      case 'f':
        vtype |= FLOAT_OUTPUT; break;
      case 'i':
        vtype |= INT32_OUTPUT; break;
      case 'c':
        vtype |= COMPLEX_INPUT; break;
      case 'n':
        vtype |= NO_COL_ID; break;
      case 'r':
        reorder_pair=atoi(optarg); break;
      case 'm':
        map_alpha++; break;
      case 's':
        split=atoi(optarg); break;
      case 'd':
        debug++; break;
      case 'b':
        nblocks=atoi(optarg); break;
      case '?':
        fprintf(stderr,"Unknown option: %c\n", optopt);
        exit(1);
    }
  }
  if(verbose>0) {
    fputs("==== Command line:\n",stderr);
    for(int i=0;i<argc;i++)
      fprintf(stderr," %s",argv[i]);
    fputs("\n",stderr);  
  }
  // check command line
  if(nblocks < 1) {
    fprintf(stderr,"Error! Option -b must be at least one\n");
    usage_and_exit(argv[0]);
  }  
  if( (vtype&INT32_OUTPUT) && (vtype&FLOAT_OUTPUT) ) {
    fprintf(stderr,"Error! Options -f and -i are mutually exclusive\n");
    usage_and_exit(argv[0]);
  }  
  const char *valext = ".val";
  if (vtype&INT32_OUTPUT) valext = ".ival";
  if (vtype&FLOAT_OUTPUT) valext = ".fval";


  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 4) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;

  // ----------- read and check # rows and cols 
  rows  = atoi(argv[2]);
  if(rows<1) quit("Invalid number of rows");
  cols  = atoi(argv[3]);
  if(cols<1) quit("Invalid number of columns");

  if(nblocks>rows) quit("Too many row blocks!");
  // compute size of each block 
  int block_size = (rows+nblocks-1)/nblocks;
  fprintf(stderr,"Block size: %d\n", block_size);

  // open files and init
  FILE *f = fopen(argv[1],"r");
  if(f==NULL) quit("Cannot open infile");
  // open output .[if]val[d] file
  strncpy(fname,argv[1],PATH_MAX-10);
  strncat(fname, valext,6);
  if(vtype & NO_COL_ID) strncat(fname,"d",2);
  FILE *fval = fopen(fname,"w");
  if(fval==NULL) quit("Cannot open valfile");
  // init counters
  int  r = 0;   // number of read rows
  int  wr = 0;  // number of written rows
  unsigned long    nonz = 0;      // total number of nonzeros  
  unsigned long   dnonz = 0;      // distinct nonzeros  
  unsigned long maxcode = 0;      // largest code in a .vc file
                                  // variables for real input values
  std::unordered_map<double,unsigned long> values; // dictionary of distinct nonzero
  double v;   // values read from file
              // same for complex values
  std::unordered_map<std::complex<double>,unsigned long, ComplexHasher> covalues; // dictionary of distinct nonzero
  std::complex<double> cov;
  double re,im;
  // extension of the matrix file .vc or .dv
  const char *mext = (vtype &NO_COL_ID) ? ".dv" : ".vc";
  const char *mext_wcode = ".wcode";

  // main loop reading csv file 
  size_t n=0, n_A1=0, n_A2=0;
  char *buffer=NULL;
  for(int bn=0;bn<nblocks;bn++) {
    if(nblocks==1) snprintf(fname,PATH_MAX,"%s%s",argv[1],mext);
    else snprintf(fname,PATH_MAX,"%s.%d.%d%s",argv[1],nblocks,bn,mext);
    FILE *fvc = fopen(fname,"w");
    if(fvc==NULL) quit("Cannot open a .vc/.dv file");
    while(true) {
      // read csv file
      int e = getline(&buffer,&n,f);
      vector<uint32_t> row;
      if(e<0) {
        if(ferror(f)) quit("Error reading input file");
        assert(bn==nblocks-1);
        break;
      }
      r += 1;
      if (wr>= rows) {
        fprintf(stderr,"Warning: more matrix rows than expected\n");
        break;
      } 
      // convert text numerical values
      char *s = strtok(buffer,",");
      // loop over entries in current row
      for(int c=0;c<cols;c++) {
        // convert s into double and check
        if(s==NULL) {
          fprintf(stderr,"Missing value in row %d column %d (one based)\n",r,c+1);
          quit("Check input file");
        }
        char *tmp;
        // ---- read a value from file either a double or a complex<double>
        if(vtype&COMPLEX_INPUT) { // complex value
          int e = sscanf(s," (%lf%lfj)",&re,&im);
          if(e!=2) {
            fprintf(stderr,"Invalid or missing complex entry in row %d column %d (one based)\n",r,c+1);
            quit("Check input file");
          }
          v= (re==0 and im ==0) ? 0 : 1; // v==0 iff cov==0
          cov.real(re); cov.imag(im);    // init cov          
        }
        else { // plain real value
          v = strtod(s,&tmp);
          if(tmp==s) {
            fprintf(stderr,"Missing value in row %d column %d (one based)\n",r,c+1);
            quit("Check input file");
          }
        }
        // ----- process element if nonzero or NO_COL_ID is set
        if(v!=0 or (vtype&NO_COL_ID)) { // process non zero value
          unsigned long id, code;
          nonz += 1;
          // get id of entry and possibly write new values to .val file
          if(!(vtype&COMPLEX_INPUT)) { // real entry
            if(values.count(v)==0) {
              id = values[v] = dnonz++;
              write_bin(v,vtype,fval);
            } else{
              id = values[v];
            }
          } else { // complex entry 
            if(covalues.count(cov)==0) {
              id = covalues[cov] = dnonz++;
              write_bin(cov.real(),vtype,fval);
              write_bin(cov.imag(),vtype,fval);
            } else id = covalues[cov];
          }
          // generate code and write it to .vc file 
          if (vtype&NO_COL_ID) code = id;
          else code = id*cols + c;
          code += 1;                // shift by 1 to allow code for endrow code 0 
          if (code>= 1ul<<30) 
            quit("Code larger than 2**30. We are in trouble");
          if (code>maxcode) maxcode=code;
          wcode = (uint32_t) code; // convert to 32 bit value

          //work line-by-line
          row.push_back(wcode);
          /*
             if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
             quit("Error writing to .vc file");
             */
        }
        s = strtok(NULL,","); // scan next value
      }
      if(s!=NULL) {
        fprintf(stderr,"Extra value in row %d (one based)\n",r);
        quit("Check input file");
      }

      // end row
      wcode=0;
      /*
         if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
         quit("Error writing to .vc file");
         */
      row.push_back(wcode);
      if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
        quit("Error writing to .vc file");

      wr += 1;
      // if a block is full, stop and start the next 
      if ((wr%block_size) == 0) break; // end while true
    }
    fclose(fvc);

    //open and overwrite the file with reordered elements (inside each line)
    if(reorder){

      //open the file and get wcode_freq
      unordered_map<uint32_t, int> wcode_freq;
      get_wcode_freq(fname, wcode_freq);

      if(debug){
        for(auto& w:wcode_freq) cout<<"<"<<w.first<<">: "<<w.second<<endl;
      }

      fprintf(stderr,"Reordering option: %d\n", reorder);
      FILE *fvc = fopen(fname,"r+b");
      if(fvc==NULL) quit("Cannot open a .vc/.dv file");

      vector<uint32_t> row;
      uint32_t value;
      while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
        row.push_back(value);
        if(value==0){

          reorder_line(row, wcode_freq, reorder);
          fseek(fvc, (-1)*(row.size()*sizeof(uint32_t)), SEEK_CUR);
          if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
            quit("Error writing to .vc file");
          row.clear();
        }
      }
      fclose(fvc);
    }                                                                                             

    //open and overwrite the file with reordered elements (inside each line)
    if(reorder_pair){

      fprintf(stderr,"Reordering option: %d\n", reorder);
      int k = reorder_pair; //top-k

      //open the file and get wcode_freq
      unordered_map<uint32_t, int> wcode_freq;
      get_wcode_freq(fname, wcode_freq);

      //TODO
      vector<pair<uint32_t, int>> topk = get_wcode_topk(wcode_freq, 100+1); //+1 because of the zeros

      if(debug){
        for(auto& w:topk) cout<<"<"<<w.first<<">: "<<w.second<<endl;
      }

      unordered_set<uint32_t> is_top;
      for (const auto& [wcode, freq] : topk) {
        is_top.insert(wcode);
      }

      map<pair<uint32_t,uint32_t>, int> pair_freq = get_pair_freq(fname, is_top);


      if(debug){
        for (const auto& [p, freq] : pair_freq) {
          std::cout << "(" << p.first << ", " << p.second << ") -> "
            << freq << "\n";
        }
      }


      FILE *fvc = fopen(fname,"r+b");
      if(fvc==NULL) quit("Cannot open a .vc/.dv file");

      vector<uint32_t> row;
      uint32_t value;
      while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
        row.push_back(value);
        if(value==0){

          reorder_row(row, k, is_top, pair_freq, wcode_freq);
          fseek(fvc, (-1)*(row.size()*sizeof(uint32_t)), SEEK_CUR);
          if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
            quit("Error writing to .vc file");
          row.clear();
        }
      }
      fclose(fvc);

    }                                                                                             

    if(map_alpha and not split){

      //store the new alphabet
      char fname_alpha[PATH_MAX];
      if(nblocks==1) snprintf(fname_alpha,PATH_MAX,"%s%s",argv[1],mext_wcode);
      else snprintf(fname_alpha,PATH_MAX,"%s.%d.%d%s",argv[1],nblocks,bn,mext_wcode);

      maxcode = map_alphabet(fname, fname_alpha, debug);
    }                                                                                             

    if(debug){
      fvc = fopen(fname,"rb");
      if(fvc==NULL) quit("Cannot open a .vc/.dv file");
      uint32_t value;
      cout<<"A = ";
      while(fread(&value, sizeof(uint32_t), 1, fvc)==1)
        cout<<"<"<<value<<"> "; 
      cout<<endl;
      fclose(fvc);
    }

    if(split){

      //open the file and get wcode_freq
      unordered_map<uint32_t, int> wcode_freq;
      get_wcode_freq(fname, wcode_freq);

      if(debug==2)
        for(auto& w:wcode_freq) cout<<"<"<<w.first<<">: "<<w.second<<endl;

      fvc = fopen(fname,"rb");
      if(fvc==NULL) quit("Cannot open a .vc/.dv file");

      //open_file()
      char fname_A1[PATH_MAX];
      if(nblocks==1) snprintf(fname_A1,PATH_MAX,"%s.%s%s",argv[1],"A1", mext);
      else snprintf(fname_A1,PATH_MAX,"%s.%d.%d.%s%s",argv[1],nblocks,bn, "A1",mext);
      FILE *fvc_A1 = fopen(fname_A1,"w");
      if(fvc_A1==NULL) quit("Cannot open a .vc/.dv file");

      //open_file()
      char fname_A2[PATH_MAX];
      if(nblocks==1) snprintf(fname_A2,PATH_MAX,"%s.%s%s",argv[1],"A2", mext);
      else snprintf(fname_A2,PATH_MAX,"%s.%d.%d.%s%s",argv[1],nblocks,bn,"A2",mext);
      FILE *fvc_A2 = fopen(fname_A2,"w");
      if(fvc_A2==NULL) quit("Cannot open a .vc/.dv file");

      vector<uint32_t> row, ones;
      uint32_t value, wr_modified=0;
      while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
        row.push_back(value);
        if(value==0){

          vector<uint32_t> row_A1, row_A2;
          for(auto it=row.begin(); it != std::prev(row.end()); it++){
            auto r = *it;
            if(wcode_freq[r]>split) row_A1.push_back(r);
            else{
              row_A2.push_back(r);
            }
          }

          n_A1 += row_A1.size();
          n_A2 += row_A2.size();

          row_A1.push_back(*row.rbegin());
          //gap-encode below
          if(row_A2.size()>0){
            wr_modified++;
            sort(row_A2.begin(), row_A2.end());
          }
          row_A2.push_back(*row.rbegin());


          if(fwrite(row_A1.data(), sizeof(uint32_t), row_A1.size(), fvc_A1)!=row_A1.size())
            quit("Error writing to .A1.vc file");
          if(fwrite(row_A2.data(), sizeof(uint32_t), row_A2.size(), fvc_A2)!=row_A2.size())
            quit("Error writing to .A2.vc file");

          row.clear();
        }
      }      
      fclose(fvc_A1);
      fclose(fvc_A2);

      if(map_alpha){
        //new alphabet for A1
        char fname_A1_alpha[PATH_MAX];
        if(nblocks==1) snprintf(fname_A1_alpha,PATH_MAX,"%s.%s%s",argv[1],"A1",mext_wcode);
        else snprintf(fname_A1_alpha,PATH_MAX,"%s.%d.%d.%s%s",argv[1],nblocks,bn,"A1",mext_wcode);
        maxcode = map_alphabet(fname_A1, fname_A1_alpha, debug);

        //new alphabet for A2
        char fname_A2_alpha[PATH_MAX];
        if(nblocks==1) snprintf(fname_A2_alpha,PATH_MAX,"%s.%s%s",argv[1],"A2",mext_wcode);
        else snprintf(fname_A2_alpha,PATH_MAX,"%s.%d.%d.%s%s",argv[1],nblocks,bn,"A2",mext_wcode);
        maxcode = map_alphabet(fname_A2, fname_A2_alpha, debug);
      }

      cout<<"wr = "<<wr<<endl;
      cout<<"wr_modified = "<<wr_modified<<endl;

      //RLE the zeros when modified lines are at least half
      bool rle_zeros = true;
      if(wr_modified > wr/2) rle_zeros = false;

      if(debug){
        //A1
        fvc = fopen(fname_A1,"rb");
        if(fvc==NULL) quit("Cannot open a .vc/.dv file");
        cout<<"A1 = ";
        uint32_t value;
        while(fread(&value, sizeof(uint32_t), 1, fvc)==1) cout<<"<"<<value<<"> "; cout<<endl;
        fclose(fvc);
        //A2
        fvc = fopen(fname_A2,"rb");
        if(fvc==NULL) quit("Cannot open a .vc/.dv file");
        cout<<"A2 = ";
        value;
        while(fread(&value, sizeof(uint32_t), 1, fvc)==1) cout<<"<"<<value<<"> "; cout<<endl;
        fclose(fvc);
      }

      //iv compress string A2
      fvc = fopen(fname_A2,"rb");
      if(fvc==NULL) quit("Cannot open a .vc/.dv file");

      if(not rle_zeros){
        uint32_t diff=0, nzeros=0;
        while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
          if(value){
            row.push_back(value-diff);
            diff+=value-diff;
          }
          else{
            row.push_back(0);
            diff=0;
            nzeros++;
          }
        }
        fclose(fvc);

        /*
           if(debug){
           cout<<"A2 (delta) = ";
           for(auto v:row) cout<<"<"<<v<<"> "; cout<<endl;
           }
           */

#if ENCODE_32
        fvc = fopen(fname_A2,"wb");
        if(fvc==NULL) quit("Cannot open a .vc/.dv file");
        if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
          quit("Error writing to .vc file");
        fclose(fvc);
#else
#if SPLIT_A2 == 0
        sdsl::int_vector<> row_iv(row.size(), 0);
        uint32_t i=0;
        for(auto &v:row) row_iv[i++]=v;
        sdsl::util::bit_compress(row_iv);
        sdsl::store_to_file(row_iv, fname_A2);
#else
        sdsl::int_vector<> row_iv_1(wr_modified, 0);
        sdsl::int_vector<> row_iv_2(row.size()-wr_modified, 0);

        uint32_t i=0, j=0, k=0;
        bool first=true;
        for(auto &v:row){
          if(not first and v!=0){
            row_iv_2[j++]=v;
          }
          else{
            if(v==0){
              row_iv_2[j++]=v;
              first=true;
            }
            else{
              row_iv_1[i++]=v;
              first=false;
            }
          }
        }

        sdsl::util::bit_compress(row_iv_1);
        sdsl::util::bit_compress(row_iv_2);
        std::ofstream out(fname_A2, std::ios::binary);
        row_iv_1.serialize(out);
        row_iv_2.serialize(out);

        out.close();
#endif
#endif
      }
      else{

        int first_zero=0;
        uint32_t value;
        if(fread(&value, sizeof(uint32_t), 1, fvc)!=1)
          quit("Cannot read a .vc file");
        if(value==0) first_zero++;
        fseek(fvc, 0, SEEK_SET);

        uint32_t zeros = 0, diff=0, nzeros=0;
        while(fread(&value, sizeof(uint32_t), 1, fvc)==1){
          if(value){
            if(zeros){//RLE (only) the zeros
              row.push_back(0);
              row.push_back(zeros);
              zeros=0;
              diff=0;
              nzeros++;
            }
            row.push_back(value-diff);
            diff+=value-diff;
          }
          else{
            zeros++;
          }
        }
        fclose(fvc);
        if(zeros){//RLE (only) the zeros
          row.push_back(0);
          row.push_back(zeros);
          nzeros++;
        }

        /*
           if(debug){
           cout<<"A2 (rle) = ";
           for(auto v:row) cout<<"<"<<v<<"> "; cout<<endl;
           }
           */

#if ENCODE_32
        fvc = fopen(fname_A2,"wb");
        if(fvc==NULL) quit("Cannot open a .vc/.dv file");
        if(fwrite(row.data(), sizeof(uint32_t), row.size(), fvc)!=row.size())
          quit("Error writing to .vc file");
        fclose(fvc);
#else
#if SPLIT_A2 == 0 
        sdsl::int_vector<> row_iv(row.size(), 0);
        uint32_t i=0;
        for(auto &v:row) row_iv[i++]=v;
        sdsl::util::bit_compress(row_iv);
        sdsl::store_to_file(row_iv, fname_A2);
#else
        sdsl::int_vector<> row_iv_1(nzeros-first_zero, 0);
        sdsl::int_vector<> row_iv_2(row.size()-(nzeros-first_zero)-nzeros, 0);
        sdsl::int_vector<> row_iv_3(nzeros, 0);
        uint32_t i=0, j=0, k=0;
        bool first=true, rle=false;
        for(auto &v:row){
          if(not first and not rle){
            row_iv_2[j++]=v;
            if(v==0) rle=true;
          }
          else if(first){
            if(v==0){
              row_iv_2[j++]=v;
              rle=true;
            }
            else{
              row_iv_1[i++]=v;
            }
            first=false;
          }
          else if(rle){
            row_iv_3[k++]=v;
            rle=false;
            first=true;
          }
        }

        sdsl::util::bit_compress(row_iv_1);
        sdsl::util::bit_compress(row_iv_2);
        sdsl::util::bit_compress(row_iv_3);

        std::ofstream out(fname_A2, std::ios::binary);

        row_iv_1.serialize(out);
        row_iv_2.serialize(out);
        row_iv_3.serialize(out);

        out.close();
#endif
#endif
      }


      if(debug){

        if(not rle_zeros){
#if ENCODE_32
          fvc = fopen(fname_A2,"rb");
          if(fvc==NULL) quit("Cannot open a .vc/.dv file");
          uint32_t v;
          cout<<"A2 (delta) = "; while(fread(&v, sizeof(uint32_t), 1, fvc)==1) cout<<"<"<<v<<"> "; cout<<endl;
          fclose(fvc);
#else
#if SPLIT_A2 == 0
          sdsl::int_vector<> row_iv;
          sdsl::load_from_file(row_iv, fname_A2);
          cout<<"A2 = "; for(auto v:row_iv) cout<<"<"<<v<<"> "; cout<<endl;
#else
          std::ifstream in(fname_A2, std::ios::binary);
          sdsl::int_vector<> v1, v2;

          v1.load(in);
          v2.load(in);
          cout<<"A2_1 = "; for(auto v:v1) cout<<"<"<<v<<"> "; cout<<endl;
          cout<<"A2_2 = "; for(auto v:v2) cout<<"<"<<v<<"> "; cout<<endl;

          in.close();
#endif
#endif
        }
        else{
#if ENCODE_32
          fvc = fopen(fname_A2,"rb");
          if(fvc==NULL) quit("Cannot open a .vc/.dv file");
          uint32_t v;
          cout<<"A2 (rle) = "; while(fread(&v, sizeof(uint32_t), 1, fvc)==1) cout<<"<"<<v<<"> "; cout<<endl;
          fclose(fvc);
#else
#if SPLIT_A2 == 0
          sdsl::int_vector<> row_iv;
          sdsl::load_from_file(row_iv, fname_A2);
          cout<<"A2 = "; for(auto v:row_iv) cout<<"<"<<v<<"> "; cout<<endl;
#else
          std::ifstream in(fname_A2, std::ios::binary);
          sdsl::int_vector<> v1, v2, v3;

          v1.load(in);
          v2.load(in);
          v3.load(in);
          cout<<"A2_1 = "; for(auto v:v1) cout<<"<"<<v<<"> "; cout<<endl;
          cout<<"A2_2 = "; for(auto v:v2) cout<<"<"<<v<<"> "; cout<<endl;
          cout<<"A2_3 = "; for(auto v:v3) cout<<"<"<<v<<"> "; cout<<endl;
          in.close();
#endif
#endif
        }
      }
    }
  }
  fclose(fval);

  if(debug) cout<<"##"<<endl;

  if(vtype&COMPLEX_INPUT) assert(dnonz==covalues.size());
  else assert(dnonz==values.size());
  if(wr!=rows)
    fprintf(stderr, "Warning! Written %d rows instead of %d\n", wr, rows);
  fprintf(stderr,"Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  fprintf(stderr,"Number of nonzeros: %ld   Nonzero ratio: %.4f\n", nonz, ((double) nonz/(wr*cols)));  
  fprintf(stderr, "%zd distinct nonzeros values\n", dnonz);
  fprintf(stderr,"Largest codeword: %lu   Bits x codeword: %d\n", maxcode, bits(maxcode));
  if(split){
    fprintf(stderr,"A1.size(): %zu\t %.2lf%%\n",n_A1, (double)(n_A1)/(n_A1+n_A2)*100.0);  
    fprintf(stderr,"A2.size(): %zu\t %.2lf%%\n",n_A2, (double)(n_A2)/(n_A1+n_A2)*100.0);  
  }

  fprintf(stderr,"==== Done\n");

  return EXIT_SUCCESS;
}
