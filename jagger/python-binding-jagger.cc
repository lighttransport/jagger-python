#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

// To increase portability, MMAP is off by default.
// #defined JAGGER_USE_MMAP_IO

#include "jagger.h"

#ifndef NUM_POS_FIELD
#define NUM_POS_FIELD 4
#endif

namespace py = pybind11;

namespace {

constexpr uint32_t kMaxThreads = 1024;

// ----------------------------------------------------------------------------
// Small vector class useful for multi-threaded environment.
//
// stack_container.h
//
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This allocator can be used with STL containers to provide a stack buffer
// from which to allocate memory and overflows onto the heap. This stack buffer
// would be allocated on the stack and allows us to avoid heap operations in
// some situations.
//
// STL likes to make copies of allocators, so the allocator itself can't hold
// the data. Instead, we make the creator responsible for creating a
// StackAllocator::Source which contains the data. Copying the allocator
// merely copies the pointer to this shared source, so all allocators created
// based on our allocator will share the same stack buffer.
//
// This stack buffer implementation is very simple. The first allocation that
// fits in the stack buffer will use the stack buffer. Any subsequent
// allocations will not use the stack buffer, even if there is unused room.
// This makes it appropriate for array-like containers, but the caller should
// be sure to reserve() in the container up to the stack buffer size. Otherwise
// the container will allocate a small array which will "use up" the stack
// buffer.
template <typename T, size_t stack_capacity>
class StackAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer pointer;
  typedef typename std::allocator<T>::size_type size_type;

  // Backing store for the allocator. The container owner is responsible for
  // maintaining this for as long as any containers using this allocator are
  // live.
  struct Source {
    Source() : used_stack_buffer_(false) {}

    // Casts the buffer in its right type.
    T *stack_buffer() { return reinterpret_cast<T *>(stack_buffer_); }
    const T *stack_buffer() const {
      return reinterpret_cast<const T *>(stack_buffer_);
    }

    //
    // IMPORTANT: Take care to ensure that stack_buffer_ is aligned
    // since it is used to mimic an array of T.
    // Be careful while declaring any unaligned types (like bool)
    // before stack_buffer_.
    //

    // The buffer itself. It is not of type T because we don't want the
    // constructors and destructors to be automatically called. Define a POD
    // buffer of the right size instead.
    char stack_buffer_[sizeof(T[stack_capacity])];

    // Set when the stack buffer is used for an allocation. We do not track
    // how much of the buffer is used, only that somebody is using it.
    bool used_stack_buffer_;
  };

  // Used by containers when they want to refer to an allocator of type U.
  template <typename U>
  struct rebind {
    typedef StackAllocator<U, stack_capacity> other;
  };

  // For the straight up copy c-tor, we can share storage.
  StackAllocator(const StackAllocator<T, stack_capacity> &rhs)
      : source_(rhs.source_) {}

  // ISO C++ requires the following constructor to be defined,
  // and std::vector in VC++2008SP1 Release fails with an error
  // in the class _Container_base_aux_alloc_real (from <xutility>)
  // if the constructor does not exist.
  // For this constructor, we cannot share storage; there's
  // no guarantee that the Source buffer of Ts is large enough
  // for Us.
  // TODO(Google): If we were fancy pants, perhaps we could share storage
  // iff sizeof(T) == sizeof(U).
  template <typename U, size_t other_capacity>
  StackAllocator(const StackAllocator<U, other_capacity> &other)
      : source_(nullptr) {
    (void)other;
  }

  explicit StackAllocator(Source *source) : source_(source) {}

  // Actually do the allocation. Use the stack buffer if nobody has used it yet
  // and the size requested fits. Otherwise, fall through to the standard
  // allocator.
  pointer allocate(size_type n, void *hint = nullptr) {
    if (source_ != nullptr && !source_->used_stack_buffer_ &&
        n <= stack_capacity) {
      source_->used_stack_buffer_ = true;
      return source_->stack_buffer();
    } else {
      return std::allocator<T>::allocate(n, hint);
    }
  }

  // Free: when trying to free the stack buffer, just mark it as free. For
  // non-stack-buffer pointers, just fall though to the standard allocator.
  void deallocate(pointer p, size_type n) {
    if (source_ != nullptr && p == source_->stack_buffer())
      source_->used_stack_buffer_ = false;
    else
      std::allocator<T>::deallocate(p, n);
  }

 private:
  Source *source_;
};

// A wrapper around STL containers that maintains a stack-sized buffer that the
// initial capacity of the vector is based on. Growing the container beyond the
// stack capacity will transparently overflow onto the heap. The container must
// support reserve().
//
// WATCH OUT: the ContainerType MUST use the proper StackAllocator for this
// type. This object is really intended to be used only internally. You'll want
// to use the wrappers below for different types.
template <typename TContainerType, int stack_capacity>
class StackContainer {
 public:
  typedef TContainerType ContainerType;
  typedef typename ContainerType::value_type ContainedType;
  typedef StackAllocator<ContainedType, stack_capacity> Allocator;

  // Allocator must be constructed before the container!
  StackContainer() : allocator_(&stack_data_), container_(allocator_) {
    // Make the container use the stack allocation by reserving our buffer size
    // before doing anything else.
    container_.reserve(stack_capacity);
  }

  // Getters for the actual container.
  //
  // Danger: any copies of this made using the copy constructor must have
  // shorter lifetimes than the source. The copy will share the same allocator
  // and therefore the same stack buffer as the original. Use std::copy to
  // copy into a "real" container for longer-lived objects.
  ContainerType &container() { return container_; }
  const ContainerType &container() const { return container_; }

