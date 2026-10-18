
#ifndef CLASSIC_NNUE_ACCUMULATOR_H_
#define CLASSIC_NNUE_ACCUMULATOR_H_

#include "../../config.h"

#if defined(EVAL_NNUE)

#include "nnue_architecture.h"

namespace YaneuraOu {
namespace Eval::NNUE {

#if defined(CYF_GP8)
#if defined(CYF_THREAT) && !defined(CYF_THREAT_NO_DERIVE)
#define CYF_THREAT_DERIVE 1
#ifndef CYF_FINNY_PFR
#define CYF_FINNY_PFR 1   
#endif
#endif
#if defined(CYF_THREAT) && !defined(CYF_THREAT_NO_I8)
#define CYF_THREAT_I8 1
#endif
#endif 
struct alignas(64) Accumulator {
  std::int16_t
      accumulation[2][kRefreshTriggers.size()][kTransformedFeatureDimensions];
  Value score = VALUE_ZERO;
  bool computed_accumulation = false;
  bool computed_score = false;
};

} 
} 

#endif  

#endif
