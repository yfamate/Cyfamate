#pragma once
#include <cinttypes>
#include <thread>
#include <time.h>
#include <x86intrin.h>

struct GpuPoolSearch {
 struct Config {
  int depth = 99, at = 2, maxreq = 96, skip_acc = 0, verify_every = 0, keep = 1, slots = 8192, ring = 32,
      per_launch = 512, full_rows = 128, batch_us = 0, keep_parent = 1, picker = 0, scan = 64,
      selective_poll = 0, poll_us = 0, stall_ms = 50, fault_every = 0, echo_check = 1, trace_cap = 4000000, demand_close = 0;
  const char* ptx = nullptr;
  const char* trace = nullptr;
  void load() {
   auto get = [](const char* k, int d) { const char* v = std::getenv(k); return v ? std::atoi(v) : d; };
   ptx = std::getenv("CYF_GPU_POOL_PTX");
   depth = get("CYF_GPU_POOL_DEPTH", 99); at = std::clamp(get("CYF_GPU_POOL_AT", 2), 1, 8);
   maxreq = std::clamp(get("CYF_GPU_POOL_MAXREQ", 96), 1, 600);
   skip_acc = get("CYF_GPU_POOL_SKIPACC", 0); verify_every = std::max(0, get("CYF_GPU_POOL_VERIFY", 0));
   keep = get("CYF_GPU_POOL_KEEP", 1); keep_parent = get("CYF_GPU_POOL_KEEP_PARENT", 1);
   slots = get("CYF_GPU_POOL_SLOTS", 8192); ring = std::clamp(get("CYF_GPU_POOL_RING", 32), 2, 256);
   per_launch = std::max(maxreq + 2, get("CYF_GPU_POOL_PER_LAUNCH", 512)); full_rows = std::max(2, get("CYF_GPU_POOL_FULL_ROWS", 128));
   batch_us = std::max(0, get("CYF_GPU_POOL_BATCH_US", 0));
   picker = get("CYF_GPU_POOL_PICKER", 0);
   scan = std::clamp(get("CYF_GPU_POOL_SCAN", 64), 1, 600);
   selective_poll = get("CYF_GPU_POOL_SELECTIVE_POLL", 0);
   poll_us = std::clamp(get("CYF_GPU_POOL_POLL_US", 0), 0, 1000);
   stall_ms = std::clamp(get("CYF_GPU_POOL_STALL_MS", 50), 0, 60000);
   demand_close = get("CYF_GPU_POOL_DEMAND_CLOSE", 0);
   fault_every = std::max(0, get("CYF_GPU_POOL_FAULT_EVERY", 0)); echo_check = get("CYF_GPU_POOL_ECHO_CHECK", 1);
   trace = std::getenv("CYF_GPU_POOL_TRACE"); trace_cap = std::max(1000, get("CYF_GPU_POOL_TRACE_CAP", 4000000));
  }
 } cfg;
 bool faulted = false;
 const char* fault_reason = "";
 uint64_t stall_drains = 0, stall_polls = 0, stall_ticks = 0, faults_injected = 0, finish_submitted = 0;
 struct TraceEv { uint64_t t, key; uint32_t a, b; uint8_t type, pad[7]; };
 static_assert(sizeof(TraceEv) == 32, "trace record layout");
 enum : uint8_t { TrGo = 1, TrTrig = 2, TrReq = 3, TrClose = 4, TrHarv = 5, TrProbe = 6, TrAbandon = 7, TrFinish = 8, TrFault = 9 };
 std::vector<TraceEv> trace;
 uint64_t trace_dropped = 0;
 void tr(uint8_t type, uint64_t key, uint32_t a, uint32_t b) {
  if (!cfg.trace) return;
  if (trace.size() < size_t(cfg.trace_cap)) trace.push_back(TraceEv{__rdtsc(), key, a, b, type, {}}); else ++trace_dropped;
 }
 void trace_flush() {
  if (!cfg.trace || trace.empty()) return;
  if (FILE* f = std::fopen(cfg.trace, "ab")) {
   const uint64_t head[4] = {0x31525443595046ull , uint64_t(trace.size()), uint64_t(tsc_mhz * 1000.0), trace_dropped};
   std::fwrite(head, sizeof head, 1, f); std::fwrite(trace.data(), sizeof(TraceEv), trace.size(), f); std::fclose(f);
  }
  trace.clear(); trace_dropped = 0;
 }
 enum : uint8_t { Empty = 0, Pending = 1, Wanted = 2, Ready = 3, Used = 4 };
 struct Entry { uint64_t key = 0; int32_t raw = 0; uint16_t model = 0; uint8_t bucket = 0, state = Empty; };
 static constexpr size_t kTable = size_t(1) << 16, kResident = size_t(1) << 14;
 std::vector<Entry> table;
 struct Resident { uint64_t key = 0; CyfGpuPool::Handle h{}; };
 std::vector<Resident> resident;
 std::vector<std::array<Move, 8>> seen;
 CyfGpuPool::Device dev;
 uint64_t model = 0, generation = 0, epoch = 0;
 bool enabled = false;
 double tsc_mhz = 0;
 uint64_t open_since = 0, batch_cycles = 0;
 std::vector<uint32_t> queue;
 std::atomic<uint64_t> q_head{0}, q_tail{0}, worker_launches{0}, worker_launch_ns{0}, worker_cpu_ns{0};
 std::atomic<bool> worker_run{false};
 std::atomic<uint64_t> worker_wake{0};
 std::thread worker;
 struct Stats {
  uint64_t triggers = 0, skipped_busy = 0, no_children = 0, parent_resident = 0, parent_row = 0, parent_kept = 0, requests = 0,
           own_king_skipped = 0, capped = 0, already_valued = 0, refused = 0, alias_splits = 0, closes = 0,
           harvested_ok = 0, harvested_bad = 0, probes = 0, hits = 0, reuse_hits = 0, pending_at_use = 0, late = 0, absent = 0,
           bucket_mismatch = 0, displaced_pending = 0, displaced_unused = 0, verified = 0, skipped_acc = 0, abandoned = 0,
           carried = 0, ready_unused_live = 0, demand_closes = 0,
           cyc_trigger = 0, cyc_gen = 0, cyc_probe = 0, cyc_harvest = 0, cyc_hit_acc = 0, cyc_verify = 0;
  uint64_t launches_at_go = 0, launch_ns_at_go = 0, worker_cpu_at_go = 0, search_cpu_at_go = 0;
 } st;
 uint64_t candidate_scans = 0, delta_built = 0, duplicate_delta_saved = 0, legal_lists = 0;
 uint64_t next_poll = 0, poll_calls = 0, poll_empty = 0, poll_skipped = 0;