  // Support operator-> to get to the container. This allows nicer syntax like:
  //   StackContainer<...> foo;
  //   std::sort(foo->begin(), foo->end());
  ContainerType *operator->() { return &container_; }
  const ContainerType *operator->() const { return &container_; }

#ifdef UNIT_TEST
  // Retrieves the stack source so that that unit tests can verify that the
  // buffer is being used properly.
  const typename Allocator::Source &stack_data() const { return stack_data_; }
#endif

 protected:
  typename Allocator::Source stack_data_;
  unsigned char pad_[7];
  Allocator allocator_;
  ContainerType container_;

  // DISALLOW_EVIL_CONSTRUCTORS(StackContainer);
  StackContainer(const StackContainer &);
  void operator=(const StackContainer &);
};

// StackVector
//
// Example:
//   StackVector<int, 16> foo;
//   foo->push_back(22);  // we have overloaded operator->
//   foo[0] = 10;         // as well as operator[]
template <typename T, size_t stack_capacity>
class StackVector
    : public StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                            stack_capacity> {
 public:
  StackVector()
      : StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                       stack_capacity>() {}

  // We need to put this in STL containers sometimes, which requires a copy
  // constructor. We can't call the regular copy constructor because that will
  // take the stack buffer from the original. Here, we create an empty object
  // and make a stack buffer of its own.
  StackVector(const StackVector<T, stack_capacity> &other)
      : StackContainer<std::vector<T, StackAllocator<T, stack_capacity>>,
                       stack_capacity>() {
    this->container().assign(other->begin(), other->end());
  }

  StackVector<T, stack_capacity> &operator=(
      const StackVector<T, stack_capacity> &other) {
    this->container().assign(other->begin(), other->end());
    return *this;
  }

  // Vectors are commonly indexed, which isn't very convenient even with
  // operator-> (using "->at()" does exception stuff we don't want).
  T &operator[](size_t i) { return this->container().operator[](i); }
  const T &operator[](size_t i) const {
    return this->container().operator[](i);
  }
};

// ----------------------------------------------------------------------------

#ifdef _WIN32
static std::wstring UTF8ToWchar(const std::string &str) {
  int wstr_size =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), nullptr, 0);
  std::wstring wstr(size_t(wstr_size), 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), &wstr[0],
                      int(wstr.size()));
  return wstr;
}

static std::string WcharToUTF8(const std::wstring &wstr) {
  int str_size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), int(wstr.size()),
                                     nullptr, 0, nullptr, nullptr);
  std::string str(size_t(str_size), 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), int(wstr.size()), &str[0],
                      int(str.size()), nullptr, nullptr);
  return str;
}
#endif

static bool FileExists(const std::string &filepath) {
  bool ret{false};
#ifdef JAGGER_ANDROID_LOAD_FROM_ASSETS
  if (asset_manager) {
    AAsset *asset = AAssetManager_open(asset_manager, filepath.c_str(),
                                       AASSET_MODE_STREAMING);
    if (!asset) {
      return false;
    }
    AAsset_close(asset);
    ret = true;
  } else {
    return false;
  }
#else
#ifdef _WIN32
#if defined(_MSC_VER) || defined(__GLIBCXX__) || defined(_LIBCPP_VERSION)
  FILE *fp = nullptr;
  errno_t err = _wfopen_s(&fp, UTF8ToWchar(filepath).c_str(), L"rb");
  if (err != 0) {
    return false;
  }
#else
  FILE *fp = nullptr;
  errno_t err = fopen_s(&fp, filepath.c_str(), "rb");
  if (err != 0) {
    return false;
  }
#endif

#else
  FILE *fp = fopen(filepath.c_str(), "rb");
#endif
  if (fp) {
    ret = true;
    fclose(fp);
  } else {
    ret = false;
  }
#endif

  return ret;
}

