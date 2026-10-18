
#ifndef CLASSIC_NNUE_FEATURES_A2_H
#define CLASSIC_NNUE_FEATURES_A2_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

class A2 {
 public:
  static constexpr const char* kName = "A2";
  static constexpr std::uint32_t kHashValue = 0xA20DCB9Bu;
  static constexpr IndexType kDimensions = e_king;
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_NB;
  static constexpr TriggerEvent kRefreshTrigger = TriggerEvent::kNone;

  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);
};

}  
}  

#endif  

#endif
