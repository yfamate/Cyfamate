
#ifndef CLASSIC_NNUE_TEST_COMMAND_H
#define CLASSIC_NNUE_TEST_COMMAND_H

#include "../../config.h"

#if defined(ENABLE_TEST_CMD) && defined(EVAL_NNUE)

namespace YaneuraOu {
namespace Eval::NNUE {

void TestCommand(Position& pos, std::istream& stream);

} 
} 

#endif  

#endif
