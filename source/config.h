#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED


#if !defined(ENGINE_VERSION)

#define ENGINE_VERSION "9.80git"

#endif



























































































constexpr int MAX_PLY_NUM = 246;

#define PRETTY_JP





#if defined(YANEURAOU_ENGINE_KPPT) || defined(YANEURAOU_ENGINE_KPP_KKPT) || defined(YANEURAOU_ENGINE_NNUE) || defined(YANEURAOU_ENGINE_MATERIAL)

	#define YANEURAOU_ENGINE
	#define USE_CLASSIC_EVAL
	#define USE_PARTIAL_KEY
	#define USE_PIECE_VALUE
	#define USE_SEE
	#define USE_EVAL_LIST
	#define USE_MATE_1PLY
	#define USE_MATE_SOLVER
	#define USE_MATE_DFPN
	#define USE_TIME_MANAGEMENT
	#define USE_MOVE_PICKER
	#define USE_EVAL

	#define ENABLE_MAKEBOOK_CMD

	#if defined(YANEURAOU_ENGINE_KPPT) || defined(YANEURAOU_ENGINE_KPP_KKPT) || defined(YANEURAOU_ENGINE_NNUE)
		#define USE_DIFF_EVAL
	#endif

	#if defined(YANEURAOU_ENGINE_NNUE) && !defined(NNUE_EMBEDDING)
		#define NNUE_EMBEDDING_OFF
	#endif


	#define ENABLE_TEST_CMD

	#define ENABLE_MAKEBOOK_CMD



	#if defined(YANEURAOU_ENGINE_MATERIAL)
		#define EVAL_MATERIAL

		#if !defined(MATERIAL_LEVEL)
			#define MATERIAL_LEVEL 001
		#endif

		#if MATERIAL_LEVEL >= 002
			#define LONG_EFFECT_LIBRARY
		#endif
	#endif

	#if defined(YANEURAOU_ENGINE_KPPT)
		#define EVAL_KPPT
	#endif

	#if defined(YANEURAOU_ENGINE_KPP_KKPT)
		#define EVAL_KPP_KKPT
	#endif

	#if defined(YANEURAOU_ENGINE_NNUE)
		#define EVAL_NNUE



	#endif

#endif 


#if defined(YANEURAOU_ENGINE_DEEP)

	#define EVAL_DEEP "DL"
	#define USE_EVAL
	#define USE_TIME_MANAGEMENT
	#define USE_MATE_1PLY
	#define USE_MATE_SOLVER
	#define USE_MATE_DFPN
	#define USE_PIECE_VALUE
	#define ENABLE_TEST_CMD

	#define WIN_TYPE_DOUBLE

#endif


#if defined(TANUKI_MATE_ENGINE)
	#define USE_CLASSIC_EVAL
	#define KEEP_LAST_MOVE
	#undef  MAX_PLY_NUM
	#define MAX_PLY_NUM 2000
	#define USE_MATE_1PLY
	#define ENABLE_TEST_CMD
#endif


#if defined(YANEURAOU_MATE_ENGINE)
	#define USE_CLASSIC_EVAL
	#undef MAX_PLY_NUM
	#define MAX_PLY_NUM 2000
	#define USE_MATE_1PLY
	#define USE_MATE_SOLVER
	#define USE_MATE_DFPN
	#define USE_PIECE_VALUE
	#define ENABLE_TEST_CMD
#endif



#if defined(USER_ENGINE)
#endif


#if defined(FOR_TOURNAMENT)
	#undef ASSERT_LV
	#undef ENABLE_TEST_CMD
	#undef KEEP_LAST_MOVE

	#define ENABLE_QUICK_DRAW
#endif



#if !defined (USE_DEBUG_ASSERT)
#define ASSERT(X) { if (!(X)) *(int*)1 = 0; }
#else
#include <iostream>
#include <chrono>
#include <thread>
#define ASSERT(X) { if (!(X)) { std::cout << "\nError : ASSERT(" << #X << "), " << __FILE__ << "(" << __LINE__ << "): " << __func__ << std::endl; \
 std::this_thread::sleep_for(std::chrono::microseconds(3000)); *(int*)1 =0;} }
#endif

#if !defined(ASSERT_LV)
#define ASSERT_LV 0
#endif

#define ASSERT_LV_EX(L, X) { if (L <= ASSERT_LV) ASSERT(X); }
#define ASSERT_LV1(X) ASSERT_LV_EX(1, X)
#define ASSERT_LV2(X) ASSERT_LV_EX(2, X)
#define ASSERT_LV3(X) ASSERT_LV_EX(3, X)
#define ASSERT_LV4(X) ASSERT_LV_EX(4, X)
#define ASSERT_LV5(X) ASSERT_LV_EX(5, X)

#define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)



#if defined(_MSC_VER)
#define UNREACHABLE ASSERT_LV3(false); __assume(0);
#elif defined(__GNUC__)
#define UNREACHABLE ASSERT_LV3(false); __builtin_unreachable();
#else
#define UNREACHABLE ASSERT_LV3(false);
#endif



#if defined(_MSC_VER)
#define ALIGNED(X) __declspec(align(X))
#elif defined(__GNUC__)
#define ALIGNED(X) __attribute__ ((aligned(X)))
#else
#define ALIGNED(X)
#endif


