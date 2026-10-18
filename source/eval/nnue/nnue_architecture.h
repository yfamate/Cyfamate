
#ifndef CLASSIC_NNUE_ARCHITECTURE_H_INCLUDED
#define CLASSIC_NNUE_ARCHITECTURE_H_INCLUDED

#include "../../config.h"

#if defined(EVAL_NNUE)


#if defined(NNUE_ARCHITECTURE_HEADER)

#include NNUE_ARCHITECTURE_HEADER

#elif defined(EVAL_NNUE_HALFKP256)


#include "architectures/halfkp_256x2-32-32.h"

#elif defined(EVAL_NNUE_KP256)

#include "architectures/kp_256x2-32-32.h"

#elif defined(EVAL_NNUE_HALFKPE9)

#include "architectures/halfkpe9_256x2-32-32.h"

#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_512X2_16_32)

#include "architectures/halfkp_512x2-16-32.h"

#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_1024X2_8_32)

#include "architectures/halfkp_1024x2-8-32.h"

#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_1024X2_8_64)

#include "architectures/halfkp_1024x2-8-64.h"

#elif defined(YANEURAOU_ENGINE_SFNN1536)

#include "architectures/sfnn-1536.h"

#elif defined(EVAL_NNUE_HALFKP_VM_256X2_32_32)

#include "architectures/halfkpvm_256x2-32-32.h"

#else

#include "architectures/halfkp_256x2-32-32.h"

#endif

namespace YaneuraOu {
namespace Eval::NNUE {

	static_assert(kTransformedFeatureDimensions % kMaxSimdWidth == 0, "");
	static_assert(Network::kOutputDimensions == 1, "");
	static_assert(std::is_same<Network::OutputType, std::int32_t>::value, "");

	constexpr auto kRefreshTriggers = RawFeatures::kRefreshTriggers;

} 
} 

#endif  

#endif 
