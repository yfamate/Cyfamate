#include "../../types.h"

#if defined (YANEURAOU_ENGINE)











#include "../../search.h"

#include <sstream>
#include <fstream>
#include <iomanip>
#include <cmath>	
#include <cstring>	

#include "yaneuraou-search.h"
#include "../../position.h"
#include "../../thread.h"
#include "../../misc.h"
#include "../../tt.h"
#include "../../book/book.h"
#include "../../movepick.h"
#include "../../usi.h"
#include "../../mate/mate.h"
#include "../../tune.h"

namespace YaneuraOu {

using namespace Search;
using namespace Eval;  












void SearchOptions::add_options(OptionsMap& options) {
    
    
    options.add("MaxMovesToDraw", Option(0, 0, 100000, [&](const Option& o) {
                    
                    
                    max_moves_to_draw = int(o);
                    if (max_moves_to_draw == 0)
                        max_moves_to_draw = 100000;
                    return std::nullopt;
                }));

    
    options.add("PvInterval", Option(300, 0, 100000000, [&](const Option& o) {
                    pv_interval = s64(o);
                    return std::nullopt;
                }));

    
    options.add("ConsiderationMode", Option(false, [&](const Option& o) {
                    consideration_mode = o;
                    return std::nullopt;
                }));

    
    options.add("OutputFailLHPV", Option(true, [&](const Option& o) {
                    outout_fail_lh_pv = o;
                    return std::nullopt;
                }));

    
    options.add("GenerateAllLegalMoves", Option(false, [&](const Option& o) {
                    generate_all_legal_moves = o;
                    return std::nullopt;
                }));

    
    options.add("EnteringKingRule",
                Option(EKR_STRINGS, EKR_STRINGS[EKR_27_POINT], [&](const Option& o) {
                    enteringKingRule = to_entering_king_rule(o);
                    return std::nullopt;
                }));
}


void SearchManager::pre_start_searching(YaneuraOuWorker& worker) {
    

    stopOnPonderhit  = false;
    ponder                                       = worker.limits.ponderMode;
    increaseDepth                                = true;
}



void YaneuraOuEngine::add_options() {

	

	Engine::add_options();

	

#if STOCKFISH
    options.add(  
        "USI_Hash", Option(16, 1, MaxHashMB, [this](const Option& o) {
            set_tt_size(o);
            return std::nullopt;
        }));
#else
	
	
	
    options.add(  
        "USI_Hash", Option(256, 1, MaxHashMB, [this](const Option& o) {
            set_tt_size(o);
            return std::nullopt;
        }));
#endif

	
    
    options.add("MultiPV", Option(1, 1, MAX_MOVES));

    options.add("DrawValueBlack", Option(-33, -30000, 30000));   
    options.add("DrawValueWhite", Option(-33, -30000, 30000));   
    options.add("Cyf_SPDrawSign", Option(1, 0, 1, [](const Option& o){ cyfSPDrawSign = (int)(int64_t)o; return std::nullopt; }));   

    
    options.add("ResignValue", Option(99999, 0, 99999));

	

	manager.search_options.add_options(options);

	
	
	
	options.add("Cyf_Enable",           Option(Cyfamate::options().enable, [](const Option& o){ Cyfamate::options().enable = ((int64_t)o != 0); return std::nullopt; }));

	options.add("Cyf_Verbose",          Option(Cyfamate::options().verbose, [](const Option& o){ Cyfamate::options().verbose = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_GP_Onset",         Option((int)(Cyfamate::options().gp_onset*100.0), 0, 150, [](const Option& o){ Cyfamate::options().gp_onset = (double)(int64_t)o / 100.0; return std::nullopt; }));
	options.add("Cyf_GP_Floor",         Option((int)(Cyfamate::options().gp_floor*100.0), 0, 150, [](const Option& o){ Cyfamate::options().gp_floor = (double)(int64_t)o / 100.0; return std::nullopt; }));
	options.add("Cyf_DL_Conf_Min",      Option((int)(Cyfamate::options().dl_conf_min*100.0), 0, 100, [](const Option& o){ Cyfamate::options().dl_conf_min = (double)(int64_t)o / 100.0; return std::nullopt; }));
    options.add("Cyf_Fusion_Mode", Option(Cyfamate::fusion_config().mode, 0, 3, [](const Option& o){ Cyfamate::fusion_config().mode = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_Model", Option("<empty>", [](const Option& o){ Cyfamate::fusion_config().model = std::string(o); return std::nullopt; }));
    options.add("Cyf_Fusion_ORT", Option("<empty>", [](const Option& o){ Cyfamate::fusion_config().ort_dll = std::string(o);
#if defined(CYF_BLEND)
        Cyfamate::mode2_link().ort = std::string(o);   
#endif
        return std::nullopt; }));
    options.add("Cyf_Fusion_Provider", Option("<empty>", [](const Option& o){ Cyfamate::fusion_config().provider = std::string(o); return std::nullopt; }));   
    options.add("Cyf_Fusion_Device", Option(Cyfamate::fusion_config().device, -1, 15, [](const Option& o){ Cyfamate::fusion_config().device = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_Calibration", Option("<empty>", [](const Option& o){ Cyfamate::fusion_config().calibration = std::string(o); return std::nullopt; }));
    options.add("Cyf_Fusion_MinDepth", Option(Cyfamate::fusion_config().min_depth, 2, 64, [](const Option& o){ Cyfamate::fusion_config().min_depth = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_Batch", Option(Cyfamate::fusion_config().batch, 1, 256, [](const Option& o){ Cyfamate::fusion_config().batch = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_AdaptiveBatch", Option(false, [](const Option& o){ Cyfamate::fusion_config().adaptive_batch = (bool)o; return std::nullopt; }));
    options.add("Cyf_Fusion_Queue", Option(Cyfamate::fusion_config().queue, 1, 65536, [](const Option& o){ Cyfamate::fusion_config().queue = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_Cache", Option(Cyfamate::fusion_config().cache, 1, 8192, [](const Option& o){ Cyfamate::fusion_config().cache = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_WaitUs", Option(Cyfamate::fusion_config().wait_us, 0, 100000, [](const Option& o){ Cyfamate::fusion_config().wait_us = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Fusion_GapPermille", Option(Cyfamate::fusion_config().gap_permille, 0, 1000, [](const Option& o){ Cyfamate::fusion_config().gap_permille = (int)(int64_t)o; return std::nullopt; }));
#if defined(CYF_BLEND)
    {   
        auto& bc = Cyfamate::blend_config();
        auto real = [](const Option& o, double& dst) {
            try { dst = std::stod(std::string(o)); } catch (...) { sync_cout << "info string Error! : not a number " << std::string(o) << sync_endl; }
            return std::nullopt; };
        options.add("Cyf_Blend_Mode", Option(bc.mode, 0, 2, [](const Option& o){ Cyfamate::blend_config().mode = Cyfamate::mode2_link().mode = (int)(int64_t)o; Cyfamate::blend_config().mode_set = true; return std::nullopt; }));   
        options.add("Cyf_Cyfnet_Auto", Option(true, [](const Option& o){ Cyfamate::blend_config().auto_detect = (bool)o; return std::nullopt; }));   
        
        options.add("Cyf_Blend_Mode2_Link", Option(true, [](const Option& o){ Cyfamate::mode2_link().enabled = ((int64_t)o != 0); return std::nullopt; }));
        options.add("Cyf_Blend_DiscoveryBudget", Option(bc.discovery_budget, 1, 1048576, [](const Option& o){ Cyfamate::blend_config().discovery_budget = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_InferBudgetMs", Option(bc.infer_budget_ms, 0, 10000, [](const Option& o){ Cyfamate::blend_config().infer_budget_ms=(int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_QueueBatches", Option(bc.queue_batches, 0, 64, [](const Option& o){ Cyfamate::blend_config().queue_batches=(int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_PublishPartial", Option(bc.publish_partial, [](const Option& o){ Cyfamate::blend_config().publish_partial=(bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_PackWaitUs", Option(bc.pack_wait_us, 0, 5000, [](const Option& o){ Cyfamate::blend_config().pack_wait_us=(int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_Model", Option("<empty>", [](const Option& o){ Cyfamate::blend_config().model = Cyfamate::mode2_link().model = std::string(o); return std::nullopt; }));
        options.add("Cyf_Blend_Provider", Option("cpu", [](const Option& o){ Cyfamate::blend_config().provider = std::string(o); return std::nullopt; }));
        options.add("Cyf_Blend_Threads", Option(bc.threads, 1, 64, [](const Option& o){ Cyfamate::blend_config().threads = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_Batch", Option(bc.batch, 1, 4096, [](const Option& o){ Cyfamate::blend_config().batch = Cyfamate::mode2_link().batch = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_WN", Option("0.5953614038380678", [real](const Option& o){ return real(o, Cyfamate::blend_config().wn); }));
        options.add("Cyf_Blend_WC", Option("0.5395981459708928", [real](const Option& o){ return real(o, Cyfamate::blend_config().wc); }));
        options.add("Cyf_Blend_B", Option("-0.024877423536843715", [real](const Option& o){ return real(o, Cyfamate::blend_config().b); }));
        options.add("Cyf_Blend_FN", Option("1.0775028130676736", [real](const Option& o){ return real(o, Cyfamate::blend_config().fn); }));
        options.add("Cyf_Blend_FB", Option("-0.04759336851126909", [real](const Option& o){ return real(o, Cyfamate::blend_config().fb); }));
        options.add("Cyf_Blend_Clip", Option(bc.clip, 1, 32000, [](const Option& o){ Cyfamate::blend_config().clip = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_CacheMB", Option(bc.cache_mb, 1, 16384, [](const Option& o){ Cyfamate::blend_config().cache_mb = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_MinNew", Option(bc.min_new, 1, 100000000, [](const Option& o){ Cyfamate::blend_config().min_new = Cyfamate::mode2_link().min_new = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_MaxUpdates", Option(bc.max_updates, 0, 256, [](const Option& o){ Cyfamate::blend_config().max_updates = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_Queue", Option(bc.queue, 16, 4194304, [](const Option& o){ Cyfamate::blend_config().queue = (int)(int64_t)o; return std::nullopt; }));
        options.add("Cyf_Blend_FallbackOnly", Option(false, [](const Option& o){ Cyfamate::blend_config().fallback_only = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_NoScrub", Option(false, [](const Option& o){ Cyfamate::blend_config().no_scrub = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_Verify", Option(false, [](const Option& o){ Cyfamate::blend_config().verify = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_Check", Option(false, [](const Option& o){ Cyfamate::blend_config().check = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_NoCorrection", Option(false, [](const Option& o){ Cyfamate::blend_config().no_correction = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_Stats", Option(true, [](const Option& o){ Cyfamate::blend_config().stats = (bool)o; return std::nullopt; }));
        
        options.add("Cyf_Blend_RootFix", Option(true, [](const Option& o){ Cyfamate::blend_config().root_fix = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_RootLog", Option(false, [](const Option& o){ Cyfamate::blend_config().root_log = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_ResetCorrection", Option(false, [](const Option& o){ Cyfamate::blend_config().reset_corr = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_Deterministic", Option(false, [](const Option& o){ Cyfamate::blend_config().det = (bool)o; return std::nullopt; }));
        
        options.add("Cyf_DL_ValueCache", Option(bc.dl_cache, [](const Option& o){ Cyfamate::blend_config().dl_cache = (bool)o; return std::nullopt; }));
        options.add("Cyf_DL_ValueCache_Audit", Option(false, [](const Option& o){ Cyfamate::blend_config().dl_audit = (bool)o; return std::nullopt; }));
        options.add("Cyf_Blend_NewRowsPerGo", Option(0, 0, 1048576, [](const Option& o){ Cyfamate::blend_config().new_rows_per_go = Cyfamate::mode2_link().rows = (int)(int64_t)o; return std::nullopt; }));
    }
#endif
#if defined(CYF_REPLAY)
    options.add("Cyf_Replay", Option("<empty>", [](const Option& o){
        std::string err;
        if (!Cyfamate::replay().set(std::string(o), err))
            sync_cout << "info string Error! : Cyf_Replay " << err << sync_endl;
        return std::nullopt; }));
#endif
	options.add("Cyf_Probe_MinDepth",   Option(Cyfamate::options().probe_min_depth, 0, 64, [](const Option& o){ Cyfamate::options().probe_min_depth = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Probe_Cap_K",      Option(Cyfamate::options().probe_cap_k, 0, 65536, [](const Option& o){ Cyfamate::options().probe_cap_k = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_NNCache_MB",       Option(Cyfamate::options().nncache_mb, 0, 4096, [](const Option& o){ Cyfamate::options().nncache_mb = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Enable",        Option(Cyfamate::options().tm_enable, [](const Option& o){ Cyfamate::options().tm_enable = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_TM_Agree",         Option(Cyfamate::options().tm_agree, 50, 100, [](const Option& o){ Cyfamate::options().tm_agree = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Disagree",      Option(Cyfamate::options().tm_disagree, 100, 200, [](const Option& o){ Cyfamate::options().tm_disagree = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_GP_Early",      Option(Cyfamate::options().tm_gp_early, 50, 150, [](const Option& o){ Cyfamate::options().tm_gp_early = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_GP_Mid",        Option(Cyfamate::options().tm_gp_mid, 50, 200, [](const Option& o){ Cyfamate::options().tm_gp_mid = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Score_Start",   Option(Cyfamate::options().tm_score_start_cp, 100, 30000, [](const Option& o){ Cyfamate::options().tm_score_start_cp = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_GP_Gamma",      Option(Cyfamate::options().tm_gp_gamma, 20, 400, [](const Option& o){ Cyfamate::options().tm_gp_gamma = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Momentum_Ext",  Option(Cyfamate::options().tm_momentum_ext, 0, 100, [](const Option& o){ Cyfamate::options().tm_momentum_ext = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Momentum_Acc",  Option(Cyfamate::options().tm_momentum_acc, 0, 100, [](const Option& o){ Cyfamate::options().tm_momentum_acc = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Sharp_Current", Option(false, [](const Option& o){ Cyfamate::options().tm_sharp_current = ((int64_t)o != 0); return std::nullopt; }));
    options.add("Cyf_TM_Sharp_Base",    Option(Cyfamate::options().tm_sharp_base, 50, 120, [](const Option& o){ Cyfamate::options().tm_sharp_base = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Sharp_Range",   Option(Cyfamate::options().tm_sharp_range, 0, 60, [](const Option& o){ Cyfamate::options().tm_sharp_range = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Tesuki_Ms",     Option(Cyfamate::options().tm_tesuki_ms, 0, 2000, [](const Option& o){ Cyfamate::options().tm_tesuki_ms = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_SP_Credit", Option(Cyfamate::options().sp_credit, 0, 100, [](const Option& o){ Cyfamate::options().sp_credit = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Ponder_Bonus",  Option(Cyfamate::options().tm_ponder_bonus, 100, 150, [](const Option& o){ Cyfamate::options().tm_ponder_bonus = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_TM_Score_Floor",   Option(Cyfamate::options().tm_score_floor, 30, 100, [](const Option& o){ Cyfamate::options().tm_score_floor = (int)(int64_t)o; return std::nullopt; }));
	
	options.add("Cyf_MultiPonder_TopK", Option(Cyfamate::options().multiponder_topk, 0, 32, [](const Option& o){ Cyfamate::options().multiponder_topk = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_MT_Obs",           Option(Cyfamate::options().mt_obs, [](const Option& o){ Cyfamate::options().mt_obs = ((int64_t)o != 0); return std::nullopt; }));   
	options.add("Cyf_MT_PrivHist",      Option(Cyfamate::options().mt_privhist, [](const Option& o){ Cyfamate::options().mt_privhist = ((int64_t)o != 0); return std::nullopt; }));   
	options.add("Cyf_Vote_Depth",       Option(Cyfamate::options().vote_depth, [](const Option& o){ Cyfamate::options().vote_depth = ((int64_t)o != 0); return std::nullopt; }));   
	options.add("Cyf_Mate_Enable",      Option(Cyfamate::options().mate_enable, [](const Option& o){ Cyfamate::options().mate_enable = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Share",        Option(Cyfamate::options().zyf_share, 0, 2, [](const Option& o){ Cyfamate::options().zyf_share = (int)(int64_t)o; return std::nullopt; }));   
	options.add("Cyf_Mate_Threads",     Option(Cyfamate::options().mate_threads, 0, 64, [](const Option& o){ Cyfamate::options().mate_threads = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Mate_Hash",        Option(Cyfamate::options().mate_hash_mb, 16, 65536, [](const Option& o){ Cyfamate::options().mate_hash_mb = (int)(int64_t)o;
		if (Cyfamate::zyf_hash_latched())   
			sync_cout << "info string warning: Cyf_Mate_Hash は df-pn の表確保後は変更できません (エンジン再起動が必要)" << sync_endl;
		return std::nullopt; }));
	options.add("Cyf_Mate_Probe_Depth", Option(Cyfamate::options().mate_probe_depth, 0, 128, [](const Option& o){ Cyfamate::options().mate_probe_depth = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_MPON_Node_Limit",  Option(Cyfamate::options().mpon_node_limit, 0, 100000000, [](const Option& o){ Cyfamate::options().mpon_node_limit = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Tonshi_Ms",        Option(Cyfamate::options().tonshi_ms, 0, 10000, [](const Option& o){ Cyfamate::options().tonshi_ms = (int)(int64_t)o; return std::nullopt; }));
	
	options.add("Cyf_Zyf_Service",      Option(Cyfamate::options().zyf_service, [](const Option& o){ Cyfamate::options().zyf_service = ((int64_t)o != 0); return std::nullopt; }));
    
    options.add("Cyf_Repetition_RootStrict", Option(Position::repetition_root_strict, [](const Option& o){ Position::repetition_root_strict = ((int64_t)o != 0); return std::nullopt; }));
    options.add("Cyf_Zyf_PlayHint", Option(Cyfamate::options().zyf_play_hint, [](const Option& o){ Cyfamate::options().zyf_play_hint = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Keep_Proofs",  Option(Cyfamate::options().zyf_keep_proofs, [](const Option& o){ Cyfamate::options().zyf_keep_proofs = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Tree_Share",   Option(Cyfamate::options().zyf_tree_share, [](const Option& o){ Cyfamate::options().zyf_tree_share = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Tree_Live",    Option(Cyfamate::options().zyf_tree_live, [](const Option& o){ Cyfamate::options().zyf_tree_live = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Tree_Bound",   Option(Cyfamate::options().zyf_tree_bound, [](const Option& o){ Cyfamate::options().zyf_tree_bound = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Tree_Extend",  Option(Cyfamate::options().zyf_tree_extend, [](const Option& o){ Cyfamate::options().zyf_tree_extend = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Tree_Max",     Option(Cyfamate::options().zyf_tree_max, 1, 16384, [](const Option& o){ Cyfamate::options().zyf_tree_max = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Go_Jobs",      Option(Cyfamate::options().zyf_go_jobs, 0, 64, [](const Option& o){ Cyfamate::options().zyf_go_jobs = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Defense",      Option(Cyfamate::options().zyf_defense, [](const Option& o){ Cyfamate::options().zyf_defense = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Defense_Jobs", Option(Cyfamate::options().zyf_defense_jobs, 0, 256, [](const Option& o){ Cyfamate::options().zyf_defense_jobs = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Defense_Ms",   Option(Cyfamate::options().zyf_defense_ms, 1, 60000, [](const Option& o){ Cyfamate::options().zyf_defense_ms = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Defense_Nodes",Option(Cyfamate::options().zyf_defense_nodes, 1, 100000000, [](const Option& o){ Cyfamate::options().zyf_defense_nodes = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Tonshi_Switch",Option(Cyfamate::options().zyf_tonshi_switch, 0, 2, [](const Option& o){ Cyfamate::options().zyf_tonshi_switch = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Safe_Order",   Option(Cyfamate::options().zyf_safe_order, [](const Option& o){ Cyfamate::options().zyf_safe_order = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Zyf_Job_Nodes",    Option(Cyfamate::options().zyf_job_nodes, 1, 100000000, [](const Option& o){ Cyfamate::options().zyf_job_nodes = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Job_Ms",       Option(Cyfamate::options().zyf_job_ms, 1, 60000, [](const Option& o){ Cyfamate::options().zyf_job_ms = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Root_Nodes",   Option(Cyfamate::options().zyf_root_nodes, 0, 1000000000, [](const Option& o){ Cyfamate::options().zyf_root_nodes = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Root_Ms",      Option(Cyfamate::options().zyf_root_ms, 0, 60000, [](const Option& o){ Cyfamate::options().zyf_root_ms = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_PV_Plies",     Option(Cyfamate::options().zyf_pv_plies, 0, 32, [](const Option& o){ Cyfamate::options().zyf_pv_plies = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Cands",        Option(Cyfamate::options().zyf_cands, 0, 16, [](const Option& o){ Cyfamate::options().zyf_cands = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Zyf_Min_Depth",    Option(Cyfamate::options().zyf_min_depth, 1, 64, [](const Option& o){ Cyfamate::options().zyf_min_depth = (int)(int64_t)o; return std::nullopt; }));
	
	
	options.add("Cyf_Parent_Info",      Option(false, [](const Option& o){ Cyfamate::parent_info_enabled() = ((int64_t)o != 0); return std::nullopt; }));
	
	
	
	Cyfamate::apply_auto_defaults();
	
	
	
	options.add("Cyf_MP_CapSplit", Option(0, 0, 1, [](const Option& o){ cyfMpCapSplit = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Mate1_MinDepth", Option(2, 0, 64, [](const Option& o){ cyfMate1MinDepth = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_MP_PawnThreat", Option(4000, 0, 30000, [](const Option& o){ cyfMpPawnThreat = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_PH_KMode", Option(CYF_PH_KMODE_DEFAULT, 0, 2, [](const Option& o){ cyfPhKMode = (int)(int64_t)o; return std::nullopt; }));   
	options.add("Cyf_YO_Compat", Option(false));   
	options.add("Cyf_LMR_CheckR", Option(CYF_LMR_CHECKR_DEFAULT, 0, 4096, [](const Option& o){ cyfLmrCheckR = (int)(int64_t)o; return std::nullopt; }));   
	options.add("Cyf_LMR_CheckSee", Option(1, 0, 1, [](const Option& o){ cyfLmrCheckSee = (int)(int64_t)o; return std::nullopt; }));   
	options.add("Cyf_MP_CapPV", Option(0, 0, 1024, [](const Option& o){ cyfMpCapPV = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_MP_WPawn", Option(0, 0, 64, [](const Option& o){ cyfMpWPawn = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_MP_W5",    Option(0, 0, 64, [](const Option& o){ cyfMpW5 = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Root_Hint",        Option(false, [](const Option& o){ CyfRootHint::state().enabled = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Root_Hint_RequireKey", Option(false, [](const Option& o){ CyfRootHint::state().require_key = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Root_Hint_TopK",   Option(8, 1, 64, [](const Option& o){ CyfRootHint::state().topk = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Root_Hint_Step",   Option(8000, 0, 100000, [](const Option& o){ CyfRootHint::state().step = (int)(int64_t)o; return std::nullopt; }));
    options.add("Cyf_Root_Hint_Trace", Option(false, [](const Option& o){ CyfRootHint::state().trace_requested = ((int64_t)o != 0); return std::nullopt; }));
	
	
	options.add("Cyf_Frontier_Export",    Option(false, [](const Option& o){ CyfFrontier::state().enabled = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_Frontier_Export_D1", Option(6, 1, 64, [](const Option& o){ CyfFrontier::state().d1 = (int)(int64_t)o; return std::nullopt; }));
	options.add("Cyf_Frontier_Export_D2", Option(12, 1, 64, [](const Option& o){ CyfFrontier::state().d2 = (int)(int64_t)o; return std::nullopt; }));
	
	
	
	options.add("Cyf_Iter_Observe",       Option(false, [](const Option& o){ CyfIter::state().enabled = ((int64_t)o != 0); return std::nullopt; }));
	
	options.add("Cyf_PreInfer_Observe", Option(false, [](const Option& o){ CyfPreInfer::state().enabled = ((int64_t)o != 0); return std::nullopt; }));
	options.add("Cyf_PreInfer_Nodes", Option(100000, 10000, 1000000, [](const Option& o){ CyfPreInfer::state().decision_nodes = int(int64_t(o)); return std::nullopt; }));
	options.add("Cyf_PreInfer_RootReady", Option(false, [](const Option& o){ CyfPreInfer::state().root_ready_requested = ((int64_t)o != 0); return std::nullopt; }));
	
	options.add("Cyf_PreInfer_MinBudgetMs", Option(0, 0, 10000, [](const Option& o){ CyfPreInfer::state().min_budget_requested_ms = int(int64_t(o)); return std::nullopt; }));
	
	
	
	
	options.add("Cyf_PreInfer_Auto", Option(false, [](const Option& o){
		Cyfamate::auto_config().requested = ((int64_t)o != 0); Cyfamate::auto_service().invalidate(); return std::nullopt; }));
	options.add("Cyf_PreInfer_Auto_Model", Option(Cyfamate::auto_default_model().c_str(), [](const Option& o){
		Cyfamate::auto_config().model = std::string(o); ++Cyfamate::auto_config().version; Cyfamate::auto_service().invalidate(); return std::nullopt; }));
	options.add("Cyf_PreInfer_Auto_ORT", Option(Cyfamate::auto_default_ort().c_str(), [](const Option& o){
		Cyfamate::auto_config().ort = std::string(o); ++Cyfamate::auto_config().version; Cyfamate::auto_service().invalidate(); return std::nullopt; }));
	options.add("Cyf_PreInfer_Auto_Dump", Option(false, [](const Option& o){ Cyfamate::auto_config().dump = ((int64_t)o != 0); return std::nullopt; }));
	
	options.add("Cyf_PreInfer_Auto_TestDelayMs", Option(0, 0, 5000, [](const Option& o){ Cyfamate::auto_config().test_delay_ms = int(int64_t(o)); return std::nullopt; }));
	
	

	

    manager.tm.add_options(options);

	

    book.add_options(options);

    
    

    
    

    
    
    
    

    
    


    
#if STOCKFISH
	
	options.add("Skill Level", Option(20, 0, 20));

	options.add("Move Overhead", Option(10, 0, 5000));

	
	
	
	
	
	options.add("nodestime", Option(0, 0, 10000));

	options.add("UCI_Chess960", Option(false));

	options.add("UCI_LimitStrength", Option(false));

	
	options.add("UCI_Elo",
		Option(Stockfish::Search::Skill::LowestElo, Stockfish::Search::Skill::LowestElo,
			Stockfish::Search::Skill::HighestElo));

	options.add("UCI_ShowWDL", Option(false));

	

	options.add(  
		"SyzygyPath", Option("", [](const Option& o) {
			Tablebases::init(o);
			return std::nullopt;
			}));

	options.add("SyzygyProbeDepth", Option(1, 1, 100));

	options.add("Syzygy50MoveRule", Option(true));

	options.add("SyzygyProbeLimit", Option(7, 0, 7));

#endif

	
	


	
	Tune::init(options);
}


void YaneuraOuEngine::isready() {
#if defined(CYF_BLEND_DIRTY_SCRUB)
    if(int(int64_t(options["Threads"]))!=1){
        sync_cout << "info string dirty TT scrub requires Threads=1" << sync_endl;
        std::abort();
    }
#endif

	

#if defined(USE_CLASSIC_EVAL)
    
    
    Engine::run_heavy_job([] { Eval::load_eval(); });

    
    
    
    
    
    static bool classic_startpos_initialized = false;
    if (!classic_startpos_initialized) {
        states = StateListPtr(new std::deque<StateInfo>(1));
        pos.set(StartSFEN, &states->back());
        classic_startpos_initialized = true;
    }
#endif

	
	
	

	
    resize_threads();

	
	tt.clear(threads);

	
	clear();

	
    book.read_book();

	
    

    Cyfamate::fusion_service().prepare(Path::Combine(
        Path::Combine(Directory::GetBinaryFolder(), options["EvalDir"]), options["EvalFile"]));
    Cyfamate::auto_service().prepare_isready();   
#if defined(CYF_BLEND)
    Cyfamate::apply_mode2_link(threads.size());   
    Cyfamate::blend_service().prepare();   
#endif
#if defined(CYF_GPU_ACC)
    Eval::NNUE::networks().feature_transformer.PrepareGpuAccumulator();
#endif
#if defined(CYF_GPU_POOL)
    Cyfamate::gpu_pool_search().prepare(threads.size());   
#endif

    sync_cout << "readyok" << sync_endl;
}



void YaneuraOuEngine::clear()
{
    
    

    
    

    main_manager()->bestPreviousAverageScore = VALUE_INFINITE;
    main_manager()->previousTimeReduction    = 0.85;

    main_manager()->callsCnt          = 0;
    main_manager()->bestPreviousScore = VALUE_INFINITE;
#if STOCKFISH
    main_manager()->originalTimeAdjust = -1;
#else
    main_manager()->lastGamePly = 0;
#endif
    main_manager()->tm.clear();
}



void YaneuraOuEngine::set_ponderhit(bool b) {

	
    
	
	
    manager.tm.ponderhitTime = now();

	manager.ponder           = b;
}


void YaneuraOuEngine::resize_threads() {
    

    
    wait_for_search_finished();   

    

    auto worker_factory = [&](SharedState& sharedState, const ThreadIds& ids)
	{

		auto p = make_unique_large_page<Search::YaneuraOuWorker>(
			
			sharedState,
			ids,
			
			*this
		);

		return LargePagePtr<Worker>(p.release());  
    };

    Cyfamate::mate_worker().stop_join();Cyfamate::mpon_worker().stop_join();Cyfamate::zyf_service().pause_join();
    
    
    int _share_req = Cyfamate::options().zyf_share, _share = _share_req;
#if defined(CYF_BLEND)
    const bool _mode2_wanted = Cyfamate::mode2_link().enabled && Cyfamate::mode2_link().mode != 1 && Cyfamate::sh11_fully_on();
    if (_share && _mode2_wanted) _share = 0;
#endif
    auto budget=Cyfamate::cpu_budget(int(options["Threads"]),Cyfamate::options().mate_enable,
                                    Cyfamate::options().mate_threads,_share);
    Cyfamate::active_cpu_budget()=budget;
    threads.set(numaContext.get_numa_config(), {options, threads, tt, sharedHists  },
                updateContext , budget.alpha, worker_factory);
    sync_cout<<"info string cpu_budget total="<<budget.total<<" alpha="<<budget.alpha
             <<" zyf="<<budget.mate<<" share="<<budget.share
             <<(_share!=_share_req?" share_requested="+std::to_string(_share_req)+" share_yield=mode2":std::string())<<sync_endl;

	
	set_tt_size(options["USI_Hash"]);
 
    

    
    threads.ensure_network_replicated();
}


void YaneuraOuEngine::set_tt_size(size_t mb){
	wait_for_search_finished();
	tt.resize(mb, threads);
}



int YaneuraOuEngine::get_hashfull(int maxAge) const
{
	return tt.hashfull(maxAge);
}

namespace {

constexpr size_t QSEARCH_PSV_CHUNK_RECORDS = 65536;

struct QSearchPsvStats {
    u64 records       = 0;
    u64 replaced      = 0;
    u64 decodeErrors  = 0;
    u64 illegalPv     = 0;
    u64 maxLeafPly    = 0;

    void merge(const QSearchPsvStats& rhs) {
        records      += rhs.records;
        replaced     += rhs.replaced;
        decodeErrors += rhs.decodeErrors;
        illegalPv    += rhs.illegalPv;
        maxLeafPly    = std::max(maxLeafPly, rhs.maxLeafPly);
    }
};




bool qsearch_psv_record(YaneuraOuWorker& worker, PsvRecord& record, QSearchPsvStats& stats) {
    ++stats.records;

    Position  pos;
    StateInfo si;

    if (pos.set_from_packed_sfen(record.sfen, &si, false, record.gamePly).is_not_ok())
    {
        ++stats.decodeErrors;
        return false;
    }

    PVMoves pv;
    worker.qsearch_pv(pos, pv);

    if (pv.empty())
        return true;

    std::vector<StateInfo> states(pv.size());
    size_t                 leafPly = 0;

    for (Move move : pv)
    {
        if (!move.is_ok() || !(pos.pseudo_legal_s<true>(move) && pos.legal(move)))
        {
            ++stats.illegalPv;
            return false;
        }

        pos.do_move(move, states[leafPly]);
        ++leafPly;
    }

    pos.sfen_pack(record.sfen);

    const int gamePly = pos.game_ply();
    record.gamePly =
      static_cast<u16>(std::min(gamePly, int((std::numeric_limits<u16>::max)())));

    
    
    if (leafPly & 1)
    {
        record.score = record.score == (std::numeric_limits<s16>::min)()
                       ? (std::numeric_limits<s16>::max)()
                       : s16(-record.score);
        record.game_result = s8(-record.game_result);
    }

    
    record.move = Move16::none().to_u16();

    ++stats.replaced;
    stats.maxLeafPly = std::max<u64>(stats.maxLeafPly, leafPly);
    return true;
}

} 





bool YaneuraOuEngine::qsearch_psv(const std::string& inputPath,
                                  const std::string& outputPath,
                                  size_t             workerCount,
                                  std::string&       message) {

    if (inputPath.empty() || outputPath.empty())
    {
        message = "usage: qsearch_psv input.psv output.psv [workers]";
        return false;
    }

    if (inputPath == outputPath)
    {
        message = "input and output path must be different.";
        return false;
    }

    wait_for_search_finished();

    if (threads.empty())
        resize_threads();

    if (threads.empty())
    {
        message = "no search worker is available.";
        return false;
    }

    if (workerCount == 0)
        workerCount = threads.num_threads();
    else if (workerCount > threads.num_threads())
    {
        message = "requested workers exceed current Threads option.";
        return false;
    }

    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        message = "failed to open input file: " + inputPath;
        return false;
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        message = "failed to open output file: " + outputPath;
        return false;
    }

    std::vector<PsvRecord> records(QSEARCH_PSV_CHUNK_RECORDS);
    QSearchPsvStats                 total;
    u64                             nextProgress = 1000000;

    const auto recordBytes = std::streamsize(sizeof(PsvRecord));
    const auto chunkBytes  = std::streamsize(records.size() * sizeof(PsvRecord));

    while (true)
    {
        input.read(reinterpret_cast<char*>(records.data()), chunkBytes);
        const std::streamsize bytesRead = input.gcount();

        if (bytesRead == 0)
            break;

        if ((bytesRead % recordBytes) != 0)
        {
            message = "truncated psv record at end of input.";
            return false;
        }

        const size_t count = size_t(bytesRead / recordBytes);

        std::atomic<size_t>         nextRecord(0);
        std::vector<QSearchPsvStats> localStats(workerCount);

        for (size_t threadId = 0; threadId < workerCount; ++threadId)
        {
            threads.run_on_thread(threadId, [&, threadId]() {
                auto* worker =
                  static_cast<YaneuraOuWorker*>(threads.threads[threadId]->worker.get());

                while (true)
                {
                    const size_t index = nextRecord.fetch_add(1, std::memory_order_relaxed);
                    if (index >= count)
                        break;

                    qsearch_psv_record(*worker, records[index], localStats[threadId]);
                }
            });
        }

        for (size_t threadId = 0; threadId < workerCount; ++threadId)
            threads.wait_on_thread(threadId);

        for (const auto& stats : localStats)
            total.merge(stats);

        output.write(reinterpret_cast<const char*>(records.data()), bytesRead);
        if (!output)
        {
            message = "failed to write output file: " + outputPath;
            return false;
        }

        if (total.records >= nextProgress)
        {
            sync_cout << "info string qsearch_psv processed " << total.records << " records"
                      << sync_endl;
            nextProgress += 1000000;
        }
    }

    if (input.bad())
    {
        message = "failed to read input file: " + inputPath;
        return false;
    }

    std::ostringstream ss;
    ss << "qsearch_psv done: records=" << total.records
       << " replaced=" << total.replaced
       << " decode_errors=" << total.decodeErrors
       << " illegal_pv=" << total.illegalPv
       << " max_leaf_ply=" << total.maxLeafPly
       << " workers=" << workerCount;
    message = ss.str();
    return total.decodeErrors == 0 && total.illegalPv == 0;
}






void YaneuraOuEngine::cyf_resource_sync(bool game_end) {
    using namespace Cyfamate;
    auto& o = Cyfamate::options();

    if (game_end){
        zyf_armed().store(false, std::memory_order_relaxed);
        return;
    }

    
    
    if (!zyf_reserved() && o.mate_enable && !bench_guard() && (active_cpu_budget().mate<=1 || zyf_defense_active())){
        char hb[24]; std::snprintf(hb, sizeof hb, "%d", std::max(16, o.mate_hash_mb));
#if defined(_WIN32)
        _putenv_s("ZYFAMATE_HASH_MB", hb);
#else
        setenv("ZYFAMATE_HASH_MB", hb, 1);
#endif
        Zyfamate::Pos zp;
        if (Zyfamate::parse_sfen(&zp, StartSFEN.c_str())){
            Zyfamate::g_stop = 0; long nd = 0;
            Zyfamate::solve(&zp, 1000L, 0.05, &nd);   
            zyf_hash_latched() = true;
        }
        zyf_reserved() = true;
        sync_cout << "info string zyf_reserve: Zyfamate threads=" << active_cpu_budget().mate
                  << " hash=" << o.mate_hash_mb << "MB (常駐・解放しない)" << sync_endl;
    }

    
    if (!zyf_armed().load(std::memory_order_relaxed) && o.mate_enable && !bench_guard()
        && Cyfamate::game_phase(pos) >= ZYF_GP_ONSET){
        zyf_armed().store(true, std::memory_order_relaxed);
        if (o.verbose)
            sync_cout << "info string zyf_armed: GP>=" << ZYF_GP_ONSET << " → df-pn 稼働開始" << sync_endl;
    }
}



void YaneuraOuEngine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
#if STOCKFISH
	p.set(pos.fen(), options["UCI_Chess960"], &trace_states->back());
#else
    p.set(pos.sfen(),&trace_states->back());
#endif
    verify_networks();

    
	
}


Value YaneuraOuEngine::evaluate() const {
	verify_networks();
    return Eval::evaluate(pos); 
}



YaneuraOuWorker* YaneuraOuWorker::get_best_thread() const {

    auto& threads = this->threads.threads;

    Thread* bestThread = threads.front().get();
    Value   minScore   = VALUE_NONE;
    
    const bool cyfVoteDepth = Cyfamate::options().vote_depth;
    int cyfMaxCd = 0;
    if (cyfVoteDepth)
        for (auto&& th : threads)
            cyfMaxCd = std::max(cyfMaxCd, int(static_cast<Search::YaneuraOuWorker*>(th->worker.get())->completedDepth));
    auto cyf_ok = [&](Thread* th) {
        return !cyfVoteDepth || int(static_cast<Search::YaneuraOuWorker*>(th->worker.get())->completedDepth) >= cyfMaxCd
            || is_win(th->worker->rootMoves[0].score);
    };
    if (cyfVoteDepth)
        for (auto&& th : threads)
            if (cyf_ok(th.get())) { bestThread = th.get(); break; }

    std::unordered_map<Move, int64_t, Move::MoveHash> votes(
      2 * std::min(threads.size(), bestThread->worker->rootMoves.size()));

    
    for (auto&& th : threads)
        minScore = std::min(minScore, th->worker->rootMoves[0].score);

    
    auto thread_voting_value = [minScore](Thread* th) {
        
		
        auto worker = static_cast<Search::YaneuraOuWorker*>(th->worker.get());
        return (th->worker->rootMoves[0].score - minScore + 14) * int(worker->completedDepth);
    };

    for (auto&& th : threads)
        if (cyf_ok(th.get()))   
            votes[th->worker->rootMoves[0].pv[0]] += thread_voting_value(th.get());

    for (auto&& th : threads)
    {
        if (!cyf_ok(th.get())) continue;   
        const auto bestThreadScore = bestThread->worker->rootMoves[0].score;
        const auto newThreadScore  = th->worker->rootMoves[0].score;

        const auto& bestThreadPV = bestThread->worker->rootMoves[0].pv;
        const auto& newThreadPV  = th->worker->rootMoves[0].pv;

        const auto bestThreadMoveVote = votes[bestThreadPV[0]];
        const auto newThreadMoveVote  = votes[newThreadPV[0]];

        const bool bestThreadInProvenWin = is_win(bestThreadScore);
        const bool newThreadInProvenWin  = is_win(newThreadScore);

        const bool bestThreadInProvenLoss =
          bestThreadScore != -VALUE_INFINITE && is_loss(bestThreadScore);
        const bool newThreadInProvenLoss =
          newThreadScore != -VALUE_INFINITE && is_loss(newThreadScore);

        
        const bool betterVotingValue =
          thread_voting_value(th.get()) * int(newThreadPV.size() > 2)
          > thread_voting_value(bestThread) * int(bestThreadPV.size() > 2);

        if (bestThreadInProvenWin)
        {
            
            if (newThreadScore > bestThreadScore)
                bestThread = th.get();
        }
        else if (bestThreadInProvenLoss)
        {
            
            if (newThreadInProvenLoss && newThreadScore < bestThreadScore)
                bestThread = th.get();
        }
        else if (newThreadInProvenWin || newThreadInProvenLoss
                 || (!is_loss(newThreadScore)
                     && (newThreadMoveVote > bestThreadMoveVote
                         || (newThreadMoveVote == bestThreadMoveVote && betterVotingValue))))
            bestThread = th.get();
    }

    
    
    
    return static_cast<YaneuraOuWorker*>(bestThread->worker.get());
}






#if STOCKFISH
namespace Stockfish {



namespace TB = Tablebases;

void syzygy_extend_pv(const OptionsMap& options,
	const Search::LimitsType& limits,
	Stockfish::Position& pos,
	Stockfish::Search::RootMove& rootMove,
	Value& v);

using namespace Search;

#endif

namespace {






constexpr int SEARCHEDLIST_CAPACITY = 32;
constexpr int mainHistoryDefault    = 0;
using SearchedList                  = ValueList<Move, SEARCHEDLIST_CAPACITY>;





















int correction_value(const YaneuraOuWorker& w, const Position& pos, const Stack* const ss) {
#if defined(CYF_BLEND)
    if (Cyfamate::blend_config().no_correction) return 0;   
#endif
    const Color us    = pos.side_to_move();
    const auto  m     = (ss - 1)->currentMove;
    const auto& shared = w.sharedHistory;
    const int   pcv    = shared.pawn_correction_entry(pos)[us].pawn;
    const int   micv   = shared.minor_piece_correction_entry(pos)[us].minor;
    const int   wnpcv  = shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite;
    const int   bnpcv  = shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack;
    const auto  cntcv =
      m.is_ok() ? (*(ss - 2)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()]
                + (*(ss - 4)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()]
                 : 8;

    
    return 12153 * pcv + 8620 * micv + 12355 * (wnpcv + bnpcv) + 7982 * cntcv;


}




int correction_extra(const YaneuraOuWorker& w, const Position& pos) {
    const Color us = pos.side_to_move();
    const auto& shared = w.sharedHistory;
    return CyfHandCorr::w * int(shared.hand_correction_entry(pos)[us].hand)
         + CyfMajorCorr::w * int(shared.major_correction_entry(pos)[us].major);
}





Value to_corrected_static_eval(const Value v, const int cv) {
	return std::clamp(v + cv / 131072, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);
}

void update_correction_history(const Position&          pos,
                               Stack* const             ss,
                               Search::YaneuraOuWorker& workerThread,
                               const int                bonus) {
    const Move  m  = (ss - 1)->currentMove;
    const Color us = pos.side_to_move();

    
    constexpr int nonPawnWeight = 187;



    auto&         shared        = workerThread.sharedHistory;

    
    shared.pawn_correction_entry(pos)[us].pawn << bonus;
    shared.minor_piece_correction_entry(pos)[us].minor << bonus * 202 / 128;
    shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite << bonus * nonPawnWeight / 128;
    shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack << bonus * nonPawnWeight / 128;
    shared.hand_correction_entry(pos)[us].hand << bonus * CyfHandCorr::u / 128;   
    shared.major_correction_entry(pos)[us].major << bonus * CyfMajorCorr::u / 128;   



    
    if (m.is_ok())
    {
        const Square to = m.to_sq();
        const Piece  pc = pos.piece_on(to);
        (*(ss - 2)->continuationCorrectionHistory)[pc][to] << bonus * 126 / 128;
        (*(ss - 2)->continuationCorrectionHistory)[pc][to] << bonus * 126 / 128;
    }


}













Value value_draw(size_t nodes) { return VALUE_DRAW - 1 + Value(nodes & 0x2); }

Value value_to_tt(Value v, int ply);
Value value_from_tt(Value v, int ply );
void  update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus);
void  update_quiet_histories(const Position&                pos,
                             Stack*                         ss,
                             Search::YaneuraOuWorker& workerThread,
                             Move                           move,
                             int                            bonus);

FORCE_INLINE void update_quiet_histories_body(
  const Position& pos, Stack* ss, Search::YaneuraOuWorker& workerThread, Move move, int bonus);







void update_all_stats(const Position&          pos,
                      Stack*                   ss,
                      Search::YaneuraOuWorker& workerThread,
                      Move                     bestMove,
                      Square                   prevSq,
                      SearchedList&            quietsSearched,
                      SearchedList&            capturesSearched,
                      Depth                    depth,
                      Move                     ttMove);


bool is_shuffling(Move move, Stack* const ss, const Position& pos) {
#if STOCKFISH
    if (pos.capture_stage(move) || pos.rule50_count() < 11)
        return false;
    if (pos.state()->pliesFromNull <= 6 || ss->ply < 18)
        return false;
    return move.from_sq() == (ss - 2)->currentMove.to_sq()
        && (ss - 2)->currentMove.from_sq() == (ss - 4)->currentMove.to_sq();
#else
    
    if (pos.capture_stage(move) || move.is_drop())
        return false;

    if (pos.state()->pliesFromNull <= 6 || ss->ply < 18)
        return false;

    const Move move2 = (ss - 2)->currentMove;
    const Move move4 = (ss - 4)->currentMove;

    if (!move2.is_ok() || !move4.is_ok() || move2.is_drop() || move4.is_drop())
        return false;

    return move.from_sq() == move2.to_sq() && move2.from_sq() == move4.to_sq();
#endif
}









template<typename RandomIt>
void stable_sort_root_moves(RandomIt first, RandomIt last) {
    const size_t n = size_t(last - first);
    if (n < 2 || std::is_sorted(first, last))
        return;

    std::vector<uint32_t> idx(n);
    for (size_t i = 0; i < n; ++i)
        idx[i] = uint32_t(i);
    std::stable_sort(idx.begin(), idx.end(),
                     [first](uint32_t a, uint32_t b) { return first[a] < first[b]; });

    
    for (size_t k = 0; k < n; ++k)
    {
        if (idx[k] == k)
            continue;
        auto   tmp = std::move(first[k]);
        size_t j   = k;
        while (idx[j] != k)
        {
            const size_t src = idx[j];
            first[j]         = std::move(first[src]);
            idx[j]           = uint32_t(j);
            j                = src;
        }
        first[j] = std::move(tmp);
        idx[j]   = uint32_t(j);
    }
}


}  


#if STOCKFISH
Search::Worker::Worker(SharedState& sharedState,
	std::unique_ptr<ISearchManager> sm,
	size_t                          threadId,
	NumaReplicatedAccessToken       token) :
	
	threadIdx(threadId),
	numaAccessToken(token),
	manager(std::move(sm)),
	options(sharedState.options),
	threads(sharedState.threads),
	tt(sharedState.tt),
	networks(sharedState.networks),
	refreshTable(networks[token]) {
	clear();
}
#endif


bool cyf_privhist_active() { return Cyfamate::options().mt_privhist; }   
static std::mutex& cyf_priv_hist_mutex() { static std::mutex m; return m; }
static std::map<size_t, std::unique_ptr<SharedHistories>>& cyf_priv_hist_pool() {
    static std::map<size_t, std::unique_ptr<SharedHistories>> p; return p; }
static SharedHistories& cyf_priv_hist(size_t idx) {
    std::lock_guard<std::mutex> lk(cyf_priv_hist_mutex()); auto& e = cyf_priv_hist_pool()[idx];
    if (!e) e = std::make_unique<SharedHistories>(1); return *e;
}
static bool cyf_is_priv_hist(const SharedHistories* h) {
    std::lock_guard<std::mutex> lk(cyf_priv_hist_mutex());
    for (auto& kv : cyf_priv_hist_pool()) if (kv.second.get() == h) return true;
    return false;
}
Search::YaneuraOuWorker::YaneuraOuWorker(SharedState& sharedState,
#if STOCKFISH
                                         std::unique_ptr<ISearchManager>,
                                         size_t                    threadIdx,
                                         size_t                    numaThreadIdx,
                                         size_t                    numaTotal,
                                         NumaReplicatedAccessToken token,
#else
                                         const ThreadIds& ids,
#endif
										 YaneuraOuEngine&          engine) :
	
    sharedHistory(Cyfamate::options().mt_privhist ? cyf_priv_hist(ids.threadIdx)   
                  : sharedState.sharedHistories.at(ids.numaAccessToken.get_numa_index())),
	
	Search::Worker(sharedState, ids),
	engine(engine),
	manager(engine.manager)
{

    

	
	
}

void Search::YaneuraOuWorker::ensure_network_replicated() {
	
	#if 0
    
    
    (void) (networks[numaAccessToken]);
	#endif
}

void Search::YaneuraOuWorker::pre_start_searching() {

	if (is_mainthread())
        
        main_manager()->pre_start_searching(*this);

    
    
#if STOCKFISH
    nmpMinPly       = 0;
#endif
    bestMoveChanges = 0;
    rootDepth = completedDepth = 0;

    
    auto& search_options = main_manager()->search_options;
    rootPos.set_ekr(search_options.enteringKingRule);
    rootPos.prepare_repetition_root();   

	
	
	if (is_mainthread())
    {
        
		
        auto bestMove = rootPos.DeclarationWin();
        if (bestMove != Move::none())
        {
            
            
            
            

            auto it_move = std::find(rootMoves.begin(), rootMoves.end(), bestMove);
            if (it_move == rootMoves.end()) 
                rootMoves.emplace_back(Move::win());
        }
    }
}

void Search::YaneuraOuWorker::start_searching() {

#if defined(USE_SFNN)
    
    
    accumulatorStack.reset();
#endif

    
    lastIterationPV.clear();

    
    

    if (!is_mainthread())
    {
        {   
            const auto& b = Cyfamate::active_cpu_budget();
            const int first = b.alpha - b.mate;
            elasticRank = (b.share == 1 && b.mate > 0 && int(threadIdx) >= first) ? int(threadIdx) - first : -1;
            elasticTick = 0;
        }
        iterative_deepening();
        return;
    }

    
#if STOCKFISH
    main_manager()->tm.init(limits, rootPos.side_to_move(), rootPos.game_ply(), options,
                            main_manager()->originalTimeAdjust);
#else
    auto& mainManager = *main_manager();

    mainManager.tm.init(limits, rootPos.side_to_move(), rootPos.game_ply(), options,
                        mainManager.search_options.max_moves_to_draw);
#endif

    
    
    Cyfamate::tm_optimum_hint() = (!limits.ponderMode && limits.use_time_management())
        ? (int)main_manager()->tm.optimum() : 0;
    Cyfamate::zyf_verify_quota().store(32); 
    Cyfamate::zyf_verify_stats().reset();
    int64_t _zyf_ms = 0;
    if (!limits.ponderMode) {
        if (limits.movetime) _zyf_ms = limits.movetime;
        else if (limits.use_time_management()) _zyf_ms = main_manager()->tm.maximum();
    }
    Cyfamate::zyf_action_deadline().store(_zyf_ms ? CyfZyf::now_us() + std::max<int64_t>(0,_zyf_ms-(now()-limits.startTime))*1000 : 0);
    
    Cyfamate::zyf_ponderhit_ms().store(limits.ponderMode && limits.use_time_management() ? int64_t(main_manager()->tm.maximum()) : 0);
    Cyfamate::zyf_go_gen() = limits.cyfGoGen;   
    CyfSharp::state().begin(limits.cyfGoGen, uint64_t(rootPos.key()), rootPos.game_ply());
    Cyfamate::on_root_mainthread(rootPos, rootMoves, limits.ponderMode);
#if defined(CYF_REPLAY)
    if (Cyfamate::replay().enabled)
        Cyfamate::replay().begin_go(rootPos, threads.size() == 1 && !Cyfamate::options().mate_enable);
#endif

    
    

    
    bool uciPvSent = false;

    
    
    

    
    tt.new_search();
    
    
    CyfRootHint::begin_go(limits.cyfGoGen, threads.size() == 1, uint64_t(rootPos.key()));
    CyfDefense::clear();
    Cyfamate::zyf_root_safe().store(false, std::memory_order_relaxed);
    
    CyfFrontier::begin_search(limits.ponderMode);
    if (CyfPreInfer::state().enabled.load(std::memory_order_relaxed)) {
        const int rules_legal=int(MoveList<LEGAL_ALL>(rootPos).size());
        const int search_legal=int(MoveList<LEGAL>(rootPos).size());
        CyfPreInfer::state().begin(limits.cyfGoGen,uint64_t(rootPos.key()),rootPos.game_ply(),rules_legal,
            threads.size()==1 && int(options["MultiPV"])==1 && !limits.ponderMode && int(rootMoves.size())==search_legal,
            search_legal);
    }
    if (Cyfamate::auto_config().requested.load(std::memory_order_relaxed))   
        Cyfamate::auto_service().begin_go(limits.cyfGoGen, uint64_t(rootPos.key()), threads.size(), int(options["MultiPV"]),
            limits.ponderMode, int(rootMoves.size()) == int(MoveList<LEGAL>(rootPos).size()));
    else
        Cyfamate::auto_service().go = {};
    CyfIter::begin_search(limits.cyfGoGen, limits.ponderMode, uint64_t(rootPos.key()), rootPos.game_ply(),
                          rootPos.side_to_move() == BLACK ? 'b' : 'w', int(rootMoves.size()), int(threads.size()));
#if defined(CYF_GPU_ACC)
    if(threads.size()!=1){sync_cout << "info string GPU accumulator requires Threads=1" << sync_endl;std::abort();}
#endif
#if defined(CYF_DEMAND_TRACE)
    CyfDemand::begin_search();   
#endif
#if defined(CYF_BLEND)
    Cyfamate::blend_service().begin_go(tt, threads.size(), limits.ponderMode);   
#endif
#if defined(CYF_GPU_POOL)
    if (threads.size() != 1) { sync_cout << "info string GPU pool requires Threads=1" << sync_endl; std::abort(); }
    Cyfamate::gpu_pool_search().begin_go();
#endif
#if defined(CYF_EVAL_PROFILE)
    Cyfamate::eval_cycles() = 0; Cyfamate::eval_calls() = 0; Cyfamate::go_tsc0() = __rdtsc();
    Eval::NNUE::FeatureTransformer::eval_prof() = {};
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
    Cyfamate::Gp8::Threat::enum_prof() = {};   
#endif
#endif 
#endif

#if STOCKFISH
#else
    

    auto& search_options = mainManager.search_options;

    
    search_options.lastPvInfoTime = limits.startTime;

    
    
    
    search_options.computed_pv_interval =
      (limits.infinite || limits.disablePvInterval || search_options.consideration_mode)
        ? 0
        : search_options.pv_interval;

    

    
    Color us         = rootPos.side_to_move();
    int   draw_value = (int) ((us == BLACK ? options["DrawValueBlack"] : options["DrawValueWhite"])
                            * Eval::PawnValue / 100);

    
    
    drawValueTable[REPETITION_DRAW][us]  = +draw_value;
    drawValueTable[REPETITION_DRAW][~us] = -draw_value;
    
    
    if (cyfSPDrawSign && limits.ponderMode && options.count("Stochastic_Ponder") && bool(options["Stochastic_Ponder"])) {
        const Color me = ~us;
        const int dv = (int) ((me == BLACK ? options["DrawValueBlack"] : options["DrawValueWhite"]) * Eval::PawnValue / 100);
        drawValueTable[REPETITION_DRAW][me]  = +dv;
        drawValueTable[REPETITION_DRAW][~me] = -dv;
        sync_cout << "info string sp drawsign me=" << (me == BLACK ? "b" : "w") << " draw " << dv << sync_endl;
    }

    
    
    bool search_skipped = true;

    
    
    
    mainManager.ponder_candidate = Move::none();

    
    Book::ProbeResult probeResult;

#if defined(SHOGI24)
    
    
    

    
    
    
    
    
    
    
    
    
    
    

    auto their_king = rootPos.square<KING>(~us);
    auto our_piece  = rootPos.attackers_to(their_king) & rootPos.pieces(us);
    
    if (our_piece)
    {
        Square from = our_piece.pop();
        Square to   = their_king;
        Move16 m16  = make_move16(from, to);
        Move   m    = rootPos.to_move(m16);

        
        

        
        
        

        probeMove.bestmove  = m;
        probeMove.bestscore = mate_in(1);
        goto SKIP_SEARCH;
    }
#endif

    
#endif

    
    
    

    if (rootMoves.empty())
    {
        

        probeResult.bestmove  = Move::resign();
        probeResult.bestscore = mated_in(1);

		
        rootMoves.emplace_back(Move::none());

#if STOCKFISH
        main_manager()->updates.onUpdateNoMoves(
          {0, {rootPos.checkers() ? -VALUE_MATE : VALUE_DRAW, rootPos}});
        
#else
		
#endif

        goto SKIP_SEARCH;
    }

    
    
    

    {
        
        
        
        
        

        auto bestMove = rootPos.DeclarationWin();
        if (bestMove != Move::none())
        {
            probeResult.bestmove  = bestMove;
            probeResult.bestscore = mate_in(1);
            goto SKIP_SEARCH;
        }
    }

    
    
    

    probeResult = engine.book.probe(rootPos, main_manager()->updates);
    if (probeResult.bestmove)
        goto SKIP_SEARCH;

    
    
    

    if (!threads.stop && CyfPreInfer::state().enabled.load(std::memory_order_relaxed)) {
        const bool _gate_tm=limits.use_time_management() && !limits.ponderMode;
        Cyfamate::preinfer_root_ready(rootPos,limits.cyfGoGen,now()-limits.startTime,
            _gate_tm ? int64_t(main_manager()->tm.optimum()) : -1,
            _gate_tm ? int64_t(main_manager()->tm.maximum()) : -1);
    }
    threads.start_searching();  
    
    
    
    

    uciPvSent = iterative_deepening();  
    

    
    
    
    
    

    
    
    
    
    

    
    
    
    
    
    
    
    
    
    
    

    
    
    search_skipped = false;

SKIP_SEARCH:

    while (!threads.stop && (main_manager()->ponder || limits.infinite))
    {
        
        

        
        
        Tools::sleep(1);
        


        
    }

    
    
    
    
    threads.stop = true;

    
    
    

    threads.wait_for_search_finished();
    
    
    Cyfamate::auto_service().end_go(limits.cyfGoGen);


#if STOCKFISH
    
    
    
    

    
    
    
    
    

    if (limits.npmsec)
        main_manager()->tm.advance_nodes_time(threads.nodes_searched()
                                              - limits.inc[rootPos.side_to_move()]);
#endif

    

    

#if STOCKFISH
    
    
    Worker* bestThread = this;

    Skill skill =
      Skill(options["Skill Level"], options["UCI_LimitStrength"] ? int(options["UCI_Elo"]) : 0);
    
    

#else
    YaneuraOuWorker* bestThread = this;

    Skill skill = Skill(20, 0);
#endif

    if (int(options["MultiPV"]) == 1 && !limits.depth && !limits.mate && !skill.enabled()
        && rootMoves[0].pv[0] != Move::none())
#if STOCKFISH
        bestThread = threads.get_best_thread()->worker.get();
#else
        
        bestThread = get_best_thread();
#endif
    if (Cyfamate::options().mt_obs)   
        for (size_t _i = 0; _i < threads.threads.size(); ++_i) {
            auto* _w = static_cast<YaneuraOuWorker*>(threads.threads[_i]->worker.get());
            sync_cout << "info string mtobs thread " << _i << " cdepth " << int(_w->completedDepth) << " nodes " << _w->nodes.load(std::memory_order_relaxed)
                      << " move " << _w->rootMoves[0].pv[0].to_usi_string() << " score " << int(_w->rootMoves[0].score)
                      << " chosen " << int(_w == bestThread) << sync_endl;
        }

    
    bool _cyf_mate_override = false;
    bool _cyf_root_advice = false;
    if (Cyfamate::options().mate_enable){
        if (Cyfamate::zyf_go().active){   
            Cyfamate::zyf_service().pause_join();
            Cyfamate::zyf_service().end_go();
        }
        Cyfamate::mate_worker().stop_join();
        Cyfamate::mpon_worker().stop_join();   
        if (Cyfamate::play_zyf_root_hint(bestThread->rootPos,bestThread->rootMoves)){
            _cyf_mate_override=true; 
            _cyf_root_advice=true;
            sync_cout << "info string cyfzyf_play_hint move=" << bestThread->rootMoves[0].pv[0]
                      << " source=solver_root score_added=0 distance=unknown" << sync_endl;
        }
        else if (Cyfamate::mate_table().active && Cyfamate::mate_root().state.load() == 1
            && Cyfamate::mate_root().move_valid.load()
            && Cyfamate::zyf_action_live(Cyfamate::mate_root().expiry.load())){
            auto& rp = bestThread->rootPos;
            Move mm = Cyfamate::csa_to_move(rp, std::string(Cyfamate::mate_root().move_csa));
            if (mm != Move::none() && rp.pseudo_legal_s<true>(mm) && rp.legal(mm)) {
                
                
                
                
                const int L = -1;
                if (L > 0) for (size_t i=0;i<bestThread->rootMoves.size();++i)
                    if (bestThread->rootMoves[i].pv[0] == mm){
                        std::swap(bestThread->rootMoves[0], bestThread->rootMoves[i]);
                        auto& rm = bestThread->rootMoves[0];
                        rm.score = rm.uciScore = rm.averageScore = mate_in(L);
                        rm.scoreLowerbound = true; rm.scoreUpperbound = false; rm.zyfMateLower = true;
                        _cyf_mate_override = true; ++Cyfamate::zyf_verify_stats().use_root;
                        if (Cyfamate::options().verbose)
                            sync_cout << "info string Cyfamate mate verified_lower L=" << L << " move=" << mm << sync_endl;
                        break;
                    }
            }
        }
    }
    
    
    
    
    
    
    const bool _cyf_throwaway_ponder = main_manager()->ponder;

    
    Cyfamate::fusion_service().end();
    Cyfamate::harvest_probe().disable();
#if defined(CYF_EVAL_PROFILE)
    sync_cout << "info string cyfevalprof total_tsc=" << (__rdtsc() - Cyfamate::go_tsc0())
              << " eval_tsc=" << Cyfamate::eval_cycles() << " eval_calls=" << Cyfamate::eval_calls()
              << " nodes=" << threads.nodes_searched() << sync_endl;
    {
        const auto& p = Eval::NNUE::FeatureTransformer::eval_prof();
        sync_cout << "info string cyfevalparts computed_n=" << p.n[0] << " computed_cyc=" << p.cyc[0]
                  << " parent_n=" << p.n[1] << " parent_cyc=" << p.cyc[1] << " ancestor_n=" << p.n[2]
                  << " ancestor_cyc=" << p.cyc[2] << " refresh_n=" << p.n[3] << " refresh_cyc=" << p.cyc[3]
                  << " forced_n=" << p.n[4] << " forced_cyc=" << p.cyc[4] << " transform_n=" << p.transform_n
#if defined(CYF_GP8)
                  << " transform_cyc=" << p.transform_cyc
                  << " threat_enum_n=" << p.threat_enum_n << " threat_enum_cyc=" << p.threat_enum_cyc
                  << " threat_rows=" << p.threat_rows << " threat_reset_n=" << p.threat_reset_n
                  << " threat_collect_cyc=" << p.threat_collect_cyc << " threat_finny_rows=" << p.threat_finny_rows << sync_endl;
#if defined(CYF_THREAT)
        
        const auto& ep = Cyfamate::Gp8::Threat::enum_prof();
        sync_cout << "info string cyfthreatparts enum_calls=" << ep.calls << " collect_cyc=" << ep.collect_cyc
                  << " bb_cyc=" << ep.bb_cyc << " enum_cyc=" << ep.enum_cyc << " index_cyc=" << ep.index_cyc
                  << " prefetch_cyc=" << ep.prefetch_cyc << " changed_sq=" << ep.changed_sq << " cand=" << ep.cand
                  << " full_n=" << ep.full_n << " full_enum_cyc=" << ep.full_enum_cyc << " full_sort_cyc=" << ep.full_sort_cyc << sync_endl;
#endif
#else
                  << " transform_cyc=" << p.transform_cyc << sync_endl;
#endif 
    }
#endif
#if defined(CYF_BLEND)
    Cyfamate::blend_service().end_go(bestThread->rootPos, bestThread->rootMoves);
#endif
#if defined(CYF_GPU_ACC)
    CyfGpuAcc::service().finish();
#endif
#if defined(CYF_GPU_POOL)
    Cyfamate::gpu_pool_search().finish();   
#endif
#if defined(CYF_DEMAND_TRACE)
    CyfDemand::end_search(threads.nodes_searched(), Eval::NNUE::networks().feature_transformer.DemandModelGeneration(),
                          [](const char* l) { sync_cout << l << sync_endl; });
#endif

    
    
    
    if (false  && Cyfamate::options().mate_enable && Cyfamate::zyf_armed().load(std::memory_order_relaxed) && !_cyf_mate_override
        && !Cyfamate::bench_guard()
        && !_cyf_throwaway_ponder                        
        && Cyfamate::options().tonshi_ms > 0 && !bestThread->rootMoves.empty()){
        
        
        
        Position& _rp = bestThread->rootPos;
        const size_t _cand = std::min<size_t>(bestThread->rootMoves.size(), 4);
        const double _hint_ms = (double)Cyfamate::tm_optimum_hint();
        double _tb = Cyfamate::options().tonshi_ms / 1000.0;
        if (_hint_ms > 0.0){
            if (_hint_ms < 200.0) _tb = 0.0;
            else _tb = std::min(_tb, (_hint_ms / 8.0) / std::max<size_t>(1, _cand) / 1000.0);
        }
        size_t _chosen = 0; bool _decided = false;
        for (size_t i = 0; _tb > 0.0 && i < _cand; ++i){
            if (bestThread->rootMoves[i].pv.empty()) continue;
            Move _mv = bestThread->rootMoves[i].pv[0];
            if (_mv == Move::none()) continue;
            StateInfo _si; _rp.do_move(_mv, _si);
            std::string _opp = _rp.sfen();
            const uint64_t _okey = (uint64_t)_rp.key();
            _rp.undo_move(_mv);
            Zyfamate::Pos _zp;
            if (!Zyfamate::parse_sfen(&_zp, _opp.c_str())){ _chosen = i; _decided = true; break; }  
            uint8_t _st=0; int16_t _ln2=0;
            int _rr=2;
            if (Cyfamate::mate_table().load(_okey,_st,_ln2)) _rr = _st==1 ? 1 : 2;
            else if (Cyfamate::zyf_action_live()) {
                Zyfamate::g_stop=0; long _nd=0;
                const auto hard=Cyfamate::zyf_action_deadline().load();
                if (hard) _tb=std::min(_tb,std::max(0.0,(hard-CyfZyf::now_us())/1e6));
                if (_tb>0) _rr=Zyfamate::solve(&_zp,3000000L,_tb,&_nd);
            }
            int bound=-1;
            if (_rr==1) {
                _rp.do_move(_mv,_si);
                
                
                
                bound=-1;
                _rp.undo_move(_mv);
            }
            if (bound<0) { _chosen=i; _decided=true; break; } 
            
        }
        if (_decided && _chosen != 0){
            const Value _orig_best = bestThread->rootMoves[0].score;   
            std::string _from = bestThread->rootMoves[0].pv[0].to_usi_string();
            auto _t = bestThread->rootMoves[_chosen];
            bestThread->rootMoves.erase(bestThread->rootMoves.begin() + _chosen);
            bestThread->rootMoves.insert(bestThread->rootMoves.begin(), _t);
            
            
            
            Value _sane = (bestThread->rootMoves[0].averageScore != -VALUE_INFINITE)
                            ? bestThread->rootMoves[0].averageScore
                            : (_orig_best != -VALUE_INFINITE ? _orig_best : VALUE_ZERO);
            bestThread->rootMoves[0].score = bestThread->rootMoves[0].uciScore
                = bestThread->rootMoves[0].averageScore = _sane;
            if (Cyfamate::options().verbose)
                sync_cout << "info string tonshi_guard fired move=" << _from
                          << " -> " << bestThread->rootMoves[0].pv[0].to_usi_string() << sync_endl;
        }
    }

    
    
    
    
    
    
    if (Cyfamate::options().zyf_tonshi_switch > 0 && Cyfamate::zyf_go().active && !_cyf_mate_override
        && !Cyfamate::bench_guard() && !_cyf_throwaway_ponder && bestThread->rootMoves.size() > 1
        && !bestThread->rootMoves[0].pv.empty() && bestThread->rootMoves[0].score < VALUE_MATE_IN_MAX_PLY){
        auto& _zg = Cyfamate::zyf_go();
        Cyfamate::zyf_service().drain(_zg.results);
        Cyfamate::zyf_safe_refresh(rootPos, bestThread->rootMoves);
        auto _verdict = [&](const auto& rm){   
            return Cyfamate::zyf_cand_verdict(rootPos, rm);
        };
        auto& _rms = bestThread->rootMoves;
        if (_verdict(_rms[0]) == 1){
            size_t _to = 0; const char* _alt = "";
            const uint16_t _safe=CyfDefense::preferred(_zg.gen, uint64_t(rootPos.key()), [](uint16_t){return true;});
            for (size_t i = 1; i < _rms.size() && !_to; ++i)
                if (!_rms[i].pv.empty() && uint16_t(_rms[i].pv[0].raw())==_safe){ _to=i; _alt="safe"; }
            if (!_to && Cyfamate::options().zyf_tonshi_switch >= 2)
                for (size_t i = 1; i < _rms.size() && !_to; ++i) if (_verdict(_rms[i]) < 0){ _to = i; _alt = "unproven"; }
            if (_to){
                const Value _orig_best = _rms[0].score;
                _zg.tonshi_from = _rms[0].pv[0].to_usi_string();
                auto _t = _rms[_to];
                _rms.erase(_rms.begin() + _to);
                _rms.insert(_rms.begin(), _t);
                
                Value _sane = (_rms[0].averageScore != -VALUE_INFINITE) ? _rms[0].averageScore
                            : (_orig_best != -VALUE_INFINITE ? _orig_best : VALUE_ZERO);
                _rms[0].score = _rms[0].uciScore = _rms[0].averageScore = _sane;
                _zg.tonshi_to = _rms[0].pv[0].to_usi_string(); _zg.tonshi_alt = _alt;
                sync_cout << "info string cyfzyf tonshi_switch from=" << _zg.tonshi_from << " to=" << _zg.tonshi_to
                          << " alt=" << _alt << " authority=zyf_cand_proof" << sync_endl;
            }
        }
    }

    
    
    

    
    
    
    if (Cyfamate::options().enable && !Cyfamate::bench_guard() && !bestThread->rootMoves.empty()){
        int _tesuki = -1;
        const double _hint_ms2 = (double)Cyfamate::tm_optimum_hint();
        double _tes_ms = (double)Cyfamate::options().tm_tesuki_ms;
        if (_hint_ms2 > 0.0) _tes_ms = std::min(_tes_ms, _hint_ms2 / 10.0);
        
        
        if (!limits.ponderMode && limits.use_time_management())
            _tes_ms = std::min(_tes_ms, std::max(0.0, double(main_manager()->tm.remain()) - double(main_manager()->tm.elapsed_time()) - 10.0));
        if (Cyfamate::options().mate_enable && Cyfamate::zyf_armed().load(std::memory_order_relaxed) && _tes_ms >= 5.0 && !_cyf_throwaway_ponder
            && !bestThread->rootMoves[0].pv.empty()
            && bestThread->rootMoves[0].pv[0] != Move::none()
            && bestThread->rootMoves[0].pv[0] != Move::win()){
            Position& _tp = bestThread->rootPos;
            const Move _bm = bestThread->rootMoves[0].pv[0];
            StateInfo _tsi; _tp.do_move(_bm, _tsi);
            const std::string _after = _tp.sfen();
            _tp.undo_move(_bm);
            Zyfamate::Pos _zt;
            if (Zyfamate::parse_sfen(&_zt, _after.c_str())){
                const double _overall = _tes_ms / 1000.0;
                Zyfamate::g_stop = 0;
                Zyfamate::g_tes_deadline = Zyfamate::now() + _overall;
                const int _r = Zyfamate::tesuki(&_zt, 3, 300000L, Zyfamate::tes_each(_overall));
                if (_r >= 0) _tesuki = _r;          
            }
        }
        float _dconf = -1.f;
        const std::string _dtop = Cyfamate::last_dl_valid() ? Cyfamate::last_dl_argmax(_dconf) : std::string();
        const bool _dl_ok = !_dtop.empty() && _dconf >= 0.f;
        const std::string _btop = bestThread->rootMoves[0].pv.empty() ? std::string()
                                : bestThread->rootMoves[0].pv[0].to_usi_string();
        const Value _bs = bestThread->rootMoves[0].score;
        const bool _cp_ok = _bs != -VALUE_INFINITE && _bs != VALUE_INFINITE;
        const auto& _pv = bestThread->rootMoves[0].pv;
        const std::string _pred = _pv.size() >= 3 ? _pv[2].to_usi_string() : std::string();
        Cyfamate::post_move_metrics(bestThread->rootPos.game_ply(),
                                    _cp_ok ? USIEngine::to_cp(_bs) : 0, _cp_ok,
                                    _dl_ok, _dl_ok ? (double)_dconf : 0.0,
                                    _dl_ok && _dtop == _btop, _tesuki, _btop, _pred);
        if (Cyfamate::options().verbose)
            sync_cout << "info string csm momentum=" << Cyfamate::last_momentum()
                      << " sharpness=" << Cyfamate::last_sharpness()
                      << " tesuki=" << _tesuki << sync_endl;
    }

    
    
    if (!Cyfamate::bench_guard() && !bestThread->rootMoves.empty() && !bestThread->rootMoves[0].pv.empty()){
        const Value _rs = bestThread->rootMoves[0].score;
        const bool _rok = _rs != -VALUE_INFINITE && _rs != VALUE_INFINITE;
        sync_cout << "info string cyfrec ply=" << bestThread->rootPos.game_ply()
                  << " bm=" << bestThread->rootMoves[0].pv[0].to_usi_string()
                  << " cp=" << (_rok ? USIEngine::to_cp(_rs) : 0) << " cp_ok=" << int(_rok)
                  << " depth=" << bestThread->completedDepth << " nodes=" << threads.nodes_searched()
                  << " elapsed_ms=" << main_manager()->tm.elapsed_time()
                  << " optimum_ms=" << main_manager()->tm.optimum() << " maximum_ms=" << main_manager()->tm.maximum()
                  << " gp=" << Cyfamate::game_phase(bestThread->rootPos)
                  << " momentum=" << Cyfamate::last_momentum() << " sharpness=" << Cyfamate::last_sharpness()
                  << " cyf=" << int(Cyfamate::options().enable) << " tm=" << int(Cyfamate::options().tm_enable)
                  << " zyf_armed=" << int(Cyfamate::zyf_armed().load(std::memory_order_relaxed))
                  << " seldepth=" << bestThread->rootMoves[0].selDepth << " hashfull=" << tt.hashfull()
                  << " ponder=" << int(limits.ponderMode) << " throwaway=" << int(_cyf_throwaway_ponder)
                  << " tesuki=" << Cyfamate::last_tesuki_n() << sync_endl;   
    }
    main_manager()->bestPreviousScore        = bestThread->rootMoves[0].score;
    main_manager()->bestPreviousAverageScore = bestThread->rootMoves[0].averageScore;
#if !STOCKFISH
	
    main_manager()->lastGamePly = rootPos.game_ply();
#endif

#if STOCKFISH
    
    
    if (bestThread != this)
        main_manager()->pv(*bestThread, threads, tt, bestThread->completedDepth);

    
    

#else
    
    
    
    
    
    
    
    
    
    
    
    if (!search_skipped && bestThread->rootMoves[0].pv.size() == 1
        && bestThread->rootMoves[0].extract_ponder_from_tt(tt, rootPos,
                                                           main_manager()->ponder_candidate))
        uciPvSent = false;

    
    
    
    
    
    
    
    bool resign_by_value = false;
    auto resign_value    = (int) options["ResignValue"];
    if (!search_skipped && bestThread->rootMoves[0].score != -VALUE_INFINITE)
    {
        Value resign_score = bestThread->rootMoves[0].uciScore;
        if (resign_score == -VALUE_INFINITE)
            resign_score = VALUE_ZERO;

        resign_by_value = USIEngine::to_cp(resign_score) <= -resign_value;
    }

    
    
    
    
    
    
    
    
    if (!uciPvSent || bestThread != this || resign_by_value)
    {
        if (search_skipped)
        {
            
            

            InfoFull info;
            info.depth     = 0;
            info.selDepth  = 0;
            info.multiPV   = 1;
            info.score     = probeResult.bestscore;
            TimePoint time = std::max(TimePoint(1), mainManager.tm.elapsed_time());
            info.timeMs    = time;
            info.nodes     = 0;
            info.nps       = 0;
            std::string pv = probeResult.bestmove.to_usi_string();
            if (probeResult.pondermove)
                pv += " " + probeResult.pondermove.to_usi_string();
            info.pv       = pv;
            info.hashfull = tt.hashfull();
            mainManager.updates.onUpdateFull(info);
        }
        else
        {
            main_manager()->pv(*bestThread, threads, tt, bestThread->completedDepth);
        }
    }

    if (resign_by_value)
    {
        
        search_skipped = true;
        probeResult    = Book::ProbeResult(Move::resign());
    };

#endif

    
    

    std::string ponder;

#if STOCKFISH
    if (bestThread->rootMoves[0].pv.size() > 1
        || bestThread->rootMoves[0].extract_ponder_from_tt(tt, rootPos))
        ponder = UCIEngine::move(bestThread->rootMoves[0].pv[1], rootPos.is_chess960());

    auto bestmove = UCIEngine::move(bestThread->rootMoves[0].pv[0], rootPos.is_chess960());
#else

	std::string bestmove;
    if (search_skipped)
    {
        bestmove = probeResult.bestmove.to_usi_string();
        if (probeResult.pondermove)
	        ponder   = probeResult.pondermove.to_usi_string();
	}
    else
    {
		
		
        if (bestThread->rootMoves[0].pv.size() > 1
            || bestThread->rootMoves[0].extract_ponder_from_tt(tt, rootPos,
                                                               main_manager()->ponder_candidate))
            ponder = USIEngine::move(bestThread->rootMoves[0].pv[1]);

        bestmove = USIEngine::move(bestThread->rootMoves[0].pv[0]);
    }

	

#endif

#if defined(CYF_REPLAY)
    if (Cyfamate::replay().enabled)
        Cyfamate::replay().end_go(threads.nodes_searched(), bestmove);
#endif
    
    
    
    
    
    
    
    if (Cyfamate::parent_info_enabled() && !Cyfamate::bestmove_muted())
    {
        std::string score = "none", bound = "none", prev = "none";
        int depth = 0, rootdepth = 0, seldepth = 0, effort_pm = -1;
        const uint64_t nodes_all = threads.nodes_searched();
        auto fmt = [](Value v) {
            if (std::abs(v) < VALUE_MATE_IN_MAX_PLY) return "cp:" + std::to_string(USIEngine::to_cp(v));
            const int n = v > 0 ? VALUE_MATE - v : -VALUE_MATE - v;   
            return std::string("mate:") + (v < 0 && n == 0 ? std::string("-0") : std::to_string(n));
        };
        if (!search_skipped && !bestThread->rootMoves.empty())
        {
            const auto& rm = bestThread->rootMoves[0];
            if (rm.uciScore != -VALUE_INFINITE && rm.uciScore != VALUE_NONE)
            {
                score = fmt(rm.uciScore);
                bound = rm.scoreLowerbound ? "lower" : rm.scoreUpperbound ? "upper" : "exact";
            }
            
            
            if (rm.previousScore != -VALUE_INFINITE && rm.previousScore != VALUE_NONE)
                prev = fmt(rm.previousScore);
            depth     = int(bestThread->completedDepth);
            rootdepth = int(bestThread->rootDepth);
            seldepth  = rm.selDepth;
            effort_pm = nodes_all ? int(rm.effort * 1000 / nodes_all) : -1;
        }
        const char* rule = "nnue";
#if defined(CYF_BLEND)
        if (Cyfamate::blend_active()) rule = "blend";
#endif
        if (_cyf_root_advice) rule = "zyf_move";
        sync_cout << "info string cyfroot v=1 gen=" << limits.cyfGoGen << " ply=" << rootPos.game_ply()
                  << " move=" << bestmove << " score=" << score << " bound=" << bound << " prev=" << prev << " depth=" << depth
                  << " rootdepth=" << rootdepth << " seldepth=" << seldepth << " nodes=" << nodes_all
                  << " ms=" << main_manager()->tm.elapsed_time() << " effort=" << effort_pm
                  << " src=" << (search_skipped ? "book" : "search") << " rule=" << rule << sync_endl;
        sync_cout << "info string pyfa_ack " << limits.cyfGoGen << sync_endl;
    }
    
    Cyfamate::auto_service().print_go(Cyfamate::bestmove_muted().load());
    
    
    if ((CyfRootHint::state().enabled.load() || CyfRootHint::tracing()) && !Cyfamate::bestmove_muted())
    {
        auto& hs = CyfRootHint::state(); auto& g = hs.go;
        int best_rank = -1;
        if (!search_skipped && !bestThread->rootMoves.empty() && !bestThread->rootMoves[0].pv.empty())
            for (size_t i = 0; i < g.order.size(); ++i)
                if (g.order[i] == bestThread->rootMoves[0].pv[0].raw()) { best_rank = int(i); break; }
        uint64_t received, stale, malformed, dropped, key_missing, key_mismatch, native_received;
        bool pending;
        { std::lock_guard<std::mutex> lk(hs.mtx); received = hs.received; stale = hs.stale; malformed = hs.malformed; dropped = hs.disabled_drops; pending = hs.pending; key_missing=hs.key_missing; key_mismatch=hs.key_mismatch; native_received=hs.native_received; }
        sync_cout << "info string cyfhint v=1 gen=" << limits.cyfGoGen << " matched=" << int(g.matched) << " kind=" << (g.kind.empty() ? "none" : g.kind)
                  << " root_key=" << g.root_key << " require_key=" << int(g.require_key)
                  << " hint_key_present=" << int(g.hint_has_key) << " hint_key=" << (g.hint_has_key ? std::to_string(g.hint_key) : "na")
                  << " key_missing_total=" << key_missing << " key_mismatch_total=" << key_mismatch
                  << " model=" << (g.model.empty() ? "none" : g.model) << " apply=" << g.apply << " n=" << g.n << " legal=" << g.legal
                  << " excluded_legal=" << g.excluded_legal << " illegal=" << g.illegal << " applied=" << g.applied << " recv_ms=" << g.recv_ms << " collect_ms=" << g.collect_ms
                  << " collect_depth=" << g.collect_depth << " top1=" << (g.top1.empty() ? "none" : g.top1) << " best_rank=" << best_rank
                  << " completed_depth=" << bestThread->completedDepth << " received_total=" << received << " stale_total=" << stale
                  << " malformed_total=" << malformed << " pending=" << int(pending) << " dropped_total=" << dropped
                  << " trace_enabled=" << int(hs.trace_enabled.load()) << " trace_frames=" << g.trace_frames
                  << " trace_compared=" << g.trace_compared << " trace_changed=" << g.trace_changed << " trace_changed_frames=" << g.trace_changed_frames
                  << " bonus_scored=" << g.bonus_scored << " accepted_hint_tt=" << g.accepted_hint_tt
                  << " accepted_hint_capture=" << g.accepted_hint_capture << " accepted_hint_quiet=" << g.accepted_hint_quiet
                  << " accepted_hint_evasion=" << g.accepted_hint_evasion
                  << " src=" << (g.matched ? (g.hint_native ? "native" : "parent") : "none")   
                  << " native_received_total=" << native_received << sync_endl;
    }
    main_manager()->updates.onBestmove(bestmove, ponder);

    
    Cyfamate::iter_observe_dump(Cyfamate::bestmove_muted().load());
    Cyfamate::preinfer_end(Cyfamate::bestmove_muted().load());
    Cyfamate::auto_service().dump_after_bestmove(Cyfamate::bestmove_muted().load());   
    if (Cyfamate::options().tm_sharp_current && !Cyfamate::bestmove_muted().load()) {
        const auto& sh = CyfSharp::state();
        const auto& r = sh.current;
        sync_cout << "info string cyfsharp gen=" << sh.gen << " key=" << std::hex << sh.key << std::dec
                  << " ply=" << sh.ply << " tm_uses=" << sh.tm_uses << " valid=" << r.valid
                  << " cp=" << r.cp << " pmax=" << r.policy_max << " I=" << r.instability
                  << " T=" << r.tightness << " C=" << r.closeness << " D=" << r.disagree
                  << " S=" << r.scalar << " mode=current native_proxy=1" << sync_endl;
    }
    Cyfamate::zyf_report(bestThread->rootMoves, Cyfamate::bestmove_muted().load());   

    
    
    Cyfamate::harvest_probe().dump();

    
    
    
    
    
    
    if (!_cyf_throwaway_ponder
        && !Cyfamate::fusion_service().cfg.mode
        && Cyfamate::options().multiponder_topk > 0 && Cyfamate::sidecar().ok
        && (Cyfamate::tm_optimum_hint() <= 0 || Cyfamate::tm_optimum_hint() >= 800)   
        && !Cyfamate::bench_guard() && !bestThread->rootMoves.empty()
        && !bestThread->rootMoves[0].pv.empty()
        && bestThread->rootMoves[0].pv[0] != Move::none()){
        Position& _mp   = bestThread->rootPos;
        Move      _our  = bestThread->rootMoves[0].pv[0];
        if (_mp.legal(_our)){
            StateInfo _s1; _mp.do_move(_our, _s1);                 
            uint32_t _pid = Cyfamate::sidecar().post_policy(_mp.sfen());
            Cyfamate::DlSidecar::Result _pr;
            
            
            const bool _got = Cyfamate::sidecar().try_get(_pid, _pr);   
            if (_got && _pr.ok && !_pr.policy.empty()){
                std::vector<std::pair<std::string,float>> _pol = _pr.policy;
                std::stable_sort(_pol.begin(), _pol.end(),
                    [](const std::pair<std::string,float>& a, const std::pair<std::string,float>& b){ return a.second > b.second; });
                const int _k = std::min<int>(Cyfamate::options().multiponder_topk, (int)_pol.size());
                for (int _t = 0; _t < _k; ++_t){
                    Move _rm = USIEngine::to_move(_mp, _pol[_t].first);
                    if (_rm == Move::none() || !_mp.legal(_rm)) continue;
                    StateInfo _s2; _mp.do_move(_rm, _s2);          
                    uint32_t _cid = Cyfamate::sidecar().post_policy(_mp.sfen());
                    Cyfamate::multiponder_pending().push_back(_cid);
                    _mp.undo_move(_rm);
                }
            } else if (!_got) {
                Cyfamate::sidecar().abandon(_pid);                 
            }
            _mp.undo_move(_our);
        }
    }
}













void Search::YaneuraOuWorker::elastic_park() {
    auto& busy = Cyfamate::zyf_cpu_busy();
    while (busy.load(std::memory_order_relaxed) > elasticRank && !threads.stop.load(std::memory_order_relaxed))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

bool Search::YaneuraOuWorker::iterative_deepening() {

    
    
    SearchManager* mainThread = (is_mainthread() ? main_manager() : nullptr);

#if !STOCKFISH
    
    SearchOptions& search_options = main_manager()->search_options;

    
    rootPos.set_ekr(search_options.enteringKingRule);
#endif

    PVMoves pv;

    
    Depth lastBestMoveDepth = 0;
    Value lastBestScore     = -VALUE_INFINITE;
    PVMoves lastBestPV;
    lastBestPV.push_back(Move::none());
#if defined(CYF_BLEND)
    uint64_t cyfLastBestRule = 0;   
    int      cyfLastBestPass = 0;
#endif

    
    
    
    
    
    
    Value  alpha, beta;
    Value  bestValue     = -VALUE_INFINITE;
    Color  us            = rootPos.side_to_move();
    double timeReduction = 1, totBestMoveChanges = 0;
    int    delta, iterIdx                        = 0;

    
    
    

    
    
    

    
    
    

    

    
    
    

    Stack  stack[MAX_PLY + 10] = {};
    Stack* ss                  = stack + 7;

    
    for (int i = 7; i > 0; --i)
    {
        (ss - i)->continuationHistory =
          &continuationHistory[0][0][NO_PIECE][0];  

        (ss - i)->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
        (ss - i)->staticEval                    = VALUE_NONE;
    }

    
    for (int i = 0; i <= MAX_PLY + 2; ++i)
        (ss + i)->ply = i;

    
    ss->pv = &pv;

    if (mainThread)
    {
#if !STOCKFISH
		
		
		if ((mainThread->lastGamePly - rootPos.game_ply()) & 1)
		{
            if (mainThread->bestPreviousScore != VALUE_INFINITE)
	            mainThread->bestPreviousScore *= -1;

            if (mainThread->bestPreviousAverageScore != VALUE_INFINITE)
	            mainThread->bestPreviousAverageScore *= -1;
		}
#endif

        if (mainThread->bestPreviousScore == VALUE_INFINITE)
            mainThread->iterValue.fill(VALUE_ZERO);
        else
            mainThread->iterValue.fill(mainThread->bestPreviousScore);
    }

    
    

    size_t multiPV = size_t(options["MultiPV"]);

#if STOCKFISH
    Skill skill(options["Skill Level"], options["UCI_LimitStrength"] ? int(options["UCI_Elo"]) : 0);
    
#else
    Skill skill = Skill(20, 0);
#endif

    
    

    
    

    

    if (skill.enabled())
        multiPV = std::max(multiPV, size_t(4));

    
    multiPV = std::min(multiPV, rootMoves.size());

    
    
    

    
    
    
    int searchAgainCounter = 0;

    
    bool uciPvSent = false;

    
    
    lowPlyHistory.fill(98);

    
    

    
    
    
    

    while (++rootDepth < MAX_PLY && !threads.stop
           && !(limits.depth && mainThread && rootDepth > limits.depth))
    {
        
        if (is_mainthread()) Cyfamate::dl_async_collect(rootPos);
        
        if (is_mainthread()) Cyfamate::root_hint_collect(rootPos, rootMoves, rootDepth);
        if (is_mainthread()) Cyfamate::zyf_safe_refresh(rootPos, rootMoves);
#if defined(CYF_BLEND)
        
        const bool cyfRuleChanged =
          is_mainthread() && Cyfamate::blend_service().advance_rule(tt, rootDepth, false);
        if (cyfRuleChanged && Cyfamate::blend_service().cfg.reset_corr)
        {
            
            
            sharedHistory.correctionHistory.clear_range(0, numaThreadIdx, numaTotal);
            for (auto& to : continuationCorrectionHistory)
                for (auto& h : to)
                    h.fill(6);
            ++Cyfamate::blend_service().corr_resets;
        }
#endif
#if defined(CYF_REPLAY)
        if (is_mainthread() && Cyfamate::replay().enabled)
            nodes += Cyfamate::replay().begin_iteration(rootPos, rootMoves[0].pv[0], rootDepth,
                                                        nodes.load(std::memory_order_relaxed));
#endif
        

        
        
        

        
        

        
        

        if (mainThread)
        {
            totBestMoveChanges /= 2;
            uciPvSent = false;
        }

        
        

        
        

        

        for (RootMove& rm : rootMoves)
            rm.previousScore = rm.score;

#if defined(CYF_BLEND)
        
        
        
        
        
        
        if (cyfRuleChanged && Cyfamate::blend_service().cfg.root_fix)
            for (size_t i = 1; i < rootMoves.size(); ++i)
            {
                rootMoves[i].score = rootMoves[i].uciScore = -VALUE_INFINITE;
                rootMoves[i].cyfRule = Cyfamate::blend_service().rule_id;
                rootMoves[i].cyfPass = rootDepth;
            }
#endif

        

        
        

        
        
#if STOCKFISH
        if (!threads.increaseDepth)
#else
        if (!main_manager()->increaseDepth)
#endif
            searchAgainCounter++;

        
        
        for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
        {
            
            
            

#if STOCKFISH
            if (pvIdx == pvLast)
            {
                pvFirst = pvLast;
                for (pvLast++; pvLast < rootMoves.size(); pvLast++)
                    if (rootMoves[pvLast].tbRank != rootMoves[pvFirst].tbRank)
                        break;
            }
#else

            
            
            

            size_t pvFirst = pvIdx;
            pvLast         = rootMoves.size();

#endif

            
            
            selDepth = 0;

            
            
            

            

            
            
            
            delta     = 9 + threadIdx % 8 + std::abs(rootMoves[pvIdx].meanSquaredScore) / 5686;



            Value avg = rootMoves[pvIdx].averageScore;
            alpha     = std::max(avg - delta, -VALUE_INFINITE);
            beta      = std::min(avg + delta,  VALUE_INFINITE);

#if 0
            
            
			optimism[us]  = 137 * avg / (std::abs(avg) + 91);
            optimism[~us] = -optimism[us];
#endif

			
            

            
            
            

            
            

            
            
            int failedHighCnt = 0;
            int cyfIterAttempt = 0;   

            while (true)
            {
                
                

                
                
                

                
                Depth adjustedDepth =
                  std::max(1, rootDepth - failedHighCnt - 3 * (searchAgainCounter + 1) / 4);
                rootDelta = beta - alpha;
                const bool cyfIterRec = is_mainthread() && CyfIter::recording();
                if (cyfIterRec)
                    CyfIter::begin_pass(rootDepth, adjustedDepth, int(pvIdx), cyfIterAttempt,
                                        alpha, beta, nodes.load(std::memory_order_relaxed));
                ++cyfIterAttempt;   
                bestValue = search<Root>(rootPos, ss, alpha, beta, adjustedDepth, false);
                if (cyfIterRec)
                    CyfIter::end_pass(bestValue, threads.stop.load(std::memory_order_relaxed),
                                      nodes.load(std::memory_order_relaxed));

                
                
                
                
                
                

                
                
                
                
                
                

                
                
                

                stable_sort_root_moves(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast);

                
                
                

                
                
                

                if (threads.stop)
                    break;

                
                
                

                
                
                

                
                

                if (mainThread && multiPV == 1 && (bestValue <= alpha || bestValue >= beta)
                    && nodes > 10000000

#if !SOTCKFISH
                    
                    && (rootDepth < 3
                        || search_options.lastPvInfoTime + search_options.computed_pv_interval
                             <= now())
                    
                    && search_options.outout_fail_lh_pv
#endif
                )
#if STOCKFISH
                    main_manager()->pv(*this, threads, tt, rootDepth);
#else
                {
                    main_manager()->pv(*this, threads, tt, rootDepth);
                    
                    search_options.lastPvInfoTime = now();
                }
#endif

                
                
                if (bestValue <= alpha)
                {
                    beta  = alpha;
                    alpha = std::max(bestValue - delta, -VALUE_INFINITE);

                    failedHighCnt = 0;
                    if (mainThread)
                        mainThread->stopOnPonderhit = false;
                }
                else if (bestValue >= beta)
                {
                    alpha = std::max(beta - delta, alpha);
                    beta  = std::min(bestValue + delta, VALUE_INFINITE);
                    ++failedHighCnt;
                }
                else
                    break;

                delta += delta / 3;

                assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
            }

            
            

            
            

            stable_sort_root_moves(rootMoves.begin() , rootMoves.begin() + pvIdx + 1);

            if (mainThread
                && (threads.stop || pvIdx + 1 == multiPV || nodes > 10000000)
                
                
                
                
                

                
                
                
                
                
                

                && !(threads.abortedSearch && is_loss(rootMoves[0].uciScore))
#if !STOCKFISH
                
                && search_options.lastPvInfoTime + search_options.computed_pv_interval <= now()
#endif
            )
#if STOCKFISH
                main_manager()->pv(*this, threads, tt, rootDepth);
#else
            {
                main_manager()->pv(*this, threads, tt, rootDepth);
                
                search_options.lastPvInfoTime = now();
                uciPvSent = (pvIdx + 1 == multiPV);
            }
#endif


            if (threads.stop)
                break;

        }  

        if (!threads.stop)
        {
            completedDepth  = rootDepth;
            if (is_mainthread() && Cyfamate::options().mt_obs)   
                sync_cout << "info string mtobs iter depth " << rootDepth << " own " << nodes.load(std::memory_order_relaxed)
                          << " total " << threads.nodes_searched() << sync_endl;

            if (lastIterationPV.empty() || rootMoves[0].pv[0] != lastIterationPV[0])
                lastBestMoveDepth = rootDepth;

            lastIterationPV = rootMoves[0].pv;
#if defined(CYF_REPLAY)
            if (is_mainthread() && Cyfamate::replay().enabled)
                Cyfamate::replay().end_iteration(rootDepth, nodes.load(std::memory_order_relaxed),
                                                 rootMoves[0].pv[0], int(rootMoves[0].score));
#endif
            if (is_mainthread() && CyfPreInfer::state().enabled.load(std::memory_order_relaxed))
                Cyfamate::preinfer_completed(rootPos,rootMoves,int(completedDepth),limits.cyfGoGen,
                                             threads.nodes_searched(),main_manager()->tm.elapsed_time());
            
            if (is_mainthread() && CyfIter::recording())
                CyfIter::on_completed(rootMoves, int(completedDepth), nodes.load(std::memory_order_relaxed),
                                      int64_t(main_manager()->tm.elapsed_time()), int32_t(-VALUE_INFINITE));
            
            if (is_mainthread() && CyfFrontier::state().enabled.load(std::memory_order_relaxed))
                Cyfamate::frontier_export(rootPos, rootMoves, int(completedDepth), limits.cyfGoGen,
                                          threads.nodes_searched(), nodes.load(std::memory_order_relaxed),
                                          main_manager()->tm.elapsed_time());
            
            if (is_mainthread() && Cyfamate::zyf_go().active)
                Cyfamate::zyf_iteration(rootPos, rootMoves, int(completedDepth));
        }

        
        
        

        
        
        

#if defined(CYF_BLEND)
        const bool cyfAborted = threads.stop;   
        bool       cyfRollback = false;
#endif
        if (threads.abortedSearch && rootMoves[0].score != -VALUE_INFINITE
            && is_loss(rootMoves[0].score))
        {
            
            
            

            Utility::move_to_front(rootMoves, [&lastBestPV = std::as_const(lastBestPV)](
                                                const auto& rm) { return rm == lastBestPV[0]; });
            rootMoves[0].pv    = lastBestPV;
            rootMoves[0].score = rootMoves[0].uciScore = lastBestScore;
#if defined(CYF_BLEND)
            rootMoves[0].cyfRule = cyfLastBestRule;
            rootMoves[0].cyfPass = cyfLastBestPass;
            cyfRollback          = true;
#endif

            if (mainThread && lastBestPV[0] != Move::none())
            {
#if STOCKFISH
                uciPvSent = true;
#else
                
                
                
                uciPvSent = false;
#endif
            }
        }
        else if (rootMoves[0].pv[0] != lastBestPV[0])
        {
            lastBestPV        = rootMoves[0].pv;
            lastBestScore     = rootMoves[0].score;
            lastBestMoveDepth = rootDepth;
#if defined(CYF_BLEND)
            cyfLastBestRule = rootMoves[0].cyfRule;
            cyfLastBestPass = rootMoves[0].cyfPass;
#endif
        }
#if defined(CYF_BLEND)
        
        if (mainThread)
            Cyfamate::blend_service().root_pass_end(rootMoves, rootDepth, cyfAborted, cyfRuleChanged,
                                                    cyfRollback);
#endif

        
        
        
        

        if (multiPV == 1)
        {
            
            
            
            
            
            if (!limits.mate && bestValue >= VALUE_MATE_IN_MAX_PLY
                && (VALUE_MATE - bestValue + 2) * 5 / 2 < (Value) (rootDepth))
                break;

            
            if (!limits.mate && bestValue <= VALUE_MATED_IN_MAX_PLY
                && (bestValue - (-VALUE_MATE) + 2) * 5 / 2 < (Value) (rootDepth))
                break;
        }

        if (!mainThread)
            continue;

        
        
        

        if (rootMoves[0].pv.size() > 1)
            mainThread->ponder_candidate = rootMoves[0].pv[1];

        
        

        
        
#if STOCKFISH
        if (limits.mate && rootMoves[0].score == rootMoves[0].usiScore
            && ((rootMoves[0].score >= VALUE_MATE_IN_MAX_PLY
                 && VALUE_MATE - rootMoves[0].score <= 2 * limits.mate)
                || (rootMoves[0].score != -VALUE_INFINITE
                    && rootMoves[0].score <= VALUE_MATED_IN_MAX_PLY
                    && VALUE_MATE + rootMoves[0].score <= 2 * limits.mate)))
            threads.stop = true;
#endif

        
        

        if (skill.enabled() && skill.time_to_pick(rootDepth))
            skill.pick_best(rootMoves, multiPV);

        
        

        for (auto&& th : threads)
        {
            auto yw = toYaneuraOuWorker(th->worker);
            totBestMoveChanges += yw->bestMoveChanges;
            yw->bestMoveChanges = 0;
        }


        
        
#if STOCKFISH
        if (limits.use_time_management() && !threads.stop && !mainThread->stopOnPonderhit)
#else
        
        
        if (limits.use_time_management() && !threads.stop && !mainThread->stopOnPonderhit
            && !mainThread->tm.search_end)
#endif
        {
            uint64_t nodesEffort =
              rootMoves[0].effort * 100000 / std::max(size_t(1), size_t(nodes));

            double fallingEval =
              (11.325 + 2.115 * (mainThread->bestPreviousAverageScore - bestValue)
               + 0.987 * (mainThread->iterValue[iterIdx] - bestValue))
              / 100.0;
            fallingEval = std::clamp(fallingEval, 0.5688, 1.5698);

            
            

            double k      = 0.5189;
            double center = lastBestMoveDepth + 11.57;
            timeReduction = 0.723 + 0.79 / (1.104 + std::exp(-k * (completedDepth - center)));
            double reduction =
              (1.455 + mainThread->previousTimeReduction) / (2.2375 * timeReduction);
            double bestMoveInstability = 1.04 + 1.8956 * totBestMoveChanges / threads.size();
			double highBestMoveEffort  = completedDepth >= 10 && nodesEffort >= 92425 ? 0.666 : 1.0;

            
            
            const double cyfTmScale = Cyfamate::tm_dl_scale(
                rootMoves[0].pv.empty() ? std::string() : rootMoves[0].pv[0].to_usi_string(),
                USIEngine::to_cp(bestValue),
                std::abs(bestValue) >= VALUE_TB_WIN_IN_MAX_PLY,
                limits.ponderMode, bestValue != -VALUE_INFINITE && bestValue != VALUE_INFINITE);

            double totalTime = mainThread->tm.optimum() * fallingEval * reduction
                             * bestMoveInstability * highBestMoveEffort * cyfTmScale;

            
            

            if (rootMoves.size() == 1)
                totalTime = std::min(502.0, totalTime);
	            

#if STOCKFISH
            auto elapsedTime = elapsed();
#else
            
            auto elapsedTime = mainThread->tm.elapsed_time();
#endif

            
            

#if STOCKFISH
            if (elapsedTime > std::min(totalTime, double(mainThread->tm.maximum())))
            {
                

                
                
                

                
                

                if (mainThread->ponder)
                    mainThread->stopOnPonderhit = true;
                else
                    threads.stop = true;
            }
            else
                threads.increaseDepth = mainThread->ponder || elapsedTime <= totalTime * 0.503;

#else
            

			if (elapsedTime > std::min(totalTime, double(mainThread->tm.maximum())))
            {
				if (mainThread->ponder)
                    mainThread->stopOnPonderhit = true;
                else
                    
                    mainThread->tm.set_search_end(elapsedTime);
            }
            else
                main_manager()->increaseDepth =
                  mainThread->ponder || elapsedTime <= totalTime * 0.503;
#endif
        }

        mainThread->iterValue[iterIdx] = bestValue;
        iterIdx                        = (iterIdx + 1) & 3;
    }

    if (!mainThread)
        return false;

    mainThread->previousTimeReduction = timeReduction;

    
    if (skill.enabled())
        std::swap(rootMoves[0],
                  *std::find(rootMoves.begin(), rootMoves.end(),
                             skill.best ? skill.best : skill.pick_best(rootMoves, multiPV)));

    return uciPvSent;
}

void Search::YaneuraOuWorker::do_move(Position & pos, const Move move, StateInfo& st,
                                        Stack* const ss) {
    
    do_move(pos, move, st, pos.gives_check_ready(move), ss);
}




template <bool TTPrefetched>
FORCE_INLINE void YaneuraOuWorker::do_move(
  Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss) {

    
    
    if (ss != nullptr)
    {
        const Piece  pc = pos.moved_piece(move);
        const Square to = move.to_sq();
        prefetch(&(*(ss - 1)->continuationCorrectionHistory)[pc][to]);
        prefetch(&(*(ss - 3)->continuationCorrectionHistory)[pc][to]);
    }

    bool capture = pos.capture_stage(move);

    
	

	nodes.store(nodes.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

#if defined(EVAL_SFNN)

    

    DirtyBoardData dirtyBoardData = [&] {
        if constexpr (TTPrefetched) return pos.do_move(move, st, givesCheck, static_cast<const int*>(nullptr));
        else return pos.do_move(move, st, givesCheck, &tt);
    }();
    accumulatorStack.push(dirtyBoardData);

#else

    if constexpr (TTPrefetched) pos.do_move(move, st, givesCheck, static_cast<const int*>(nullptr));
    else pos.do_move(move, st, givesCheck, &tt);
#if defined(CYF_FT_PREFETCH)
    Eval::NNUE::networks().feature_transformer.prefetch_changed(pos);
#endif

#endif

    if (ss != nullptr)
    {
        
        ss->currentMove = move;

#if STOCKFISH
        ss->currentMove = move;
        ss->continuationHistory =
          &continuationHistory[ss->inCheck][capture][dirtyBoardData.dp.pc][move.to_sq()];
        ss->continuationCorrectionHistory =
          &continuationCorrectionHistory[dirtyBoardData.dp.pc][move.to_sq()];
#else
        
        
        Piece dp_pc             = pos.moved_piece(move);
        ss->continuationHistory = &continuationHistory[ss->inCheck][capture][dp_pc][move.to_sq()];
        ss->continuationCorrectionHistory = &continuationCorrectionHistory[dp_pc][move.to_sq()];
#endif

        
        
        
        
        prefetch(&sharedHistory.pawn_correction_entry(pos));
        prefetch(&sharedHistory.minor_piece_correction_entry(pos));
        prefetch(&sharedHistory.nonpawn_correction_entry<WHITE>(pos));
        prefetch(&sharedHistory.nonpawn_correction_entry<BLACK>(pos));
        prefetch(&sharedHistory.hand_correction_entry(pos));    
        prefetch(&sharedHistory.major_correction_entry(pos));   
    }
}

void YaneuraOuWorker::do_null_move(Position& pos, StateInfo& st) { pos.do_null_move(st, tt); }

void YaneuraOuWorker::undo_move(Position& pos, const Move move) {

	pos.undo_move(move);

#if defined(EVAL_SFNN)
    
#endif
}

void YaneuraOuWorker::undo_null_move(Position& pos) { pos.undo_null_move(); }




void YaneuraOuWorker::clear() {

	
	mainHistory.fill(mainHistoryDefault);
    
    captureHistory.fill(-678);

    
    {   
        const bool cyfPriv = cyf_is_priv_hist(&sharedHistory);
        sharedHistory.correctionHistory.clear_range(0, cyfPriv ? 0 : numaThreadIdx, cyfPriv ? 1 : numaTotal);
        sharedHistory.pawnHistory.clear_range(-1805, cyfPriv ? 0 : numaThreadIdx, cyfPriv ? 1 : numaTotal);
    }

	ttMoveHistory = 0;

    for (auto& to : continuationCorrectionHistory)
        for (auto& h : to)
            h.fill(6);



    
    
    
    
    

	
	for (bool inCheck : {false, true})
        for (StatsType c : {NoCaptures, Captures})
            for (auto& to : continuationHistory[inCheck][c])
                for (auto& h : to)
                    h.fill(-523);



	
	
    for (size_t i = 1; i < reductions.size(); ++i)
        reductions[i] = int(2763 / 128.0 * std::log(i));



	

#if defined(EVAL_SFNN)
    refreshTable.clear(networks[numaAccessToken]);
#endif
}




Value YaneuraOuWorker::qsearch_pv(Position& pos, PVMoves& pv) {

#if defined(USE_SFNN)
    
    accumulatorStack.reset();
#endif

    nodes.store(0, std::memory_order_relaxed);
    selDepth = 0;
    lowPlyHistory.fill(98);
    pv.clear();

    
    Stack  stack[MAX_PLY + 10] = {};
    Stack* ss                  = stack + 7;

    for (int i = 7; i > 0; --i)
    {
        (ss - i)->continuationHistory =
          &continuationHistory[0][0][NO_PIECE][0];
        (ss - i)->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
        (ss - i)->staticEval                    = VALUE_NONE;
        (ss - i)->currentMove                   = Move::none();
    }

    for (int i = 0; i <= MAX_PLY + 2; ++i)
        (ss + i)->ply = i;

    ss->pv          = &pv;
    ss->currentMove = Move::none();
    ss->staticEval  = VALUE_NONE;
    ss->ttPv        = false;
    ss->ttHit       = false;

    pos.set_ekr(main_manager()->search_options.enteringKingRule);

    return qsearch<PV, false>(pos, ss, -VALUE_INFINITE, VALUE_INFINITE);
}













template<NodeType nodeType>
Value YaneuraOuWorker::search_body(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode)
{
    
    
    

	
	

    constexpr bool PvNode   = nodeType != NonPV;

	
    constexpr bool rootNode = nodeType == Root;

	
    
    

    const bool     allNode  = !(PvNode || cutNode);

    
    
    
    
    ASSERT_LV3(depth > 0);

    
    

    depth = std::min(depth, MAX_PLY - 1);

	
    
    

#if STOCKFISH
    
    

	if (!rootNode && alpha < VALUE_DRAW && pos.upcoming_repetition(ss->ply))
    {
        alpha = value_draw(nodes);
        if (alpha >= beta)
            return alpha;
    }
#endif

	ASSERT_LV3(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    ASSERT_LV3(PvNode || (alpha == beta - 1));
    ASSERT_LV3(0 < depth && depth < MAX_PLY);
    
    ASSERT_LV3(!(PvNode && cutNode));

	
    
    

	
    

    PVMoves   pv;
	StateInfo st;

	

	Key        posKey;

    
    
    

	Move  move, excludedMove, bestMove;

	
	

	Depth extension, newDepth;

    
    
    
    
    
#if STOCKFISH
	Value bestValue, value, eval, maxValue, probCutBeta;
#else
	Value bestValue, value, eval, probCutBeta;
#endif
	
	
	
	
	
	
	

	bool  givesCheck, improving, priorCapture, opponentWorsening;

	
    

	bool  capture, ttCapture;

	
	

	int   priorReduction;
    Piece movedPiece;

	
    

    SearchedList capturesSearched;
    SearchedList quietsSearched;

	
    
	
    

	

    ss->inCheck        = pos.checkers();
    priorCapture       = pos.captured_piece();
    Color us           = pos.side_to_move();
    ss->moveCount      = 0;
    bestValue          = -VALUE_INFINITE;
#if STOCKFISH
    maxValue           =  VALUE_INFINITE;
    
#else
    
    auto& search_options = main_manager()->search_options;
#endif

    
    
    
    ss->followPV = rootNode
                || ((ss - 1)->followPV && static_cast<size_t>(ss->ply - 1) < lastIterationPV.size()
                    && (ss - 1)->currentMove == lastIterationPV[ss->ply - 1]);

#if defined(USE_CLASSIC_EVAL) && defined(USE_LAZY_EVALUATE)
	
	
    bool evaluated = false;
    auto evaluate  = [&](Position& pos) {
        evaluated = true;
        return this->evaluate(pos);
    };
    auto do_move = [&](Position & pos, Move move, StateInfo& st, bool givesCheck, Stack* ss) {
        if (!evaluated)
        {
            evaluated = true;
            Eval::evaluate_with_no_return(pos);
        }
        this->do_move(pos, move, st, givesCheck, ss);
    };

	
	
    auto do_move_ = [&](Position & pos, Move move, StateInfo& st, Stack* ss) {
        if (!evaluated)
        {
            evaluated = true;
            Eval::evaluate_with_no_return(pos);
        }
        this->do_move(pos, move, st, ss);
    };
    auto do_null_move = [&](Position& pos, StateInfo& st) {
        if (!evaluated)
        {
            evaluated = true;
            Eval::evaluate_with_no_return(pos);
        }
        this->do_null_move(pos, st);
    };
#else
    auto do_move_ = [&](Position& pos, Move move, StateInfo& st, Stack* ss) { this->do_move(pos, move, st, ss); };
#endif

	

    
    
    
	if (is_mainthread())
        main_manager()->check_time(*this);
    else if (elasticRank >= 0 && !(++elasticTick & 255))   
        elastic_park();

#if defined(CYF_REPLAY)
    if (Cyfamate::replay().enabled)
        Cyfamate::replay().visit(uint64_t(pos.key()), int(depth), int(alpha), int(beta), ss->ply,
                                 nodes.load(std::memory_order_relaxed));
#endif

    
	
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

	
    
    

    if (!rootNode)
    {
        
        
        
        

		
        

        
        

        
        

        
        
        
        

		

#if STOCKFISH
        if (threads.stop.load(std::memory_order_relaxed) || pos.is_draw(ss->ply)
            || ss->ply >= MAX_PLY)
            return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos)
                                                        : value_draw(nodes);
#else

		auto draw_type = pos.is_repetition(ss->ply);
        if (draw_type != REPETITION_NONE)
        { 
            if (draw_type == REPETITION_DRAW)
				
				
				
                return draw_value(draw_type, pos.side_to_move()) + value_draw(nodes);
            else
	            return value_from_tt(draw_value(draw_type, pos.side_to_move()), ss->ply);
        }

		
        if (threads.stop.load(std::memory_order_relaxed) || ss->ply >= MAX_PLY
            || pos.game_ply() > search_options.max_moves_to_draw)
            return draw_value(REPETITION_DRAW, pos.side_to_move()) + value_draw(nodes);
#endif

		

		
		
		
		

        
        
        
        
        
        

		
        
        
        
        
        

		

        alpha = std::max(mated_in(ss->ply), alpha);
        beta  = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta)
            return alpha;
    }

	
    
    

	
    ASSERT_LV3(0 <= ss->ply && ss->ply < MAX_PLY);

	
    
    Square prevSq  = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;
    bestMove       = Move::none();
    priorReduction = (ss - 1)->reduction;
    (ss - 1)->reduction = 0;
    ss->statScore       = 0;
    (ss + 2)->cutoffCnt = 0;

	
    
    
    

	
    excludedMove                   = ss->excludedMove;

	

	

    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey, pos);

    
    

	ss->ttHit    = ttHit;

	
	
	

	
	const bool _zyf_shallow = !PvNode && ss->ply <= 2 && Cyfamate::zyf_shallow_probe().load(std::memory_order_relaxed);
	
	if ((PvNode || _zyf_shallow) && !rootNode && excludedMove == Move::none() && Cyfamate::mate_table().active
	    && (_zyf_shallow || depth >= Cyfamate::options().mate_probe_depth)){
		uint8_t _mst; int16_t _mln; int64_t _mexpiry=0;
		if (Cyfamate::mate_table().load((uint64_t)posKey, _mst, _mln, &_mexpiry)){
			if (Cyfamate::zyf_shallow_probe().load(std::memory_order_relaxed)){
				auto& _zs = Cyfamate::zyf_probe_stats();
				(PvNode ? (_mst == 1 ? _zs.pv_mate : _zs.pv_nomate) : (_mst == 1 ? _zs.shallow_mate : _zs.shallow_nomate))
					.fetch_add(1, std::memory_order_relaxed);
			}
            if (_mst == 1 && _mln > 0){
                
                
                
                const int L = -1;
                if (L > 0 && ss->ply + L < MAX_PLY) {
                    const Value lower = mate_in(ss->ply + L);
                    if (lower >= beta){ ++Cyfamate::zyf_verify_stats().use_cut; return lower; } 
                }
            }
            
		}
	}

	

	ttData.move  = rootNode ? rootMoves[pvIdx].pv[0]
                 : ttHit    ? ttData.move
                            : Move::none();

	
    

    
    

	ttData.value =
          ttHit ? value_from_tt(ttData.value, ss->ply ) : VALUE_NONE;

	
	
	ASSERT_LV3(pos.legal_promote(ttData.move));

    ss->ttPv     = excludedMove ? ss->ttPv : PvNode || (ttHit && ttData.is_pv);

	

    ttCapture    = ttData.move && pos.capture_stage(ttData.move);

    
    
	
    

	
    
    
    
    

    
    

	
	

	if (!PvNode
		&& !excludedMove
		&& ttData.depth > depth - (ttData.value <= beta) 
        && is_valid(ttData.value)   
							        
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER))
        && (cutNode == (ttData.value >= beta) || depth > 5))

		
    {
        
        

        if (ttData.move && ttData.value >= beta)
        {
			

            
            

            
            if (!ttCapture)
                update_quiet_histories(pos, ss, *this, ttData.move,
                                       std::min(130 * depth - 71, 1043));



            
            

            

            
            if (prevSq != SQ_NONE && (ss - 1)->moveCount <= 4 && !priorCapture)
                update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -2142);


        }

        
        

		
        

        
        

        
        
#if 0
		if (pos.rule50_count() < 96)
        {
            

            if (depth >= 8 && ttData.move
#if STOCKFISH
				&& pos.pseudo_legal(ttData.move)
#else
                && pos.pseudo_legal(ttData.move, search_options.generate_all_legal_moves)
#endif
				&& pos.legal(ttData.move)
                && !is_decisive(ttData.value))
            {
#if STOCKFISH
                pos.do_move(ttData.move, st);
                Key nextPosKey                             = pos.key();
#else
                pos.do_move(ttData.move, st);
                HASH_KEY nextPosKey                        = pos.hash_key();
#endif
				auto [ttHitNext, ttDataNext, ttWriterNext] = tt.probe(nextPosKey, pos);
                pos.undo_move(ttData.move);

                
                if (!is_valid(ttDataNext.value))
                    return ttData.value;
                if ((ttData.value >= beta) == (-ttDataNext.value >= beta))
                    return ttData.value;
            }
            else
                return ttData.value;
        }
#else
        return ttData.value;
#endif        
    }

	
	
	
	
	
	Move _zyfHint = Move::none();   
	if (!rootNode && !excludedMove && Cyfamate::mate_table().hint_count.load(std::memory_order_relaxed))
	{
		if (const uint16_t _hraw = Cyfamate::mate_table().load_hint((uint64_t)posKey))
		{
			
			auto _count = [](std::atomic<uint64_t>& c){ if (Cyfamate::options().verbose) c.fetch_add(1, std::memory_order_relaxed); };
			auto& _ts = Cyfamate::zyf_tree_stats();
			const Move _hm = pos.to_move(Move16(_hraw));
			if (_hm == Move::none() || !pos.pseudo_legal_s<true>(_hm) || !pos.legal(_hm))
				_count(_ts.hint_illegal);
			else if ((_zyfHint = _hm) != ttData.move)
			{
				_count(ttData.move ? _ts.hint_override : _ts.hint_new);
				ttData.bound = BOUND_NONE;
				ttData.move = _hm;
				ttCapture   = pos.capture_stage(_hm);
			}
			else _count(_ts.hint_hits);
			
			
			
			
			
			
			if (_zyfHint != Move::none() && Cyfamate::options().zyf_tree_bound){
				const Value _zb = (ss->ply == 1 && Cyfamate::zyf_root_safe().load(std::memory_order_relaxed))
				                ? mate_in(ss->ply + 1) : VALUE_MATE_IN_MAX_PLY;
				if (_zb >= beta) return _zb;
			}
		}
	}

	
    
    

    
    

#if STOCKFISH
    if (!rootNode && !excludedMove && tbConfig.cardinality)
    {
        int piecesCount = pos.count<ALL_PIECES>();

        if (piecesCount <= tbConfig.cardinality
            && (piecesCount < tbConfig.cardinality || depth >= tbConfig.probeDepth)
            && pos.rule50_count() == 0 && !pos.can_castle(ANY_CASTLING))
        {
            TB::ProbeState err;
            TB::WDLScore   wdl = Tablebases::probe_wdl(pos, &err);

            
            if (is_mainthread())
                main_manager()->callsCnt = 0;

            if (err != TB::ProbeState::FAIL)
            {
                
                tbHits.store(tbHits.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

                int drawScore = tbConfig.useRule50 ? 1 : 0;

                Value tbValue = VALUE_TB - ss->ply;

                
                value = wdl < -drawScore ? -tbValue
                      : wdl > drawScore  ? tbValue
                                         : VALUE_DRAW + 2 * wdl * drawScore;

                Bound b = wdl < -drawScore ? BOUND_UPPER
                        : wdl > drawScore  ? BOUND_LOWER
                                           : BOUND_EXACT;

                if (b == BOUND_EXACT || (b == BOUND_LOWER ? value >= beta : value <= alpha))
                {
                    ttWriter.write(posKey, value_to_tt(value, ss->ply), ss->ttPv, b,
                                   std::min(MAX_PLY - 1, depth + 6), Move::none(), VALUE_NONE,
                                   tt.generation());

                    return value;
                }

                if (PvNode)
                {
                    if (b == BOUND_LOWER)
                        bestValue = value, alpha = std::max(alpha, bestValue);
                    else
                        maxValue = value;
                }
            }
        }
    }
#else

	
    

    

    
    
    

	

	if (!rootNode && !ttHit
		
        && !excludedMove
    )
    {
        if (!ss->inCheck && depth >= cyfMate1MinDepth)
        {
            move = Mate::mate_1ply(pos);

            if (move != Move::none())
            {
                

                bestValue = mate_in(ss->ply + 1);

                ASSERT_LV3(pos.legal_promote(move));
#if defined(CYF_BLEND_TTSHADOW)
                cyf_tt_shadow().pending_site = CYF_TT_SITE_MATE1PLY;
#endif
                if (!excludedMove)
                    ttWriter.write(posKey, bestValue, ss->ttPv, BOUND_EXACT,
                                    std::min(MAX_PLY - 1, depth + 6), move, VALUE_NONE,
                                    tt.generation());

				
                

				

				return bestValue;
            }
        }
	}

	
    
    

	
    
    if (!ttData.move || PvNode)
    {
        
        
        
        move = pos.DeclarationWin();
        if (move != Move::none())
        {
            bestValue = mate_in(
                ss->ply + 1);  

            ASSERT_LV3(pos.legal_promote(move));
            

			

            return bestValue;
        }
        
    }

#endif 

	
    
    
    

    Value      unadjustedStaticEval = VALUE_NONE;
    const auto correctionValue      = correction_value(*this, pos, ss);
    const auto correctionExtra      = correction_extra(*this, pos);   

	if (ss->inCheck)
    {
        
        

		ss->staticEval = eval = (ss - 2)->staticEval;
        improving             = false;
        goto moves_loop;
    }
    else if (excludedMove)
        unadjustedStaticEval = eval = ss->staticEval;
    
    else if (ss->ttHit)
    {
        
        

        
        

#if defined(CYF_BLEND)
        if (Cyfamate::blend_config().verify && is_valid(ttData.eval))
#if defined(CYF_BLEND_TTSHADOW)
            
            Cyfamate::blend_service().verify_tt_eval(pos, ttData.eval, 0, ttWriter.debug_entry());
#else
            Cyfamate::blend_service().verify_tt_eval(pos, ttData.eval);
#endif
#endif
		unadjustedStaticEval = ttData.eval;
        if (!is_valid(unadjustedStaticEval))
            unadjustedStaticEval = evaluate(pos);

#if !STOCKFISH
#if defined(YANEURAOU_ENGINE_NNUE)
#if defined(USE_LAZY_EVALUATE)
        
        else if (PvNode)
        {
			unadjustedStaticEval = evaluate(pos);

			
        }
#else
        
        else
			unadjustedStaticEval = evaluate(pos);
#endif
#endif
#endif

        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue + correctionExtra);

        
        

		

        if (is_valid(ttData.value)
            && (ttData.bound & (ttData.value > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttData.value;
    }
    else
    {
        unadjustedStaticEval = evaluate(pos);

        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue + correctionExtra);

        
        

		

#if defined(CYF_BLEND_TTSHADOW)
        cyf_tt_shadow().pending_site = CYF_TT_SITE_STATIC;
#endif
        ttWriter.write(posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, DEPTH_UNSEARCHED, Move::none(),
                       unadjustedStaticEval, tt.generation());

		
        
    }


	
    
    

    
    

	


    if (((ss - 1)->currentMove).is_ok() && !(ss - 1)->inCheck && !priorCapture)
    {
        
        int evalDiff = std::clamp(-int((ss - 1)->staticEval + ss->staticEval), -214, 171) + 60;
        mainHistory[~us][((ss - 1)->currentMove).raw()] << evalDiff * 10;


        
        if (!ttHit && type_of(pos.piece_on(prevSq)) != PAWN
            && ((ss - 1)->currentMove).type_of() != PROMOTION)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << evalDiff * 12;



    }

    
    
    
    
    
    

	
    
    
    
	
	

	

    improving = ss->staticEval > (ss - 2)->staticEval;

	

    opponentWorsening = ss->staticEval > -(ss - 1)->staticEval;

	

    
	

    if (priorReduction >= 3 && !opponentWorsening)
        depth++;
    
    if (priorReduction >= 2 && depth >= 2 && ss->staticEval + (ss - 1)->staticEval > 173)
        depth--;



	
    
    

    
    

	
    

    
    if (!PvNode && eval < alpha - 502 - 306 * depth * depth)
        return qsearch<NonPV>(pos, ss, alpha, beta);



	
    
    
    

    
    

	

    {
        
        

		
		auto futility_margin = [&](Depth d) {
            Value futilityMult = 76 - 21 * !ss->ttHit;

            return futilityMult * d                                
                 - (2686 * improving + 362 * opponentWorsening) * futilityMult / 1024
                 + std::abs(correctionValue) / 180600;
        };



        if (!ss->ttPv && depth < 15 && eval >= beta && eval - futility_margin(depth) >= beta
            && (!ttData.move || ttCapture) && !is_loss(beta) && !is_win(eval))
            return (2 * beta + eval) / 3;
    }

	
    
    
    

    
    
    if (cutNode && ss->staticEval >= beta - 16 * depth - 53 * improving + 378 && !excludedMove



#if STOCKFISH
        && pos.non_pawn_material(us)
    
    
#endif
        && ss->ply >= nmpMinPly
        
        && !is_loss(beta)
    )
    {
        ASSERT_LV3((ss - 1)->currentMove != Move::null());

        
        

        
        Depth R = 7 + depth / 3;



        ss->currentMove                   = Move::null();
        ss->continuationHistory           = &continuationHistory[0][0][NO_PIECE][0];
        ss->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];

        
        
        
        
        
        

        do_null_move(pos, st);

        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, false);

        undo_null_move(pos);

        if (nullValue >= beta && !is_win(nullValue))
        
        
        {
            
            

            
            if (nmpMinPly || depth < 16)
                return nullValue;



            ASSERT_LV3(!nmpMinPly);  
                                     

            
            
            
            
            

            
            nmpMinPly = ss->ply + 3 * (depth - R) / 4;



            
            

            Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);

            nmpMinPly = 0;

            if (v >= beta)
                return nullValue;
        }
    }

	

    improving |= ss->staticEval >= beta;

	
    
    
    

	
	

    
    

    
    if (!ss->followPV && !allNode && depth >= 6 && !ttData.move && priorReduction <= 3)
        depth--;



#if OLD_CODE
    

	if (    PvNode
		&& !ttData.move)
		depth -= 3;

	
	

	if (depth <= 0)
		return qsearch<PV>(pos, ss, alpha, beta);

	
	

	
	

	if (cutNode && depth >= 7 && (!ttData.move || ttData.bound == BOUND_UPPER))
		depth -= 1 + !ttData.move;
#endif
        
	
    
    

    
    

	
    
    

    
    
    probCutBeta = beta + 224 - 61 * improving;



	if (depth >= 3
        && !is_decisive(beta)
        
        
        
        && !(is_valid(ttData.value) && ttData.value < probCutBeta))
    {
        ASSERT_LV3(probCutBeta < VALUE_INFINITE && probCutBeta > beta);

#if STOCKFISH
        MovePicker mp(pos, ttData.move, probCutBeta - ss->staticEval, &captureHistory);
#else
        MovePicker mp(pos, ttData.move, probCutBeta - ss->staticEval, &captureHistory,
                      search_options.generate_all_legal_moves);
#endif

        Depth      probCutDepth = depth - 4;

		
        

        while ((move = mp.next_move()) != Move::none())
        {
            ASSERT_LV3(move.is_ok());
            ASSERT_LV5(pos.pseudo_legal_s<true>(move) && pos.legal_promote(move));

            if (move == excludedMove || !pos.legal(move))
                continue;

            
            
            
            

#if STOCKFISH
            do_move(pos, move, st, ss);
#else
            do_move_(pos, move, st, ss);
#endif

            
            

            value = -qsearch<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);

            
            

            if (value >= probCutBeta && probCutDepth > 0)
                value = -search<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, probCutDepth,
                                       !cutNode);

            undo_move(pos, move);

            if (value >= probCutBeta)
            {
                
                

                
                
                
                if (!excludedMove)
                {

#if defined(CYF_BLEND_TTSHADOW)
                cyf_tt_shadow().pending_site = CYF_TT_SITE_PROBCUT;
#endif
				ttWriter.write(posKey, value_to_tt(value, ss->ply), ss->ttPv, BOUND_LOWER,
                               probCutDepth + 1, move, unadjustedStaticEval, tt.generation());
                }
#if defined(CYF_BLEND_TTSHADOW)
                else
                    ++cyf_tt_shadow().probcut_excluded_skips;
#endif

                if (!is_decisive(value))
                    return value - (probCutBeta - beta);
            }
        } 
    }

moves_loop:  

    
    
    
    
			 

	
    
    
    

    
    probCutBeta = beta + 416;


    if ((ttData.bound & BOUND_LOWER) && ttData.depth >= depth - 4 && ttData.value >= probCutBeta
        && !is_decisive(beta) && is_valid(ttData.value) && !is_decisive(ttData.value))
        return probCutBeta;

    
    const bool cyfNode = !rootNode && !ss->inCheck && !excludedMove && !is_decisive(ss->staticEval);
    Cyfamate::harvest_probe().log((uint64_t)posKey, depth, rootDepth, threadIdx, 0, 0,
        cyfNode ? 16 : 0);
    
    
    std::optional<Cyfamate::FusionHint> cyfHint;
    
    
    
    
    
    
    
    const bool cyfFusion = cyfNode && Cyfamate::fusion_service().cfg.mode;
    if (cyfFusion)
        cyfHint.emplace(Cyfamate::fusion_visit(pos, (uint64_t)posKey, depth, ss->staticEval));

	
    
    

	
    
    

    const PieceToHistory* contHist[] = {
      (ss - 1)->continuationHistory, (ss - 2)->continuationHistory, (ss - 3)->continuationHistory,
      (ss - 4)->continuationHistory, (ss - 5)->continuationHistory, (ss - 6)->continuationHistory};


    
    
    Move cyfRootPick = ttData.move;
    
    if (rootNode && Cyfamate::options().zyf_safe_order && !Cyfamate::bench_guard()
        && (ttData.move == Move::none()
            || CyfDefense::mated(limits.cyfGoGen, uint64_t(pos.key()), uint16_t(ttData.move.raw())))) {
        const uint16_t raw = CyfDefense::preferred(limits.cyfGoGen, uint64_t(pos.key()), [&](uint16_t m) {
            return std::any_of(rootMoves.begin()+pvIdx, rootMoves.end(), [&](const auto& rm) {
                return !rm.pv.empty() && uint16_t(rm.pv[0].raw())==m;
            });
        });
        if (raw) {
            const Move m=pos.to_move(Move16(raw));
            if (m!=excludedMove && pos.pseudo_legal_s<true>(m) && pos.legal(m)) {
                cyfRootPick=m;
                CyfDefense::picks().fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    MovePicker mp(pos, cyfRootPick, depth, &mainHistory, &lowPlyHistory, &captureHistory, contHist,
                  &sharedHistory, ss->ply
#if !STOCKFISH
                  ,
                  search_options.generate_all_legal_moves
#endif
    );

    
    
    
    if constexpr (rootNode)
    {
        rootMoveKeys.resize(rootMoves.size());
        for (size_t i = 0; i < rootMoves.size(); ++i)
            rootMoveKeys[i] = rootMoves[i].pv[0];
    }
    value = bestValue;

	
    int moveCount = 0;
    
    
    int lmrDeltaTerm = (beta - alpha) * 764 / rootDelta;

	
    
    
    
    
    

	
	

    while ((move = mp.next_move()) != Move::none())
    {
        ASSERT_LV3(move.is_ok());
        ASSERT_LV5(pos.pseudo_legal_s<true>(move) && pos.legal_promote(move));

        if (move == excludedMove)
            continue;

        
        

        

        if (!pos.legal(move))
            continue;
        
        
        
        prefetch(tt.first_entry(pos.key_after(move), ~pos.side_to_move()));

        
        
        

        
        
        

        

        if (rootNode
#if STOCKFISH
            && !std::count(rootMoveKeys.begin() + pvLast,
        
#else
            && !std::count(rootMoveKeys.begin() + pvIdx,
#endif
                           rootMoveKeys.end(), move))   
            continue;

        
        if (rootNode) mp.trace_root_accept(move, rootMoves.begin()
#if STOCKFISH
            + pvLast,
#else
            + pvIdx,
#endif
            rootMoves.end());
        if (rootNode && is_mainthread() && CyfIter::recording())
            CyfIter::on_accept(move.raw(), cyf_stage_code(mp.cyf_stage()));
        ss->moveCount = ++moveCount;
#if defined(CYF_GPU_POOL)
        if (!rootNode) Cyfamate::gpu_pool_search().on_move(pos, ss->ply, depth, moveCount, move, mp, excludedMove);
#endif


#if STOCKFISH

		if (rootNode && is_mainthread() && nodes > 10000000)
        {
            main_manager()->updates.onIter(
              {depth, UCIEngine::move(move, pos.is_chess960()), moveCount + pvIdx});
        }
#endif

        

        if (PvNode)
            (ss + 1)->pv = nullptr;

        
        
        
        

        
        extension = 0;

        
        capture    = pos.capture_stage(move);

        
        movedPiece = pos.moved_piece(move);

        
        givesCheck = pos.gives_check_ready(move);   

        
        
        newDepth = depth - 1;

        int delta = beta - alpha;

        
        int r = reduction_dt(improving, depth, moveCount, lmrDeltaTerm);

        
        

        
		

		
        
        

        
        if (ss->ttPv)
            r += 1013;

        
        
        



        
        
        
        

        
        

        

        if (!rootNode
            
            && !is_loss(bestValue))
        {
            
            

            if (moveCount >= (3 + depth * depth) / (2 - improving))
                mp.skip_quiet_moves();

            
            

            
            int lmrDepth = newDepth - r / 1024;

            if (capture || givesCheck)
            {
                Piece capturedPiece = pos.piece_on(move.to_sq());
                int   captHist =
                  captureHistory[movedPiece][move.to_sq()][type_of(capturedPiece)];

                
                

                if (!givesCheck && lmrDepth < 7)
                {
                    
                    
                    
                    

                    
        Value futilityValue = ss->staticEval + 218 + 223 * lmrDepth
                            + PieceValue[capturedPiece] + 131 * captHist / 1024;




                    if (futilityValue <= alpha)
                        continue;
                }

                
                
                
				

                
                int margin = std::max(167 * depth + captHist * 34 / 1024, 0);


#if STOCKFISH
				if ((alpha >= VALUE_DRAW || pos.non_pawn_material(us) != PieceValue[movedPiece])
#else
				
				

                if ((alpha >= VALUE_DRAW)
#endif
                    && !pos.see_ge(move, -margin))
                    continue;

            }
            else if (!ss->followPV || !PvNode)
            {
                int history = (*contHist[0])[movedPiece][move.to_sq()]
                            + (*contHist[1])[movedPiece][move.to_sq()]
                            + sharedHistory.pawn_entry(pos)[movedPiece][move.to_sq()];

                
                

                
        if (history < -4097 * depth)
            continue;

        history += 71 * mainHistory[us][move.raw()] / 32;

        
        

        lmrDepth += history / 3220;



                
        Value futilityValue = ss->staticEval + 42 + 151 * !bestMove + 120 * lmrDepth
                            + 86 * (ss->staticEval > alpha);



                
                
				

                
                

                
                
                

                if (!ss->inCheck && lmrDepth < 13 && futilityValue <= alpha)
                {
                    if (bestValue <= futilityValue && !is_decisive(bestValue) && !is_win(futilityValue))
                        bestValue = futilityValue;
                    continue;
                }

                

                

                lmrDepth = std::max(lmrDepth, 0);

                
                
                

                
        if (!pos.see_ge(move, -25 * lmrDepth * lmrDepth))
            continue;


            }
        }

		
        
        
        

        
        
        
        
        
        

		
        
        
        
        
        

        
        

        
        

		

		
        if (!rootNode && move == ttData.move && !excludedMove && depth >= 6 + ss->ttPv
            && is_valid(ttData.value) && !is_decisive(ttData.value) && (ttData.bound & BOUND_LOWER)
            && ttData.depth >= depth - 3 && !is_shuffling(move, ss, pos))
        {
            

            

            
            Value singularBeta  = ttData.value - (81 + 98 * (ss->ttPv && !PvNode)) * depth / 69;
            Depth singularDepth = newDepth / 2;



            

            ss->excludedMove = move;

            
            

            value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
            ss->excludedMove = Move::none();

            
            

            if (value < singularBeta)
            {
				
                int corrValAdj   = std::abs(correctionValue) / 221035;


                
                int doubleMargin = -4 + 212 * PvNode - 182 * !ttCapture - corrValAdj
                                 - 906 * ttMoveHistory / 116517 - (ss->ply > rootDepth) * 44;


                
                int tripleMargin = 73 + 320 * PvNode - 218 * !ttCapture + 92 * ss->ttPv - corrValAdj
                                 - (ss->ply > rootDepth) * 45;




                

                extension =
                    1 + (value < singularBeta - doubleMargin) + (value < singularBeta - tripleMargin);

                depth++;
            }

            
            
            
            
            
            

            
            
            
            
            
            

            

            else if (value >= beta && !is_decisive(value))
            {
                ttMoveHistory << std::max(-424 - 107 * depth, -3375);
                return value;
            }

            
            
            
            
            
            

            
            
            
            
            
            


            
            

            else if (ttData.value >= beta)
                extension = -3;

            
            
            
            

            else if (cutNode)
                extension = -2;

            
        }

		
        
        
        

		
        
        if (cyfHint && cyfHint->lease && !cyfHint->arrived)
            Cyfamate::fusion_poll(pos, *cyfHint, ss->staticEval);
        
        
        if (move == _zyfHint && extension < 1 && ss->ply < 2 * rootDepth && Cyfamate::options().zyf_tree_extend)
            extension = 1;
		
        do_move<true>(pos, move, st, givesCheck, ss);

        
        

		newDepth += extension;

		uint64_t nodeCount = rootNode ? uint64_t(nodes) : 0;

        
        

		
		if (ss->ttPv)
            r -= 2819 + PvNode * 973 + (ttData.value > alpha) * 905
               + (ttData.depth >= depth) * (935 + cutNode * 959);



        
        

        
        r += 850;  


                   

        
		r -= moveCount * 65;
        r -= std::abs(correctionValue) / 25600;



        
        

		

        
        if (cutNode)
            r += 3611 + 985 * !ttData.move;



        
        

        
        if (ttCapture)
            r += 1054;



        
        

        
        if ((ss + 1)->cutoffCnt > 1)
            r += 251 + 1124 * ((ss + 1)->cutoffCnt > 2) + 1042 * allNode;




        
        

        
        if (move == ttData.move)
            r -= 2239;



        
        if (capture)
            ss->statScore = 863 * int(PieceValue[pos.captured_piece()]) / 128
                          + captureHistory[movedPiece][move.to_sq()][type_of(pos.captured_piece())];


        else
            
            
            ss->statScore = 2 * mainHistory[us][move.raw()]
                          + (*contHist[0])[movedPiece][move.to_sq()]
                          + (*contHist[1])[movedPiece][move.to_sq()];

        
        

        
        r -= ss->statScore * 428 / 4096;
        if (cyfLmrCheckR && givesCheck && (!cyfLmrCheckSee || (!capture && pos.see_ge(move, Value(0)))))   
            r -= cyfLmrCheckR;



        
        
        if (allNode)
            r += r * 273 / (256 * depth + 260);



		
        
        
        

		

        if (depth >= 2 && moveCount > 1)
        {
            

            
            
            
            
            

			
            
            
            
            
            

			Depth d = std::max(1, std::min(newDepth - r / 1024, newDepth + 2)) + PvNode;

            const int cyfBaseline = d;
            if (cyfFusion && depth >= Cyfamate::fusion_service().cfg.min_depth)
                d = Cyfamate::fusion_depth(*cyfHint, move, !capture && !givesCheck, d, newDepth);   
#if defined(CYF_REPLAY)
            const bool cyfForceResearch = Cyfamate::replay().enabled && !excludedMove
                && Cyfamate::replay().at_lmr(ss->ply, uint64_t(posKey), move, d, newDepth);
#endif
            
            
            if (Cyfamate::harvest_probe().on && !rootNode && !ss->inCheck && !excludedMove && !capture && !givesCheck
                && cyfBaseline < newDepth)
                Cyfamate::harvest_probe().log((uint64_t)posKey, depth, rootDepth, threadIdx,
                    uint16_t(move.raw()), 1, uint8_t((cyfHint && cyfHint->arrived)
                    | ((move == (cyfHint ? cyfHint->move : Move::none())) << 1)
                    | ((cyfHint && cyfHint->disagrees) << 2) | ((d != cyfBaseline) << 3)), cyfBaseline, d);
            ss->reduction = newDepth - d;
            value         = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, d, true);
            ss->reduction = 0;

            
            

            
            

            if (value > alpha)
            {
                
                
                
                

                
                const bool doDeeperSearch    = d < newDepth && value > bestValue + 27;
                const bool doShallowerSearch = value < bestValue + 9;



                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

                
                

                
        update_continuation_histories(ss, movedPiece, move.to_sq(), 1426);


            }
#if defined(CYF_REPLAY)
            
            else if (cyfForceResearch && newDepth > d)
            {
                ++Cyfamate::replay().applied;
                value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);
            }
#endif
        }

		
        
        
        

        else if (!PvNode || moveCount > 1)
        {
            
            

            
            if (!ttData.move)
                r += 1057;



            
            
            
			value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha,
                                   newDepth - (r > 4628) - (r > 5772 && newDepth > 2), !cutNode);


		}

        
        
        
        

		

        if (PvNode && (moveCount == 1 || value > alpha))
        {
            
            (ss + 1)->pv = &pv;
            (ss + 1)->pv->clear();

            
            
			
			

			if (move == ttData.move
                && ((is_valid(ttData.value) && is_decisive(ttData.value) && ttData.depth > 0)
                    || ttData.depth > 1))
                newDepth = std::max(newDepth, 1);

			
            value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
        }

		
        
        
        

		undo_move(pos, move);

		ASSERT_LV3(-VALUE_INFINITE < value && value < VALUE_INFINITE);

		
        
        

        
        
        
		
        

        if (threads.stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

		
        
        

        if (rootNode)
        {
            
            RootMove& rm = rootMoves[std::find(rootMoveKeys.begin(), rootMoveKeys.end(), move) - rootMoveKeys.begin()];

			

            
			rm.effort += nodes - nodeCount;

			rm.averageScore =
              rm.averageScore != -VALUE_INFINITE ? (value + rm.averageScore) / 2 : value;

            rm.meanSquaredScore = rm.meanSquaredScore != -VALUE_INFINITE * VALUE_INFINITE
                                  ? (value * std::abs(value) + rm.meanSquaredScore) / 2
                                  : value * std::abs(value);

            
            

            if (moveCount == 1 || value > alpha)
            {
                
                

                rm.score = rm.uciScore = value;
                rm.selDepth            = selDepth;
                rm.scoreLowerbound = rm.scoreUpperbound = false;

                if (value >= beta)
                {
                    rm.scoreLowerbound = true;
                    rm.uciScore        = beta;
                }
                else if (value <= alpha)
                {
                    rm.scoreUpperbound = true;
                    rm.uciScore        = alpha;
                }

				
                rm.pv.resize(1);

				
                ASSERT_LV3((ss + 1)->pv);

				
                

                for (Move pvMove : *(ss + 1)->pv)
                    rm.pv.push_back(pvMove);

                
                
                

				
                
                

				
                

                if (moveCount > 1 && !pvIdx)
                    ++bestMoveChanges;
            }
            else
                
                
                

				
                
                
                

                rm.score = -VALUE_INFINITE;

				

            
            if (is_mainthread() && CyfIter::recording())
                CyfIter::on_root_score(move.raw(), moveCount == 1 || value > alpha);
#if defined(CYF_BLEND)
            
            rm.cyfRule = Cyfamate::blend_service().rule_id;
            rm.cyfPass = rootDepth;
#endif
        }

		
        
        

        
        

		
        
        

        int inc = (value == bestValue && ss->ply + 2 >= rootDepth && (int(nodes) & 14) == 0
                   && !is_win(std::abs(value) + 1));

        if (value + inc > bestValue)
        {
            bestValue = value;

            if (value + inc > alpha)
            {
                bestMove = move;

                if (PvNode && !rootNode)  
					                      
                    ss->pv->update(move, (ss + 1)->pv);

                if (value >= beta)
                {
					
					

                    ss->cutoffCnt += (extension < 2) || PvNode;
                    ASSERT_LV3(value >= beta);  

					
                    break;
                }

                
                

                if (depth > 2 && depth < 14 && !is_decisive(value))
                    depth -= 2;

				ASSERT_LV3(depth > 0);
                alpha = value;  
                lmrDeltaTerm = (beta - alpha) * 764 / rootDelta;   
                                

				
            }
        }

        
        

		
        

        if (move != bestMove && moveCount <= SEARCHEDLIST_CAPACITY)
        {
            if (capture)
                
                capturesSearched.push_back(move);
			else
                
                quietsSearched.push_back(move);
        }
    } 

	
    
    

    
    
    

	
	
	

	

	
    ASSERT_LV5(moveCount || !ss->inCheck || excludedMove || !MoveList<LEGAL_ALL>(pos).size());

    
    

    if (bestValue >= beta && !is_decisive(bestValue) && !is_decisive(alpha))
        bestValue = (bestValue * depth + beta) / (depth + 1);

	
    

#if STOCKFISH
    if (!moveCount)
        bestValue = excludedMove ? alpha : ss->inCheck ? mated_in(ss->ply) : VALUE_DRAW;
#else
    

	
    
    if (!moveCount)
        bestValue = excludedMove ? alpha : mated_in(ss->ply);
#endif

	
    
    

	else if (bestMove)
    {
        

        update_all_stats(pos, ss, *this, bestMove, prevSq, quietsSearched, capturesSearched, depth,
                         ttData.move);
        
        if (!PvNode)
            ttMoveHistory << (bestMove == ttData.move ? 805 : -787);


    }

    
    

	
    else if (!priorCapture && prevSq != SQ_NONE)
    {
        
        int bonusScale = -232;
        bonusScale -= (ss - 1)->statScore / 108;
        bonusScale += std::min(59 * depth, 454);
        bonusScale += 169 * ((ss - 1)->moveCount > 8);
        bonusScale += 145 * (!ss->inCheck && bestValue <= ss->staticEval - 110);
        bonusScale += 154 * (!(ss - 1)->inCheck && bestValue <= -(ss - 1)->staticEval - 73);

        bonusScale = std::max(bonusScale, 0);

        const int scaledBonus = std::min(135 * depth - 80, 1400) * bonusScale;

        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                      scaledBonus * 221 / 16384);

        mainHistory[~us][((ss - 1)->currentMove).raw()] << scaledBonus * 235 / 32768;

		
        if (type_of(pos.piece_on(prevSq)) != PAWN && ((ss - 1)->currentMove).type_of() != PROMOTION)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << scaledBonus * 290 / 8192;


    }

    
    

    else if (priorCapture && prevSq != SQ_NONE)
    {
        Piece capturedPiece = pos.captured_piece();
        assert(capturedPiece != NO_PIECE);
        
        captureHistory[pos.piece_on(prevSq)][prevSq][type_of(capturedPiece)] << 1018;


    }

    
    
#if STOCKFISH
    if (PvNode)
        bestValue = std::min(bestValue, maxValue);
#endif

    
    

	
    
    

    if (bestValue <= alpha)
        ss->ttPv = ss->ttPv || (ss - 1)->ttPv;

	
    
    

    
    

	
    

	

#if defined(CYF_BLEND_TTSHADOW)
    cyf_tt_shadow().pending_site = CYF_TT_SITE_SEARCH_END;
#endif
    if (!excludedMove && !(rootNode && pvIdx))
        ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                       bestValue >= beta    ? BOUND_LOWER
                       : PvNode && bestMove ? BOUND_EXACT
                                            : BOUND_UPPER,
                       moveCount != 0 ? depth : std::min(MAX_PLY - 1, depth + 6), bestMove,
                       unadjustedStaticEval, tt.generation());

    
    
	
	
	

	if (!ss->inCheck && !(bestMove && pos.capture(bestMove))
        && (bestValue > ss->staticEval) == bool(bestMove))
    {
        
        auto bonus =
          std::clamp(int(bestValue - ss->staticEval) * depth * (bestMove ? 12 : 17) / 128,
                     -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);

        update_correction_history(pos, ss, *this, 1069 * bonus / 1024);


    }

	
	ASSERT_LV3(-VALUE_INFINITE < bestValue && bestValue < VALUE_INFINITE);

    return bestValue;
}















template<NodeType nodeType, bool ReadTT>
Value Search::YaneuraOuWorker::qsearch(Position& pos, Stack* ss, Value alpha, Value beta) {

    

    
    
    

    
    

    static_assert(nodeType != Root);
    constexpr bool PvNode = nodeType == PV;

    ASSERT_LV3(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    ASSERT_LV3(PvNode || (alpha == beta - 1));

    
    
#if STOCKFISH
    
    

    
    
    

    if (alpha < VALUE_DRAW && pos.upcoming_repetition(ss->ply))
    {
        alpha = value_draw(nodes);
        if (alpha >= beta)
            return alpha;
    }
#endif

    
    
    PVMoves pv;

    
    StateInfo st;

    
	Key posKey;

    
    
    Move move, bestMove;

    
    
    
    Value bestValue, value, futilityBase;

    
    
    
    bool pvHit, givesCheck, capture;

    
    int moveCount;

    
    
    
    

    if (PvNode)
    {
        (ss + 1)->pv = &pv;
        ss->pv->clear();
    }

    bestMove                    = Move::none();
    ss->inCheck                 = pos.checkers();
    moveCount                   = 0;

#if defined(USE_CLASSIC_EVAL) && defined(USE_LAZY_EVALUATE)
    bool evaluated = false;
    auto evaluate  = [&](Position& pos) {
        evaluated = true;
        return this->evaluate(pos);
    };
    auto do_move = [&](Position& pos, Move move, StateInfo& st, bool givesCheck, Stack* ss) {
        if (!evaluated)
        {
            evaluated = true;
            Eval::evaluate_with_no_return(pos);
        }
        this->do_move(pos, move, st, givesCheck, ss);
    };
#endif

#if STOCKFISH
#else
    
    auto& search_options = main_manager()->search_options;
#endif

    
    

    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    
    
    
    

    
    
    
    

#if STOCKFISH
    if (pos.is_draw(ss->ply) || ss->ply >= MAX_PLY)
        return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos) : VALUE_DRAW;

#else

    
    

    

    
    Color us = pos.side_to_move();

    auto draw_type = pos.is_repetition(ss->ply);
    if (draw_type != REPETITION_NONE)
    
    {
        if (draw_type == REPETITION_DRAW)
            
            
            
            return draw_value(draw_type, pos.side_to_move()) + value_draw(nodes);
		else
	        return value_from_tt(draw_value(draw_type, us), ss->ply);
    }


#if 0
	
    
    
    if (depth <= -16)
        return draw_value(REPETITION_DRAW, us);
#endif

    
    if (ss->ply >= MAX_PLY || pos.game_ply() > search_options.max_moves_to_draw)
        return draw_value(REPETITION_DRAW, us) + value_draw(nodes);

    ASSERT_LV3(0 <= ss->ply && ss->ply < MAX_PLY);

#endif

    
    
    
    

    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey, pos);

#if !STOCKFISH
    if constexpr (!ReadTT)
        ttHit = false;
#endif

    
    

    ss->ttHit   = ttHit;
    ttData.move = ttHit ? ttData.move : Move::none();

    ttData.value =
#if STOCKFISH
      ttHit ? value_from_tt(ttData.value, ss->ply , pos.rule50_count()) : VALUE_NONE;
#else
      ttHit ? value_from_tt(ttData.value, ss->ply ) : VALUE_NONE;
#endif      
    pvHit = ttHit && ttData.is_pv;

    
    ASSERT_LV3(pos.legal_promote(ttData.move));

    
    

    

    if (!PvNode && ttData.depth >= DEPTH_QS
        && is_valid(ttData.value)  
        
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER)))
        
        return ttData.value;
    

    
    
    
    

    Value unadjustedStaticEval = VALUE_NONE;
    if (ss->inCheck)
    {
        
        bestValue = futilityBase = -VALUE_INFINITE;
	}
    else
    {
        const auto correctionValue = correction_value(*this, pos, ss) + correction_extra(*this, pos);   

		if (ss->ttHit)
        {
            
            

			
            

#if defined(CYF_BLEND)
            if (Cyfamate::blend_config().verify && is_valid(ttData.eval))
#if defined(CYF_BLEND_TTSHADOW)
                
                Cyfamate::blend_service().verify_tt_eval(pos, ttData.eval, PvNode ? 1 : 2, ttWriter.debug_entry());
#else
                Cyfamate::blend_service().verify_tt_eval(pos, ttData.eval);
#endif
#endif
            unadjustedStaticEval = ttData.eval;
            if (!is_valid(unadjustedStaticEval))
                unadjustedStaticEval = evaluate(pos);
#if defined(USE_CLASSIC_EVAL)
			else if (PvNode) {
				
				unadjustedStaticEval = evaluate(pos);
				
			}
#endif

			ss->staticEval = bestValue =
				to_corrected_static_eval(unadjustedStaticEval, correctionValue);

			
            

			

            if (is_valid(ttData.value) && !is_decisive(ttData.value)
                && (ttData.bound & (ttData.value > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttData.value;
        }
        else
        {
            
            
            

            
            

			ASSERT_LV3(!ss->inCheck && !ss->ttHit);
            if (true)
            {
                
                
                
                
                

                
                move = Mate::mate_1ply(pos);
                if (move != Move::none())
                {
                    bestValue = mate_in(ss->ply + 1);

                    if (true)
                    {
                        
						
						

#if defined(CYF_BLEND_TTSHADOW)
						cyf_tt_shadow().pending_site = CYF_TT_SITE_QS_MATE;
#endif
						ttWriter.write(posKey, bestValue, ss->ttPv, BOUND_EXACT, DEPTH_QS, move,
                                       unadjustedStaticEval, tt.generation());

                    }

                    return bestValue;
                }
            }

			

            unadjustedStaticEval = evaluate(pos);

            ss->staticEval = bestValue =
				to_corrected_static_eval(unadjustedStaticEval, correctionValue);

#if 0       
            unadjustedStaticEval =
              (ss - 1)->currentMove != Move::null()
                ? evaluate(pos)
                : -(ss - 1)->staticEval;
#endif

        }

        
        

		

        if (bestValue >= beta)
        {
            if (!is_decisive(bestValue))
                
                bestValue = (bestValue + beta) / 2;

#if defined(CYF_BLEND_TTSHADOW)
            cyf_tt_shadow().pending_site = CYF_TT_SITE_QS_STANDPAT;
#endif
            if (!ss->ttHit)
                ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), false, BOUND_LOWER,
                               DEPTH_UNSEARCHED, Move::none(), unadjustedStaticEval,
                               tt.generation());
            return bestValue;
        }

		 
		if (bestValue > alpha)
            alpha = bestValue;

        
        

        
		futilityBase = ss->staticEval + 328;



    }

	
    
    

    const PieceToHistory* contHist[] = {(ss - 1)->continuationHistory};

	
    
	

    Square prevSq = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;

    
    
    

	
    
    

    MovePicker mp(pos, ttData.move, DEPTH_QS, &mainHistory, &lowPlyHistory, &captureHistory,
                  contHist, &sharedHistory, ss->ply
#if !STOCKFISH
                  ,
                  search_options.generate_all_legal_moves
#endif
    );

	
    
    
    

    while ((move = mp.next_move_qsearch()) != Move::none())
    {
        

		
        ASSERT_LV3(pos.pseudo_legal_s<true>(move) && pos.legal_promote(move));

		

        if (!pos.legal(move))
            continue;

		

        givesCheck = pos.gives_check_ready(move);   
        capture    = pos.capture_stage(move);

        moveCount++;

		
        
        
        

		

		if (!is_loss(bestValue))
        {
            
            

            if (!givesCheck && move.to_sq() != prevSq && !is_loss(futilityBase)
#if STOCKFISH            
				
                && move.type_of() != PROMOTION
				
				
#endif                
				)
            {
				
				
				

				
				if (moveCount > 2)
                    continue;



				

                Value futilityValue = futilityBase +
                                    PieceValue[pos.piece_on(move.to_sq())];
                                
                                

                
                

				
                

				

				if (futilityValue <= alpha)
                {
                    bestValue = std::max(bestValue, futilityValue);
                    continue;
                }

                
                

				
                

				
                

				if (!pos.see_ge(move, alpha - futilityBase))
                {
                    bestValue = std::max(bestValue, std::min(alpha, futilityBase));
                    continue;
                }
            }

            
			

			if (!capture)
                continue;

            
            

			

            
            if (!pos.see_ge(move, -73))
                continue;



        }

		
        
        
        

		

        do_move(pos, move, st, givesCheck, ss);

        value = -qsearch<nodeType, ReadTT>(pos, ss + 1, -beta, -alpha);
        undo_move(pos, move);

		ASSERT_LV3(-VALUE_INFINITE < value && value < VALUE_INFINITE);

        
		
        
        

		

        if (value > bestValue)
        {
            bestValue = value;

            if (value > alpha)
            {
                bestMove = move;

                if (PvNode)  
							 
                    ss->pv->update(move, (ss + 1)->pv);

                if (value < beta)  
                                   
				                   
                    alpha = value;
                else
                    break;  
            }
        }
    }

	
    
    
    

	
    

	
    

	


#if STOCKFISH        
	if (ss->inCheck && bestValue == -VALUE_INFINITE)
    {
        assert(!MoveList<LEGAL>(pos).size());
#else
	if (ss->inCheck && moveCount == 0)
    {
		
        
        ASSERT_LV5(!MoveList<LEGAL_ALL>(pos).size());
#endif

		return mated_in(ss->ply);  
                                   
    }

    if (!is_decisive(bestValue) && bestValue > beta)
        bestValue = (bestValue + beta) / 2;

	
	
#if STOCKFISH
    Color us = pos.side_to_move();
    if (!ss->inCheck && !moveCount && !pos.non_pawn_material(us)
        && type_of(pos.captured_piece()) >= ROOK)
    {
        if (!((us == WHITE ? shift<NORTH>(pos.pieces(us, PAWN))
                           : shift<SOUTH>(pos.pieces(us, PAWN)))
              & ~pos.pieces()))  
        {
            pos.state()->checkersBB = Rank1BB;  
            if (!MoveList<LEGAL>(pos).size())   
                bestValue = VALUE_DRAW;
            pos.state()->checkersBB = 0;
        }
    }
#endif

    
    

	
    

	
    

#if defined(CYF_BLEND_TTSHADOW)
    cyf_tt_shadow().pending_site = CYF_TT_SITE_QS_END;
#endif
    ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), pvHit,
                   bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, DEPTH_QS, bestMove,
                   unadjustedStaticEval, tt.generation());

	

	ASSERT_LV3(-VALUE_INFINITE < bestValue && bestValue < VALUE_INFINITE);

    return bestValue;
}



int Search::YaneuraOuWorker::reduction(bool i, Depth d, int mn, int delta) const {
    int reductionScale = reductions[d] * reductions[mn];
    return reductionScale - delta * 585 / rootDelta + !i * reductionScale * 206 / 512 + 1133;

}





#if STOCKFISH
















TimePoint Search::Worker::elapsed() const {
    return main_manager()->tm.elapsed([this]() { return threads.nodes_searched(); });
}

TimePoint Search::Worker::elapsed_time() const { return main_manager()->tm.elapsed_time(); }
#endif

Value Search::YaneuraOuWorker::evaluate(const Position& pos) {
#if defined(CYF_EVAL_PROFILE)
    struct Timer { uint64_t t0 = __rdtsc(); ~Timer() { Cyfamate::eval_cycles() += __rdtsc() - t0; ++Cyfamate::eval_calls(); } } timer;
#endif

#if defined(EVAL_SFNN)
	

    return Eval::evaluate(networks[numaAccessToken], pos, accumulatorStack, refreshTable,
                          optimism[pos.side_to_move()]);

#else
#if defined(CYF_BLEND)
    
    if (Cyfamate::blend_active())
#if defined(CYF_BLEND_DEFER_NNUE)
        return Cyfamate::blend_service().evaluate(pos, VALUE_ZERO, true, true);
#else
        return Cyfamate::blend_service().evaluate(pos, Cyfamate::exact_nnue(pos));
#endif
#endif
	return Cyfamate::exact_nnue(pos);
#endif
}

namespace {












Value value_to_tt(Value v, int ply) { return is_win(v) ? v + ply : is_loss(v) ? v - ply : v; }

















Value value_from_tt(Value v, int ply
#if STOCKFISH
    , int r50c
#endif
) {

    if (!is_valid(v))
        return VALUE_NONE;

    
    if (is_win(v))
    {
		
#if STOCKFISH
        
        if (v >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - v > 100 )
            return VALUE_TB_WIN_IN_MAX_PLY - 1;

        
        if (VALUE_TB - v > 100 )
            return VALUE_TB_WIN_IN_MAX_PLY - 1;
#endif

        return v - ply;
    }

    
    if (is_loss(v))
    {
		

#if STOCKFISH
        
        if (v <= VALUE_MATED_IN_MAX_PLY && VALUE_MATE + v > 100 )
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;

        
        if (VALUE_TB + v > 100 )
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;
#endif
        return v + ply;
    }

    return v;
}










void update_all_stats(const Position&          pos,
                      Stack*                   ss,
                      Search::YaneuraOuWorker& workerThread,
                      Move                     bestMove,
                      Square                   prevSq,
                      SearchedList&            quietsSearched,
                      SearchedList&            capturesSearched,
                      Depth                    depth,
                      Move                     ttMove) {


    CapturePieceToHistory& captureHistory = workerThread.captureHistory;
    Piece                  movedPiece     = pos.moved_piece(bestMove);
    PieceType              capturedPiece;

    
    int bonus =
      std::min(128 * depth - 77, 1529) + 353 * (bestMove == ttMove) + (ss - 1)->statScore / 32;
    int malus = std::min(882 * depth - 204, 2122);




    

    if (!pos.capture_stage(bestMove))
    {
        update_quiet_histories_body(pos, ss, workerThread, bestMove, bonus * 806 / 1024);

        int actualMalus = malus * 1113 / 1024;
        
        

        for (Move move : quietsSearched)
        {
            actualMalus = actualMalus * 977 / 1024;
            update_quiet_histories_body(pos, ss, workerThread, move, -actualMalus);
        }
    }
    else
    {
        
        

        capturedPiece = type_of(pos.piece_on(bestMove.to_sq()));
        captureHistory[movedPiece][bestMove.to_sq()][capturedPiece] << bonus * 1286 / 1024;
    }

    
    

    
    

    
    

    if (prevSq != SQ_NONE && ((ss - 1)->moveCount == 1 + (ss - 1)->ttHit) && !pos.captured_piece())
        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -malus * 616 / 1024);

    
    

    for (Move move : capturesSearched)
    {
        

        movedPiece    = pos.moved_piece(move);
        capturedPiece = type_of(pos.piece_on(move.to_sq()));
        captureHistory[movedPiece][move.to_sq()][capturedPiece] << -malus * 1559 / 1024;
    }
}









void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {
    
    static constexpr std::array<ConthistBonus, 6> conthist_bonuses = {
      {{1, 1157}, {2, 648}, {3, 288}, {4, 576}, {5, 140}, {6, 441}}};



    for (const auto [i, weight] : conthist_bonuses)
    {
        
        

        if (ss->inCheck && i > 2)
            break;
        if (((ss - i)->currentMove).is_ok())
            (*(ss - i)->continuationHistory)[pc][to] << (bonus * weight / 1024) + 88 * (i < 2);
    }
}









void update_quiet_histories(
  const Position& pos, Stack* ss, Search::YaneuraOuWorker& workerThread, Move move, int bonus) {
    update_quiet_histories_body(pos, ss, workerThread, move, bonus);
}

FORCE_INLINE void update_quiet_histories_body(
  const Position& pos, Stack* ss, Search::YaneuraOuWorker& workerThread, Move move, int bonus) {

    Color us = pos.side_to_move();
    workerThread.mainHistory[us][move.raw()] << bonus;  
														

    
    if (ss->ply < LOW_PLY_HISTORY_SIZE)
        workerThread.lowPlyHistory[ss->ply][move.raw()] << bonus * 761 / 1024;

    update_continuation_histories(ss, pos.moved_piece(move), move.to_sq(), bonus * 955 / 1024);



	
	workerThread.sharedHistory.pawn_entry(pos)[pos.moved_piece(move)][move.to_sq()]
      << bonus * (bonus > 0 ? 850 : 550) / 1024;



}

} 


#if STOCKFISH





Move Skill::pick_best(const RootMoves& rootMoves, size_t multiPV) {
    static PRNG rng(now());  
							 

    
	

	Value  topScore = rootMoves[0].score;
    int    delta    = std::min(topScore - rootMoves[multiPV - 1].score, int(PawnValue));
    int    maxScore = -VALUE_INFINITE;
    double weakness = 120 - 2 * level;

    
    
    

	
	
	

	for (size_t i = 0; i < multiPV; ++i)
    {
        
		

		int push = int(weakness * int(topScore - rootMoves[i].score)
                       + delta * (rng.rand<unsigned>() % int(weakness)))
                 / 128;

        if (rootMoves[i].score + push >= maxScore)
        {
            maxScore = rootMoves[i].score + push;
            best     = rootMoves[i].pv[0];
        }
    }

    return best;
}
#endif










void SearchManager::check_time_expired(Search::YaneuraOuWorker& worker) {

    
    

    callsCnt = worker.limits.nodes ? std::min(512, int(worker.limits.nodes / 1024)) : 512;

    static TimePoint lastInfoTime = now();

    TimePoint elapsed = tm.elapsed([&worker]() { return worker.threads.nodes_searched(); });
    TimePoint tick    = worker.limits.startTime + elapsed;

    if (tick - lastInfoTime >= 1000)
    {
        lastInfoTime = tick;
        dbg_print();
    }

    
    
    
    

    if (ponder)
        return;

	if (
    
    

    
    
    

    

#if STOCKFISH
      worker.completedDepth >= 1
      && ((worker.limits.use_time_management() && (elapsed > tm.maximum() || stopOnPonderhit)) || (worker.limits.movetime && elapsed >= worker.limits.movetime)
          || (worker.limits.nodes && worker.threads.nodes_searched() >= worker.limits.nodes)))
        worker.threads.stop = worker.threads.abortedSearch = true;
#else
      worker.completedDepth >= 1)
    {
        if (
          
          (worker.limits.movetime && elapsed >= worker.limits.movetime)
          
          || (worker.limits.nodes && worker.threads.nodes_searched() >= worker.limits.nodes)
          
          || (tm.search_end && tm.search_end <= elapsed))

            worker.threads.stop = worker.threads.abortedSearch = true;

        else if (!tm.search_end && worker.limits.use_time_management() &&
				 
				 (elapsed > tm.maximum()
				 
				  || stopOnPonderhit))
            tm.set_search_end(elapsed);
    }


#endif
}


#if STOCKFISH




void syzygy_extend_pv(const OptionsMap&         options,
                      const Search::LimitsType& limits,
                      Position&                 pos,
                      RootMove&                 rootMove,
                      Value&                    v) {

    auto t_start      = std::chrono::steady_clock::now();
    int  moveOverhead = int(options["Move Overhead"]);
    bool rule50       = bool(options["Syzygy50MoveRule"]);

    
    auto time_abort = [&t_start, &moveOverhead, &limits]() -> bool {
        auto t_end = std::chrono::steady_clock::now();
        return limits.use_time_management()
            && 2 * std::chrono::duration<double, std::milli>(t_end - t_start).count()
                 > moveOverhead;
    };

    std::list<StateInfo> sts;

    
    auto& stRoot = sts.emplace_back();
    pos.do_move(rootMove.pv[0], stRoot);
    int ply = 1;

    
    while (size_t(ply) < rootMove.pv.size())
    {
        Move& pvMove = rootMove.pv[ply];

        RootMoves legalMoves;
        for (const auto& m : MoveList<LEGAL>(pos))
            legalMoves.emplace_back(m);

        TB::Config config = TB::rank_root_moves(options, pos, legalMoves, false, time_abort);
        RootMove&  rm     = *std::find(legalMoves.begin(), legalMoves.end(), pvMove);

        if (legalMoves[0].tbRank != rm.tbRank)
            break;

        ply++;

        auto& st = sts.emplace_back();
        pos.do_move(pvMove, st);

        
        if (config.rootInTB && ((rule50 && pos.is_draw(ply)) || pos.is_repetition(ply)))
        {
            pos.undo_move(pvMove);
            ply--;
            break;
        }

        
        
        if (config.rootInTB && time_abort())
            break;
    }

    
    rootMove.pv.resize(ply);

    
    
    
    while (!(rule50 && pos.is_draw(0)))
    {
        if (time_abort())
            break;

        RootMoves legalMoves;
        for (const auto& m : MoveList<LEGAL>(pos))
        {
            auto&     rm = legalMoves.emplace_back(m);
            StateInfo tmpSI;
            pos.do_move(m, tmpSI);
            
            
            for (const auto& mOpp : MoveList<LEGAL>(pos))
                rm.tbRank -= pos.capture(mOpp) ? 100 : 1;
            pos.undo_move(m);
        }

        
        if (legalMoves.size() == 0)
            break;

        
        
        std::stable_sort(
          legalMoves.begin(), legalMoves.end(),
          [](const Search::RootMove& a, const Search::RootMove& b) { return a.tbRank > b.tbRank; });

        
        TB::Config config = TB::rank_root_moves(options, pos, legalMoves, true, time_abort);

        
        if (!config.rootInTB || config.cardinality > 0)
            break;

        ply++;

        Move& pvMove = legalMoves[0].pv[0];
        rootMove.pv.push_back(pvMove);
        auto& st = sts.emplace_back();
        pos.do_move(pvMove, st);
    }

    
    
    
    
    
    
    
    
    
    
    if (pos.is_draw(0))
        v = VALUE_DRAW;

    
    for (size_t i = rootMove.pv.size(); i > 0; --i)
        pos.undo_move(rootMove.pv[i - 1]);

    
    if (time_abort())
        sync_cout
          << "info string Syzygy based PV extension requires more time, increase Move Overhead as needed."
          << sync_endl;
}
#endif


void SearchManager::pv(Search::YaneuraOuWorker&  worker,
                       const ThreadPool&         threads,
                       const TranspositionTable& tt,
                       Depth                     depth) {

    const auto nodes     = threads.nodes_searched();
    auto&      rootMoves = worker.rootMoves;
    auto&      pos       = worker.rootPos;
    size_t     pvIdx     = worker.pvIdx;
    size_t     multiPV   = std::min(size_t(worker.options["MultiPV"]), rootMoves.size());
#if STOCKFISH
    uint64_t tbHits = threads.tb_hits() + (worker.tbConfig.rootInTB ? rootMoves.size() : 0);
#endif

    for (size_t i = 0; i < multiPV; ++i)
    {
        bool updated = rootMoves[i].score != -VALUE_INFINITE;

        if (depth == 1 && !updated && i > 0)
            continue;

        Depth d = updated ? depth : std::max(1, depth - 1);
        Value v = updated ? rootMoves[i].uciScore : rootMoves[i].previousScore;

        if (v == -VALUE_INFINITE)
            v = VALUE_ZERO;

#if STOCKFISH
        bool tb = worker.tbConfig.rootInTB && std::abs(v) <= VALUE_TB;
        v       = tb ? rootMoves[i].tbScore : v;
#endif

#if STOCKFISH
        bool isExact = i != pvIdx || tb || !updated;  

        
        

        if (is_decisive(v) && std::abs(v) < VALUE_MATE_IN_MAX_PLY
            && ((!rootMoves[i].scoreLowerbound && !rootMoves[i].scoreUpperbound) || isExact))
            syzygy_extend_pv(worker.options, worker.limits, pos, rootMoves[i], v);
#else
        bool isExact =
          i != pvIdx  || !updated;  
                                               

#endif

        std::string pv;
#if STOCKFISH
        for (Move m : rootMoves[i].pv)
            pv += UCIEngine::move(m, pos.is_chess960()) + " ";
#else
		
        if (worker.main_manager()->search_options.consideration_mode)
        {
            Move      moves[MAX_PLY + 1];
            StateInfo si[MAX_PLY];
            int       ply = 0;

            while (ply < MAX_PLY)
            {
                
                
                
                auto rep = pos.is_repetition(ply);
                if (rep != REPETITION_NONE && ply >= 1)
                {
                    
                    pv += to_usi_string(rep) + ' ';
                    break;
                }

                Move m;

                
                
                if (ply < int(rootMoves[i].pv.size()))
                    m = rootMoves[i].pv[ply];
                else
                {
                    
                    auto [ttHit, ttData, ttWriter] = tt.probe(pos.key(), pos);
                    
                    if (!ttHit)
                        break;

                    m = ttData.move;

                    
                    if (m == Move::none())
                        break;

                    
                    if (!(pos.pseudo_legal_s<true>(m) && pos.legal(m)))
                        break;
                }
                
                
                
                if (!m.is_ok())
                {
                    pv += USIEngine::move(m) + ' ';
                    break;
                }

                moves[ply] = m;
                pv += USIEngine::move(m) + ' ';

                pos.do_move(m, si[ply]);
                ++ply;
            }
            while (ply > 0)
                pos.undo_move(moves[--ply]);
		}
        else
            for (Move m : rootMoves[i].pv)
                pv += USIEngine::move(m) + ' ';
#endif

        
        

        if (!pv.empty())
            pv.pop_back();

#if STOCKFISH
        auto wdl = worker.options["UCI_ShowWDL"] ? UCIEngine::wdl(v, pos) : "";
#endif
        auto bound = rootMoves[i].scoreLowerbound
                     ? "lowerbound"
                     : (rootMoves[i].scoreUpperbound ? "upperbound" : "");

        InfoFull info;

        info.depth    = d;
        info.selDepth = rootMoves[i].selDepth;
        info.multiPV  = i + 1;
#if STOCKFISH
        info.score = {v, pos};  
        info.wdl   = wdl;
#else
        info.score = v;
#endif

        if (!isExact || rootMoves[i].zyfMateLower)
            info.bound = bound;

        TimePoint time = std::max(TimePoint(1), tm.elapsed_time());
        info.timeMs    = time;
        info.nodes     = nodes;
        info.nps       = nodes * 1000 / time;
#if STOCKFISH
        info.tbHits = tbHits;
#endif
        info.pv       = pv;
        info.hashfull = tt.hashfull();

        updates.onUpdateFull(info);
    }
}











bool RootMove::extract_ponder_from_tt(const TranspositionTable& tt,
                                              Position&                 pos,
                                              Move                      ponder_candidate) {
    StateInfo st;

    ASSERT_LV3(pv.size() == 1);

    
    
    
    

    if (!pv[0].is_ok())
        return false;

    pos.do_move(pv[0], st);

    auto [ttHit, ttData, ttWriter] = tt.probe(pos.key(), pos);
    if (ttHit)
    {
        Move m = ttData.move;
        
        
        if (pos.pseudo_legal_s<true>(m) && pos.legal(m))
            pv.push_back(m);
    }
    
    
    else if (ponder_candidate)
    {
        Move m = ponder_candidate;
        if (pos.pseudo_legal_s<true>(m) && pos.legal(m))
            pv.push_back(m);
    }

    pos.undo_move(pv[0]);
    return pv.size() > 1;
}

}  

using namespace YaneuraOu;
namespace {


void engine_main() {

	
    auto engine = std::make_unique<YaneuraOuEngine>();

    
    auto usi = std::make_unique<USIEngine>();
    usi->set_engine(*engine);  
    usi->enqueue_startup_commands(CommandLine::g);

    
    usi->loop();
}


static EngineFuncRegister r(engine_main, "YaneuraOuEngine", 0);
}

#endif 


#undef Options
#undef Threads
#undef Threads
#include <cstdlib>
#include <sstream>
#include <queue>


namespace YaneuraOu {
namespace Cyfamate {

inline char csa_code_to_usi_drop(const std::string& pc){
	if(pc=="FU")return 'P'; if(pc=="KY")return 'L'; if(pc=="KE")return 'N';
	if(pc=="GI")return 'S'; if(pc=="KA")return 'B'; if(pc=="HI")return 'R'; if(pc=="KI")return 'G';
	return 0;
}
inline bool csa_is_promoted(const std::string& pc){
	return pc=="TO"||pc=="NY"||pc=="NK"||pc=="NG"||pc=="UM"||pc=="RY";
}

inline std::string csa_sq_to_usi(const std::string& c2){
	return std::string(1,c2[0]) + char('a' + (c2[1]-'1'));
}

inline Move csa_to_move(const Position& pos, const std::string& csa_in){
	std::string s = csa_in;
	size_t c = s.find(','); if (c!=std::string::npos) s = s.substr(0,c);
	while(!s.empty() && (s.front()==' '||s.front()=='\r'||s.front()=='\t')) s.erase(s.begin());
	while(!s.empty() && (s.back()==' '||s.back()=='\r'||s.back()=='\t')) s.pop_back();
	if(!s.empty() && (s[0]=='+'||s[0]=='-')) s = s.substr(1);
	if (s.size() < 6) return Move::none();
	std::string from = s.substr(0,2), to = s.substr(2,2), pc = s.substr(4,2);
	std::string usi_to = csa_sq_to_usi(to);
	if (from == "00"){
		char d = csa_code_to_usi_drop(pc);
		if(!d) return Move::none();
		return USIEngine::to_move(pos, std::string(1,d) + "*" + usi_to);
	}
	std::string usi_from = csa_sq_to_usi(from);
	std::string usi = usi_from + usi_to;
	if (csa_is_promoted(pc)){
		File f = File(usi_from[0]-'1'); Rank r = Rank(usi_from[1]-'a');
		Square fsq = (f | r);
		PieceType on = type_of(pos.piece_on(fsq));
		if (on>=1 && on<PRO_PAWN) usi += "+";
	}
	return USIEngine::to_move(pos, usi);
}

} 
} 