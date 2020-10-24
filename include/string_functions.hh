//
// Created by grakra on 2020/10/20.
//

#ifndef CPP_ETUDES_INCLUDE_STRING_FUNCTIONS_HH_
#define CPP_ETUDES_INCLUDE_STRING_FUNCTIONS_HH_

#include<string>
#include<climits>
#include<vector>
#include<cassert>
#include<random>
#include<iostream>
#include <immintrin.h>

class Slice;
class StringVector;

struct Slice {
  const char *data;
  const size_t size;
  std::string to_string() {
    return std::string(data, data + size);
  }
  const char *begin() { return data; }
  const char *end() { return data + size; }
};

struct StringVector {
  std::string blob;
  std::vector<int> offsets;
  StringVector() : blob({}), offsets({0}) {}
  size_t size() const {
    return offsets.size() - 1;
  }
  void append(std::string const &s) {
    blob.append(s);
    offsets.push_back(offsets.back() + s.size());
  }

  void append(const char *begin, const char *end) {
    blob.append(begin, end);
    offsets.push_back(offsets.back() + (end - begin));
  }

  Slice get_slice(int i) const {
    return {
        .data = blob.data() + offsets[i],
        .size = static_cast<size_t>(offsets[i + 1] - offsets[i])
    };
  }

  Slice get_last_slice() {
    return get_slice(size() - 1);
  }

  std::vector<std::string> to_vector() {
    std::vector<std::string> ss;
    ss.reserve(size());
    for (auto i = 0; i < size(); ++i) {
      ss.push_back(get_slice(i).to_string());
    }
    return ss;
  }
};
static uint8_t *create_utf8_length_table() {
  uint8_t *tbl = new uint8_t[257];
  for (int byte = 0; byte < 257; ++byte) {
    if (byte >= 0xFC) {
      tbl[byte] = 6;
    } else if (byte >= 0xF8) {
      tbl[byte] = 5;
    } else if (byte >= 0xF0) {
      tbl[byte] = 4;
    } else if (byte >= 0xE0) {
      tbl[byte] = 3;
    } else if (byte >= 0xC0) {
      tbl[byte] = 2;
    } else {
      tbl[byte] = 1;
    }
  }

  return tbl;
}

static uint8_t *create_utf8_length_table2() {
  struct uint8_array {
    alignas(128) uint8_t tbl[64];
  };
  uint8_array *data = new uint8_array;
  uint8_t *tbl = data->tbl;
  for (int byte = 0; byte < 64; ++byte) {
    if (byte >= 0b1111'0) {
      tbl[byte] = 4;
    } else if (byte >= 0b1110'0) {
      tbl[byte] = 3;
    } else if (byte >= 0b1100'0) {
      tbl[byte] = 2;
    } else {
      tbl[byte] = 1;
    }
  }
  return tbl;
}
// SIZE: 257 * uint8_t
static const uint8_t *UTF8_BYTE_LENGTH_TABLE = create_utf8_length_table();
static const uint8_t *UTF8_BYTE_LENGTH_TABLE2 = create_utf8_length_table2();

struct StringFunctions {

  static inline int utf8_length(std::string const &str) {
    int len = 0;
    for (int i = 0, char_size = 0; i < str.size(); i += char_size) {
      char_size = UTF8_BYTE_LENGTH_TABLE[static_cast<unsigned char>(str.data()[i])];
      ++len;
    }
    return len;
  }

  static inline int utf8_length2(std::string const &str) {
    int len = 0;
    for (int i = 0, char_size = 0; i < str.size(); i += char_size) {
      char_size = UTF8_BYTE_LENGTH_TABLE2[static_cast<unsigned char>(str.data()[i]) >> 3];
      ++len;
    }
    return len;
  }

