// [movepicker-20261014 MP] move-ordering weights (USI Cyf_MP_*). Defaults reproduce the unmodified ordering exactly.
//   capture score += PieceValue[captured] * cyfMpCapPV / 64   (P2 = 8, i.e. PieceValue / 8)
//   quiet score   += pawnHistory * cyfMpWPawn / 8              (default 16 = 2x; P5 = 0)
//   quiet score   += continuationHistory[5] * cyfMpW5 / 8      (default 8 = 1x; P4 = 0)
//   cyfMpCapSplit = 1: the good/bad capture SEE threshold (-value / 15) ignores the PieceValue term (ordering only)
inline int cyfMpCapPV = 0, cyfMpWPawn = 0, cyfMpW5 = 0, cyfMpCapSplit = 0;   // 2026-10-14 P4+P5 adopted (P5: G2 +4.3, logic: shogi pawn hash churns with drops, cold rows) (G5 +14.8 [-19,+48], W5=0)
// Evaluation transplant; no research search/time-management changes.
#if defined(CYF_FT_WIDTH) || defined(CYF_H1) || defined(CYF_THREAT)
#ifndef CYF_GP8
#define CYF_GP8 1
#endif
#endif
#include "cyf_tt_guard.h"
// ============================================================================
//  cyfamate.cpp ― Cyfamate 融合CSAエンジン 単一ファイル全部入り
//  YaneuraOu V9.80 + Cyfamate(GP/TM/harvest) + Zyfamate df-pn + native CSA を1ファイルへ集約。
//  [2026-09-15] V9.41 → V9.80 へ基盤更新 (source を include 展開した amalgamation + Cyfamate 層を移植)。
//  build (AVX512VNNI 専用。Windows msys2 ucrt64 / Linux):
//    g++ -std=c++20 -O3 -fno-rtti -march=cascadelake -mavx512f -mavx512bw -mavx512vl -mavx512dq -mavx512vnni 
//        -mbmi -mbmi2 -mprefer-vector-width=512 cyfamate.cpp -o cyfamate -lpthread
//  run : CYFAMATE_DL_MODEL=model.onnx ./cyfamate   (eval/nn.bin を隣に、または EvalDir)
//        USI:  usi / isready / position.../ go ...
//        CSA:  csa selfplay 40 1000  |  csa respond  |  csa server host port id pw
// ============================================================================
#if !defined(CYFAMATE_TESTING)
#define NDEBUG
#endif
// Finny is part of the normal NNUE path, including king-move refreshes.
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
// Cyfamate GP8 network: game-phase (GP) routed layer stacks with threat inputs (architectures/cyfamate_gp8.h,
//   cyfamate/cyf_gp8_threat.h). The stack count and GP bounds are read from the nn.bin header.
// CYF_ARCH_K3K3: build for the standard YaneuraOu SFNN_halfka2_1024_7_64_k3k3 net instead (for measurements
//   against k3k3 nets; header = output of YaneuraOu's nnue_arch_gen.py). Build without -DCYF_FT_WIDTH/-DCYF_H1/-DCYF_THREAT.
#if defined(CYF_ARCH_K3K3)
#define NNUE_ARCHITECTURE_HEADER "architectures/SFNN_halfka2_1024_7_64_k3k3.h"
#else
#define CYFAMATE_NNUE_5GP
#define NNUE_ARCHITECTURE_HEADER "architectures/cyfamate_gp8.h"
#endif
#if !defined(CYF_TARGET_AVX2)
#define TARGET_CPU "AVX512VNNI"
#define USE_AVX512
#define USE_VNNI
#else
// Test builds only (tests/linux_check.sh on CPUs without AVX512VNNI, e.g. GitHub-hosted runners).
#define TARGET_CPU "AVX2"
#define USE_AVX2
#endif
#if defined(CYF_EXPERIMENT_VNNI_TAIL)
// Experimental only: compare the small tail's VNNI path against maddubs/madd.
// The specialized dense fc0 already uses VNNI independently of this switch.
#define USE_NNUE_VNNI
#endif
#define USE_BMI2
#define CYF_ZYF_POSITIVE_BOUNDARY_V1 1
#ifndef CYF_ZYF_TEST_STAGE
#define CYF_ZYF_TEST_STAGE(n) ((void)0)
#endif
#define CYF_ZYF_SCHEDULING_V1 1
#define CYF_ZYF_PLAY_HINT_V1 1
#define CYF_ZYF_PROOF_DETACHED_V1 1
#include "cyf_sharpness.h"
#include "cyf_zyf_core.h"       // ZY-01 resident Zyfamate service core (Cyf_Zyf_Service, default off)
#include "cyf_zyf_defense.h"    // current-go safe root ordering with applied Cyfnet ranks
