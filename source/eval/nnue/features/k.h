
#ifndef CLASSIC_NNUE_FEATURES_K_H
#define CLASSIC_NNUE_FEATURES_K_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

class K {
 public:
  static constexpr const char* kName = "K";
  static constexpr std::uint32_t kHashValue = 0xD3CEE169u;

  static constexpr IndexType kDimensions = uint32_t(SQ_NB) * 2;

  static constexpr IndexType kMaxActiveDimensions = 2;
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