bool ReadWholeFile(std::vector<uint8_t> *out, std::string *err,
                   const std::string &filepath, size_t filesize_max,
                   void *userdata) {
  (void)userdata;

#ifdef TINYUSDZ_ANDROID_LOAD_FROM_ASSETS
  if (tinyusdz::io::asset_manager) {
    AAsset *asset = AAssetManager_open(asset_manager, filepath.c_str(),
                                       AASSET_MODE_STREAMING);
    if (!asset) {
      if (err) {
        (*err) += "File open error(from AssestManager) : " + filepath + "\n";
      }
      return false;
    }
    size_t size = AAsset_getLength(asset);
    if (size == 0) {
      if (err) {
        (*err) += "Invalid file size : " + filepath +
                  " (does the path point to a directory?)";
      }
      return false;
    }
    out->resize(size);
    AAsset_read(asset, reinterpret_cast<char *>(&out->at(0)), size);
    AAsset_close(asset);
    return true;
  } else {
    if (err) {
      (*err) += "No asset manager specified : " + filepath + "\n";
    }
    return false;
  }

#else
#ifdef _WIN32
#if defined(__GLIBCXX__)  // mingw
  int file_descriptor =
      _wopen(UTF8ToWchar(filepath).c_str(), _O_RDONLY | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(file_descriptor, std::ios_base::in);
  std::istream f(&wfile_buf);
#elif defined(_MSC_VER) || defined(_LIBCPP_VERSION)
  // For libcxx, assume _LIBCPP_HAS_OPEN_WITH_WCHAR is defined to accept
  // `wchar_t *`
  std::ifstream f(UTF8ToWchar(filepath).c_str(), std::ifstream::binary);
#else
  // Unknown compiler/runtime
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
#else
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
  if (!f) {
    if (err) {
      (*err) += "File open error : " + filepath + "\n";
    }
    return false;
  }

  // For directory(and pipe?), peek() will fail(Posix gnustl/libc++ only)
  int buf = f.peek();
  (void)buf;
  if (!f) {
    if (err) {
      (*err) +=
          "File read error. Maybe empty file or invalid file : " + filepath +
          "\n";
    }
    return false;
  }

  f.seekg(0, f.end);
  size_t sz = static_cast<size_t>(f.tellg());
  f.seekg(0, f.beg);

  if (int64_t(sz) < 0) {
    if (err) {
      (*err) += "Invalid file size : " + filepath +
                " (does the path point to a directory?)";
    }
    return false;
  } else if (sz == 0) {
    if (err) {
      (*err) += "File is empty : " + filepath + "\n";
    }
    return false;
  } else if (uint64_t(sz) >= uint64_t(std::numeric_limits<int64_t>::max())) {
    // Posixish environment.
    if (err) {
      (*err) += "Invalid File(Pipe or special device?) : " + filepath + "\n";
    }
    return false;
  }

  if ((filesize_max > 0) && (sz > filesize_max)) {
    if (err) {
      (*err) += "File size is too large : " + filepath +
                " sz = " + std::to_string(sz) +
                ", allowed max filesize = " + std::to_string(filesize_max) +
                "\n";
    }
    return false;
  }

  out->resize(sz);
  f.read(reinterpret_cast<char *>(&out->at(0)),
         static_cast<std::streamsize>(sz));

  if (!f) {
    // read failure.
    if (err) {
      (*err) += "Failed to read file: " + filepath + "\n";
    }
    return false;
  }

  return true;
#endif
}

static inline bool is_line_ending(const char *p, size_t i, size_t end_i) {
  if (p[i] == '\0') return true;
  if (p[i] == '\n') return true;  // this includes \r\n
  if (p[i] == '\r') {
    if (((i + 1) < end_i) && (p[i + 1] != '\n')) {  // detect only \r case
      return true;
    }
  }
  return false;
}

struct LineInfo {
  size_t pos{0};
  size_t len{0};
};

using LineInfoVector = StackVector<std::vector<LineInfo>, kMaxThreads>;

//
// Return: List of address of line begin/end in `src`.
//
static LineInfoVector split_lines(const std::string &src,
                                  uint32_t req_nthreads = 0) {
  // From nanocsv. https://github.com/lighttransport/nanocsv

  uint32_t num_threads = (req_nthreads == 0)
                             ? uint32_t(std::thread::hardware_concurrency())
                             : req_nthreads;
  num_threads = (std::max)(
      1u, (std::min)(static_cast<uint32_t>(num_threads), kMaxThreads));

  const size_t buffer_length = src.size();
  const char *buffer = src.c_str();

  LineInfoVector line_infos;
  line_infos->resize(kMaxThreads);

  for (size_t t = 0; t < static_cast<size_t>(num_threads); t++) {
    // Pre allocate enough memory. len / 128 / num_threads is just a heuristic
    // value.
    line_infos[t].reserve(buffer_length / 128 / size_t(num_threads));
  }

  // Find newline('\n', '\r' or '\r\n') and create line data.
  {
    StackVector<std::thread, kMaxThreads> workers;

    auto chunk_size = buffer_length / size_t(num_threads);

    // input is too small. use single-threaded parsing
    if (buffer_length < size_t(num_threads)) {
      num_threads = 1;
      chunk_size = buffer_length;
    }

    for (size_t t = 0; t < static_cast<size_t>(num_threads); t++) {
      workers->push_back(std::thread([&, t]() {
        auto start_idx = (t + 0) * chunk_size;
        auto end_idx = (std::min)((t + 1) * chunk_size, buffer_length - 1);
        if (t == static_cast<size_t>((num_threads - 1))) {
          end_idx = buffer_length - 1;
        }

        // true if the line currently read must be added to the current line
        // info
        bool new_line_found =
            (t == 0) || is_line_ending(buffer, start_idx - 1, end_idx);

        size_t prev_pos = start_idx;
        for (size_t i = start_idx; i < end_idx; i++) {
          if (is_line_ending(buffer, i, end_idx)) {
            if (!new_line_found) {
              // first linebreak found in (chunk > 0), and a line before this
              // linebreak belongs to previous chunk, so skip it.
              prev_pos = i + 1;
              new_line_found = true;
            } else {
              LineInfo info;
              info.pos = prev_pos;
              info.len = i - prev_pos;

              if (info.len > 0) {
                line_infos[t].push_back(info);
              }

              prev_pos = i + 1;
            }
          }
        }

        // If at least one line started in this chunk, find where it ends in the
        // rest of the buffer
        if (new_line_found && (t < size_t(num_threads)) &&
            (buffer[end_idx - 1] != '\n')) {
          for (size_t i = end_idx; i < buffer_length; i++) {
            if (is_line_ending(buffer, i, buffer_length)) {
              LineInfo info;
              info.pos = prev_pos;
              info.len = i - prev_pos;

              if (info.len > 0) {
                line_infos[t].push_back(info);
              }

              break;
            }
          }
        }
      }));
    }

    for (size_t t = 0; t < workers->size(); t++) {
      workers[t].join();
    }
  }

  return line_infos;
}

}  // namespace

// jagger.cc(with some modification) BEGIN --------------------

static const size_t MAX_KEY_BITS = 14;
static const size_t MAX_FEATURE_BITS = 7;

