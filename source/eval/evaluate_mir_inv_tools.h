#ifndef _EVALUATE_MIR_INV_TOOLS_
#define _EVALUATE_MIR_INV_TOOLS_


#include "../config.h"
#if defined(USE_EVAL_LIST)

#include "../evaluate.h"
#include <functional>

namespace YaneuraOu {
namespace Eval {




	Eval::BonaPiece inv_piece(Eval::BonaPiece p);

	Eval::BonaPiece mir_piece(Eval::BonaPiece p);


	extern std::function<void()> mir_piece_init_function;
	extern s16 mir_piece_[Eval::fe_end];
	extern s16 inv_piece_[Eval::fe_end];

	void init_mir_inv_tables();

} 
} 

#endif 
#endif 
