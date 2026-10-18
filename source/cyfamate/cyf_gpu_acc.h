#pragma once
#include <cuda.h>
#include <array>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <chrono>
namespace CyfGpuAcc {
struct Service {
 static constexpr int B=32,N=4096;
 struct Entry { bool valid=false,used=false; uint64_t age=0; std::array<int,80> ix{}; std::array<short,2048> state{}; };
 std::vector<Entry> cache;
 CUcontext ctx{};CUmodule module{};CUfunction fn{};CUstream stream{};
 CUdeviceptr bias{},weights{},input{},output{};
#if defined(CYF_GPU_ACC_VRAM_CACHE)
 CUdeviceptr vkeys{},vstates{},vlocks{},vvalid{},vstats{};
 int vslots=4096;
#endif
 int* host_ix=nullptr;short* host_out=nullptr;
 bool active=false;int count=0,mode=0;uint64_t generation=0;
 uint64_t probes=0,hits=0,requests=0,batches=0,ready=0,not_ready=0,verified=0,drain_us=0;
 uint64_t inserts=0,evictions=0,unused_evictions=0,first_hits=0,tick=0;
 static void ck(CUresult r){if(r!=CUDA_SUCCESS){std::fprintf(stderr,"GPU accumulator CUDA error %d\n",int(r));std::abort();}}
 static unsigned slot(const int* ix){uint64_t h=1469598103934665603ull;for(int i=0;i<80;++i){h^=unsigned(ix[i]);h*=1099511628211ull;}h^=h>>33;h*=0xff51afd7ed558ccdull;h^=h>>33;h*=0xc4ceb9fe1a85ec53ull;h^=h>>33;return unsigned(h)&(N-1);}
 Service(){const char* m=std::getenv("CYF_GPU_ACC_MODE");mode=m?std::atoi(m):0;if(mode<0||mode>2)std::abort();}
 void init(const short* b,const short* w,size_t bytes,uint64_t gen){
  if(!mode)return;
  if(cache.empty())cache.resize(N);
  if(generation==gen)return;
  auto started=std::chrono::steady_clock::now();
  if(active)collect(true);
  for(auto& e:cache)e.valid=false;count=0;generation=gen;
  if(mode==2)return;
  if(!ctx){
   const char* ptx=std::getenv("CYF_GPU_ACC_PTX");if(!ptx)std::abort();
   ck(cuInit(0));CUdevice dev;ck(cuDeviceGet(&dev,0));
#if defined(CYF_GPU_SHARED_PRIMARY)
   unsigned flags=0;int primary_active=0;ck(cuDevicePrimaryCtxGetState(dev,&flags,&primary_active));
   if(!primary_active)ck(cuDevicePrimaryCtxSetFlags(dev,CU_CTX_SCHED_BLOCKING_SYNC));
   ck(cuDevicePrimaryCtxRetain(&ctx,dev));ck(cuCtxSetCurrent(ctx));
#else
   ck(cuCtxCreate(&ctx,CU_CTX_SCHED_BLOCKING_SYNC,dev));
#endif
   ck(cuModuleLoad(&module,ptx));ck(cuModuleGetFunction(&fn,module,
#if defined(CYF_GPU_ACC_VRAM_CACHE)
   "acc_cached"
#else
   "acc"
#endif
   ));
   ck(cuStreamCreate(&stream,CU_STREAM_NON_BLOCKING));
   ck(cuMemAlloc(&bias,2048));ck(cuMemAlloc(&weights,bytes));
   ck(cuMemAlloc(&input,B*80*4));ck(cuMemAlloc(&output,B*2048*2));
   ck(cuMemAllocHost(reinterpret_cast<void**>(&host_ix),B*80*4));
   ck(cuMemAllocHost(reinterpret_cast<void**>(&host_out),B*2048*2));
  }
  ck(cuCtxSetCurrent(ctx));
#if defined(CYF_GPU_ACC_VRAM_CACHE)
  if(!vkeys){
   ck(cuMemAlloc(&vkeys,size_t(vslots)*320));ck(cuMemAlloc(&vstates,size_t(vslots)*4096));
   ck(cuMemAlloc(&vlocks,size_t(vslots)*4));ck(cuMemAlloc(&vvalid,size_t(vslots)*4));ck(cuMemAlloc(&vstats,12));
  }
  ck(cuMemsetD8(vlocks,0,size_t(vslots)*4));ck(cuMemsetD8(vvalid,0,size_t(vslots)*4));ck(cuMemsetD8(vstats,0,12));
#endif
  ck(cuMemcpyHtoD(bias,b,2048));ck(cuMemcpyHtoD(weights,w,bytes));
  std::fill(host_ix,host_ix+80,-1);count=1;submit();collect(true);
  batches=ready=not_ready=drain_us=0;
  std::printf("info string gpuacc_init_us=%lld\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());std::fflush(stdout);
 }
 void put(const int* ix,const short* state){
  unsigned i=slot(ix);
#if defined(CYF_GPU_ACC_2WAY)
  i&=~1u;auto& a=cache[i];auto& b=cache[i+1];
  if(b.valid&&!std::memcmp(b.ix.data(),ix,320))++i;
  else if(!(a.valid&&!std::memcmp(a.ix.data(),ix,320)) && a.valid && (!b.valid || b.age<a.age))++i;
#endif
  auto& e=cache[i];bool same=e.valid&&!std::memcmp(e.ix.data(),ix,320);
  if(!same){++inserts;if(e.valid){++evictions;unused_evictions+=!e.used;}e.used=false;}
  std::memcpy(e.ix.data(),ix,320);std::memcpy(e.state.data(),state,4096);e.valid=true;e.age=++tick;
 }
 void collect(bool wait){
  if(!active)return;ck(cuCtxSetCurrent(ctx));auto r=cuStreamQuery(stream);
  if(r==CUDA_ERROR_NOT_READY){++not_ready;if(!wait)return;}else{ck(r);++ready;}
  auto t=std::chrono::steady_clock::now();
  if(wait)ck(cuStreamSynchronize(stream));
  drain_us+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t).count();
  for(int i=0;i<count;++i)put(host_ix+i*80,host_out+i*2048);
  active=false;count=0;
 }
 void submit(){
  if(!count||active)return;
#if defined(CYF_GPU_ADMISSION)
  if(!CyfGpuAdmission::gate().take(1)){count=0;return;}
#endif
  ck(cuCtxSetCurrent(ctx));
  ck(cuMemcpyHtoDAsync(input,host_ix,count*320,stream));
  void* args[]={&input,&bias,&weights,&output,&count
#if defined(CYF_GPU_ACC_VRAM_CACHE)
   ,&vkeys,&vstates,&vlocks,&vvalid,&vslots,&vstats
#endif
  };
  ck(cuLaunchKernel(fn,count,1,1,256,1,1,0,stream,args,nullptr));
  ck(cuMemcpyDtoHAsync(host_out,output,count*4096,stream));
#if defined(CYF_GPU_ADMISSION)
  ck(cuLaunchHostFunc(stream,[](void* p){static_cast<CyfGpuAdmission::Gate*>(p)->release();},&CyfGpuAdmission::gate()));
#endif
  active=true;++batches;
 }
 bool get(const int* ix,short* dest){
  ++probes;if(mode==1)collect(false);
  unsigned index=slot(ix),ways=1;
#if defined(CYF_GPU_ACC_2WAY)
  index&=~1u;ways=2;
#endif
  for(unsigned j=0;j<ways;++j){auto& e=cache[index+j];if(e.valid&&!std::memcmp(e.ix.data(),ix,320)){std::memcpy(dest,e.state.data(),4096);++hits;first_hits+=!e.used;e.used=true;e.age=++tick;return true;}}
  if(mode==1&&!active&&requests<2048){std::memcpy(host_ix+count*80,ix,320);++count;++requests;if(count==8)submit();}
  return false;
 }
 void finish(){
#if defined(CYF_GPU_ADMISSION)
  auto& g=CyfGpuAdmission::gate();
  std::printf("info string gpu_admission limit=%u active=%u peak=%u cnn_accept=%llu cnn_reject=%llu acc_accept=%llu acc_reject=%llu\n",g.limit,g.active.load(),g.peak.load(),g.accepted[0].load(),g.rejected[0].load(),g.accepted[1].load(),g.rejected[1].load());
#endif
#if defined(CYF_GPU_ACC_NO_FINISH_WAIT)
  const int carried=active?count:0, dropped=active?0:count;
  if(!active)count=0;
  std::printf("info string gpuacc_async carried=%d dropped=%d\n",carried,dropped);
#else
#if defined(CYF_GPU_ACC_VRAM_CACHE)
  if(mode==1 && ctx){
   if(!active)submit();collect(true);
   unsigned counts[3]{};ck(cuMemcpyDtoH(counts,vstats,sizeof(counts)));
   std::printf("info string gpuacc_vram hits=%u misses=%u contention=%u bytes=%llu\n",counts[0],counts[1],counts[2],(unsigned long long)(size_t(vslots)*4424+12));
   ck(cuMemsetD8(vstats,0,12));
  }
#endif
  if(mode==1){if(!active)submit();collect(true);}
#endif
  std::printf("info string gpuacc mode=%d probes=%llu hits=%llu requests=%llu batches=%llu ready=%llu pending=%llu verified=%llu drain_us=%llu\n",mode,(unsigned long long)probes,(unsigned long long)hits,(unsigned long long)requests,(unsigned long long)batches,(unsigned long long)ready,(unsigned long long)not_ready,(unsigned long long)verified,(unsigned long long)drain_us);std::fflush(stdout);
  std::printf("info string gpuacc_cache inserts=%llu evictions=%llu unused_evictions=%llu first_hits=%llu bytes=%llu\n",(unsigned long long)inserts,(unsigned long long)evictions,(unsigned long long)unused_evictions,(unsigned long long)first_hits,(unsigned long long)(cache.size()*sizeof(Entry)));std::fflush(stdout);
  inserts=evictions=unused_evictions=first_hits=0;
  probes=hits=requests=batches=ready=not_ready=verified=drain_us=0;
 }
 ~Service(){if(ctx && cuCtxSetCurrent(ctx)==CUDA_SUCCESS){if(active)cuStreamSynchronize(stream);cuMemFreeHost(host_ix);cuMemFreeHost(host_out);
#if defined(CYF_GPU_SHARED_PRIMARY)
 cuStreamDestroy(stream);cuModuleUnload(module);
 cuMemFree(bias);cuMemFree(weights);cuMemFree(input);cuMemFree(output);
#if defined(CYF_GPU_ACC_VRAM_CACHE)
 cuMemFree(vkeys);cuMemFree(vstates);cuMemFree(vlocks);cuMemFree(vvalid);cuMemFree(vstats);
#endif
 CUdevice dev;if(cuDeviceGet(&dev,0)==CUDA_SUCCESS)cuDevicePrimaryCtxRelease(dev);
#else
 cuCtxDestroy(ctx);
#endif
 }}
};
inline Service& service(){static Service s;return s;}
}
