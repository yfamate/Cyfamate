
#ifndef _NNUE_FEATURES_HALF_KA_HM1_H_
#define _NNUE_FEATURES_HALF_KA_HM1_H_

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

	template <Side AssociatedKing>
	class HalfKA_hm1 {
	public:
		static constexpr const char* kName =
			(AssociatedKing == Side::kFriend) ? "HalfKA_hm1(Friend)" : "HalfKA_hm(Enemy)";
		static constexpr std::uint32_t kHashValue =
			0x7f134cb9u ^ (AssociatedKing == Side::kFriend);
		static constexpr IndexType kDimensions =
			5 * static_cast<IndexType>(FILE_NB) * static_cast<IndexType>(fe_end2);
		static constexpr IndexType kMaxActiveDimensions = PIECE_NUMBER_NB;
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