  static inline int utf8_length3(std::string const &str) {
    int len = 0;
    for (int i = 0, char_size = 0; i < str.size(); i += char_size) {
      uint8_t c8 = ~static_cast<unsigned char>(str.data()[i]);
      int c = c8;
      int k = __builtin_clz(c);
      if (k == 28) {
        char_size = 4;
      } else if (k == 27) {
        char_size = 3;
      } else if (k == 26) {
        char_size = 2;
      } else {
        char_size = 1;
      }
      ++len;
    }
    return len;
  }

  static inline int utf8_length4(std::string const &str) {
    int len = 0;
    const char *data = str.data();
    uintptr_t p_start = reinterpret_cast<uintptr_t>(data);
    uintptr_t p_end = p_start + str.size();

    int skip_start = static_cast<int>(p_start & 7);
    p_start = p_start & ~7;

    int limit_end = static_cast<int>(p_end & 7);
    p_end = p_end & ~7;

    int char_size = 0;
    int k = skip_start;
    uintptr_t p = p_start;
    for (; p < p_end; p += 8) {
      register uint64_t oct asm("r12") = *reinterpret_cast<uint64_t *>(p);
      do {
        k &= 7;
        oct >>= k << 3;
        uint8_t c = static_cast<uint8_t>(oct ^ 0xff);
        int zeros = __builtin_clz(c);

        if (zeros == 28) {
          char_size = 4;
        } else if (zeros == 27) {
          char_size = 3;
        } else if (zeros == 26) {
          char_size = 2;
        } else {
          char_size = 1;
        }
        ++len;
        k = k + char_size;
      } while (k < 8);
    }

    if (limit_end == 0 || p_start == p_end) {
      return len;
    }

    uint64_t oct = *reinterpret_cast<uint64_t *>(p_end);
    do {
      oct >>= k << 3;
      uint8_t c = static_cast<uint8_t>(oct ^ 0xff);
      int zeros = __builtin_clz(c);

      if (zeros == 28) {
        char_size = 4;
      } else if (zeros == 27) {
        char_size = 3;
      } else if (zeros == 26) {
        char_size = 2;
      } else {
        char_size = 1;
      }
      ++len;
      k = k + char_size;
    } while (k < limit_end);
    return len;
  }

  static inline int utf8_length_simd_avx2(std::string const &str) {
    int len = 0;
    const char *p = str.data();
    const char *end = p + str.size();
#if defined(__AVX2__)
    constexpr auto bytes_avx2 = sizeof(__m256i);
    const auto src_end_avx2 = p + (str.size() & ~(bytes_avx2 - 1));
    const auto threshold = _mm256_set1_epi8(0b1011'1111);
    for (; p < src_end_avx2; p += bytes_avx2)
      len += __builtin_popcount(
          _mm256_movemask_epi8(
              _mm256_cmpgt_epi8(
                  _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)), threshold)));
#elif defined(__SSE2__)
    constexpr auto bytes_sse2 = sizeof(__m128i);
    const auto src_end_sse2 = p + (str.size() & ~(bytes_sse2 - 1));
    const auto threshold = _mm_set1_epi8(0b1011'1111);
    for (; p < src_end_sse2; p += bytes_sse2) {
      len += __builtin_popcount(
          _mm_movemask_epi8(
              _mm_cmpgt_epi8(
                  _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)), threshold)));
    }
#endif
    for (; p < end; ++p) {
      len += static_cast<int8_t>(*p) > static_cast<int8_t>(0xbf);
    }
    return len;
  }

  static inline int utf8_length_simd_sse2(std::string const &str) {
    int len = 0;
    const char *p = str.data();
    const char *end = p + str.size();
#if defined(__SSE2__)
    constexpr auto bytes_sse2 = sizeof(__m128i);
    const auto src_end_sse2 = p + (str.size() & ~(bytes_sse2 - 1));
    const auto threshold = _mm_set1_epi8(0b1011'1111);
    for (; p < src_end_sse2; p += bytes_sse2) {
      len += __builtin_popcount(
          _mm_movemask_epi8(
              _mm_cmpgt_epi8(
                  _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)), threshold)));
    }