#if defined (PRETTY_JP)
constexpr bool pretty_jp = true;
#else
constexpr bool pretty_jp = false;
#endif


#if defined(USE_POLICY_BOOK)
#define HASH_KEY_BITS 128
#endif


#if !defined(HASH_KEY_BITS)
#define HASH_KEY_BITS 64
#endif


#if HASH_KEY_BITS <= 64
#define Key Key64
#elif HASH_KEY_BITS <= 128
#define Key Key128
#else
#define Key Key256
#endif

#if !defined(TT_CLUSTER_SIZE)
#define TT_CLUSTER_SIZE 3
#endif



#if defined (USE_KIF_CONVERT_TOOLS)
	#define KEEP_LAST_MOVE
#endif


#if (defined(_WIN64) && defined(_MSC_VER)) || (defined(__GNUC__) && defined(__x86_64__)) || defined(IS_64BIT)
	constexpr bool Is64Bit = true;
	#ifndef IS_64BIT
		#define IS_64BIT
	#endif
#else
	constexpr bool Is64Bit = false;
#endif

#if !defined(TARGET_CPU)
	#if defined(USE_BMI2)
	#define BMI2_STR "BMI2"
	#else
	#define BMI2_STR ""
	#endif

	#if defined(USE_AVX512VNNI)
	#define TARGET_CPU "AVX512VNNI" BMI2_STR
	#elif defined(USE_AVX512)
	#define TARGET_CPU "AVX512" BMI2_STR
	#elif defined(USE_AVX2)
	#define TARGET_CPU "AVX2" BMI2_STR
	#elif defined(USE_SSE42)
	#define TARGET_CPU "SSE4.2"
	#elif defined(USE_SSE41)
	#define TARGET_CPU "SSE4.1"
	#elif defined(USE_SSSE3)
	#define TARGET_CPU "SSSE3"
	#elif defined(USE_SSE2)
	#define TARGET_CPU "SSE2"
	#elif defined(USE_NEON_DOTPROD)
	#define TARGET_CPU "ARMV8_DOTPROD"
	#elif defined(USE_NEON)
	#define TARGET_CPU "ARMV8"
	#else
	#define TARGET_CPU "noSSE"
	#endif
#endif


#if defined (USE_AVX512VNNI)
#define USE_AVX512
#endif

#if defined (USE_AVX512)
#define USE_AVX2
#endif

#if defined (USE_AVX2)
#define USE_SSE42
#endif

#if defined (USE_SSE42)
#define USE_SSE41
#endif

#if defined (USE_SSE41)
#define USE_SSSE3
#endif

#if defined (USE_SSSE3)
#define USE_SSE2
#endif


#if defined(EVAL_MATERIAL)
	#if defined(MATERIAL_LEVEL)
		#define EVAL_TYPE_NAME "MaterialLv" + std::to_string(MATERIAL_LEVEL)
	#else
		#define EVAL_TYPE_NAME ""
	#endif
#elif defined(EVAL_KPPT)
	#define EVAL_TYPE_NAME "KPPT"
#elif defined(EVAL_KPP_KKPT)
	#define EVAL_TYPE_NAME "KPP_KKPT"
#elif defined(EVAL_NNUE_KP256)
	#define EVAL_TYPE_NAME "NNUE KP256"
#elif defined(EVAL_NNUE_HALFKPE9)
	#define EVAL_TYPE_NAME "NNUE halfKPE9"
	#define LONG_EFFECT_LIBRARY
	#define USE_BOARD_EFFECT_PREV
#elif defined(EVAL_NNUE) 
	#define EVAL_TYPE_NAME "NNUE"
#elif defined(EVAL_DEEP)
	#if defined(ONNXRUNTIME)
		#if defined(ORT_CPU)
			#define EVAL_TYPE_NAME "ORT_CPU-" EVAL_DEEP
		#elif defined(ORT_DML)
			#define EVAL_TYPE_NAME "ORT_DML-" EVAL_DEEP
		#else
			#define EVAL_TYPE_NAME "ORT-" EVAL_DEEP
		#endif
	#elif defined(TENSOR_RT)
		#define EVAL_TYPE_NAME "TRT-" EVAL_DEEP
	#elif defined(COREML)
		#define EVAL_TYPE_NAME "CoreML-" EVAL_DEEP
	#endif

#else
	#define EVAL_TYPE_NAME ""
#endif




#define ADD_BOARD_EFFECT(color_,sq_,e1_) { board_effect[color_].e[sq_] += (uint8_t)e1_; }
#define ADD_BOARD_EFFECT_BOTH(color_,sq_,e1_,e2_) { board_effect[color_].e[sq_] += (uint8_t)e1_; board_effect[~color_].e[sq_] += (uint8_t)e2_; }

#define ADD_BOARD_EFFECT_REWIND(color_,sq_,e1_) { board_effect[color_].e[sq_] += (uint8_t)e1_; }
#define ADD_BOARD_EFFECT_BOTH_REWIND(color_,sq_,e1_,e2_) { board_effect[color_].e[sq_] += (uint8_t)e1_; board_effect[~color_].e[sq_] += (uint8_t)e2_; }

#endif 