namespace ccedar {
class da_ : public ccedar::da<int, int, MAX_KEY_BITS> {
 public:
  struct utf8_feeder {  // feed one UTF-8 character by one while mapping codes
    const char *p, *const end;
    utf8_feeder(const char *key_, const char *end_) : p(key_), end(end_) {}
    int read(int &b) const { return p == end ? 0 : unicode(p, b); }
    void advance(const int b) { p += b; }
  };
  int longestPrefixSearchWithPOS(const char *key, const char *const end,
                                 int fi_prev, const uint16_t *const c2i,
                                 size_t from = 0) const {
    size_t from_ = 0;
    int n(0), i(0), b(0);
    for (utf8_feeder f(key, end); (i = c2i[f.read(b)]); f.advance(b)) {
      size_t pos = 0;
      const int n_ = traverse(&i, from, pos, pos + 1);
      if (n_ == CEDAR_NO_VALUE) continue;
      if (n_ == CEDAR_NO_PATH) break;
      from_ = from;
      n = n_;
    }
    // ad-hock matching at the moment; it prefers POS-ending patterns
    if (!fi_prev) return n;
    for (const node *const array_ = reinterpret_cast<const node *>(array());;
         from = array_[from].check) {  // hopefully, in the cache
      const int n_ = exactMatchSearch<int>(&fi_prev, 1, from);
      if (n_ != CEDAR_NO_VALUE) return n_;
      if (from == from_) return n;
    }
  }
};
}  // namespace ccedar
namespace jagger {

namespace {

inline std::string ltrim(const std::string &s)
{
  const std::string kWhitespace = " \n\r\t\f\v";

  size_t start = s.find_first_not_of(kWhitespace);
  return (start == std::string::npos) ? "" : s.substr(start);
}

inline std::string rtrim(const std::string &s)
{
  const std::string kWhitespace = " \n\r\t\f\v";

  size_t end = s.find_last_not_of(kWhitespace);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline std::vector<std::string> split(
    const std::string &str, const std::string &sep,
    const uint32_t kMaxItems = (std::numeric_limits<int32_t>::max)() / 100) {
  size_t s;
  size_t e = 0;

  size_t count = 0;
  std::vector<std::string> result;

  while ((s = str.find_first_not_of(sep, e)) != std::string::npos) {
    e = str.find(sep, s);
    result.push_back(str.substr(s, e - s));
    if (++count > kMaxItems) {
      break;
    }
  }

  return result;
}

// Support quoted string'\"' (do not consider `delimiter` character in quoted string)
// delimiter must be a ASCII char.
// quote_char must be a single UTF-8 char.
inline std::vector<std::string> parse_feature(const char *p, const size_t len, const char delimiter = ',', const char *quote_char = "\"")
{
  std::vector<std::string> tokens;

  if (len == 0) {
    return tokens;
  }

  size_t quote_size = u8_len(quote_char);

  bool in_quoted_string = false;
  size_t s_start = 0;

  const char *curr_p = p;

  for (size_t i = 0; i < len; i += u8_len(curr_p)) {

    curr_p = &p[i];

    if (is_line_ending(p, i, len - 1)) {
      break;
    }

    if ((i + quote_size) < len) {
      if (memcmp(curr_p, quote_char, quote_size) == 0) {
        in_quoted_string = !in_quoted_string;
        continue;
      }
    }

    if (!in_quoted_string && (p[i] == delimiter)) {
      //std::cout << "s_start = " << s_start << ", (i-1) = " << i-1 << "\n";
      //std::cout << "p[i] = " << p[i] << "\n";
      if (s_start < i) {
        std::string tok(p + s_start, i - s_start);

        tokens.push_back(tok);
      } else {
        // Add empty string
        tokens.push_back(std::string());
      }

      s_start = i + 1; // next to delimiter char
    }
  }

  // the remainder
  //std::cout << "remain: s_start = " << s_start << ", len - 1 = " << len-1 << "\n";

  if (s_start <= (len - 1)) {
    std::string tok(p + s_start, len - s_start);
    tokens.push_back(tok);
  }

  return tokens;
}

} // namespace



class PyToken {
 public:
  PyToken() = default;

  // TODO: Use string_view?
  const std::string &surface() const { return _surface; }

  std::string &get_surface() { return _surface; }

  // comma separated feature
  // TODO: Use string_view when compiled with C++17
  const std::string &feature() const { return _feature; }

  // NOTE: feature string contains leading whitespaces.
  std::string &get_feature() { return _feature; }

  void set_quote_char(const std::string &quote_char) {
    _quote_char = quote_char;
  }

  uint32_t n_tags() const {
    if (_feature.empty()) {
      return 0;
    }

    // cache result.
    if (_tags.empty()) {
      // remove leading whitespaces.
      std::string trimmed_string = ltrim(_feature);

      _tags = parse_feature(trimmed_string.data(), trimmed_string.size(), ',', _quote_char.c_str());
    }

    return _tags.size();
  }

  // TODO: use string_view
  std::string tag(uint32_t idx) const {
    if (idx < n_tags()) {
      return _tags[idx];
    }
    return std::string();
  }

  const std::string str() {
    return _surface + "\t" + _feature;
  }

 private:
  std::string _surface;

  // TODO: Use string_view when compiled with C++17
  std::string _feature;
  mutable std::vector<std::string> _tags;
  std::string _quote_char = "\"";
};

class tagger {
 private:
  ccedar::da_ da;
  const uint16_t *c2i{nullptr};  // mapping from utf8, BOS, unk to character ID
  const uint64_t *p2f{nullptr};  // mapping from pattern ID to feature strings
  const char *fs{nullptr};       // feature strings

#if defined(JAGGER_USE_MMAP_IO)
  std::vector<std::pair<void *, size_t>> mmaped;
#else
  std::vector<uint8_t> buffers[4];  // up to 4 dicts
#endif

  static inline void write_string(char *&p, const char *s, size_t len = 0) {
#ifdef USE_COMPACT_DICT
    if (!len) {
      len = *reinterpret_cast<const uint16_t *>(s);
      s += sizeof(uint16_t);
    }
#endif
    std::memcpy(p, s, len);
    p += len;
  }

#if 0
  static inline std::string to_string(char *curr_p, char *start_p) {
    if (curr_p > start_p) {
      std::string(start_p, static_cast<size_t>(curr_p - start_p));
    }
    return std::string();
  }
#endif

