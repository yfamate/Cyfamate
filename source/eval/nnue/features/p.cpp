
#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "p.h"
#include "index_list.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

void P::AppendActiveIndices(
    const Position& pos, Color perspective, IndexList* active) {
  if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

  const BonaPiece* pieces = (perspective == BLACK) ?
      pos.eval_list()->piece_list_fb() :
      pos.eval_list()->piece_list_fw();
  for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_KING; ++i) {
    active->push_back(pieces[i]);
  }
}

void P::AppendChangedIndices(
    const Position& pos, Color perspective,
    IndexList* removed, IndexList* added) {
  const auto& dp = pos.state()->dirtyPiece;
  for (int i = 0; i < dp.dirty_num; ++i) {
    if (dp.pieceNo[i] >= PIECE_NUMBER_KING) continue;
    removed->push_back(dp.changed_piece[i].old_piece.from[perspective]);
    added->push_back(dp.changed_piece[i].new_piece.from[perspective]);
  }
}

} 
} 

#endif  
