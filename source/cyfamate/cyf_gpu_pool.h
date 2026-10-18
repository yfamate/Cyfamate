#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>

namespace CyfGpuPool {

struct Handle { int32_t slot = -1; uint64_t version = 0, model = 0; bool valid() const { return slot >= 0 && version; } };

struct PoolReq {
 int32_t parent = -1, out_slot = -1;
 uint64_t parent_version = 0, out_version = 0;
 int32_t bucket = 0, stm = 0, reset = 0, full = -1;
 int32_t nrem[2]{}, nadd[2]{};         
 int32_t nfull[2]{};                  
 int32_t rem[2][4]{}, add[2][4]{};
};
static_assert(sizeof(PoolReq) == 128, "PoolReq layout must match the CUDA struct");

constexpr unsigned kPoolAbi = 3;

inline uint64_t echo_term(uint64_t w, int i) { return (w ^ (0x9e3779b97f4a7c15ull * uint64_t(i + 1))) * 0xff51afd7ed558ccdull; }
inline uint64_t echo_final(uint64_t h) { h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull; h ^= h >> 33; return h; }
inline uint64_t request_checksum(const PoolReq& q, const int32_t* full, int full_rows) {
 uint64_t words[sizeof(PoolReq) / 8]; std::memcpy(words, &q, sizeof(PoolReq));
 uint64_t h = 0;
 for (int i = 0; i < int(sizeof(PoolReq) / 8); ++i) h += echo_term(words[i], i);
 if (q.full >= 0 && q.full < full_rows) {
  const int32_t* r = full + size_t(q.full) * 80;
  for (int k = 0; k < 40; ++k) h += echo_term(uint64_t(uint32_t(r[2 * k])) | (uint64_t(uint32_t(r[2 * k + 1])) << 32), 16 + k);
 }
 return echo_final(h);
}

struct SlotTable {
 enum : uint8_t { Free = 0, Writing = 1, Ready = 2 };
 struct Slot { uint8_t state = Free; bool doomed = false; uint32_t pins = 0; uint64_t version = 0, model = 0, writer = 0, last = 0; };
 std::vector<Slot> slots;
 uint64_t next_version = 1, tick = 0;
 uint32_t hand = 0;
 uint64_t allocations = 0, evictions = 0, refusals = 0, stale = 0, doomed_frees = 0;

 void resize(int n) { slots.assign(size_t(n), Slot{}); hand = 0; }
 int size() const { return int(slots.size()); }
 bool holds(const Handle& h) const {
  if (h.slot < 0 || h.slot >= size() || !h.version) return false;
  const Slot& s = slots[size_t(h.slot)];
  return !s.doomed && s.state != Free && s.version == h.version && s.model == h.model;
 }
 bool pin(const Handle& h) {
  if (!holds(h)) { ++stale; return false; }
  Slot& s = slots[size_t(h.slot)];
  ++s.pins; s.last = ++tick; return true;
 }
 void unpin(int slot) {
  Slot& s = slots[size_t(slot)];
  if (!s.pins) std::abort();
  --s.pins; release_if_doomed(slot);
 }
 Handle allocate(uint64_t model, uint64_t writer) {
  const int n = size();
  for (int step = 0; step < 2 * n; ++step) {
   const int i = int(hand); hand = (hand + 1) % uint32_t(n);
   Slot& s = slots[size_t(i)];
   const bool free = s.state == Free;
   const bool evictable = s.state == Ready && !s.pins && !s.doomed && (step >= n || s.last + uint64_t(n) < tick);
   if (!free && !evictable) continue;
   if (!free) ++evictions;
   s.state = Writing; s.doomed = false; s.version = next_version++; s.model = model; s.writer = writer; s.last = ++tick;
   ++allocations;
   return Handle{i, s.version, model};
  }
  ++refusals;
  return Handle{};
 }
 void complete(int slot, uint64_t writer, bool ok) {
  Slot& s = slots[size_t(slot)];
  if (s.state != Writing || s.writer != writer) std::abort();
  s.state = Ready;
  if (!ok) s.doomed = true;
  release_if_doomed(slot);
 }
 void invalidate_all() {
  for (size_t i = 0; i < slots.size(); ++i) {
   Slot& s = slots[i];
   if (s.pins || s.state == Writing) s.doomed = true;
   else { s = Slot{}; }
  }
 }
 void release_if_doomed(int slot) {
  Slot& s = slots[size_t(slot)];
  if (s.doomed && !s.pins && s.state != Writing) { s = Slot{}; ++doomed_frees; }
 }
 bool consistent() const {
  for (const auto& s : slots) {
   if (s.state == Free && (s.pins || s.version)) return false;
   if (s.state != Free && !s.version) return false;
  }
  return true;
 }
};

struct Launch {
 enum : uint8_t { Idle = 0, Open = 1, InFlight = 2 };
 uint8_t state = Idle;
 uint64_t id = 0, model = 0, epoch = 0;
 uint32_t seq = 0;
 int n = 0, full_rows = 0;
 std::vector<int32_t> pinned;        
 std::vector<int32_t> outputs;       
 std::vector<uint64_t> tags;         
 PoolReq* req = nullptr; int32_t* full = nullptr; int32_t* raw = nullptr; int32_t* ok = nullptr;
 uint64_t* echo = nullptr;           
 volatile uint32_t* done = nullptr;
 std::vector<uint64_t> expect;       
 std::vector<uint8_t> good;
 bool checked = false;
 uint64_t submit_tick = 0;           
 bool token(int j) const { return (uint32_t(ok[j]) & ~1u) == seq * 2u; }
 bool valid(int j) const { return (ok[j] & 1) != 0; }
 bool echo_ok(int j) const { return !echo || echo[j] == expect[size_t(j)]; }
};

struct Pool {
 SlotTable table;
 std::vector<Launch> ring;
 int capacity = 0, full_capacity = 0;
 uint64_t model = 0, epoch = 0, next_launch = 1;
 uint32_t next_seq = 1;
 int open = -1;                      
 size_t rr = 0;                      
 size_t oldest = 0;                  
 std::vector<size_t> in_flight;      
 uint64_t requests = 0, refused_no_launch = 0, refused_parent = 0, refused_slot = 0, refused_alias = 0,
          launches = 0, harvested = 0, harvested_ok = 0, harvested_bad = 0, token_lag = 0,
          echo_mismatch = 0, stalled_launches = 0;
 bool fault = false;                 
 bool echo_check = true;             

