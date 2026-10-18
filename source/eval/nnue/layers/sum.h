
#ifndef CLASSIC_NNUE_LAYERS_SUM_H
#define CLASSIC_NNUE_LAYERS_SUM_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../nnue_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Layers {

template <typename FirstPreviousLayer, typename... RemainingPreviousLayers>
class Sum : public Sum<RemainingPreviousLayers...> {
 private:
  using Head = FirstPreviousLayer;
  using Tail = Sum<RemainingPreviousLayers...>;

 public:
  using InputType = typename Head::OutputType;
  using OutputType = InputType;
  static_assert(std::is_same<InputType, typename Tail::InputType>::value, "");

  static constexpr IndexType kInputDimensions = Head::kOutputDimensions;
  static constexpr IndexType kOutputDimensions = kInputDimensions;
  static_assert(kInputDimensions == Tail::kInputDimensions , "");

  static constexpr std::size_t kSelfBufferSize =
      CeilToMultiple(kOutputDimensions * sizeof(OutputType), kCacheLineSize);

  static constexpr std::size_t kBufferSize =
      std::max(Head::kBufferSize + kSelfBufferSize, Tail::kBufferSize);

  static constexpr std::uint32_t GetHashValue() {
    std::uint32_t hash_value = 0xBCE400B4u;
    hash_value ^= Head::GetHashValue() >> 1;
    hash_value ^= Head::GetHashValue() << 31;
    hash_value ^= Tail::GetHashValue() >> 2;
    hash_value ^= Tail::GetHashValue() << 30;
    return hash_value;
  }

  static std::string GetStructureString() {
    return "Sum[" +
        std::to_string(kOutputDimensions) + "](" + GetSummandsString() + ")";
  }

  Tools::Result ReadParameters(std::istream& stream) {
    auto result = Tail::ReadParameters(stream);
	if (result.is_not_ok()) return result;
    return previous_layer_.ReadParameters(stream);
  }

  bool WriteParameters(std::ostream& stream) const {
    if (!Tail::WriteParameters(stream)) return false;
    return previous_layer_.WriteParameters(stream);
  }

  const OutputType* Propagate(
      const TransformedFeatureType* transformed_features, char* buffer) const {
    Tail::Propagate(transformed_features, buffer);
    const auto head_output = previous_layer_.Propagate(
        transformed_features, buffer + kSelfBufferSize);
    const auto output = reinterpret_cast<OutputType*>(buffer);
    for (IndexType i = 0; i < kOutputDimensions; ++i) {
      output[i] += head_output[i];
    }
    return output;
  }

 protected:
  static std::string GetSummandsString() {
    return Head::GetStructureString() + "," + Tail::GetSummandsString();
  }

  FirstPreviousLayer previous_layer_;
};

template <typename PreviousLayer>
class Sum<PreviousLayer> {
 public:
  using InputType = typename PreviousLayer::OutputType;
  using OutputType = InputType;

  static constexpr IndexType kInputDimensions =
      PreviousLayer::kOutputDimensions;
  static constexpr IndexType kOutputDimensions = kInputDimensions;

  static constexpr std::size_t kBufferSize = PreviousLayer::kBufferSize;

  static constexpr std::uint32_t GetHashValue() {
    std::uint32_t hash_value = 0xBCE400B4u;
    hash_value ^= PreviousLayer::GetHashValue() >> 1;
    hash_value ^= PreviousLayer::GetHashValue() << 31;
    return hash_value;
  }

  static std::string GetStructureString() {
    return "Sum[" +
        std::to_string(kOutputDimensions) + "](" + GetSummandsString() + ")";
  }

  Tools::Result ReadParameters(std::istream& stream) {
    return previous_layer_.ReadParameters(stream);
  }

  bool WriteParameters(std::ostream& stream) const {
    return previous_layer_.WriteParameters(stream);
  }

  const OutputType* Propagate(
      const TransformedFeatureType* transformed_features, char* buffer) const {
    return previous_layer_.Propagate(transformed_features, buffer);
  }

 protected:
  static std::string GetSummandsString() {
    return PreviousLayer::GetStructureString();
  }

  PreviousLayer previous_layer_;
};

} 
} 

#endif  

#endif
