
#ifndef CLASSIC_NNUE_FEATURES_PE9_H
#define CLASSIC_NNUE_FEATURES_PE9_H

#include "../../../config.h"

#if defined(EVAL_NNUE) && defined(LONG_EFFECT_LIBRARY) && defined(USE_BOARD_EFFECT_PREV)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

class PE9 {
 public:
  static constexpr const char* kName = "PE9";
  static constexpr std::uint32_t kHashValue = 0x764CFB4Bu;
  static constexpr IndexType kDimensions = static_cast<IndexType>(fe_end) * 3 * 3;
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_KING;

  static constexpr TriggerEvent kRefreshTrigger = TriggerEvent::kNone;

  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);

  static IndexType MakeIndex(BonaPiece p, int effect1, int effect2);

 private:
  static void GetPieces(const Position& pos, Color perspective,
                        BonaPiece** pieces);
};

} 
} 

#endif  

#endif
