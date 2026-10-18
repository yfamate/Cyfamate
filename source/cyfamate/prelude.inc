#define CYF_HALF_ACC 1   
inline int cyfMpPawnThreat = 4000;
#ifndef CYF_PH_KMODE_DEFAULT
#define CYF_PH_KMODE_DEFAULT 2
#endif
inline int cyfPhKMode = CYF_PH_KMODE_DEFAULT;   
inline int cyfSPDrawSign = 1;   
#ifndef CYF_LMR_CHECKR_DEFAULT
#define CYF_LMR_CHECKR_DEFAULT 512
#endif
inline int cyfLmrCheckR = CYF_LMR_CHECKR_DEFAULT, cyfLmrCheckSee = 1;   
inline int cyfMate1MinDepth = 2;
#ifndef CYF_FC0_TDENSE
#define CYF_FC0_TDENSE 1
#endif
inline int cyfMpCapPV = 0, cyfMpWPawn = 0, cyfMpW5 = 0, cyfMpCapSplit = 0;   
#if defined(CYF_FT_WIDTH) || defined(CYF_H1) || defined(CYF_THREAT)
#ifndef CYF_GP8
#define CYF_GP8 1
#endif
#endif
#include "cyf_tt_guard.h"
#include "cyf_yo_compat.h"   
#if defined(CYF_GPU_ADMISSION)
#include "cyf_gpu_admission.h"
#endif
#if !defined(CYFAMATE_TESTING)
#define NDEBUG
#endif
#if !defined(CYFAMATE_DISABLE_FINNY_TABLES)
#define USE_FINNY_TABLES
#endif
#define _LINUX
#define UNICODE
#define NO_EXCEPTIONS
#define IS_64BIT
#define YANEURAOU_ENGINE_NNUE
#define SFNNwoPSQT
#define NNUE_SFNN_HIDDEN1_7
#define FOR_TOURNAMENT
#if !defined(CYF_NNUE_ONLY_BUILD)
#ifndef CYF_BLEND
#define CYF_BLEND
#endif
#ifndef CYF_BLEND_PERSISTENT_WORKER
#define CYF_BLEND_PERSISTENT_WORKER
#endif
#ifndef CYF_BLEND_WAKE
#define CYF_BLEND_WAKE
#endif
#endif
#define CYFAMATE_NNUE_5GP
#if defined(CYFAMATE_NNUE_5GP)
#define NNUE_ARCHITECTURE_HEADER "architectures/SFNN_halfka2_1024_7_64_5gp.h"
#else
#define NNUE_ARCHITECTURE_HEADER "architectures/SFNN_halfka2_1024_7_64_k3k3.h"
#endif
#if !defined(CYF_TARGET_AVX2)
#define TARGET_CPU "AVX512VNNI"
#define USE_AVX512
#define USE_VNNI
#else
#define TARGET_CPU "AVX2"
#define USE_AVX2
#endif
#if defined(CYF_EXPERIMENT_VNNI_TAIL)
#define USE_NNUE_VNNI
#endif
#define USE_BMI2
#if defined(CYF_GPU_ACC)
#include "cyf_gpu_acc.h"
#endif
#if defined(CYF_GPU_POOL)
#include "cyf_gpu_pool.h"
#endif
#if defined(CYF_DEMAND_TRACE)
#include "cyf_demand_trace.h"   
#endif
#include "cyf_root_hint.h"   
#include "cyf_frontier_export.h"   
#include "cyf_sharpness.h"
#include "cyf_preinfer_gate.h" 
#include "cyf_auto_hint.h"     
#include "cyf_iter_observe.h"   
#define CYF_ZYF_POSITIVE_BOUNDARY_V1 1
#ifndef CYF_ZYF_TEST_STAGE
#define CYF_ZYF_TEST_STAGE(n) ((void)0)
#endif
#define CYF_ZYF_SCHEDULING_V1 1
#define CYF_ZYF_PLAY_HINT_V1 1
#define CYF_ZYF_PROOF_DETACHED_V1 1
#include "cyf_zyf_core.h"       
#include "cyf_zyf_defense.h"    