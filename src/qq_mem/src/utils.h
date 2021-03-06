#ifndef UTILS_H
#define UTILS_H

#include <sys/mman.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <locale>


#include "engine_services.h"
#include "types.h"

namespace utils {

typedef std::chrono::time_point<std::chrono::system_clock> time_point;
const std::vector<std::string> explode(const std::string& s, const char& c);
const std::vector<std::string> explode_by_non_letter(const std::string& text);
const std::vector<std::string> explode_strict(const std::string& s, const char& c);
void sleep(int n_secs);
const time_point now();
const double duration(std::chrono::time_point<std::chrono::system_clock> t1, 
    std::chrono::time_point<std::chrono::system_clock> t2);
const std::string fill_zeros(const std::string &s, std::size_t width);

const std::vector<Offsets> parse_offsets(const std::string& s);
const std::string format_double(const double &x, const int &precision);

std::vector<DocIdType> find_top_k(const DocScoreVec &doc_scores, int k);
int count_terms(const std::string field);
typedef std::unordered_map<Term, int> CountMapType;
CountMapType count_tokens(const std::string &token_text);

typedef std::map<std::string, std::string> ResultRow;


void add_term_offset_entry(std::map<Term, OffsetPairs> *result, const Term &buf, const int &i);
std::map<Term, OffsetPairs> extract_offset_pairs(const std::string & token_str);
std::map<Term, OffsetPairs> construct_offset_pairs(const TermList & tokens, const std::vector<Offsets> & token_offsets);

class LineDoc {
 private:
  std::ifstream infile_;
  std::vector<std::string> col_names_;

 public:
  LineDoc(std::string path) {
    infile_.open(path);

    if (infile_.good() == false) {
      throw std::runtime_error("File may not exist");
    }

    std::string line; 
    std::getline(infile_, line);
    std::vector<std::string> items = explode(line, '#');
    // TODO: bad! this may halt the program when nothing is in line
    items = explode_by_non_letter(items.back()); 
    col_names_ = items;
  }

  bool GetRow(std::vector<std::string> &items) {
    std::string line;
    auto &ret = std::getline(infile_, line);

    if (ret) {
      items = explode_strict(line, '\t');
      return true;
    } else {
      return false;
    }
  }
};


inline std::vector<std::string> ReadLines(const std::string &path) {
  std::vector<std::string> lines;

  std::ifstream infile;
  infile.open(path);

  if (infile.good() == false) {
    throw std::runtime_error("File may not exist");
  }

  std::string line;
  while (std::getline(infile, line)) {
    lines.push_back(line);
  }

  return lines;
}

inline void AppendLines(const std::string &path, std::vector<std::string> lines) 
{
  std::ofstream myfile;
  myfile.open (path, std::ios::app);
  for (auto &line : lines) {
    myfile << line << std::endl;
  }
  myfile.close();
}


class ResultTable {
 public:
  std::vector<ResultRow> table_;


  void Append(const ResultRow &row) {
    table_.push_back(row);
  }

  std::string ToStr() {
    if (table_.size() == 0) {
      return "";
    }

    int width = 12;
    std::ostringstream oss;

    // print header
    for (auto it : table_[0]) {
      oss << std::setw(width) << it.first << "\t\t";
    }
    oss << std::endl;

    for (auto row : table_) {
      for (auto col : row) {
        oss << std::setw(width)  << col.second << "\t\t";
      }
      oss << std::endl;
    }

    return oss.str();
  }
};

// Stair:
// 1           -----                                         -----
// 1
// ...         step height = number of layers in this step
// 1           -----
// 1 2
// 1 2
// ...                                                   number of steps
// 1 2
// 1 2 3
// ...
//
// 1 2 3 ..                                                  -----
class Staircase {
 private:
  const int step_height_;
  const int step_width_;
  const int n_steps_;
  int cur_layer_;
  const int max_layer_;
  std::string layer_string_;
 
 public:
  // step_height and n_steps_ must be at least 1
  Staircase(const int &step_height, const int &step_width, const int &n_steps)
    :step_height_(step_height), step_width_(step_width), n_steps_(n_steps), cur_layer_(0), 
    max_layer_(step_height * n_steps) {}

  std::string NextLayer() {
    if (cur_layer_ >= max_layer_) 
      return "";

    const int step = cur_layer_ / step_height_;

    if (cur_layer_ % step_height_ == 0) {
      if (cur_layer_ != 0) {
        layer_string_ += " ";
      }
      int end = (step + 1) * step_width_;
      for (int col = step * step_width_; col < end; col++) {
        layer_string_ += std::to_string(col);
        if (col < end - 1) {
          layer_string_ += " ";
        }
      }
    }

    cur_layer_++;
    return layer_string_;
  }

