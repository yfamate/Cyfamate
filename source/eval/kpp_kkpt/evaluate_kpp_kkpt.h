#ifndef _EVALUATE_KPP_KKPT_H_
#define _EVALUATE_KPP_KKPT_H_

#include "../../config.h"


#if defined(EVAL_KPP_KKPT)

#include "../../evaluate.h"
#include "../evaluate_common.h"

namespace YaneuraOu {
namespace Eval {



	
	typedef std::array<int32_t, 2> ValueKk;
	typedef std::array<int32_t, 2> ValueKkp;
	typedef int16_t ValueKpp;




	extern ValueKk(*kk_)[SQ_NB][SQ_NB];
	extern ValueKkp(*kkp_)[SQ_NB][SQ_NB][fe_end];
	extern ValueKpp(*kpp_)[SQ_NB][fe_end][fe_end];

#define kk (*kk_)
#define kkp (*kkp_)
#define kpp (*kpp_)

	const u64 size_of_kk = (u64)SQ_NB*(u64)SQ_NB*(u64)sizeof(ValueKk);
	const u64 size_of_kkp = (u64)SQ_NB*(u64)SQ_NB*(u64)fe_end*(u64)sizeof(ValueKkp);
	const u64 size_of_kpp = (u64)SQ_NB*(u64)fe_end*(u64)fe_end*(u64)sizeof(ValueKpp);
	const u64 size_of_eval = size_of_kk + size_of_kkp + size_of_kpp;

} 
} 

#endif 


#endif
