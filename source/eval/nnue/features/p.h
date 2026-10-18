
#ifndef CLASSIC_NNUE_FEATURES_P_H
#define CLASSIC_NNUE_FEATURES_P_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

class P {
 public:
  static constexpr const char* kName = "P";
  static constexpr std::uint32_t kHashValue = 0x764CFB4Bu;
  static constexpr IndexType kDimensions = fe_end;
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_KING;
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
