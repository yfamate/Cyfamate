
#ifndef CLASSIC_NNUE_FEATURES_COMMON_H_INCLUDED
#define CLASSIC_NNUE_FEATURES_COMMON_H_INCLUDED

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "../../../evaluate.h"
#include "../nnue_common.h"

namespace YaneuraOu {
namespace Eval::NNUE::Features {

class IndexList;

template <typename... FeatureTypes>
class FeatureSet;

enum class TriggerEvent {
  kNone,             
  kFriendKingMoved,  
  kEnemyKingMoved,   
  kAnyKingMoved,     
  kAnyPieceMoved,    
};

enum class Side {
  kFriend,  
  kEnemy,   
};

} 
} 

#endif  

#endif 

