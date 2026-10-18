
#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "k.h"
#include "index_list.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

void K::AppendActiveIndices(
    const Position& pos, Color perspective, IndexList* active) {
  if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

  const BonaPiece* pieces = (perspective == BLACK) ?
      pos.eval_list()->piece_list_fb() :
      pos.eval_list()->piece_list_fw();
  ASSERT_LV5(pieces[PIECE_NUMBER_BKING] != BONA_PIECE_ZERO);
  ASSERT_LV5(pieces[PIECE_NUMBER_WKING] != BONA_PIECE_ZERO);
  for (PieceNumber i = PIECE_NUMBER_KING; i < PIECE_NUMBER_NB; ++i) {
    active->push_back(pieces[i] - fe_end);
  }
}

void K::AppendChangedIndices(
    const Position& pos, Color perspective,
    IndexList* removed, IndexList* added) {
  const auto& dp = pos.state()->dirtyPiece;
  if (dp.pieceNo[0] >= PIECE_NUMBER_KING) {
    removed->push_back(
        dp.changed_piece[0].old_piece.from[perspective] - fe_end);
    added->push_back(
        dp.changed_piece[0].new_piece.from[perspective] - fe_end);
  }
}


} 
} 

#endif  