  static inline void write_buffer(char *&p, char *buf, const size_t limit) {
    if (ptrdiff_t(p - buf) <= ptrdiff_t(limit)) return;
    ::write(1, buf, static_cast<size_t>(p - buf));
    p = buf;
  }
  template <typename T>
  static inline void write_array(T &data, const std::string &fn) {
    FILE *fp = std::fopen(fn.c_str(), "wb");
    if (!fp) my_errx(1, "no such file: %s", fn.c_str());
    std::fwrite(&data[0], sizeof(typename T::value_type), data.size(), fp);
    std::fclose(fp);
  }
  const void *read_array(const std::string &fn, size_t idx, size_t &len) {
#if defined(JAGGER_USE_MMAP_IO)
    (void)idx;
    int fd = ::open(fn.c_str(), O_RDONLY);
    if (fd == -1) my_errx(1, "no such file: %s", fn.c_str());
    // get size and read;
    const size_t size = ::lseek(fd, 0, SEEK_END);
    ::lseek(fd, 0, SEEK_SET);
#if defined(_WIN32)
    HANDLE hFile = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    HANDLE hMapping =
        CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapping == NULL) {
      my_errx(1, "CreateFileMappingA failed for: %s", fn.c_str());
    }
    void *data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
      my_errx(1, "MapViewOfFile failed for: %s", fn.c_str());
    }
    CloseHandle(hMapping);
#else
    void *data{nullptr};  // = ::mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
      my_errx(1, "mmap failed for: %s", fn.c_str());
    }
    // close() causes segmentation fault for some reason, so
    ::close(fd);
#endif
    mmaped.push_back(std::make_pair(data, size));
    len = size;
    return data;
#else
    std::vector<uint8_t> data;
    std::string err;
    if (!ReadWholeFile(&data, &err, fn, 1024ull * 1024ull * 1024ull, nullptr)) {
      py::print("Failed to read file: ", err);
      return nullptr;
    }
    buffers[idx] = data;

    len = data.size();
    // Assume pointer address does not change.
    return reinterpret_cast<const void *>(buffers[idx].data());
#endif
  }

 public:
  tagger()
      : da(),
        c2i(0),
        p2f(0),
        fs(0)
#if defined(JAGGER_USE_MMAP_IO)
        ,
        mmaped()
