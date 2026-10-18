
#ifndef CLASSIC_NNUE_LAYERS_INPUT_SLICE_H_INCLUDED
#define CLASSIC_NNUE_LAYERS_INPUT_SLICE_H_INCLUDED

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../nnue_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Layers {

template <IndexType OutputDimensions, IndexType Offset = 0>
class InputSlice {
 public:
  static_assert(Offset % kMaxSimdWidth == 0, "");

  using OutputType = TransformedFeatureType;

  static constexpr IndexType kOutputDimensions = OutputDimensions;

  static constexpr std::size_t kBufferSize = 0;

  static constexpr std::uint32_t GetHashValue() {
    std::uint32_t hash_value = 0xEC42E90Du;
    hash_value ^= kOutputDimensions ^ (Offset << 10);
    return hash_value;
  }

  static std::string GetStructureString() {
    return "InputSlice[" + std::to_string(kOutputDimensions) + "(" +
        std::to_string(Offset) + ":" +
        std::to_string(Offset + kOutputDimensions) + ")]";
  }

  Tools::Result ReadParameters(std::istream& ) {
    return Tools::ResultCode::Ok;
  }

  bool WriteParameters(std::ostream& ) const {
    return true;
  }

  const OutputType* Propagate(
      const TransformedFeatureType* transformed_features,
      char* ) const {
    return transformed_features + Offset;
  }

 private:
};

} 
} 

#endif  

#endif 
