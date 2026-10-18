
#ifndef CLASSIC_NNUE_FEATURES_HALF_RELATIVE_KP_H
#define CLASSIC_NNUE_FEATURES_HALF_RELATIVE_KP_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

template <Side AssociatedKing>
class HalfRelativeKP {
 public:
  static constexpr const char* kName = (AssociatedKing == Side::kFriend) ?
      "HalfRelativeKP(Friend)" : "HalfRelativeKP(Enemy)";
  static constexpr std::uint32_t kHashValue =
      0xF9180919u ^ (AssociatedKing == Side::kFriend);
  static constexpr IndexType kNumPieceKinds = (fe_end - fe_hand_end) / SQ_NB;
  static constexpr IndexType kBoardWidth = FILE_NB * 2 - 1;
  static constexpr IndexType kBoardHeight = RANK_NB * 2 - 1;
  static constexpr IndexType kDimensions =
      kNumPieceKinds * kBoardHeight * kBoardWidth;
  static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_KING;
  static constexpr TriggerEvent kRefreshTrigger =
      (AssociatedKing == Side::kFriend) ?
      TriggerEvent::kFriendKingMoved : TriggerEvent::kEnemyKingMoved;

  static void AppendActiveIndices(const Position& pos, Color perspective,
                                  IndexList* active);

  static void AppendChangedIndices(const Position& pos, Color perspective,
                                   IndexList* removed, IndexList* added);

  static IndexType MakeIndex(Square sq_k, BonaPiece p);

 private:
  static void GetPieces(const Position& pos, Color perspective,
                        BonaPiece** pieces, Square* sq_target_k);
};

} 
} 

#endif  

#endif
