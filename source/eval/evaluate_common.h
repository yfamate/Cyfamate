#ifndef _EVALUATE_COMMON_H_
#define _EVALUATE_COMMON_H_


#if defined (EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_NNUE)
#include <functional>

namespace YaneuraOu {
namespace Eval
{

#if defined (EVAL_KPPT) || defined(EVAL_KPP_KKPT) 

	constexpr const char* KK_BIN = "KK_synthesized.bin";

	constexpr const char* KKP_BIN = "KKP_synthesized.bin";

	constexpr const char* KPP_BIN = "KPP_synthesized.bin";

#endif

#if defined(USE_EVAL_HASH)
    using Key64 = uint64_t;
	void prefetch_evalhash(const Key64 key);
#endif

	void foreach_eval_param(std::function<void(s32, s32)>f, int type = -1);


} 
} 

#endif

#endif 
