// NNUE評価関数の差分計算用のクラス

#ifndef CLASSIC_NNUE_ACCUMULATOR_H_
#define CLASSIC_NNUE_ACCUMULATOR_H_
#if !defined(CYF_ARCH_K3K3)

#include "../../config.h"

#if defined(EVAL_NNUE)

#include "nnue_architecture.h"

namespace YaneuraOu {
namespace Eval::NNUE {

// 入力特徴量をアフィン変換した結果を保持するクラス
// 最終的な出力である評価値も一緒に持たせておく
// AVX-512命令を使用する場合に64bytesのアライメントが要求される。
#if defined(CYF_THREAT) && !defined(CYF_THREAT_NO_DERIVE)
// [threat-speed CYF_THREAT_DERIVE] The threat rows are king-independent. On an own-king move the threat part of the
// older accumulator is recovered exactly (combined - HalfKA2 part, modulo 2^16) with a HalfKA2-only Finny entry of
// the old king square, so the reset needs neither a full threat enumeration nor a diff against a stale Finny threat
// list, and ordinary updates are unchanged.
#define CYF_THREAT_DERIVE 1
#endif
#if defined(CYF_THREAT) && !defined(CYF_THREAT_NO_I8)
// [threat-speed CYF_THREAT_I8] threat rows stay int8 at the file scale (half the bytes); every kernel widens a
// row and doubles it (x + x), which is the int16 internal-scale row bit for bit.
#define CYF_THREAT_I8 1
#endif
struct alignas(64) Accumulator {
  std::int16_t
      accumulation[2][kRefreshTriggers.size()][kTransformedFeatureDimensions];
  Value score = VALUE_ZERO;
  bool computed_accumulation = false;
  bool computed_score = false;
};

} // namespace Eval::NNUE
} // namespace YaneuraOu

#endif  // defined(EVAL_NNUE)

#else

#include "../../config.h"

#if defined(EVAL_NNUE)

#include "nnue_architecture.h"

namespace YaneuraOu {
namespace Eval::NNUE {

// 入力特徴量をアフィン変換した結果を保持するクラス
// 最終的な出力である評価値も一緒に持たせておく
// AVX-512命令を使用する場合に64bytesのアライメントが要求される。
struct alignas(64) Accumulator {
  std::int16_t
      accumulation[2][kRefreshTriggers.size()][kTransformedFeatureDimensions];
  Value score = VALUE_ZERO;
  bool computed_accumulation = false;
  bool computed_score = false;
};

} // namespace Eval::NNUE
} // namespace YaneuraOu

#endif  // defined(EVAL_NNUE)

#endif
#endif