#endif
  {
  }
  ~tagger() {
#if defined(JAGGER_USE_MMAP_IO)
    for (size_t i = 0; i < mmaped.size(); ++i)
#if defined(_WIN32)
      if (!UnmapViewOfFile(mmaped[i].first)) {
        fprintf(stderr, "jagger: warn: UnmapViewOfFile failed.");
      }
#else
      ::munmap(mmaped[i].first, mmaped[i].second);
#endif
#endif
  }
  bool read_model(const std::string &m) {  // read patterns to memory
    const std::string da_fn(m + ".da"), c2i_fn(m + ".c2i"), p2f_fn(m + ".p2f"),
        fs_fn(m + ".fs");
    // struct stat st;
    // if (::stat (da_fn.c_str (), &st) != 0) { // compile
    if (!FileExists(da_fn)) {
      py::print("building DA trie from patterns..");
      std::vector<uint16_t> c2i_;  // mapping from utf8, BOS, unk to char ID
      std::vector<uint64_t> p2f_;  // mapping from pattern ID to feature str
      std::vector<char> fs_;       // feature strings
      sbag_t fbag("\tBOS");
#ifdef USE_COMPACT_DICT
      fbag.to_i(FEAT_UNK);
      sbag_t fbag_(",*,*,*\n");
#else
      sbag_t fbag_((std::string(FEAT_UNK) + ",*,*,*\n").c_str());
#endif
      std::map<uint64_t, int> fs2pid;
      fs2pid.insert(std::make_pair((1ull << 32) | 2, fs2pid.size()));
      p2f_.push_back((1ull << 32) | 2);
      // count each character to obtain dense mapping
      std::vector<std::pair<size_t, int>> counter(CP_MAX + 3);
      for (size_t u = 0; u < counter.size(); ++u)  // allow 43 bits for counting
        counter[u] = std::make_pair(0, u);
      std::vector<std::pair<std::string, uint64_t>> keys;
      char *line = 0;
      simple_reader reader(m.c_str());
      while (const size_t len = reader.gets(&line)) {  // find pos offset
        // pattern format: COUNT PATTEN PREV_POS BYTES CHAR_TYPE FEATURES
        char *p(line), *const p_end(p + len);
        const size_t count = std::strtoul(p, &p, 10);
        const char *pat = ++p;
        for (int b = 0; *p != '\t'; p += b)
          counter[unicode(p, b)].first += count + 1;
        size_t fi_prev = 0;
        const char *f_prev = p;  // starting with '\t'
        if (*++p != '\t') {      // with pos context
          p = const_cast<char *>(skip_to(p, 1, '\t')) - 1;
          fi_prev = fbag.to_i(f_prev, p - f_prev) + 1;
          if (fi_prev + CP_MAX == counter.size())  // new part-of-speech
            counter.push_back(std::make_pair(0, (fi_prev + CP_MAX)));
          counter[fi_prev + CP_MAX].first += count + 1;
        }
        const size_t bytes = std::strtoul(++p, &p, 10);
        const size_t ctype = std::strtoul(++p, &p, 10);
        const char *f = p;  // starting with '\t'
        p = const_cast<char *>(skip_to(p, NUM_POS_FIELD, ',')) - 1;
        const size_t fi_ = fbag.to_i(f, p - f) + 1;
#ifndef USE_COMPACT_DICT
        p = const_cast<char *>(f);
#endif
        const size_t fi = fbag_.to_i(p, p_end - p) + 1;
        if (fi_ + CP_MAX == counter.size())  // new part-of-speech
          counter.push_back(std::make_pair(0, fi_ + CP_MAX));
        std::pair<std::map<uint64_t, int>::iterator, bool> itb =
            fs2pid.insert(std::make_pair((fi << 32) | fi_, fs2pid.size()));
        if (itb.second) p2f_.push_back((fi << 32) | fi_);
        keys.push_back(std::make_pair(std::string(pat, f_prev - pat),
                                      (((bytes << 23) | ((ctype & 0x7) << 20) |
                                        (itb.first->second & 0xfffff))
                                       << 12) |
                                          fi_prev));
      }
      // save c2i
      std::sort(counter.begin() + 1, counter.end(),
                std::greater<std::pair<size_t, int>>());
      c2i_.resize(counter.size());
      for (unsigned int i = 1; i < counter.size() && counter[i].first; ++i)
        c2i_[counter[i].second] = static_cast<uint16_t>(i);
      // save feature strings
      std::vector<size_t> offsets;
#ifdef USE_COMPACT_DICT
      fbag.serialize(fs_, offsets);  // required only for compact dict
#endif
      fbag_.serialize(fs_, offsets);
      write_array(fs_, fs_fn);
      // save mapping from morpheme ID to morpheme feature strings
      for (size_t i = 0; i < p2f_.size(); ++i) {
#ifdef USE_COMPACT_DICT
        p2f_[i] = (offsets[(p2f_[i] >> 32) - 1 + fbag.size()] << 34) |
                  (offsets[(p2f_[i] & 0xffffffff) - 1] << MAX_KEY_BITS) |
#else
        const std::string &f = fbag_.to_s((p2f_[i] >> 32) - 1);
        const char *q = skip_to(f.c_str(), NUM_POS_FIELD, ',') - 1;
        p2f_[i] = (offsets[(p2f_[i] >> 32) - 1] << 34) |
                  (fbag_.to_s((p2f_[i] >> 32) - 1).size()
                   << (MAX_KEY_BITS + MAX_FEATURE_BITS)) |
                  (q - f.c_str()) << MAX_KEY_BITS |
#endif
                  c2i_[(p2f_[i] & 0xffffffff) + CP_MAX];
      }
      write_array(p2f_, p2f_fn);
      // save pattern trie
      for (std::vector<std::pair<std::string, uint64_t>>::const_iterator it =
               keys.begin();
           it != keys.end(); ++it) {
        std::vector<int> key;
        for (int offset(0), b(0); size_t(offset) < it->first.size();
             offset += b)
          key.push_back(c2i_[unicode(&it->first[offset], b)]);
        if (it->second & 0xfff)
          key.push_back(c2i_[(it->second & 0xfff) + CP_MAX]);
        da.update(&key[0], key.size()) = it->second >> 12;
      }
      c2i_.resize(CP_MAX + 2);  // chop most of part-of-speech mapping
      write_array(c2i_, c2i_fn);
      da.save(da_fn.c_str());
      py::print("Model conversion done.\n");
    }
    size_t buf_size{0};
    const void *da_buf = read_array(da_fn, 0, buf_size);
    if (!da_buf) {
      py::print("da_fn not found:", da_fn);
      return false;
    }
    //da.set_array(da_buf, buf_size / sizeof();
    da.set_array(da_buf, buf_size);
    c2i = static_cast<const uint16_t *>(read_array(c2i_fn, 1, buf_size));
    if (!c2i) {
      py::print("c2i_fn not found:", c2i_fn);
      return false;
    }
    p2f = static_cast<const uint64_t *>(read_array(p2f_fn, 2, buf_size));
    if (!p2f) {
      py::print("p2f_fn not found:", p2f_fn);
      return false;
    }
    fs = static_cast<const char *>(read_array(fs_fn, 3, buf_size));
    if (!fs) {
      py::print("fs_fn not found:", fs_fn);
      return false;
    }
    // py::print("All dict read OK");

    return true;
  }