 void configure(int slots, int launches_in_ring, int per_launch, int full_per_launch) {
  table.resize(slots); ring.assign(size_t(launches_in_ring), Launch{});
  capacity = per_launch; full_capacity = full_per_launch;
  for (auto& l : ring) { l.pinned.reserve(size_t(per_launch) * 2); l.outputs.reserve(size_t(per_launch)); l.tags.reserve(size_t(per_launch)); l.good.reserve(size_t(per_launch)); l.expect.reserve(size_t(per_launch)); }
 }
 bool has_room() const { return open >= 0 || in_flight.size() < ring.size(); }
 Launch* open_launch() {
  if (open >= 0) return &ring[size_t(open)];
  for (size_t k = 0; k < ring.size(); ++k) {
   const size_t i = (rr + k) % ring.size();
   Launch& l = ring[i];
   if (l.state != Launch::Idle) continue;
   rr = i + 1;
   l.state = Launch::Open; l.id = next_launch++; l.model = model; l.epoch = epoch; l.n = 0; l.full_rows = 0;
   l.pinned.clear(); l.outputs.clear(); l.tags.clear(); l.good.clear(); l.checked = false;
   open = int(i); return &l;
  }
  ++refused_no_launch; return nullptr;
 }
 int add(const Handle& parent, bool needs_parent, bool keep_child, bool needs_full, uint64_t tag, Handle* child_out, int share_full = -1) {
  Launch* l = open_launch();
  if (!l) return -1;
  if (share_full >= l->full_rows) std::abort();
  const bool new_row = needs_full && share_full < 0;
  if (l->n >= capacity || (new_row && l->full_rows >= full_capacity)) { ++refused_no_launch; return -1; }
  if (needs_parent) {
   for (int j = 0; j < l->n; ++j) if (l->outputs[size_t(j)] == parent.slot) { ++refused_alias; return -1; }
   if (!table.pin(parent)) { ++refused_parent; return -1; }
   l->pinned.push_back(parent.slot);
  }
  Handle child{};
  if (keep_child) {
   child = table.allocate(model, l->id);
   if (!child.valid()) {
    if (needs_parent) { l->pinned.pop_back(); table.unpin(parent.slot); }
    ++refused_slot; return -1;
   }
  }
  const int index = l->n++;
  PoolReq& q = l->req[index];
  q = PoolReq{};
  q.parent = needs_parent ? parent.slot : -1;
  q.parent_version = needs_parent ? parent.version : 0;
  q.out_slot = child.slot; q.out_version = child.version;
  if (new_row) {
   q.full = l->full_rows++;
   std::fill(l->full + size_t(q.full) * 80, l->full + size_t(q.full) * 80 + 80, -1);
  } else if (needs_full) q.full = share_full;
  l->outputs.push_back(child.slot);
  l->tags.push_back(tag);
  if (child_out) *child_out = child;
  ++requests;
  return index;
 }
 Launch* close() {
  if (open < 0) return nullptr;
  Launch& l = ring[size_t(open)];
  open = -1;
  if (!l.n) { l.state = Launch::Idle; return nullptr; }
  l.state = Launch::InFlight; l.seq = next_seq++; if (next_seq >= 0x80000000u) next_seq = 1;   
  for (int j = 0; j < l.n; ++j) l.ok[j] = 0;   
  if (l.echo) {
   l.expect.resize(size_t(l.n));
   for (int j = 0; j < l.n; ++j) { l.expect[size_t(j)] = request_checksum(l.req[j], l.full, l.full_rows); l.echo[j] = ~l.expect[size_t(j)]; }
  }
  l.good.clear(); l.checked = false;
  __atomic_store_n(&l.submit_tick, uint64_t(0), __ATOMIC_RELAXED);
  *l.done = 0;
  in_flight.push_back(size_t(&l - ring.data()));
  ++launches;
  return &l;
 }
 Launch* completed() {
  if (in_flight.empty()) return nullptr;
  Launch& l = ring[in_flight.front()];
  if (__atomic_load_n(l.done, __ATOMIC_ACQUIRE) != l.seq) return nullptr;
  for (int j = 0; j < l.n; ++j)
   if ((uint32_t(__atomic_load_n(&l.ok[j], __ATOMIC_ACQUIRE)) & ~1u) != l.seq * 2u) { ++token_lag; return nullptr; }
  if (!l.checked) check(l);
  return &l;
 }
 bool stalled(uint64_t now, uint64_t limit) const {
  if (in_flight.empty()) return false;
  const Launch& l = ring[in_flight.front()];
  const uint64_t sub = __atomic_load_n(&l.submit_tick, __ATOMIC_ACQUIRE);
  return sub && now >= sub && now - sub >= limit;
 }
 int check(Launch& l) {
  int bad_echo = 0;
  l.good.assign(size_t(l.n), 0);
  for (int j = 0; j < l.n; ++j) {
   const bool echo = !echo_check || l.echo_ok(j);
   bad_echo += !echo;
   l.good[size_t(j)] = uint8_t(echo && l.valid(j));
  }
  l.checked = true;
  if (bad_echo) { echo_mismatch += uint64_t(bad_echo); fault = true; std::fill(l.good.begin(), l.good.end(), uint8_t(0)); }
  return bad_echo;
 }
 void fail_front() {
  if (in_flight.empty()) std::abort();
  Launch& l = ring[in_flight.front()];
  l.good.assign(size_t(l.n), 0); l.checked = true;
  ++stalled_launches; fault = true;
  release(l);
 }
 void release(Launch& l) {
  if (in_flight.empty() || &ring[in_flight.front()] != &l || l.state != Launch::InFlight || !l.checked) std::abort();
  in_flight.erase(in_flight.begin());
  for (int j = 0; j < l.n; ++j) {
   const bool good = l.good[size_t(j)] != 0;
   good ? ++harvested_ok : ++harvested_bad;
   if (l.outputs[size_t(j)] >= 0) table.complete(l.outputs[size_t(j)], l.id, good);
  }
  for (int32_t s : l.pinned) table.unpin(s);
  harvested += uint64_t(l.n);
  l.state = Launch::Idle; l.n = 0; l.full_rows = 0; l.pinned.clear(); l.outputs.clear(); l.tags.clear();
 }
 void change_model(uint64_t new_model) {
  if (!in_flight.empty() || open >= 0) std::abort();
  table.invalidate_all(); model = new_model;
 }
 void abandon_open() {
  if (open < 0) return;
  Launch& l = ring[size_t(open)];
  open = -1;
  for (int j = 0; j < l.n; ++j) if (l.outputs[size_t(j)] >= 0) table.complete(l.outputs[size_t(j)], l.id, false);
  for (int32_t s : l.pinned) table.unpin(s);
  l.state = Launch::Idle; l.n = 0; l.full_rows = 0; l.pinned.clear(); l.outputs.clear(); l.tags.clear();
 }
};

inline int nat_index(int m) { return (m & ~0x38) | (((((m >> 3) & 3) << 1) | ((m >> 5) & 1)) << 3); }
inline void tail_to_device_order(const int32_t* natural, int32_t* device, int buckets) {
 for (int b = 0; b < buckets; ++b) {
  const int32_t* s = natural + size_t(b) * 9225; int32_t* d = device + size_t(b) * 9225;
  std::memcpy(d, s, sizeof(int32_t) * 9225);
  for (int k = 0; k < 8; ++k)
   for (int p = 0; p < 2; ++p)
    for (int m = 0; m < 512; ++m) d[8 + k * 1024 + p * 512 + m] = s[8 + k * 1024 + p * 512 + nat_index(m)];
 }
}

} 

