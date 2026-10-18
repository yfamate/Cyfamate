#ifndef __DLSHOGI_TYPES_H_INCLUDED__
#define __DLSHOGI_TYPES_H_INCLUDED__

#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)

#include "../../types.h"
#include <cfloat> 


namespace dlshogi {

constexpr int VIRTUAL_LOSS = 1;
	
typedef uint32_t NodeCountType;

#if defined(WIN_TYPE_DOUBLE)
typedef double WinType;
#else
typedef float WinType;
#endif

typedef uint16_t ChildNumType;

constexpr uint32_t VALUE_WIN  = 0x1000000;
constexpr uint32_t VALUE_LOSE = 0x2000000;
constexpr uint32_t VALUE_DRAW = 0x4000000;

constexpr NodeCountType NOT_EXPANDED = std::numeric_limits<NodeCountType>::max();



constexpr float QUEUING   =  FLT_MAX;

constexpr float DISCARDED = -FLT_MAX;
} 

#endif 
#endif 
