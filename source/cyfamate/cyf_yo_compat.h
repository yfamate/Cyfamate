#pragma once
#include <string>

namespace CyfYoCompat {
struct Row { const char* option; const char* yo_value; const char* what; };
inline const Row kDiff[] = {
    {"Cyf_Mate_Enable",           "false", "Zyfamate resident df-pn (mate solver threads)"},
    {"Cyf_Zyf_Service",           "false", "Zyfamate root / PV / candidate service"},
    {"Cyf_Zyf_Defense",           "false", "Zyfamate defense (proven no-checking-mate root ordering)"},
    {"Cyf_Zyf_Safe_Order",        "false", "Zyfamate safe root order"},
    {"Cyf_Zyf_PlayHint",          "false", "Zyfamate play hint"},
    {"Cyf_Zyf_Tree_Share",        "false", "Zyfamate proven OR nodes as search hints (already off, user #11)"},
    {"Cyf_Tonshi_Ms",             "0",     "頓死 guard df-pn budget"},
    {"Cyf_Repetition_RootStrict", "false", "strict repetition handling at the root"},
    {"Cyf_Mate1_MinDepth",        "0",     "mate-in-1 probe only from depth 2 (d2)"},
    {"Cyf_MP_PawnThreat",         "0",     "movepicker: quiet onto enemy-pawn-attacked square penalty (pthreat)"},
    {"Cyf_PH_KMode",              "0",     "history: pawn-history key xor hash(king pair) (khk2, CAND6 10-19)"},
    {"Cyf_SPDrawSign",            "0",     "draw: engine-colour draw table in Stochastic_Ponder searches (sp-drawsign, user-approved 10-19)"},
    {"Cyf_LMR_CheckR",            "0",     "search: LMR r -= CheckR for non-capture checks with see >= 0 (lmrc2, CAND6 10-19)"},
    {"Cyf_MP_CapSplit",           "0",     "movepicker capture split"},
    {"Cyf_MP_CapPV",              "0",     "movepicker capture PV bonus"},
    {"Cyf_MP_WPawn",              "0",     "movepicker pawn-history weight"},
    {"Cyf_MP_W5",                 "0",     "movepicker 5th history weight"},
    {"Cyf_MT_PrivHist",           "false", "per-thread pawn / correction history (MT)"},
    {"Cyf_Vote_Depth",            "false", "bestmove vote among deepest threads only (MT)"},
    {"Cyf_HandCorr_Enable",       "false", "hand + major corrhist terms in the corrected static eval (corrhist v3)"},
    {"Cyf_SP_Credit",             "0",     "stochastic-ponder time credit at ponderhit"},
    {"Cyf_TM_Ponder_Bonus",       "125",   "ponder extension % (125 = YO's 1.25)"},
    {"Cyf_TM_Fischer_Horizon",    "0",     "Fischer TM horizon"},
    {"Cyf_TM_Inc_Spend",          "100",   "Fischer increment spend %"},
    {"Cyf_TM_Fischer_Cap",        "0",     "Fischer optimum cap"},
    {"Cyf_Enable",                "false", "legacy DL time aggression"},
    {"Cyf_TM_Enable",             "false", "legacy DL time management"},
    {"Cyf_MultiPonder_TopK",      "0",     "multi-ponder sidecar"},
    {"Cyf_Fusion_Mode",           "0",     "DL fusion"},
    {"Cyf_Cyfnet_Auto",           "false", "Cyfnet auto-detect: model present -> Blend mode 2 unless Cyf_Blend_Mode set (user 10-19)"},
    {"Cyf_Blend_Mode",            "0",     "Cyfnet blend (CNN value in the static eval) + hetcorr"},
    {"Cyf_Root_Hint",             "false", "Cyfnet root ordering hint"},
    {"Cyf_PreInfer_Auto",         "false", "Cyfnet pre-inference"},
    {"Cyf_PreInfer_RootReady",    "false", "Cyfnet root readiness wait"},
};
struct Gate { const char* prefix; const char* gate; };
inline const Gate kGated[] = {
    {"Cyf_Blend_", "Cyf_Blend_Mode"}, {"Cyf_DL_ValueCache", "Cyf_Blend_Mode"}, {"Cyf_Fusion_", "Cyf_Fusion_Mode"},
    {"Cyf_NNCache_MB", "Cyf_Fusion_Mode"}, {"Cyf_Zyf_", "Cyf_Mate_Enable"}, {"Cyf_Mate_", "Cyf_Mate_Enable"},
    {"Cyf_TM_", "Cyf_Enable"}, {"Cyf_GP_", "Cyf_Enable"}, {"Cyf_DL_Conf_Min", "Cyf_Enable"},
    {"Cyf_Root_Hint_", "Cyf_Root_Hint"}, {"Cyf_PreInfer_", "Cyf_PreInfer_Auto"}, {"Cyf_MPON_", "Cyf_MultiPonder_TopK"},
    {"Cyf_LMR_CheckSee", "Cyf_LMR_CheckR"},   
};
inline const char* kOutputOnly[] = {
    "Cyf_Verbose", "Cyf_Probe_MinDepth", "Cyf_Probe_Cap_K", "Cyf_Iter_Observe", "Cyf_Frontier_Export", "Cyf_Frontier_Export_D1",
    "Cyf_Frontier_Export_D2", "Cyf_MT_Obs", "Cyf_Parent_Info" , "Cyf_Replay" , "Cyf_HandCorr_Enable" ,
    "Cyf_YO_Compat",
};
inline bool& flag() { static bool f = false; return f; }   
}  