#if defined(CYF_GPU_POOL)
#include <cuda.h>
#include <x86intrin.h>
namespace CyfGpuPool {
struct Device {
 CUcontext ctx{}; CUmodule module{}; CUfunction fn{}; CUstream stream{}; CUdevice dev{};
 CUdeviceptr states{}, versions{}, ftb{}, ftw{}, tail{}, counters{};
 int slots = 0, features = 0, buckets = 0, l1_shift = 0, per_launch = 0, full_per_launch = 0;
 size_t ft_weight_bytes = 0;
 struct Block { char* host = nullptr; CUdeviceptr dev = 0; size_t req = 0, full = 0, raw = 0, ok = 0, echo = 0, done = 0; };
 std::vector<Block> blocks;
 Pool pool;
 uint64_t launch_ns = 0, drains = 0;
 static void ck(CUresult r, const char* what) {
  if (r != CUDA_SUCCESS) { const char* s = nullptr; cuGetErrorString(r, &s); std::fprintf(stderr, "CyfGpuPool %s: %s\n", what, s ? s : "?"); std::abort(); }
 }
 bool initialised() const { return ctx != nullptr; }
 void init(const char* ptx, int slot_count, int ring, int requests_per_launch, int full_rows_per_launch) {
  if (ctx) return;
  slots = slot_count; per_launch = requests_per_launch; full_per_launch = full_rows_per_launch;
  ck(cuInit(0), "init"); ck(cuDeviceGet(&dev, 0), "device");
  unsigned flags = 0; int active = 0; ck(cuDevicePrimaryCtxGetState(dev, &flags, &active), "primary state");
  if (!active) ck(cuDevicePrimaryCtxSetFlags(dev, CU_CTX_SCHED_BLOCKING_SYNC), "primary flags");
  ck(cuDevicePrimaryCtxRetain(&ctx, dev), "retain"); ck(cuCtxSetCurrent(ctx), "current");
  ck(cuModuleLoad(&module, ptx), "module"); ck(cuModuleGetFunction(&fn, module, "pool_eval"), "function");
  {  
   CUdeviceptr abi_ptr{}; size_t abi_bytes = 0; unsigned abi = 0;
   ck(cuModuleGetGlobal(&abi_ptr, &abi_bytes, module, "cyf_pool_abi"), "abi symbol (PTX older than kPoolAbi)");
   ck(cuMemcpyDtoH(&abi, abi_ptr, sizeof abi), "abi read");
   if (abi_bytes != sizeof abi || abi != kPoolAbi) { std::fprintf(stderr, "CyfGpuPool PTX abi %u, host expects %u\n", abi, kPoolAbi); std::abort(); }
  }
  ck(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING), "stream");
  ck(cuMemAlloc(&states, size_t(slots) * 4096), "states"); ck(cuMemAlloc(&versions, size_t(slots) * 8), "versions");
  ck(cuMemsetD8Async(versions, 0, size_t(slots) * 8, stream), "versions clear");
  ck(cuMemAlloc(&counters, size_t(ring) * 64), "counters"); ck(cuMemsetD8Async(counters, 0, size_t(ring) * 64, stream), "counters clear");
  ck(cuStreamSynchronize(stream), "init sync");
  pool.configure(slots, ring, per_launch, full_per_launch);
  blocks.assign(size_t(ring), Block{});
  for (int i = 0; i < ring; ++i) {
   Block& b = blocks[size_t(i)];
   b.req = 0; b.full = b.req + sizeof(PoolReq) * size_t(per_launch);
   b.raw = b.full + sizeof(int32_t) * 80 * size_t(full_per_launch); b.ok = b.raw + 4 * size_t(per_launch);
   b.echo = (b.ok + 4 * size_t(per_launch) + 7) & ~size_t(7);
   b.done = (b.echo + 8 * size_t(per_launch) + 63) & ~size_t(63);
   const size_t bytes = b.done + 64;
   void* p = nullptr;
   ck(cuMemHostAlloc(&p, bytes, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_PORTABLE), "host block");
   std::memset(p, 0, bytes);
   b.host = static_cast<char*>(p);
   ck(cuMemHostGetDevicePointer(&b.dev, p, 0), "host block device pointer");
   Launch& l = pool.ring[size_t(i)];
   l.req = reinterpret_cast<PoolReq*>(b.host + b.req); l.full = reinterpret_cast<int32_t*>(b.host + b.full);
   l.raw = reinterpret_cast<int32_t*>(b.host + b.raw); l.ok = reinterpret_cast<int32_t*>(b.host + b.ok);
   l.echo = reinterpret_cast<uint64_t*>(b.host + b.echo);
   l.done = reinterpret_cast<volatile uint32_t*>(b.host + b.done);
  }
 }
 void drain() { if (ctx) { ck(cuCtxSetCurrent(ctx), "current"); ck(cuStreamSynchronize(stream), "drain"); ++drains; } }
 void load_model(const int16_t* biases, const int16_t* weights, int feature_count,
                 const int32_t* natural_tail, int bucket_count, int shift, uint64_t identity) {
  drain();
  pool.change_model(identity);
  ck(cuMemsetD8Async(versions, 0, size_t(slots) * 8, stream), "versions clear");
  ck(cuMemsetD8Async(counters, 0, blocks.size() * 64, stream), "counters clear");
  const size_t wbytes = size_t(feature_count) * 1024 * 2;
  if (wbytes != ft_weight_bytes) {
   if (ftw) cuMemFree(ftw);
   if (!ftb) ck(cuMemAlloc(&ftb, 2048), "ft bias");
   ck(cuMemAlloc(&ftw, wbytes), "ft weights"); ft_weight_bytes = wbytes;
  }
  ck(cuMemcpyHtoDAsync(ftb, biases, 2048, stream), "ft bias upload"); ck(cuMemcpyHtoDAsync(ftw, weights, wbytes, stream), "ft weights upload");
  std::vector<int32_t> device_tail(size_t(bucket_count) * 9225);
  tail_to_device_order(natural_tail, device_tail.data(), bucket_count);
  if (bucket_count != buckets) { if (tail) cuMemFree(tail); ck(cuMemAlloc(&tail, device_tail.size() * 4), "tail"); }
  ck(cuMemcpyHtoDAsync(tail, device_tail.data(), device_tail.size() * 4, stream), "tail upload");
  ck(cuStreamSynchronize(stream), "model upload sync");   
  features = feature_count; buckets = bucket_count; l1_shift = shift;
 }
 void submit(Launch& l) {
  const auto t0 = std::chrono::steady_clock::now();
  const size_t index = size_t(&l - pool.ring.data());
  Block& b = blocks[index];
  CUdeviceptr req = b.dev + b.req, full = b.dev + b.full, raw = b.dev + b.raw, ok = b.dev + b.ok, done = b.dev + b.done;
  CUdeviceptr echo = b.dev + b.echo;
  CUdeviceptr counter = counters + index * 64;
  int n = l.n, full_rows = l.full_rows; unsigned seq = l.seq;
  void* args[] = {&states, &versions, &slots, &req, &full, &full_rows, &ftb, &ftw, &features, &tail, &buckets, &l1_shift,
                  &raw, &ok, &n, &counter, &done, &seq, &echo};
  ck(cuCtxSetCurrent(ctx), "current");
  __atomic_store_n(&l.submit_tick, uint64_t(__rdtsc()) | 1u, __ATOMIC_RELEASE);   
  ck(cuLaunchKernel(fn, unsigned(n), 1, 1, 256, 1, 1, 0, stream, args, nullptr), "launch");
  const CUresult flushed = cuStreamQuery(stream);
  if (flushed != CUDA_SUCCESS && flushed != CUDA_ERROR_NOT_READY) ck(flushed, "flush");
  launch_ns += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
 }
 void read_state(int slot, int16_t* dest) {
  drain(); ck(cuMemcpyDtoH(dest, states + size_t(slot) * 4096, 4096), "state read");
 }
 uint64_t device_version(int slot) {
  drain(); uint64_t v = 0; ck(cuMemcpyDtoH(&v, versions + size_t(slot) * 8, 8), "version read"); return v;
 }
 ~Device() {
  if (!ctx || cuCtxSetCurrent(ctx) != CUDA_SUCCESS) return;
  cuStreamSynchronize(stream);
  for (auto& b : blocks) if (b.host) cuMemFreeHost(b.host);
  cuStreamDestroy(stream); cuModuleUnload(module);
  for (CUdeviceptr p : {states, versions, ftb, ftw, tail, counters}) if (p) cuMemFree(p);
  cuDevicePrimaryCtxRelease(dev);
 }
};
} 
#endif
