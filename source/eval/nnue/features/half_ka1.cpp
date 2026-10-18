
#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "half_ka1.h"
#include "index_list.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

	template <Side AssociatedKing>
	inline IndexType HalfKA1<AssociatedKing>::MakeIndex(Square sq_k, BonaPiece p) {
		return static_cast<IndexType>(fe_end2) * static_cast<IndexType>(sq_k) + static_cast<IndexType>(p);
	}

	template <Side AssociatedKing>
	inline void HalfKA1<AssociatedKing>::GetPieces(
		const Position& pos, Color perspective,
		BonaPiece** pieces, Square* sq_target_k) {
		*pieces = (perspective == BLACK) ?
			pos.eval_list()->piece_list_fb() :
			pos.eval_list()->piece_list_fw();
		const PieceNumber target = (AssociatedKing == Side::kFriend) ?
			static_cast<PieceNumber>(PIECE_NUMBER_KING + perspective) :
			static_cast<PieceNumber>(PIECE_NUMBER_KING + ~perspective);
		*sq_target_k = static_cast<Square>(((*pieces)[target] - f_king) % SQ_NB);
	}

	template <Side AssociatedKing>
	void HalfKA1<AssociatedKing>::AppendActiveIndices(
		const Position& pos, Color perspective, IndexList* active) {
		if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

		BonaPiece* pieces;
		Square sq_target_k;
		GetPieces(pos, perspective, &pieces, &sq_target_k);
		for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i) {
			active->push_back(MakeIndex(sq_target_k, pieces[i]));
		}
	}

	template <Side AssociatedKing>
	void HalfKA1<AssociatedKing>::AppendChangedIndices(
		const Position& pos, Color perspective,
		IndexList* removed, IndexList* added) {
		BonaPiece* pieces;
		Square sq_target_k;
		GetPieces(pos, perspective, &pieces, &sq_target_k);
		const auto& dp = pos.state()->dirtyPiece;
		for (int i = 0; i < dp.dirty_num; ++i) {
			const auto old_p = static_cast<BonaPiece>(
				dp.changed_piece[i].old_piece.from[perspective]);
			removed->push_back(MakeIndex(sq_target_k, old_p));
			const auto new_p = static_cast<BonaPiece>(
				dp.changed_piece[i].new_piece.from[perspective]);
			added->push_back(MakeIndex(sq_target_k, new_p));
		}
	}

	template class HalfKA1<Side::kFriend>;
	template class HalfKA1<Side::kEnemy>;

}  
}  

#endif  