#if 0 // not used
  template <const int BUF_SIZE_, const bool POS_TAGGING>
  void run() const {
    if (BUF_SIZE_ == 0) std::fprintf(stderr, "(input: stdin)\n");
    char _res[BUF_SIZE], *_ptr(&_res[0]), *line(0);
    simple_reader reader;
    while (const size_t len = reader.gets(&line)) {
      int bytes(0), bytes_prev(0), id(0), ctype(0), ctype_prev(0);
      uint64_t offsets = c2i[CP_MAX + 1];
      bool bos(true), ret(line[len - 1] == '\n'), concat(false);
      for (const char *p(line), *const p_end(p + len - ret); p != p_end;
           bytes_prev = bytes, ctype_prev = ctype,
           offsets = p2f[static_cast<size_t>(id)], p += bytes) {
        const int r = da.longestPrefixSearchWithPOS(p, p_end, offsets & 0x3fff,
                                                    &c2i[0]);  // found word
        id = r & 0xfffff;
        bytes = (r >> 23) ? (r >> 23) : u8_len(p);
        ctype = (r >> 20) & 0x7;  // 0: num|unk / 1: alpha / 2: kana / 3: other
        if (!bos) {  // word that may concat with the future context
          if (ctype_prev != ctype ||  // different character types
              ctype_prev == 3 ||      // seen words in non-num/alpha/kana
              (ctype_prev == 2 && bytes_prev + bytes >= 18)) {
            if (POS_TAGGING) {
#ifdef USE_COMPACT_DICT
              write_string(_ptr, &fs[((offsets >> MAX_KEY_BITS) & 0xfffff)]);
              if (concat)
                write_string(_ptr, ",*,*,*\n", 7);
              else
                write_string(_ptr, &fs[(offsets >> 34)]);
#else
              if (concat) {
                write_string(_ptr, &fs[(offsets >> 34)],
                             (offsets >> MAX_KEY_BITS) & 0x7f);
                write_string(_ptr, ",*,*,*\n", 7);
              } else
                write_string(
                    _ptr, &fs[(offsets >> 34)],
                    (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff);
#endif
              concat = false;
            } else
              write_string(_ptr, " ", 1);
          } else
            concat = true;
        } else
          bos = false;
        write_string(_ptr, p, static_cast<size_t>(bytes));
      }
      if (!bos)  // output fs of last token
        if (POS_TAGGING) {
#ifdef USE_COMPACT_DICT
          write_string(_ptr, &fs[((offsets >> MAX_KEY_BITS) & 0xfffff)]);
          if (concat)
            write_string(_ptr, ",*,*,*\n", 7);
          else
            write_string(_ptr, &fs[(offsets >> 34)]);
#else
          if (concat) {
            write_string(_ptr, &fs[(offsets >> 34)],
                         (offsets >> MAX_KEY_BITS) & 0x7f);
            write_string(_ptr, ",*,*,*\n", 7);
          } else
            write_string(
                _ptr, &fs[(offsets >> 34)],
                (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff);
#endif
        }
      write_string(_ptr, POS_TAGGING ? "EOS\n" : "\n", POS_TAGGING ? 4 : 1);
      write_buffer(_ptr, &_res[0], BUF_SIZE_);
    }
    write_buffer(_ptr, &_res[0], 0);
  }
#endif

  // Tokenize single line.
  //
  // TODO:
  //
  // - [ ] Support compact dict
  // - [ ] Optimize output(stringstream & constcut std::string are rather slow)
  // - [x] Return structured output(PyToken)
  //
  // @return Unified output.
  std::vector<PyToken> tokenize_line(const char *addr, const size_t len) const {
#define BUF_SIZE_ (BUF_SIZE >> 1)
#define POS_TAGGING 1

    std::vector<PyToken> toks;

    std::stringstream ss;

    char _res[BUF_SIZE], *_ptr(&_res[0]);
    const char *line = addr;
    {
      int bytes(0), bytes_prev(0), id(0), ctype(0), ctype_prev(0);
      uint64_t offsets = c2i[CP_MAX + 1];
      bool bos(true), ret(line[len - 1] == '\n'), concat(false);
      for (const char *p(line), *const p_end(p + len - ret); p != p_end;
           bytes_prev = bytes, ctype_prev = ctype,
           offsets = p2f[static_cast<size_t>(id)], p += bytes) {
        const int r = da.longestPrefixSearchWithPOS(p, p_end, offsets & 0x3fff,
                                                    &c2i[0]);  // found word
        id = r & 0xfffff;
        bytes = (r >> 23) ? (r >> 23) : u8_len(p);
        ctype = (r >> 20) & 0x7;  // 0: num|unk / 1: alpha / 2: kana / 3: other
        if (!bos) {  // word that may concat with the future context
          if (ctype_prev != ctype ||  // different character types
              ctype_prev == 3 ||      // seen words in non-num/alpha/kana
              (ctype_prev == 2 && bytes_prev + bytes >= 18)) {
            if (POS_TAGGING) {
              if (concat) {
                write_string(_ptr, &fs[(offsets >> 34)],
                             (offsets >> MAX_KEY_BITS) & 0x7f);
                write_string(_ptr, ",*,*,*\n", 7);

                toks.back().get_feature() = ltrim(std::string(&fs[(offsets >> 34)],
                             (offsets >> MAX_KEY_BITS) & 0x7f));

                toks.back().get_feature() += ",*,*,*";
              } else {
                write_string(
                    _ptr, &fs[(offsets >> 34)],
                    (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff);

                // feature contains leading '\t' and ending '\n'. we remove it.
                toks.back().get_feature() = ltrim(rtrim(std::string(&fs[(offsets >> 34)],
                             (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff)));
              }
              concat = false;
            } else {
              write_string(_ptr, " ", 1);
            }
          } else {
            concat = true;
          }
        } else {
          bos = false;
        }

        // surface
        write_string(_ptr, p, static_cast<size_t>(bytes));

        if (concat) {
          // concat word to the surface of last token
          toks.back().get_surface() += std::string(p, static_cast<size_t>(bytes));
        } else {
          PyToken tok;
          tok.get_surface() = std::string(p, static_cast<size_t>(bytes));
          toks.push_back(tok);
        }
      }
      if (!bos)  // output fs of last token
        if (POS_TAGGING) {
          if (concat) {
            write_string(_ptr, &fs[(offsets >> 34)],
                         (offsets >> MAX_KEY_BITS) & 0x7f);
            write_string(_ptr, ",*,*,*\n", 7);

            toks.back().get_feature() = ltrim(std::string(&fs[(offsets >> 34)],
                         (offsets >> MAX_KEY_BITS) & 0x7f));

            toks.back().get_feature() += ",*,*,*";
          } else {
            write_string(
                _ptr, &fs[(offsets >> 34)],
                (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff);
            // feature contains leading '\t' and ending '\n'. we remove it.
            toks.back().get_feature() = ltrim(rtrim(std::string(&fs[(offsets >> 34)],
                         (offsets >> (MAX_KEY_BITS + MAX_FEATURE_BITS)) & 0x3ff)));
          }
        }
      write_string(_ptr, POS_TAGGING ? "EOS\n" : "\n", POS_TAGGING ? 4 : 1);
      // write_buffer(_ptr, &_res[0], BUF_SIZE_);
      if (ptrdiff_t(_ptr - &_res[0]) <
          ptrdiff_t(BUF_SIZE_)) {  // This should not happen though
        ss << std::string(&_res[0], _ptr);
      }
    }
    // write_buffer(_ptr, &_res[0], 0); // = fflush
    return toks;
  }
#undef BUF_SIZE_
#undef POS_TAGGING

  std::vector<PyToken> tokenize(const std::string &str) const {
    std::vector<PyToken> toks;
    if (str.empty()) {
      return toks;
    }

    toks = tokenize_line(&str[0], str.size());

    return toks;
  }
};

}  // namespace jagger
// jagger.cc END --------------------

namespace pyjagger {

class PyJagger {
 public:
  PyJagger() : _tagger(new jagger::tagger()), _model_loaded{false} {}
  PyJagger(const std::string &model_path)
      : _model_path(model_path), _tagger(new jagger::tagger()) {
    load_model(_model_path);
  }

  bool load_model(const std::string &model_path) {
    if (_model_loaded) {
      // discard previous model&instance.
      delete _tagger;

      _tagger = new jagger::tagger();
    }

    if (_tagger->read_model(model_path)) {
      _model_loaded = true;
      _model_path = model_path;
      //py::print("Model loaded:", model_path);
    } else {
      _model_loaded = false;
      py::print("Model load failed:", model_path);
    }

    return _model_loaded;
  }

  void set_threads(uint32_t nthreads) {
    _nthreads = nthreads;
  }

  ///
  /// Tokenize single-line string(char pointer version).
  ///
  std::vector<jagger::PyToken> tokenize_line(const char *s, const size_t len) const;

  ///
  /// Tokenize single-line string(std::string version).
  ///
  std::vector<jagger::PyToken> tokenize(const std::string &src) const;

  ///
  /// Tokenize string which is composed of multiple lines(delimited by '\n') in
  /// batch. Uses C++11 threads when the number of lines > 1024.
  ///
  /// @param[in] src string which is composed of multiple lines.
  /// @param[in] threads Optional. Control the number of C++11 threads(CPU
  /// cores) to use.
  ///
  std::vector<std::vector<jagger::PyToken>> tokenize_batch(const std::string &src) const;

 private:
  uint32_t _nthreads{0};  // 0 = use all cores
  std::string _model_path;
  jagger::tagger *_tagger{nullptr};
  bool _model_loaded{false};
};

std::vector<jagger::PyToken> PyJagger::tokenize(const std::string &src) const {
  std::vector<jagger::PyToken> dst;
  if (!_model_loaded) {
    py::print("Model is not loaded.");
    return dst;
  }

  if (!_tagger) {
    py::print("PyJagger: ??? tagger instance is nullptr.");
    return dst;
  }

  dst = _tagger->tokenize(src);

  return dst;
}

std::vector<std::vector<jagger::PyToken>> PyJagger::tokenize_batch(const std::string &src) const {


  std::vector<std::vector<jagger::PyToken>> dst;

  if (!_tagger) {
    py::print("PyJagger: ??? tagger instance is nullptr.");
    return dst;
  }

  if (src.empty()) {
    return dst;
  }

  uint32_t num_threads = (_nthreads == 0)
                             ? uint32_t(std::thread::hardware_concurrency())
                             : _nthreads;
  num_threads = (std::max)(
      1u, (std::min)(static_cast<uint32_t>(num_threads), kMaxThreads));


  // result = [thread][lines]
  LineInfoVector line_infos = split_lines(src, num_threads);

  // concat LineInfos
  std::vector<LineInfo> lines;
  for (size_t i = 0; i < line_infos->size(); i++) {
    lines.insert(lines.end(), line_infos[i].begin(), line_infos[i].end());
  }

  size_t num_lines = lines.size();

  dst.resize(num_lines);

  std::vector<std::thread> workers;
  std::atomic<size_t> count{0};
  const char *addr = src.data();

  for (uint32_t t = 0; t < num_threads; t++) {
    workers.emplace_back(std::thread([&]() {

      size_t k = 0;
      while ((k = count++) < num_lines) {

        std::vector<jagger::PyToken> toks;
        toks = _tagger->tokenize_line(addr + lines[k].pos, lines[k].len);

        dst[k] = std::move(toks);

      }
    }));
  }

  for (auto &worker : workers) {
    worker.join();
  }

  return dst;
}

}  // namespace pyjagger

PYBIND11_MODULE(jagger_ext, m) {
  m.doc() = "Python binding for Jagger.";

  // Add Ext prefix to avoid name conflict of 'Jagger' class in Python
  // world(defined in `jagger/__init__.py`)
  py::class_<pyjagger::PyJagger>(m, "JaggerExt")
      .def(py::init<>())
      .def(py::init<std::string>())
      .def("load_model", &pyjagger::PyJagger::load_model)
      .def("tokenize", &pyjagger::PyJagger::tokenize)
      .def("tokenize_batch", &pyjagger::PyJagger::tokenize_batch)
      .def("set_threads", &pyjagger::PyJagger::set_threads);

  py::class_<jagger::PyToken>(m, "Token")
      .def(py::init<>())
      .def("surface", &jagger::PyToken::surface)
      .def("feature", &jagger::PyToken::feature)
      .def("n_tags", &jagger::PyToken::n_tags)
      .def("tag", &jagger::PyToken::tag)
      .def("set_quote_char", &jagger::PyToken::set_quote_char)
      .def("__repr__", &jagger::PyToken::str)
      ;

}
