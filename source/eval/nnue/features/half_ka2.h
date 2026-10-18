
#ifndef _NNUE_FEATURES_HALF_KA2_H_
#define _NNUE_FEATURES_HALF_KA2_H_

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

	template <Side AssociatedKing>
	class HalfKA2 {
	public:
		static constexpr const char* kName =
			(AssociatedKing == Side::kFriend) ? "HalfKA2(Friend)" : "HalfKA(Enemy)";
		static constexpr std::uint32_t kHashValue =
			0x5f234cb9u ^ (AssociatedKing == Side::kFriend);
		static constexpr IndexType kDimensions =
			static_cast<IndexType>(SQ_NB) * static_cast<IndexType>(e_king);
		static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_NB;
		static constexpr TriggerEvent kRefreshTrigger =
			(AssociatedKing == Side::kFriend) ?
			TriggerEvent::kFriendKingMoved : TriggerEvent::kEnemyKingMoved;

		static void AppendActiveIndices(const Position& pos, Color perspective,
			IndexList* active);

		static void AppendChangedIndices(const Position& pos, Color perspective,
			IndexList* removed, IndexList* added);

		static IndexType MakeIndex(Square sq_k, BonaPiece p);

		static bool MirrorKing(Square& sq_k);
		static IndexType MakeIndex(Square sq_k, BonaPiece p, bool mirrored);

	private:
		static void GetPieces(const Position& pos, Color perspective,
			BonaPiece** pieces, Square* sq_target_k);
	};

}  
}  

#endif  

#endif
