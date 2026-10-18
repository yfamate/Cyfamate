
#ifndef CLASSIC_NNUE_FEATURES_HALF_KPE9_H
#define CLASSIC_NNUE_FEATURES_HALF_KPE9_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {


template <Side AssociatedKing>
class HalfKPE9 {
 public:
  static constexpr const char* kName =
      (AssociatedKing == Side::kFriend) ? "HalfKPE9(Friend)" : "HalfKPE9(Enemy)";
  static constexpr std::uint32_t kHashValue =
      0x5D69D5B9u ^ (AssociatedKing == Side::kFriend);
  static constexpr IndexType kDimensions =
      static_cast<IndexType>(SQ_NB) * static_cast<IndexType>(fe_end) * 3 * 3;
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_KING;

  static constexpr TriggerEvent kRefreshTrigger =
      (AssociatedKing == Side::kFriend) ?
      TriggerEvent::kFriendKingMoved : TriggerEvent::kEnemyKingMoved;

  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);

  static IndexType MakeIndex(Square sq_k, BonaPiece p, int effect1, int effect2);

 private:
  static void GetPieces(const Position& pos, Color perspective,
                        BonaPiece** pieces, Square* sq_target_k);
};

} 
} 

#endif  

#endif
