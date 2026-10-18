
#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "half_ka2.h"
#include "index_list.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

	template <Side AssociatedKing>
	inline IndexType HalfKA2<AssociatedKing>::MakeIndex(Square sq_k, BonaPiece p) {
		return static_cast<IndexType>(e_king) * static_cast<IndexType>(sq_k) + static_cast<IndexType>(p >= e_king ? p - SQ_NB : p);
	}

	inline constexpr auto kMirroredPieceOffset = [] {
		std::array<std::uint16_t, fe_end2> t{};
		for (int p = 0; p < int(fe_end2); ++p) {
			int q = p;
			if (p >= int(fe_hand_end))
				q = int(fe_hand_end) + (p - int(fe_hand_end)) / int(SQ_NB) * int(SQ_NB)
				  + int(Mir(static_cast<Square>((p - int(fe_hand_end)) % int(SQ_NB))));
			t[p] = static_cast<std::uint16_t>(q >= int(e_king) ? q - int(SQ_NB) : q);
		}
		return t;
	}();
	static_assert(int(SQ_61) == 5 * 9 && int(SQ_NB) == 81, "files 6-9 must be squares 45..80");
	static_assert(int(e_king) == int(fe_end2) - int(SQ_NB), "enemy-king plane must end the BonaPiece range");
#if defined(CYF_GP8)
	static_assert(Cyfamate::Gp8::kHmRows == 45 * int(e_king), "hm file = 45 king buckets x e_king rows");
#endif
	static_assert([] {
		bool seen[e_king] = {};
		for (int p = 0; p < int(e_king); ++p) {
			const int q = kMirroredPieceOffset[p];
			if (q < 0 || q >= int(e_king) || seen[q] || (p < int(fe_hand_end) && q != p)) return false;
			seen[q] = true;
		}
		for (int p = int(e_king); p < int(fe_end2); ++p)   
			if (kMirroredPieceOffset[p] != kMirroredPieceOffset[p - int(SQ_NB)]) return false;
		return true;
	}(), "kMirroredPieceOffset must permute [0, e_king) and fold the enemy-king plane");

	template <Side AssociatedKing>
	inline bool HalfKA2<AssociatedKing>::MirrorKing(Square& sq_k) {
#if defined(CYF_GP8)
		if (Cyfamate::Gp8::hm && sq_k >= SQ_61) {
			sq_k = Mir(sq_k);
			return true;
		}
#endif
		return false;
	}

	template <Side AssociatedKing>
	inline IndexType HalfKA2<AssociatedKing>::MakeIndex(Square sq_k, BonaPiece p, bool mirrored) {
		return mirrored ? static_cast<IndexType>(e_king) * static_cast<IndexType>(sq_k) + kMirroredPieceOffset[p]
		                : MakeIndex(sq_k, p);
	}

	template <Side AssociatedKing>
	inline void HalfKA2<AssociatedKing>::GetPieces(
		const Position& pos, Color perspective,
		BonaPiece** pieces, Square* sq_target_k) {
		*pieces = (perspective == BLACK) ?
			pos.eval_list()->piece_list_fb() :
			pos.eval_list()->piece_list_fw();
		const PieceNumber target = (AssociatedKing == Side::kFriend) ?
			static_cast<PieceNumber>(PIECE_NUMBER_KING + perspective) :
			static_cast<PieceNumber>(PIECE_NUMBER_KING + ~perspective);
		*sq_target_k = (*pieces)[target] == BONA_PIECE_ZERO ? SQ_NB
		    : static_cast<Square>(((*pieces)[target] - f_king) % SQ_NB);
	}

	template <Side AssociatedKing>
	void HalfKA2<AssociatedKing>::AppendActiveIndices(
		const Position& pos, Color perspective, IndexList* active) {
		if (RawFeatures::kMaxActiveDimensions < kMaxActiveDimensions) return;

		BonaPiece* pieces;
		Square sq_target_k;
		GetPieces(pos, perspective, &pieces, &sq_target_k);
		if (sq_target_k == SQ_NB) return;
		if (MirrorKing(sq_target_k)) {
			for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i)
				active->push_back(MakeIndex(sq_target_k, pieces[i], true));
			return;
		}
		for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i) {
			active->push_back(MakeIndex(sq_target_k, pieces[i]));
		}
	}

	template <Side AssociatedKing>
	void HalfKA2<AssociatedKing>::AppendChangedIndices(
		const Position& pos, Color perspective,
		IndexList* removed, IndexList* added) {
		BonaPiece* pieces;
		Square sq_target_k;
		GetPieces(pos, perspective, &pieces, &sq_target_k);
		if (sq_target_k == SQ_NB) return;
		const bool mirrored = MirrorKing(sq_target_k);
		const auto& dp = pos.state()->dirtyPiece;
		for (int i = 0; i < dp.dirty_num; ++i) {
			const auto old_p = static_cast<BonaPiece>(
				dp.changed_piece[i].old_piece.from[perspective]);
			removed->push_back(MakeIndex(sq_target_k, old_p, mirrored));
			const auto new_p = static_cast<BonaPiece>(
				dp.changed_piece[i].new_piece.from[perspective]);
			added->push_back(MakeIndex(sq_target_k, new_p, mirrored));
		}
	}

	template class HalfKA2<Side::kFriend>;
	template class HalfKA2<Side::kEnemy>;

}  
}  

#endif  