#endif
    for (; p < end; ++p) {
      len += static_cast<int8_t>(*p) > static_cast<int8_t>(0xbf);
    }
    return len;
  }

  static constexpr uint8_t UTF8_FIRST_CHAR[7] = {
      0b0000'0000,
      0b1000'0000,
      0b1100'0000,
      0b1110'0000,
      0b1111'0000,
      0b1111'1000,
      0b1111'1100,
  };

  static inline bool validate_ascii_fast(const char *src, size_t len) {
#ifdef __AVX2__
    size_t i = 0;
    __m256i has_error = _mm256_setzero_si256();
    if (len >= 32) {
      for (; i <= len - 32; i += 32) {
        __m256i current_bytes = _mm256_loadu_si256((const __m256i *) (src + i));
        has_error = _mm256_or_si256(has_error, current_bytes);
      }
    }
    int error_mask = _mm256_movemask_epi8(has_error);

    char tail_has_error = 0;
    for (; i < len; i++) {
      tail_has_error |= src[i];
    }
    error_mask |= (tail_has_error & 0x80);

    return !error_mask;
#elif defined(__SSE2__)
    size_t i = 0;
    __m128i has_error = _mm_setzero_si128();
    if (len >= 16) {
        for (; i <= len - 16; i += 16) {
            __m128i current_bytes = _mm_loadu_si128((const __m128i*)(src + i));
            has_error = _mm_or_si128(has_error, current_bytes);
        }
    }
    int error_mask = _mm_movemask_epi8(has_error);

    char tail_has_error = 0;
    for (; i < len; i++) {
        tail_has_error |= src[i];
    }
    error_mask |= (tail_has_error & 0x80);

    return !error_mask;
#else
    char tail_has_error = 0;
    for (size_t i = 0; i < len; i++) {
        tail_has_error |= src[i];
    }
    return !(tail_has_error & 0x80);
#endif
  }

  template<bool use_length>
  static inline void ascii_substr(StringVector const &src, StringVector &dst, int offset, int len) {
    const auto n = src.size();
    for (auto i = 0; i < n; ++i) {
      Slice s = src.get_slice(i);
      auto from_pos = offset;
      // negative offset, count bytes from right side;
      if (from_pos < 0) {
        from_pos = from_pos + s.size;
      }
      // pos should between [0,s.size);
      if (from_pos < 0 || from_pos >= s.size) {
        from_pos = s.size;
      }

      // set end to s.size when end exceeds the string tail.
      auto to_pos = size_t(0);
      if constexpr (use_length) {
        to_pos = from_pos + len;
        if (to_pos > s.size) {
          to_pos = s.size;
        }
      } else {
        to_pos = s.size;
      }
      dst.append(s.data + from_pos, s.data + to_pos);
    }
  }

  static inline const char *skip_leading_utf8(const char *p, const char *end, size_t n) {
    size_t char_size = 0;
    for (auto i = 0; i < n && p < end; ++i, p += char_size) {
      uint8_t b = static_cast<uint8_t>(*p);
      if (b >> 7 == 0) {
        char_size = 1;
      } else {
        char_size = __builtin_clz(b) - 24;
      }
    }
    return p;
  }

  static inline const char *skip_trailing_utf8(const char *p, const char *begin, size_t n) {
    constexpr auto threshold = static_cast<int8_t>(0b1011'1111);
    for (auto i = 0; i < n && p > begin; ++i) {
      --p;
      while (p > begin && static_cast<int8_t>(*p) <= threshold)--p;
    }
    return p;
  }

  template<bool use_length>
  static inline void utf8_substr_from_left(StringVector const &src,
                                           StringVector &dst,
                                           int offset,
                                           [[maybe_unused]]int len) {
    auto const size = src.size();
    for (auto i = 0; i < size; ++i) {
      auto s = src.get_slice(i);
      auto begin = s.begin();
      auto end = s.end();
      auto from_ptr = skip_leading_utf8(begin, end, offset);
      if constexpr (use_length) {
        if (from_ptr >= end) {
          dst.append("");
        } else {
          auto to_ptr = skip_leading_utf8(from_ptr, end, len);
          dst.append(from_ptr, to_ptr);
        }
      } else {
        dst.append(from_ptr, end);
      }
    }
  }

  template<bool use_length>
  static inline void utf8_substr_from_right(StringVector const &src,
                                            StringVector &dst,
                                            int offset,
                                            [[maybe_unused]]int len) {
    const auto size = src.size();
    for (auto i = 0; i < size; ++i) {
      auto s = src.get_slice(i);
      auto begin = s.begin();
      auto end = s.end();

      if (offset > s.size) {
        continue;
      }

      auto from_ptr = skip_trailing_utf8(end, begin, offset);

      if (from_ptr < begin) {
        continue;
      }

      if constexpr(use_length) {
        std::cout<<"end-from_ptr="<<end-from_ptr<<", len="<<len<<std::endl;
        if (len > end - from_ptr) {
          dst.append(from_ptr, end);
        } else {
          auto to_ptr = skip_leading_utf8(from_ptr, end, len);
          std::cout << "to_ptr=" << to_ptr - begin << std::endl;
          dst.append(from_ptr, to_ptr);
        }
      } else {
        dst.append(from_ptr, end);
      }
    }
  }

  template<bool check_ascii, bool use_length>
  static inline void substr(StringVector const &src, StringVector &dst, int offset, [[maybe_unused]] int len) {
    if (offset == 0) {
      return;
    } else if (offset > 0) {
      offset -= 1;
    }
    if constexpr(check_ascii) {
      auto is_ascii = validate_ascii_fast(src.blob.data(), src.blob.size());
      if (is_ascii) {
        ascii_substr<use_length>(src, dst, offset, len);
      } else {
        substr<false, use_length>(src, dst, offset, len);
      }
    } else {
      if (offset >= 0) {
        utf8_substr_from_left<use_length>(src, dst, offset, len);
      } else if (offset < 0) {
        utf8_substr_from_right<use_length>(src, dst, -offset, len);
      }
    }
  }

  static std::string gen_utf8(std::vector<int> const &weights, int n1, int n2) {
    assert(weights.size() == 6);
    assert(n1 >= 0 && n2 >= 0 && n1 <= n2);

    assert(std::all_of(weights.begin(), weights.end(), [](auto i) {
      return i >= 0;
    }));

    assert(std::any_of(weights.begin(), weights.end(), [](auto i) {
      return i > 0;
    }));

    if (n2 == 0) {
      return std::string();
    }
    std::vector<int> pdf(weights.size() + 1, 0);
    pdf[0] = 0;
    for (auto i = 1; i < pdf.size(); ++i) {
      pdf[i] = pdf[i - 1] + weights[i - 1];
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> rand_n(n1, n2);
    std::uniform_int_distribution<int> rand_pdf(1, pdf.back());
    std::uniform_int_distribution<uint8_t> rand_char(0, 255);
    int n = rand_n(gen);
    std::string text(n, '\0');
    auto p = text.begin();
    while (p < text.end()) {
      auto sample = rand_pdf(gen);
      int char_bytes = 0;
      for (size_t i = 1; i < pdf.size(); ++i) {
        if (pdf[i - 1] < sample && sample <= pdf[i]) {
          char_bytes = i;
          break;
        }
      }

      assert(0 <= char_bytes && char_bytes < pdf.size());
      char_bytes = std::min(char_bytes, static_cast<int>(text.end() - p));
      if (char_bytes == 1) {
        *p = rand_char(gen) & 0b0111'1111;
        ++p;
      } else {
        *p = (rand_char(gen) >> (char_bytes + 1)) | UTF8_FIRST_CHAR[char_bytes];
        ++p;
        while (--char_bytes > 0) {
          *p = (rand_char(gen) >> 2) | 0b1000'0000;
          ++p;
        }
      }
    }
    return text;
  }

  static inline std::vector<std::string> gen_utf8_vector(std::vector<int> const &weights, int size, int n1, int n2) {
    assert(size > 0);
    std::vector<std::string> batch;
    batch.resize(size);
    for (auto i = 0; i < size; ++i) {
      batch[i] = gen_utf8(weights, n1, n2);
    }
    return batch;
  }

  static inline int utf8_char_bytes(uint8_t c) {
    return UTF8_BYTE_LENGTH_TABLE[c];
  }
  static inline void stat_utf8(std::vector<std::string> batch) {
    std::vector<int> stat(7, 0);
    for (auto &text: batch) {
      if (text.empty()) {
        continue;
      }
      auto p = text.begin();
      while (p < text.end()) {
        int len = utf8_char_bytes(*p);
        ++stat[len];
        p += len;
      }
    }
    for (int i = 1; i < stat.size(); ++i) {
      std::cout << "utf8-" << i << "byte: " << stat[i] << std::endl;
    }
  }

};

int env_to_int(std::string const &name, std::string const &default_value) {
  const char *val = getenv(name.c_str());
  if (val == nullptr) {
    val = default_value.c_str();
  }
  auto n = static_cast<int>(strtoul(val, nullptr, 10));
  std::cerr << "ENV " << name << "=" << n << std::endl;
  return n;
}

std::vector<int> env_to_csv_int(std::string const &name, std::string const &default_value) {
  const char *val = getenv(name.c_str());
  if (val == nullptr) {
    val = default_value.c_str();
  }
  std::string s(val);
  std::string::size_type start = 0;
  std::vector<int> csv;
  while (true) {
    auto end = s.find(',', start);
    if (end == std::string::npos) {
      auto subs = s.substr(start, s.size() - start);
      auto n = static_cast<int>(strtoul(subs.c_str(), nullptr, 10));
      csv.push_back(n);
      break;
    } else {
      auto subs = s.substr(start, end - start);
      auto n = static_cast<int>(strtoul(subs.c_str(), nullptr, 10));
      csv.push_back(n);
      start = end + 1;
    }
  }
  std::cerr << "ENV " << name << "=";
  if (csv.empty()) {
    return csv;
  }
  auto it = csv.begin();
  std::cerr << *it;
  while (++it != csv.end()) {
    std::cerr << "," << *it;
  }
  std::cerr << std::endl;
  return csv;
}

struct prepare_utf8_data {
  int vector_size = 8192;
  std::vector<int> weights{10, 10, 4, 3, 0, 0};
  int min_length = 100;
  int max_length = 1000;
  std::vector<std::string> data;
  StringVector string_data;
  std::vector<int> result;
  prepare_utf8_data() {
    vector_size = env_to_int("VECTOR_SIZE", "8192");
    weights = std::move(env_to_csv_int("WEIGHTS", "10,10,4,3,0,0"));
    min_length = env_to_int("MIN_LENGTH", "100");
    max_length = env_to_int("MIN_LENGTH", "1000");
    data = std::move(StringFunctions::gen_utf8_vector(weights, vector_size, min_length, max_length));

    for (auto &s: data) {
      string_data.append(s);
    }

    std::cerr << "generate " << data.size() << " strings: " << std::endl;
    StringFunctions::stat_utf8(data);
    result.resize(data.size());
  }
};

template<typename T, typename U, typename F, typename... Args>
void apply_vector(std::vector<T> &data, std::vector<U> &result, F f, Args &&... args) {
  for (auto i = 0; i < data.size(); ++i) {
    auto &e = data[i];
    result[i] = f(e, std::forward<Args>(args)...);
  }
}

#endif //CPP_ETUDES_INCLUDE_STRING_FUNCTIONS_HH_