  int MaxWidth() {
    return n_steps_ * step_width_;
  }
};

inline void AdviseDoNotNeed(void *addr, size_t len) {
  int ret = posix_madvise(addr, len, POSIX_MADV_DONTNEED);
  if (ret != 0) {
    perror("AdviseDoNotNeed()");
    exit(1);
  }
}

inline void AdviseRandom(void *addr, size_t len) {
  if (madvise(addr, len, MADV_RANDOM) == -1) {
    LOG(FATAL) << "Failed to advise RANDOM";
  }
}

template<class T>
std::string format_with_commas(T value)
{
	std::stringstream ss;
	ss.imbue(std::locale(""));
	ss << std::fixed << value;
	return ss.str();
}


std::string str_qq_search_reply(const qq::SearchReply &reply);
int varint_expand_and_encode(uint64_t value, std::string *buf, const off_t offset);
int varint_encode(uint64_t value, std::string *buf, off_t offset);

inline int varint_decode_uint32(
    const char *buf, const off_t offset, uint32_t *value) noexcept {
  uint32_t v = buf[offset];
  if (v < 0x80) {
    *value = v;
    return 1;
  }

  *value = buf[offset] & 0x7f;
  int i = 1;
  // inv: buf[offset, offset + i) has been copied to value 
  //      (buf[offset + i] is about to be copied)
  while ((buf[offset + i - 1] & 0x80) > 0) {
    *value += (0x7f & buf[offset + i]) << (i * 7);
    i++;
  }
  return i; 
}

inline int varint_decode_64bit(
    const char *buf, const off_t offset, uint64_t *value) noexcept {
  uint64_t v = buf[offset];
  if (v < 0x80) {
    *value = v;
    return 1;
  }

  *value = buf[offset] & 0x7f;
  int i = 1;
  // inv: buf[offset, offset + i) has been copied to value 
  //      (buf[offset + i] is about to be copied)
  while ((buf[offset + i - 1] & 0x80) > 0) {
    *value += ((uint64_t)(0x7f & buf[offset + i])) << (i * 7);
    i++;
  }
  return i; 
}




inline int varint_decode_uint8(
    const uint8_t *buf, const off_t offset, uint32_t *value) noexcept {
  return varint_decode_uint32((const char *)buf, offset, value);
}



// from varint code to int
// return: length of the buffer decoded
inline int varint_decode(
    const std::string &buf, const off_t offset, uint32_t *value) noexcept {
  return varint_decode_uint32(buf.data(), offset, value);
}


inline int NumOfBits(uint32_t val) {
  int n = 0;
  while (val > 0) {
    val >>= 1;
    n++;
  }

  return n;
}

// Refer to longToInt4() in Lucene for details
// Basically, we use float-like bit format: keep left-most 4 bits 
// and the signifcance.
//
// val should not be larger than 0x80000000
inline char UintToChar4(uint32_t val) {
  if (val < 0x08) {
    // value has only 3 bits
    return val & 0xFF;
  } else {
    int num_of_bits = NumOfBits(val);
    int shift = num_of_bits - 4;
    uint32_t encoded = val >> shift;
    encoded &= 0x07;
    encoded |= (shift + 1) << 3;
    return encoded;
  }
}

inline uint32_t Char4ToUint(char val) {
	uint32_t bits = val & 0x07;
	int shift = ((val & 0xff) >> 3) - 1;
	uint32_t decoded;

	if (shift == -1) {
		// subnormal value
		decoded = bits;
	} else {
		// normal value
		decoded = (bits | 0x08) << shift;
	}

	return decoded;
}


inline ssize_t Write(int fd, const char *buf, size_t size) {
  size_t written = 0;
  while (size - written > 0) {
    auto n = write(fd, buf + written, size - written);
    if (n == -1) {
      LOG(FATAL) << "Failed to write.";
    }

    written += n;
  }

  return written;
}

inline ssize_t Read(int fd, char *buf, size_t size) {
  size_t n_read = 0;
  while (size - n_read > 0) {
    auto n = read(fd, buf + n_read, size - n_read);
    if (n == -1) {
      LOG(FATAL) << "Failed to write.";
    }

    n_read += n;
  }

  return n_read;
}


void MapFile(std::string path, char **ret_addr, int *ret_fd, size_t *ret_file_length);
void UnmapFile(char *addr, int fd, size_t file_length);


class FileMap {
 public:
  FileMap() {}

  void Open(const std::string path) {
    if (fd_ != -1)
      LOG(FATAL) << "File is likely to be open already!";

    MapFile(path, &addr_, &fd_, &file_length_);
  }

  void MAdviseRand() {
    AdviseRandom(addr_, file_length_);
  }

  void Close() {
    UnmapFile(addr_, fd_, file_length_);
    addr_ = nullptr;
    fd_ = -1;
    file_length_ = -1;
  }