 static uint64_t thread_cpu_ns() { timespec ts{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec); }
 static int bp_index(int ksq, int bp) {
  using F = Eval::NNUE::Features::HalfKA2<Eval::NNUE::Features::Side::kFriend>;
  Square k = Square(ksq);
  const bool mirrored = F::MirrorKing(k);   
  return int(F::MakeIndex(k, BonaPiece(bp), mirrored));
 }
 static bool pre_move_delta(const Position& pos, Move m, CyfGpuPool::PoolReq& q) {
  const Color us = pos.side_to_move();
  const int ksq[2] = { int(pos.square<KING>(BLACK)), int(Inv(pos.square<KING>(WHITE))) };
  const Square to = m.to_sq();
  const Piece after = pos.moved_piece_after(m);
  auto board_bp = [](Piece pc, Square s, int c) { return c == 0 ? int(Eval::kpp_board_index[pc].fb) + int(s) : int(Eval::kpp_board_index[pc].fw) + int(Inv(s)); };
  auto hand_bp = [](Color owner, PieceType pt, int i, int c) { return (c == 0 ? int(Eval::kpp_hand_index[owner][pt].fb) : int(Eval::kpp_hand_index[owner][pt].fw)) + i; };
  q.reset = 0; q.nrem[0] = q.nrem[1] = q.nadd[0] = q.nadd[1] = 0;
  if (m.is_drop()) {
   const PieceType pt = type_of(after);
   const int ct = hand_count(pos.hand_of(us), pt);
   for (int c = 0; c < 2; ++c) { q.rem[c][q.nrem[c]++] = bp_index(ksq[c], hand_bp(us, pt, ct - 1, c)); q.add[c][q.nadd[c]++] = bp_index(ksq[c], board_bp(after, to, c)); }
   return true;
  }
  const Square from = m.from_sq();
  const Piece before = pos.piece_on(from), captured = pos.piece_on(to);
  if (type_of(before) == KING) return false;
  for (int c = 0; c < 2; ++c) {
   q.rem[c][q.nrem[c]++] = bp_index(ksq[c], board_bp(before, from, c));
   q.add[c][q.nadd[c]++] = bp_index(ksq[c], board_bp(after, to, c));
   if (captured != NO_PIECE) {
    const PieceType raw = raw_type_of(captured);
    q.rem[c][q.nrem[c]++] = bp_index(ksq[c], board_bp(captured, to, c));
    q.add[c][q.nadd[c]++] = bp_index(ksq[c], hand_bp(us, raw, hand_count(pos.hand_of(us), raw), c));
   }
  }
  return true;
 }
 static int predict_bucket(const Position& pos, Move m, int parent_bucket) {
  if (!gp_inc_usable()) return parent_bucket;
  const Color us = pos.side_to_move();
  const Square to = m.to_sq();
  const PieceType ta = type_of(pos.moved_piece_after(m));
  const int rto = int(rank_of(to));
  const bool to_camp = us == BLACK ? rto <= 2 : rto >= 6;
  double gs = pos.state()->gp_s;
  if (m.is_drop()) {
   gs -= gp_s_hand_by_pt[ta];
   if (to_camp) gs += gp_s_camp_by_pt[ta];
  } else {
   const PieceType tb = m.is_promote() ? PieceType(ta - PIECE_TYPE_PROMOTE) : ta;
   if (tb != KING) {
    const int rfr = int(rank_of(m.from_sq()));
    if (us == BLACK ? rfr <= 2 : rfr >= 6) gs -= gp_s_camp_by_pt[tb];
    if (to_camp) gs += gp_s_camp_by_pt[ta];
   }
   if (m.is_promote()) gs += gp_s_promo;
   const Piece captured = pos.piece_on(to);
   if (captured != NO_PIECE) {
    const PieceType tc = type_of(captured);
    if (us == BLACK ? rto >= 6 : rto <= 2) gs -= gp_s_camp_by_pt[tc];
    if (tc > KING) gs -= gp_s_promo;
    gs += gp_s_hand_by_pt[raw_type_of(captured)];
   }
  }
#if defined(CYF_GP8)
  const auto& row = gp_king_rows[gp_king_code(pos)];
#else
  const auto& row = kGpKingRows[gp_king_code(pos)];
#endif 
  return gs < row.boundary[1] ? int(gs >= row.boundary[0]) : (gs < row.boundary[2] ? 2 : 3 + int(gs >= row.boundary[3]));
 }
 static std::vector<int32_t> natural_tail(int& shift) {
  auto& nets = Eval::NNUE::networks();
  std::vector<int32_t> tail(size_t(Eval::NNUE::kLayerStacks) * 9225);
  for (int b = 0; b < int(Eval::NNUE::kLayerStacks); ++b) {
   std::ostringstream os(std::ios::binary); nets.network[b].WriteParameters(os);
   const std::string s = os.str();
   if (s.size() != 10596) { sync_cout << "info string gpupool unexpected tail layout" << sync_endl; std::abort(); }
   const char* p = s.data();
   auto i32 = [&](size_t off) { int32_t v; std::memcpy(&v, p + off, 4); return v; };
   auto i8 = [&](size_t off) { return int32_t(int8_t(p[off])); };
   int32_t* d = tail.data() + size_t(b) * 9225;
   for (int k = 0; k < 8; ++k) d[k] = i32(size_t(k) * 4);
   for (int k = 0; k < 8; ++k) for (int i = 0; i < 1024; ++i) d[8 + k * 1024 + i] = i8(32 + size_t(i >> 2) * 32 + size_t(k) * 4 + size_t(i & 3));
   for (int t = 0; t < 64; ++t) d[8200 + t] = i32(8224 + size_t(t) * 4);
   for (int t = 0; t < 64; ++t) for (int i = 0; i < 14; ++i) d[8264 + t * 14 + i] = i8(8480 + size_t(i >> 2) * 256 + size_t(t) * 4 + size_t(i & 3));
   d[9160] = i32(10528);
   for (int i = 0; i < 64; ++i) d[9161 + i] = i8(10532 + size_t(i));
   shift = nets.network[b].l1_main_shift_;
  }
  return tail;
 }
 void worker_loop() {
  uint64_t last_cpu = thread_cpu_ns();
  while (true) {
   const uint64_t wake = worker_wake.load(std::memory_order_acquire);
   const uint64_t head = q_head.load(std::memory_order_relaxed);
   if (head != q_tail.load(std::memory_order_acquire)) {
    const uint32_t idx = queue[head % queue.size()];
    const uint64_t before = dev.launch_ns;
    dev.submit(dev.pool.ring[idx]);
    worker_launch_ns.fetch_add(dev.launch_ns - before, std::memory_order_relaxed);
    worker_launches.fetch_add(1, std::memory_order_relaxed);
    q_head.store(head + 1, std::memory_order_release);
   } else {
    if (!worker_run.load(std::memory_order_acquire)) break;
    const uint64_t now = thread_cpu_ns();
    worker_cpu_ns.fetch_add(now - last_cpu, std::memory_order_relaxed); last_cpu = now;
    worker_wake.wait(wake, std::memory_order_acquire);
   }
   const uint64_t now = thread_cpu_ns(); worker_cpu_ns.fetch_add(now - last_cpu, std::memory_order_relaxed); last_cpu = now;
  }
  worker_cpu_ns.fetch_add(thread_cpu_ns() - last_cpu, std::memory_order_relaxed);
 }
 void start_worker() { if (worker_run.load()) return; worker_run = true; worker = std::thread([this] { worker_loop(); }); }
 void wake_worker() { worker_wake.fetch_add(1, std::memory_order_release); worker_wake.notify_one(); }
 void stop_worker() { if (!worker_run.load()) return; worker_run = false; wake_worker(); if (worker.joinable()) worker.join(); }
 void close_open() {
  if (auto* l = dev.pool.close()) {
   tr(TrClose, model, uint32_t(l->id), uint32_t(l->n));
   if (cfg.fault_every && (st.closes + 1) % uint64_t(cfg.fault_every) == 0) {
    for (int j = 0; j < l->n; ++j) if (l->tags[size_t(j)]) {
     auto& q = l->req[j]; q.nadd[0] = std::max(1, q.nadd[0]); q.add[0][0] = (q.add[0][0] + 1) % std::max(1, dev.features);
     ++faults_injected; break;
    }
   }
   const uint64_t t = q_tail.load(std::memory_order_relaxed);
   queue[t % queue.size()] = uint32_t(l - dev.pool.ring.data());
   q_tail.store(t + 1, std::memory_order_release);
   wake_worker();
   ++st.closes;
  }
  open_since = 0;
 }
 void disable(const char* reason) {
  if (faulted) return;
  faulted = true; enabled = false; fault_reason = reason;
  sync_cout << "info string gpupool_fault reason=" << reason << " echo_mismatch=" << dev.pool.echo_mismatch
            << " stalled_launches=" << dev.pool.stalled_launches << " stall_drains=" << stall_drains
            << " (pool disabled; search continues on the CPU)" << sync_endl;
 }
 void prepare(size_t threads) {
  cfg.load();
  enabled = cfg.ptx && cfg.depth < 99;
  if (!enabled) return;
  if (faulted) {
   enabled = false;
   sync_cout << "info string gpupool disabled after fault reason=" << fault_reason << sync_endl;
   return;
  }
  if (threads != 1) { sync_cout << "info string gpupool requires Threads=1 (got " << threads << "); refusing" << sync_endl; std::abort(); }
  auto& ft = Eval::NNUE::networks().feature_transformer;
  if (!dev.initialised()) {
   dev.init(cfg.ptx, cfg.slots, cfg.ring, cfg.per_launch, cfg.full_rows);
   table.assign(kTable, Entry{}); resident.assign(kResident, Resident{}); seen.assign(size_t(MAX_PLY) + 16, {});
   queue.assign(size_t(cfg.ring) + 1, 0);
   const auto t0 = std::chrono::steady_clock::now(); const uint64_t c0 = __rdtsc();
   while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(20)) {}
   tsc_mhz = double(__rdtsc() - c0) / double(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count()) * 1000.0;
  }
  if (generation != ft.PoolGeneration()) {
   const auto t0 = std::chrono::steady_clock::now();
   stop_worker();                                   
   if (dev.pool.open >= 0) { st.abandoned += uint64_t(dev.pool.ring[size_t(dev.pool.open)].n); dev.pool.abandon_open(); open_since = 0; }
   while (q_head.load() != q_tail.load()) {         
    const uint64_t h = q_head.load(); dev.submit(dev.pool.ring[queue[h % queue.size()]]); q_head.store(h + 1);
   }
   dev.drain();
   while (!dev.pool.in_flight.empty()) {
    if (auto* l = dev.pool.completed()) dev.pool.release(*l);
    else dev.pool.fail_front();
   }
   if (dev.pool.fault) {
    disable(dev.pool.stalled_launches ? "missing_token_at_model_change" : "echo_mismatch_at_model_change");
    return;
   }
   int shift = 0; const auto tail = natural_tail(shift);
   dev.load_model(reinterpret_cast<const int16_t*>(ft.PoolBiases()), reinterpret_cast<const int16_t*>(ft.PoolWeights()), int(ft.PoolFeatures()),
                  tail.data(), int(Eval::NNUE::kLayerStacks), shift, ++model);
   std::fill(table.begin(), table.end(), Entry{});
   std::fill(resident.begin(), resident.end(), Resident{});
   st.ready_unused_live = 0;
   generation = ft.PoolGeneration();
   sync_cout << "info string gpupool_model model=" << model << " load_us=" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count()
             << " depth=" << cfg.depth << " at=" << cfg.at << " maxreq=" << cfg.maxreq << " skip_acc=" << cfg.skip_acc << " verify_every=" << cfg.verify_every
             << " keep=" << cfg.keep << " keep_parent=" << cfg.keep_parent << " slots=" << cfg.slots << " ring=" << cfg.ring << " per_launch=" << cfg.per_launch
             << " batch_us=" << cfg.batch_us << " tsc_mhz=" << tsc_mhz << sync_endl;
  }
  batch_cycles = uint64_t(double(cfg.batch_us) * tsc_mhz);
  stall_ticks = uint64_t(double(cfg.stall_ms) * 1000.0 * tsc_mhz);
  dev.pool.echo_check = cfg.echo_check != 0;
  if (cfg.trace) trace.reserve(std::min<size_t>(size_t(cfg.trace_cap), 1u << 20));
  start_worker();
 }
 void begin_go() {
  if (!enabled) return;
  tr(TrGo, epoch + 1, 0, 0);
  ++epoch; const uint64_t live = st.ready_unused_live; st = Stats{}; st.ready_unused_live = live;
  candidate_scans = delta_built = duplicate_delta_saved = legal_lists = 0;
  next_poll = poll_calls = poll_empty = poll_skipped = 0;
  st.carried = dev.pool.in_flight.size();
  st.launches_at_go = worker_launches.load(); st.launch_ns_at_go = worker_launch_ns.load(); st.worker_cpu_at_go = worker_cpu_ns.load();
  st.search_cpu_at_go = thread_cpu_ns();
 }
 void poll(uint64_t now = 0, bool force = false) {
  if (cfg.poll_us && !force) {
   if (now < next_poll) { ++poll_skipped; return; }
   next_poll = now + uint64_t(double(cfg.poll_us) * tsc_mhz);
  }
  ++poll_calls;
  bool any = false;
  while (!dev.pool.fault) { auto* l = dev.pool.completed(); if (!l) break; any = true; consume(l); }
  if (!any && !dev.pool.fault) {
   ++poll_empty;
   if ((++stall_polls & 255) == 0 && dev.pool.stalled(__rdtsc(), stall_ticks)) {
    const uint64_t front = dev.pool.ring[dev.pool.in_flight.front()].id;
    ++stall_drains;
    dev.drain();
    while (!dev.pool.fault) { auto* l = dev.pool.completed(); if (!l) break; consume(l); }
    if (!dev.pool.fault && !dev.pool.in_flight.empty() && dev.pool.ring[dev.pool.in_flight.front()].id == front
        && !dev.pool.completed()) dev.pool.fail_front();
   }
  }
  if (dev.pool.fault) { tr(TrFault, 0, uint32_t(dev.pool.echo_mismatch), uint32_t(dev.pool.stalled_launches)); disable(dev.pool.stalled_launches ? "missing_token" : "echo_mismatch"); }
 }
 void consume(CyfGpuPool::Launch* l) {
  const uint64_t c0 = __rdtsc();
  for (int j = 0; j < l->n; ++j) {
   const uint64_t key = l->tags[size_t(j)];
   if (!key) continue;                                  
   Entry& e = table[key & (kTable - 1)];
   const bool ok = l->good[size_t(j)] != 0 && !dev.pool.fault;
   ok ? ++st.harvested_ok : ++st.harvested_bad;
   if (e.key != key || (e.state != Pending && e.state != Wanted)) continue;
   if (e.state == Wanted) ++st.late;
   if (ok) { e.raw = l->raw[j]; e.bucket = uint8_t(l->req[j].bucket); e.model = uint16_t(model); e.state = Ready; ++st.ready_unused_live; }
   else e = Entry{};
  }
  if (cfg.trace) { int good = 0; for (int j = 0; j < l->n; ++j) good += l->good[size_t(j)]; tr(TrHarv, __atomic_load_n(&l->submit_tick, __ATOMIC_ACQUIRE), uint32_t(l->id), uint32_t(l->n) | (uint32_t(good) << 16)); }
  dev.pool.release(*l);
  st.cyc_harvest += __rdtsc() - c0;
 }
 static Value to_value(int32_t raw) {
  auto v = static_cast<Value>(raw / Eval::NNUE::FV_SCALE);
  return Math::clamp(v, -VALUE_MAX_EVAL, VALUE_MAX_EVAL);
 }
 bool probe(const Position& pos, Value& out) {
  const StateInfo* prev = pos.state()->previous;
  if (!enabled || !prev || !prev->cyf_pool_req || pos.state()->accumulator.computed_score) return false;
  const uint64_t c0 = __rdtsc();
  ++st.probes;
  if (open_since && c0 - open_since > batch_cycles) close_open();
  const uint64_t key = uint64_t(pos.key());
  Entry& e = table[key & (kTable - 1)];
  if (!cfg.selective_poll || (e.key == key && (e.state == Pending || e.state == Wanted))) poll(c0, true);
  else ++poll_skipped;
  if (!enabled) { st.cyc_probe += __rdtsc() - c0; return false; }   
  bool hit = false;
  const uint32_t trace_state = uint32_t(e.state) | (uint32_t(e.key == key) << 8);
  uint32_t outcome = 0;
  if (e.key == key && e.state == Pending) {
   e.state = Wanted; ++st.pending_at_use; outcome = 1;
   if (cfg.demand_close && dev.pool.open >= 0 && uint32_t(e.raw) == uint32_t(dev.pool.ring[size_t(dev.pool.open)].id)) {
    close_open(); ++st.demand_closes; outcome = 5;
   }
  }
  else if (e.key == key && (e.state == Ready || e.state == Used) && e.model == uint16_t(model)) {
   const int bucket = Eval::NNUE::stack_index_for_nnue(pos);
   if (bucket != e.bucket) { ++st.bucket_mismatch; outcome = 4; }
   else {
    hit = true; ++st.hits; outcome = e.state == Used ? 3 : 2;
    if (e.state == Used) ++st.reuse_hits; else { e.state = Used; --st.ready_unused_live; }
    auto& acc = pos.state()->accumulator;
    out = to_value(e.raw);
    if (cfg.verify_every && st.hits % uint64_t(cfg.verify_every) == 0) {
     const uint64_t v0 = __rdtsc();
     alignas(64) Eval::NNUE::TransformedFeatureType tf[Eval::NNUE::FeatureTransformer::kBufferSize];
     Eval::NNUE::networks().feature_transformer.Transform(pos, tf, false);
     alignas(64) char buf[Eval::NNUE::Network::kBufferSize];
     const int32_t cpu = Eval::NNUE::networks().network[bucket].Propagate(tf, buf)[0];
     if (cpu != e.raw) { sync_cout << "info string gpupool VERIFY mismatch cpu=" << cpu << " gpu=" << e.raw << " sfen " << pos.sfen() << sync_endl; std::abort(); }
     ++st.verified; st.cyc_verify += __rdtsc() - v0;
    } else if (!cfg.skip_acc) {
     const uint64_t a0 = __rdtsc();
     Eval::NNUE::networks().feature_transformer.EnsureAccumulator(pos, false);
     st.cyc_hit_acc += __rdtsc() - a0;
    } else ++st.skipped_acc;
    acc.score = out; acc.computed_score = true;
   }
  } else ++st.absent;
  tr(TrProbe, key, outcome, trace_state);
  st.cyc_probe += __rdtsc() - c0;
  return hit;
 }
 CyfGpuPool::Handle resident_of(uint64_t key) const {
  const Resident& r = resident[key & (kResident - 1)];
  return r.key == key && dev.pool.table.holds(r.h) ? r.h : CyfGpuPool::Handle{};
 }
 void remember(uint64_t key, const CyfGpuPool::Handle& h) { if (h.valid()) resident[key & (kResident - 1)] = Resident{key, h}; }
 void on_move(Position& pos, int ply, int depth, int moveCount, Move move, const MovePicker& picker, Move excluded) {
  if (!enabled || depth < cfg.depth) return;
  auto& s = seen[size_t(ply)];
  if (moveCount <= int(s.size())) s[size_t(moveCount - 1)] = move;
  if (moveCount != cfg.at) return;
  const uint64_t c0 = __rdtsc();
  ++st.triggers;
  if (open_since && c0 - open_since > batch_cycles) close_open();
  poll(c0);
  auto& pool = dev.pool;
  const uint32_t trig_bits = uint32_t(std::clamp(depth, 0, 255)) | (uint32_t(std::min(moveCount, 255)) << 8);
  if (!enabled) { st.cyc_trigger += __rdtsc() - c0; return; }          
  if (!pool.has_room()) { ++st.skipped_busy; tr(TrTrig, uint64_t(pos.key()), 0, trig_bits | (1u << 24)); st.cyc_trigger += __rdtsc() - c0; return; }
  struct Want { Move m; uint64_t key; int bucket; CyfGpuPool::PoolReq d; };
  thread_local std::vector<Want> want; want.clear();
  const int parent_bucket = Eval::NNUE::stack_index_for_nnue(pos);
  bool any_valued = false;
  auto consider = [&](Move m, bool check_legal) {
   ++candidate_scans;
   if (m == excluded) return;
   bool already = false;
   for (int i = 0; i < std::min<int>(moveCount, int(s.size())); ++i) already |= s[size_t(i)] == m;
   if (already) return;
   if (int(want.size()) >= cfg.maxreq) { ++st.capped; return; }
   if (!m.is_drop() && type_of(pos.piece_on(m.from_sq())) == KING) { ++st.own_king_skipped; return; }
   if (check_legal && !pos.legal(m)) return;
   const uint64_t child_key = uint64_t(pos.key_after(m));
   const Entry& e = table[child_key & (kTable - 1)];
   if (e.key == child_key && e.model == uint16_t(model) && (e.state == Pending || e.state == Wanted)) {
    ++st.already_valued; ++duplicate_delta_saved; any_valued = true; return;
   }
   const int child_bucket = predict_bucket(pos, m, parent_bucket);
   if (e.key == child_key && e.model == uint16_t(model)
       && (e.state == Ready || e.state == Used) && e.bucket == child_bucket) {
    ++st.already_valued; ++duplicate_delta_saved; any_valued = true; return;
   }
   Want w{m, child_key, child_bucket, {}};
   ++delta_built;
   if (!pre_move_delta(pos, m, w.d)) return;
   want.push_back(w);
  };
  if (cfg.picker) {
   const auto [first, last] = picker.pool_candidates();
   int scanned = 0;
   for (auto p = first; p != last && scanned < cfg.scan && int(want.size()) < cfg.maxreq; ++p, ++scanned)
    consider(Move(*p), true);
  } else {
   ++legal_lists;
   for (Move m : MoveList<LEGAL>(pos)) consider(m, false);
  }
  st.cyc_gen += __rdtsc() - c0;
  StateInfo* node = pos.state();
  if (want.empty()) {
   ++st.no_children;
   if (any_valued) node->cyf_pool_req = 1;
   tr(TrTrig, uint64_t(pos.key()), 0, trig_bits | (2u << 24) | (uint32_t(any_valued) << 16));
   st.cyc_trigger += __rdtsc() - c0;
   return;
  }
  const uint64_t pkey = uint64_t(pos.key());
  CyfGpuPool::Handle parent = resident_of(pkey);
  const bool keep_children = cfg.keep && depth - 1 >= cfg.depth;
  const int needed = int(want.size()) + 1;
  if (parent.valid() && pool.open >= 0) {
   const auto& sl = pool.table.slots[size_t(parent.slot)];
   if (sl.state == CyfGpuPool::SlotTable::Writing && sl.writer == pool.ring[size_t(pool.open)].id) { close_open(); ++st.alias_splits; }
  }
  if (pool.open >= 0) {
   const auto& l = pool.ring[size_t(pool.open)];
   if (l.n + needed > pool.capacity || (!parent.valid() && l.full_rows + 1 > pool.full_capacity)) close_open();
  }
  int row = -1, row_nfull[2] = {0, 0};
  if (parent.valid()) ++st.parent_resident;
  else {
   ++st.parent_row;
   CyfGpuPool::Handle kept{};
   const int j = pool.add(CyfGpuPool::Handle{}, false, cfg.keep_parent != 0, true, 0, cfg.keep_parent ? &kept : nullptr);
   if (j < 0) { ++st.refused; tr(TrTrig, pkey, 0, trig_bits | (3u << 24)); st.cyc_trigger += __rdtsc() - c0; return; }
   CyfGpuPool::Launch& l = pool.ring[size_t(pool.open)];
   auto& q = l.req[j];
   q.bucket = parent_bucket; q.stm = int(pos.side_to_move()); q.reset = 3;
   Eval::NNUE::Features::IndexList active[2];
   Eval::NNUE::RawFeatures::AppendActiveIndices(pos, Eval::NNUE::kRefreshTriggers[0], active);
   int32_t* r = l.full + size_t(q.full) * 80;
   for (int c = 0; c < 2; ++c) { row_nfull[c] = q.nfull[c] = int(active[c].size()); for (size_t k = 0; k < active[c].size(); ++k) r[c * 40 + k] = int32_t(active[c][k]); }
   row = q.full;
   if (kept.valid()) { remember(pkey, kept); ++st.parent_kept; }
  }
  if (!open_since) open_since = __rdtsc();
  const int child_stm = int(~pos.side_to_move());
  const uint64_t requests_before = st.requests;
  uint32_t rank = 0;
  for (const Want& w : want) {
   CyfGpuPool::Handle child{};
   const int j = row >= 0 ? pool.add(CyfGpuPool::Handle{}, false, keep_children, true, w.key, keep_children ? &child : nullptr, row)
                          : pool.add(parent, true, keep_children, false, w.key, keep_children ? &child : nullptr);
   if (j < 0) { ++st.refused; break; }
   CyfGpuPool::Launch& l = pool.ring[size_t(pool.open)];
   auto& q = l.req[j];
   q.bucket = w.bucket; q.stm = child_stm;
   if (row >= 0) { q.reset = 3; q.nfull[0] = row_nfull[0]; q.nfull[1] = row_nfull[1]; }
   else q.reset = 0;
   for (int c = 0; c < 2; ++c) { q.nrem[c] = w.d.nrem[c]; q.nadd[c] = w.d.nadd[c]; for (int k = 0; k < 4; ++k) { q.rem[c][k] = w.d.rem[c][k]; q.add[c][k] = w.d.add[c][k]; } }
   Entry& e = table[w.key & (kTable - 1)];
   if (e.state == Pending || e.state == Wanted) ++st.displaced_pending;
   else if (e.state == Ready) { ++st.displaced_unused; --st.ready_unused_live; }
   e = Entry{w.key, int32_t(uint32_t(l.id)), uint16_t(model), uint8_t(w.bucket), Pending};   
   if (child.valid()) remember(w.key, child);
   tr(TrReq, w.key, uint32_t(l.id), rank | (uint32_t(std::clamp(depth, 0, 255)) << 8) | (uint32_t(w.bucket) << 16));
   ++st.requests; ++rank;
  }
  node->cyf_pool_req = 1;
  tr(TrTrig, pkey, uint32_t(st.requests - requests_before), trig_bits | (uint32_t(parent.valid()) << 16));
  if (cfg.batch_us == 0) close_open();
  st.cyc_trigger += __rdtsc() - c0;
 }
 void finish() {
  if (!enabled && !faulted) return;
  if (enabled && dev.pool.open >= 0) { finish_submitted += uint64_t(dev.pool.ring[size_t(dev.pool.open)].n); close_open(); }
  else if (dev.pool.open >= 0) { st.abandoned += uint64_t(dev.pool.ring[size_t(dev.pool.open)].n); dev.pool.abandon_open(); open_since = 0; }
  poll(0, true);
  const uint64_t search_cpu = thread_cpu_ns() - st.search_cpu_at_go;
  std::printf("info string gpupool_poll selective=%d interval_us=%d calls=%" PRIu64 " empty=%" PRIu64 " skipped=%" PRIu64 "\n",
              cfg.selective_poll, cfg.poll_us, poll_calls, poll_empty, poll_skipped);
  std::printf("info string gpupool_candidates picker=%d scans=%" PRIu64 " delta_built=%" PRIu64
              " duplicate_delta_saved=%" PRIu64 " legal_lists=%" PRIu64 "\n",
              cfg.picker, candidate_scans, delta_built, duplicate_delta_saved, legal_lists);
  std::printf("info string gpupool triggers=%" PRIu64 " skipped_busy=%" PRIu64 " no_children=%" PRIu64 " parent_resident=%" PRIu64 " parent_row=%" PRIu64
              " parent_kept=%" PRIu64 " requests=%" PRIu64 " own_king_skipped=%" PRIu64 " capped=%" PRIu64 " already_valued=%" PRIu64 " refused=%" PRIu64
              " alias_splits=%" PRIu64 " closes=%" PRIu64 " launches=%" PRIu64 " harvested_ok=%" PRIu64 " harvested_bad=%" PRIu64 " token_lag=%" PRIu64
              " probes=%" PRIu64 " hits=%" PRIu64 " reuse_hits=%" PRIu64 " pending_at_use=%" PRIu64 " late=%" PRIu64 " absent=%" PRIu64
              " bucket_mismatch=%" PRIu64 " displaced_pending=%" PRIu64 " displaced_unused=%" PRIu64 " ready_unused_live=%" PRIu64
              " verified=%" PRIu64 " skipped_acc=%" PRIu64 " abandoned=%" PRIu64 " carried_in=%" PRIu64 " in_flight_out=%zu"
              " cyc_trigger=%" PRIu64 " cyc_gen=%" PRIu64 " cyc_probe=%" PRIu64 " cyc_harvest=%" PRIu64 " cyc_hit_acc=%" PRIu64 " cyc_verify=%" PRIu64
              " worker_launch_ns=%" PRIu64 " worker_cpu_ns=%" PRIu64 " search_cpu_ns=%" PRIu64 " tsc_khz=%" PRIu64
              " echo_mismatch=%" PRIu64 " stalled_launches=%" PRIu64 " stall_drains=%" PRIu64 " fault=%d"
              " faults_injected=%" PRIu64 " finish_submitted=%" PRIu64 " echo_check=%d trace_events=%zu trace_dropped=%" PRIu64
              " demand_closes=%" PRIu64 "\n",
              st.triggers, st.skipped_busy, st.no_children, st.parent_resident, st.parent_row, st.parent_kept, st.requests, st.own_king_skipped,
              st.capped, st.already_valued, st.refused, st.alias_splits, st.closes, worker_launches.load() - st.launches_at_go, st.harvested_ok,
              st.harvested_bad, dev.pool.token_lag, st.probes, st.hits, st.reuse_hits, st.pending_at_use, st.late, st.absent, st.bucket_mismatch,
              st.displaced_pending, st.displaced_unused, st.ready_unused_live, st.verified, st.skipped_acc, st.abandoned, st.carried,
              dev.pool.in_flight.size(), st.cyc_trigger, st.cyc_gen, st.cyc_probe, st.cyc_harvest, st.cyc_hit_acc, st.cyc_verify,
              worker_launch_ns.load() - st.launch_ns_at_go, worker_cpu_ns.load() - st.worker_cpu_at_go, search_cpu, uint64_t(tsc_mhz * 1000.0),
              dev.pool.echo_mismatch, dev.pool.stalled_launches, stall_drains, int(faulted),
              faults_injected, finish_submitted, int(dev.pool.echo_check), trace.size(), trace_dropped, st.demand_closes);
  std::fflush(stdout);
  tr(TrFinish, epoch, 0, 0);
  trace_flush();
 }
 ~GpuPoolSearch() { stop_worker(); }
};
inline GpuPoolSearch& gpu_pool_search() { static GpuPoolSearch s; return s; }
