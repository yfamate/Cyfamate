
#ifndef CLASSIC_NNUE_FEATURES_HALF_KP_VM_H
#define CLASSIC_NNUE_FEATURES_HALF_KP_VM_H

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "features_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

template <Side AssociatedKing>
class HalfKP_vm {
public:
	static constexpr const char* kName =
		(AssociatedKing == Side::kFriend) ? "HalfKP_vm(Friend)" : "HalfKP_vm(Enemy)";
	static constexpr std::uint32_t kHashValue =
		0x0B6B1D9Bu ^ (AssociatedKing == Side::kFriend);
	static constexpr IndexType kDimensions =
		5 * static_cast<IndexType>(FILE_NB) * static_cast<IndexType>(fe_end);
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
