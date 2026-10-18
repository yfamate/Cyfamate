
#ifndef CLASSIC_NNUE_COMMON_H_INCLUDED
#define CLASSIC_NNUE_COMMON_H_INCLUDED

#include <algorithm>   
#include <cassert>
#include <cstring>		

#include "../../config.h"

#if defined(EVAL_NNUE)

#include "../../misc.h"  

#if defined(USE_WASM_SIMD)
#include "./wasm_simd.h"
#endif

namespace YaneuraOu {



#if defined(USE_AVX2)
#if defined(__GNUC__ ) && (__GNUC__ < 9) && defined(_WIN32) && !defined(__clang__)
#define _mm256_loadA_si256  _mm256_loadu_si256
#define _mm256_storeA_si256 _mm256_storeu_si256
#else
#define _mm256_loadA_si256  _mm256_load_si256
#define _mm256_storeA_si256 _mm256_store_si256
#endif
#endif

#if defined(USE_AVX512)
#if defined(__GNUC__ ) && (__GNUC__ < 9) && defined(_WIN32) && !defined(__clang__)
#define _mm512_loadA_si512   _mm512_loadu_si512
#define _mm512_storeA_si512  _mm512_storeu_si512
#else
#define _mm512_loadA_si512   _mm512_load_si512
#define _mm512_storeA_si512  _mm512_store_si512
#endif
#endif

namespace Eval::NNUE {

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  constexpr bool IsLittleEndian = true;
#else
  constexpr bool IsLittleEndian = false;
#endif

  constexpr std::uint32_t kVersion = 0x7AF32F16u;

  constexpr const char        Leb128MagicString[] = "COMPRESSED_LEB128";
  constexpr const std::size_t Leb128MagicStringSize = sizeof(Leb128MagicString) - 1;

  extern int FV_SCALE; 

  constexpr int kWeightScaleBits = 6;

  constexpr std::size_t kCacheLineSize = 64;

  #if defined(USE_AVX2)
  constexpr std::size_t kSimdWidth = 32;
  #elif defined(USE_SSE2)
  constexpr std::size_t kSimdWidth = 16;
  #elif defined(USE_MMX)
  constexpr std::size_t kSimdWidth = 8;

  #elif defined(USE_NEON)
  constexpr std::size_t kSimdWidth = 16;
  #elif defined(USE_WASM_SIMD)
  constexpr std::size_t kSimdWidth = 16;
  #endif
  constexpr std::size_t kMaxSimdWidth = 32;

  #if 0 
  enum {
    PS_NONE     =  0,
    PS_W_PAWN   =  1,
    PS_B_PAWN   =  1 * SQUARE_NB + 1,
    PS_W_KNIGHT =  2 * SQUARE_NB + 1,
    PS_B_KNIGHT =  3 * SQUARE_NB + 1,
    PS_W_BISHOP =  4 * SQUARE_NB + 1,
    PS_B_BISHOP =  5 * SQUARE_NB + 1,
    PS_W_ROOK   =  6 * SQUARE_NB + 1,
    PS_B_ROOK   =  7 * SQUARE_NB + 1,
    PS_W_QUEEN  =  8 * SQUARE_NB + 1,
    PS_B_QUEEN  =  9 * SQUARE_NB + 1,
    PS_W_KING   = 10 * SQUARE_NB + 1,
    PS_END      = PS_W_KING, 
    PS_B_KING   = 11 * SQUARE_NB + 1,
    PS_END2     = 12 * SQUARE_NB + 1
  };
  extern const uint32_t kpp_board_index[PIECE_NB][COLOR_NB];

  #endif

  using TransformedFeatureType = std::uint8_t;

  using IndexType = std::uint32_t;

  template <typename IntType>
  constexpr IntType CeilToMultiple(IntType n, IntType base) {
    return (n + base - 1) / base * base;
  }

  template <typename IntType>
  inline IntType read_little_endian(std::istream& stream) {

      IntType result;
      std::uint8_t u[sizeof(IntType)];
      typename std::make_unsigned<IntType>::type v = 0;

      stream.read(reinterpret_cast<char*>(u), sizeof(IntType));
      for (std::size_t i = 0; i < sizeof(IntType); ++i)
          v = (v << 8) | u[sizeof(IntType) - i - 1];

      std::memcpy(&result, &v, sizeof(IntType));
      return result;
  }

  template <typename IntType>
  inline void read_little_endian(std::istream& stream, IntType* out, std::size_t count) {
	  if (IsLittleEndian)
		  stream.read(reinterpret_cast<char*>(out), sizeof(IntType) * count);
	  else
		  for (std::size_t i = 0; i < count; ++i)
			  out[i] = read_little_endian<IntType>(stream);
  }

  template<typename IntType>
  inline void read_leb_128(std::istream& stream, IntType* out, std::size_t count) {

	  char leb128MagicString[Leb128MagicStringSize];
	  stream.read(leb128MagicString, Leb128MagicStringSize);
	  assert(strncmp(Leb128MagicString, leb128MagicString, Leb128MagicStringSize) == 0);

	  static_assert(std::is_signed_v<IntType>, "Not implemented for unsigned types");

	  const std::uint32_t BUF_SIZE = 4096;
	  std::uint8_t        buf[BUF_SIZE];

	  auto bytes_left = read_little_endian<std::uint32_t>(stream);

	  std::uint32_t buf_pos = BUF_SIZE;
	  for (std::size_t i = 0; i < count; ++i)
	  {
		  IntType result = 0;
		  size_t  shift = 0;
		  do
		  {
			  if (buf_pos == BUF_SIZE)
			  {
				  stream.read(reinterpret_cast<char*>(buf), std::min(bytes_left, BUF_SIZE));
				  buf_pos = 0;
			  }

			  std::uint8_t byte = buf[buf_pos++];
			  --bytes_left;
			  result |= (byte & 0x7f) << shift;
			  shift += 7;

			  if ((byte & 0x80) == 0)
			  {
				  out[i] = (sizeof(IntType) * 8 <= shift || (byte & 0x40) == 0)
					  ? result
					  : result | ~((1 << shift) - 1);
				  break;
			  }
		  } while (shift < sizeof(IntType) * 8);
	  }

	  assert(bytes_left == 0);
  }
} 
} 

#endif  

#endif 
