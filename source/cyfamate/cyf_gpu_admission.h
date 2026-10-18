#pragma once
#include <atomic>
#include <cstdlib>
namespace CyfGpuAdmission {
struct Gate {
 std::atomic<unsigned> active{0},peak{0};
 std::atomic<unsigned long long> accepted[2]{},rejected[2]{};
 unsigned limit=2;
 Gate(){if(auto p=std::getenv("CYF_GPU_MAX_INFLIGHT")){int n=std::atoi(p);if(n<1||n>2)std::abort();limit=unsigned(n);}}
 bool take(unsigned kind){
  unsigned n=active.load(std::memory_order_relaxed);
  for(int tries=0;tries<2 && n<limit;++tries)
   if(active.compare_exchange_weak(n,n+1,std::memory_order_acq_rel)){
    ++accepted[kind];unsigned p=peak.load();while(p<n+1&&!peak.compare_exchange_weak(p,n+1)){}
    return true;
   }
  ++rejected[kind];return false;
 }
 void release(){unsigned n=active.fetch_sub(1,std::memory_order_acq_rel);if(!n)std::abort();}
};
inline Gate& gate(){static Gate* g=new Gate;return *g;}
}
