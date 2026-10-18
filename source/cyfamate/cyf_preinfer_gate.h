#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace CyfPreInfer {
enum class Status { scored, unknown, na };
inline const char* name(Status s) { return s==Status::scored ? "scored" : s==Status::unknown ? "unknown" : "na"; }
struct Input { int cp=0, depth=0; uint16_t move=0; bool cp_ok=false; char bound='n'; uint64_t nodes=0; };
struct Result { Status status=Status::na; unsigned mask=0; double score=-1; int stratum=-1; };
inline Result evaluate(const Input& prev, const Input& cur, int legal) {
    Result r;
    if (!cur.cp_ok || !cur.move) { r.mask=4; return r; }
    if (legal<1) { r.mask=8; return r; }
    if (cur.depth<0 || (cur.bound!='e' && cur.bound!='l' && cur.bound!='u')) { r.mask=16; return r; }
    if (!prev.move) r.mask|=1;
    if (!prev.cp_ok) r.mask|=2;
    if (r.mask) { r.status=Status::unknown; return r; }
    const double abs_cp=std::abs(double(cur.cp));
    const double x[]={ double(prev.move!=cur.move), std::min(abs_cp,3000.0)/600.0,
        std::min(std::abs(double(cur.cp)-double(prev.cp)),3000.0)/600.0,
        double(cur.depth), double(cur.bound!='e'), std::log(double(legal)) };
    constexpr double mu[]={.3706244087038789,3.535071349731955,1.4622031693472075,14.949858088930936,0.,4.664474040119763};
    constexpr double sd[]={.48297200371946813,1.7720307318065474,1.3835944927010835,1.9176747190324244,1.,.5177751537104957};
    constexpr double w[]={.45922440834285655,.20102287953918116,-.014395794130557813,-.3428588074512996,0.,-.08738723272832465};
    double terms=0;
    for (int i=0;i<6;++i) terms+=w[i]*(x[i]-mu[i])/sd[i];
    const double z=-.9046330400219769+terms;
    r.score=1.0/(1.0+std::exp(-z));
    r.status=Status::scored;
    r.stratum=(abs_cp<=300 ? 0 : abs_cp<=1000 ? 2 : 4)+int(prev.move!=cur.move);
    return r;
}
inline double threshold(int percent) {
    return percent==10 ? .44461758526802975 : percent==20 ? .4051388378818618 :
           percent==30 ? .36580359866098067 : percent==50 ? .2606278016900741 : 2.;
}
inline bool fires(double score, int percent) { return score>=threshold(percent); } 
enum class BudgetStatus { disabled, unknown, insufficient, eligible };
inline const char* name(BudgetStatus s) {
    return s==BudgetStatus::disabled ? "disabled" : s==BudgetStatus::unknown ? "unknown" :
           s==BudgetStatus::insufficient ? "insufficient" : "eligible";
}
struct BudgetResult { BudgetStatus status=BudgetStatus::unknown; int64_t remaining_ms=-1; };
inline BudgetResult budget_decision(int64_t optimum_ms,int64_t elapsed_ms,int min_ms) {
    const bool known=optimum_ms>=0 && elapsed_ms>=0;
    const int64_t remaining=known ? (elapsed_ms>=optimum_ms ? 0 : optimum_ms-elapsed_ms) : -1;
    if (min_ms<=0) return {BudgetStatus::disabled,remaining};
    if (!known) return {BudgetStatus::unknown,-1};
    return {remaining>=min_ms ? BudgetStatus::eligible : BudgetStatus::insufficient,remaining};
}
inline const char* budget_eligible(const BudgetResult& r) {
    return r.status==BudgetStatus::unknown ? "na" : r.status==BudgetStatus::insufficient ? "0" : "1";
}
struct State {
    std::atomic<bool> enabled{false};
    std::atomic<int> decision_nodes{100000};
    std::atomic<bool> root_ready_requested{false};
    std::atomic<int> min_budget_requested_ms{0};
    uint64_t gen=0,key=0,rejected=0;
    int ply=0, legal=0, search_legal=0;
    uint64_t target_nodes=100000, prev_target_nodes=10000;
    bool active=false,emitted=false,ready_mode=false;
    const char* reason="off";
    Input prev,cur;
    Result result;
    int min_budget_ms=0;
    BudgetResult budget;
    void begin(uint64_t g,uint64_t k,int p,int n,bool supported,int search_n=-1) {
        gen=g; key=k; ply=p; legal=n; rejected=0; prev={}; cur={}; result={}; emitted=false;
        search_legal=search_n<0 ? n : search_n;
        target_nodes=uint64_t(std::clamp(decision_nodes.load(std::memory_order_relaxed),10000,1000000));
        prev_target_nodes=target_nodes/10;
        ready_mode=root_ready_requested.load(std::memory_order_relaxed);
        min_budget_ms=std::clamp(min_budget_requested_ms.load(std::memory_order_relaxed),0,10000);
        budget={};
        active=enabled.load(std::memory_order_relaxed) && supported;
        reason=!enabled.load(std::memory_order_relaxed) ? "off" : !supported ? "unsupported" :
               ready_mode ? "await_root_ready" : "below_budget";
    }
    bool root_ready(uint64_t g,uint64_t k) {
        if (!active || emitted || !ready_mode || legal<1 || search_legal<1) return false;
        if (g!=gen || k!=key || !gen) { ++rejected; return false; }
        result={Status::unknown,7,-1,-1}; emitted=true; reason="root_ready";
        return true;
    }
    bool completed(uint64_t g,uint64_t k,const Input& in) {
        if (!active || emitted || ready_mode) return false;
        if (g!=gen || k!=key || !gen) { ++rejected; return false; }
        if (in.nodes>=target_nodes) {
            cur=in; result=evaluate(prev,cur,legal); emitted=true; reason="iteration_proxy"; return true;
        }
        if (!prev.nodes && in.nodes>=prev_target_nodes) prev=in;
        return false;
    }
};
inline State& state() { static State s; return s; }
} 