  char *Addr() {
    return addr_;
  }

  size_t Length() const {
    return file_length_;
  }

 private: 
  int fd_ = -1;
  char *addr_ = nullptr;
  size_t file_length_ = -1;
};

void RemoveDir(std::string path);
void CreateDir(std::string path);
void PrepareDir(std::string path);
void PrintVInts(const uint8_t *buf);
std::string JoinPath(const std::string a, const std::string b);

template <typename T>
void PrintVec(const std::vector<T> vec) {
  for (auto &x : vec) {
    std::cout << x << ",";
  }
  std::cout << std::endl;
}

inline std::string OffsetPairToStr(OffsetPair pair) {
  std::string ret;

  ret += "(";
  ret += std::to_string(std::get<0>(pair));
  ret += ",";
  ret += std::to_string(std::get<1>(pair));
  ret += ")";

  return ret;
}


template <typename PLIter_T>
std::vector<int> CollectDodId(PLIter_T iter) {
  std::vector<int> ret;

  while (iter.IsEnd() != true) {
    ret.push_back(iter.DocId());
    iter.Advance();
  }

  return ret;
}

template <typename PLIter_T>
std::vector<int> CollectTermFreq(PLIter_T iter) {
  std::vector<int> ret;

  while (iter.IsEnd() != true) {
    ret.push_back(iter.TermFreq());
    iter.Advance();
  }

  return ret;
}

template <typename PLIter_T, typename PosIter_T>
std::vector<int> CollectPositions(PLIter_T iter) {
  std::vector<int> ret;

  int index = 0;
  while (iter.IsEnd() != true) {
    PosIter_T pos_iter;
    iter.AssignPositionBegin(&pos_iter);
    
    while( pos_iter.IsEnd() == false ) {
      auto v = pos_iter.Pop();
      std::cout << "i:" << index <<  " pos: " << v << std::endl;
      index++;
      ret.push_back(v);
    }

    iter.Advance();
  }

  return ret;
}


template <typename PLIter_T, typename PosIter_T>
void PrintIter(PLIter_T iter) {
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Showing posting list ~~~~~~~\n";
  std::cout << "Posting list size: " << iter.Size() << std::endl;

  std::cout << " sssssssssssssss skip list ssssssssssssssssssss \n";
  std::cout << iter.SkipListString();

  std::cout << "Doc ID: ";
  auto v1 = CollectDodId(iter);
  PrintVec<int>(v1);

  std::cout << "Term Freq: ";
  auto v2 = CollectTermFreq(iter);
  PrintVec<int>(v2);

  std::cout << "Positions: ";
  auto v3 = CollectPositions<PLIter_T, PosIter_T>(iter);
  PrintVec<int>(v3);
}


static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

inline std::string MakeString(char ch) {
  return std::string(1, ch);
}

inline std::string SerializeFloat(const float &val) {
  return std::string((char *)&val, sizeof(val));
}

inline float DeserializeFloat(const char *buf) {
  float val; 
  memcpy((char *)&val, buf, sizeof(val));
  return val;
}

inline void LockAllMemory() {
    std::cout << "========================" << std::endl;
    std::cout << "Locking all memory (MCL_CURRENT) ...." << std::endl;

    int ret = mlockall(MCL_CURRENT);
    if (ret == -1) {
      perror("Failed to lock memory!");
      exit(-1);
    }

    std::cout << "Successfully Locked." << std::endl;
    std::cout << "========================" << std::endl;
}

inline std::size_t DivideRoundUp(std::size_t val, std::size_t divider) {
  return (val + divider - 1) / divider;
}

// return if a starts with b
inline bool StartsWith(const std::string &a, const std::string &b) {
  return a.compare(0, b.size(), b) == 0;
}

// index is from right to left
// 63 62 ... 1 0
inline void SetBit(uint64_t *val, int index) {
  *val = *val | ((uint64_t) 1 << index);
}

// index is from left to right
// 0 1 .. 62 63
inline void SetBitReverse(uint64_t *val, int index) {
  *val = *val | ((uint64_t) 1 << (63 - index));
}

bool PathExists(std::string path);

inline std::ifstream::pos_type GetFileSize(const std::string &path)
{
  std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
  return in.tellg(); 
}

template <typename T>
inline std::vector<T> EncodeDelta(const std::vector<T> &values) {
  T prev = 0;
  std::vector<T> vals;

  for (auto &v : values) {
    vals.push_back(v - prev);
    prev = v;
  }

  return vals;
}

inline std::string FormatThousands(long long val) {
	std::stringstream ss;
	ss.imbue(std::locale("en_US.UTF-8"));
	ss << val;
	return ss.str();
}



} // namespace util
#endif

