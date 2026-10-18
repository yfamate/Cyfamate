
struct BlendConfig {
    int mode = 0;                 
    bool mode_set = false;        
    bool auto_detect = true;      
    std::string model, provider = "cpu";
    int threads = 1;              
    int batch = 1;                
    double wn = 0.5953614038380678, wc = 0.5395981459708928, b = -0.024877423536843715;
    double fn = 1.0775028130676736, fb = -0.04759336851126909;
    int clip = 4144;
    int cache_mb = 256;           
    int min_new = 4096;           
    int max_updates = 0;          
    int queue = 16384;            
    int discovery_budget = 16384; 
    int infer_budget_ms = 0;      
    int queue_batches = 0;        
    bool publish_partial = false; 
    int pack_wait_us = 0;         
    int het_w = 96, het_u = 64;   
    bool fallback_only = false;   
    bool no_scrub = false;        
    bool verify = false;          
    bool check = false;           
    bool no_correction = false;   
    bool stats = true;
    bool root_fix = true;         
    bool root_log = false;        
    bool reset_corr = false;      
    bool det = false;             
    bool dl_cache = true;        
    bool dl_audit = false;        
    int new_rows_per_go = 0;      
};
inline BlendConfig blend_config_instance;
inline BlendConfig& blend_config() { return blend_config_instance; }

#include "cyf_dl_value_cache.h"

struct BlendBackend {
    const CyfOrtApi* api = nullptr;
    CyfOrtEnv* env = nullptr; CyfOrtSession* sess = nullptr; CyfOrtMemoryInfo* mem = nullptr;
#if defined(_WIN32)
    HMODULE dll = nullptr;
#endif
    std::string provider, library, model; int threads = 1; size_t rows = 1;
    std::vector<uint8_t> cells, hand, side;
    std::string ort_version, identity;
    std::atomic<uint64_t> ort_runs{0}, ort_rows{0}, ort_real{0}, ort_failed{0};
    ~BlendBackend() { close(); }
    bool check(CyfOrtStatus* st) {
        if (!st) return true;
        std::fprintf(stderr, "cyf blend ORT: %s\n", api->GetErrorMessage(st));
        api->ReleaseStatus(st); return false;
    }
    void close() {
        if (api) {
            if (sess) api->ReleaseSession(sess);
            if (mem) api->ReleaseMemoryInfo(mem);
            if (env) api->ReleaseEnv(env);
        }
        sess = nullptr; mem = nullptr; env = nullptr; api = nullptr; identity.clear(); ort_version.clear();
#if defined(_WIN32)
        if (dll) FreeLibrary(dll); dll = nullptr;
#endif
    }
    bool contract(bool input, size_t idx, const char* name, int type, const std::vector<int64_t>& dims) {
        CyfOrtAllocator* allocator = nullptr; char* nm = nullptr;
        if (!check(api->GetAllocatorWithDefaultOptions(&allocator))) return false;
        if (!check(input ? api->SessionGetInputName(sess, idx, allocator, &nm)
                         : api->SessionGetOutputName(sess, idx, allocator, &nm))) return false;
        bool ok = std::string(nm) == name; check(api->AllocatorFree(allocator, nm));
        CyfOrtTypeInfo* ti = nullptr; const CyfOrtTensorTypeAndShapeInfo* shape = nullptr;
        if (!check(input ? api->SessionGetInputTypeInfo(sess, idx, &ti)
                         : api->SessionGetOutputTypeInfo(sess, idx, &ti))) return false;
        size_t rank = 0; int element = 0; std::vector<int64_t> actual(dims.size());
        ok = ok && check(api->CastTypeInfoToTensorInfo(ti, &shape))
            && check(api->GetDimensionsCount(shape, &rank)) && rank == dims.size()
            && check(api->GetTensorElementType(shape, &element)) && element == type
            && check(api->GetDimensions(shape, actual.data(), rank)) && actual == dims;
        api->ReleaseTypeInfo(ti); return ok;
    }
    bool open(const BlendConfig& c, std::string& why) {
        close();
#if defined(_WIN32)
        provider = c.provider == "trt" ? "tensorrt" : c.provider;   
        model = c.model; threads = c.threads; rows = size_t(std::max(1, c.batch));
        if (provider != "cpu" && provider != "cuda" && provider != "tensorrt") { why = "provider must be cpu, cuda or tensorrt"; return false; }
        why = "no loadable onnxruntime.dll (set CYFAMATE_ORT_DLL or Cyf_Fusion_ORT)";
        for (const auto& l : fusion_library_candidates()) {
            if (provider != "cpu") fusion_cuda_runtime_hunt(l);
            dll = LoadLibraryExA(l.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!dll) continue;
            auto base = reinterpret_cast<const CyfOrtApiBase*(CYF_ORT_CALL*)()>(GetProcAddress(dll, "OrtGetApiBase"));
            if (base && (api = base()->GetApi(16))) { library = l; ort_version = base()->GetVersionString(); break; }
            FreeLibrary(dll); dll = nullptr; api = nullptr;
        }
        if (!api) return false;
        why = "session rejected";
        if (!check(api->CreateEnv(2, "cyf-blend", &env))) return false;
        CyfOrtSessionOptions* so = nullptr;
        if (!check(api->CreateSessionOptions(&so))) return false;
        bool ok = check(api->SetIntraOpNumThreads(so, threads)) && check(api->SetSessionGraphOptimizationLevel(so, 99));
        if (ok && provider == "tensorrt") {   
            CyfOrtTensorRTProviderOptionsV2* trt = nullptr;
            char dev[16]; std::snprintf(dev, sizeof dev, "%d", fusion_device());
            const std::string cache = model + ".trtcache";
            CreateDirectoryA(cache.c_str(), nullptr);
            const char* keys[] = {"device_id", "trt_fp16_enable", "trt_engine_cache_enable", "trt_engine_cache_path", "trt_timing_cache_enable"};
            const char* vals[] = {dev, "1", "1", cache.c_str(), "1"};
            ok = check(api->CreateTensorRTProviderOptions(&trt)) && check(api->UpdateTensorRTProviderOptions(trt, keys, vals, 5))
              && check(api->SessionOptionsAppendExecutionProvider_TensorRT_V2(so, trt));
            if (trt) api->ReleaseTensorRTProviderOptions(trt);
            CyfOrtCUDAProviderOptionsV2* cu = nullptr;
            const char* ck[] = {"device_id", "cudnn_conv_algo_search"}; const char* cv[] = {dev, "HEURISTIC"};
            ok = ok && check(api->CreateCUDAProviderOptions(&cu)) && check(api->UpdateCUDAProviderOptions(cu, ck, cv, 2))
              && check(api->SessionOptionsAppendExecutionProvider_CUDA_V2(so, cu))
              && check(api->AddSessionConfigEntry(so, "session.disable_cpu_ep_fallback", "1"));
            if (cu) api->ReleaseCUDAProviderOptions(cu);
        }
        if (ok && provider == "cuda") {
#if defined(CYF_BLEND_BLOCKING_CTX)
            static HMODULE driver = LoadLibraryA("nvcuda.dll");
            auto init = reinterpret_cast<int(__stdcall*)(unsigned)>(GetProcAddress(driver,"cuInit"));
            auto device = reinterpret_cast<int(__stdcall*)(int*,int)>(GetProcAddress(driver,"cuDeviceGet"));
            auto state = reinterpret_cast<int(__stdcall*)(int,unsigned*,int*)>(GetProcAddress(driver,"cuDevicePrimaryCtxGetState"));
            auto flags_set = reinterpret_cast<int(__stdcall*)(int,unsigned)>(GetProcAddress(driver,"cuDevicePrimaryCtxSetFlags_v2"));
            int dev_id=0, active=0; unsigned flags=0;
            bool policy_ok=driver && init && device && state && flags_set;
            if (policy_ok) policy_ok=init(0)==0 && device(&dev_id,fusion_device())==0 && state(dev_id,&flags,&active)==0;
            if (policy_ok && (flags & 7u)!=4u)
                policy_ok=!active && flags_set(dev_id,(flags & ~7u)|4u)==0;
            if (policy_ok) policy_ok=state(dev_id,&flags,&active)==0 && (flags & 7u)==4u;
            if (!policy_ok) {
                why="CUDA blocking context policy unavailable";
                api->ReleaseSessionOptions(so); return false;
            }
            sync_cout << "info string cyf blend: cuda_context_flags=" << flags << " active=" << active << sync_endl;
#endif
            CyfOrtCUDAProviderOptionsV2* cu = nullptr;
            char dev[16]; std::snprintf(dev, sizeof dev, "%d", fusion_device());
            const char* keys[] = {"device_id", "cudnn_conv_algo_search"};
            const char* vals[] = {dev, "HEURISTIC"};   
            ok = check(api->CreateCUDAProviderOptions(&cu)) && check(api->UpdateCUDAProviderOptions(cu, keys, vals, 2))
              && check(api->SessionOptionsAppendExecutionProvider_CUDA_V2(so, cu))
              && check(api->AddSessionConfigEntry(so, "session.disable_cpu_ep_fallback", "1"));
            if (cu) api->ReleaseCUDAProviderOptions(cu);
        }
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> path(std::max(1, length));
        ok = ok && length > 0 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model.c_str(), -1, path.data(), length);
        if (ok) ok = check(api->CreateSession(env, path.data(), so, &sess));
        api->ReleaseSessionOptions(so);
        if (!ok) return false;
        size_t inputs = 0, outputs = 0;
        why = "model does not match cells/hand/side -> value[B,1]";
        ok = check(api->SessionGetInputCount(sess, &inputs)) && inputs == 3
          && check(api->SessionGetOutputCount(sess, &outputs)) && outputs == 1
          && contract(true, 0, "cells", 2, {-1, 81}) && contract(true, 1, "hand", 2, {-1, 14})
          && contract(true, 2, "side", 2, {-1}) && contract(false, 0, "value", 1, {-1, 1})
          && check(api->CreateCpuMemoryInfo(0, -1, &mem));
        cells.assign(rows * 81, 0); hand.assign(rows * 14, 0); side.assign(rows, 0);
        if (ok) {
            why = "ok";
            uint64_t bytes = 0; const std::string sha = DlSha256::file(model, &bytes);
            if (!sha.empty()) {
                std::ostringstream id;
                id << "nc01.dl-value.v1|model_sha256=" << sha << "|model_bytes=" << bytes
                   << "|encoder=fusion_encode.v1(cells81=ids15+14*color,hand14=PLNSGBRx2,side1;no-history)"
                   << "|schema=cells:u8[-1,81],hand:u8[-1,14],side:u8[-1]->value:f32[-1,1]"
                   << "|provider=" << provider << "|ort=" << ort_version << "|lib=" << library
                   << "|intra_threads=" << threads << "|graph_opt=99|shape=" << rows;
                if (provider == "tensorrt") id << "|device=" << fusion_device() << "|trt_fp16=1|then_cuda=HEURISTIC|cpu_ep_fallback=0";
                if (provider == "cuda") {
                    id << "|device=" << fusion_device() << "|cudnn_algo=HEURISTIC|cpu_ep_fallback=0";
#if defined(CYF_BLEND_BLOCKING_CTX)
                    id << "|blocking_ctx=1";
#endif
                }
                identity = id.str();
            }
        }
        return ok;
#else
        (void)c; why = "windows only"; return false;
#endif
    }
    bool run(const FusionRaw* raw, size_t n, float* out) {
        if (!sess || n == 0 || n > rows) return false;
        for (size_t i = 0; i < rows; ++i) {
            if (i < n) {
                std::memcpy(cells.data() + i * 81, raw[i].cells, 81);
                std::memcpy(hand.data() + i * 14, raw[i].hand, 14); side[i] = raw[i].side;
            } else if (i == n) {   
                std::memset(cells.data() + i * 81, 0, (rows - i) * 81);
                std::memset(hand.data() + i * 14, 0, (rows - i) * 14);
                std::memset(side.data() + i, 0, rows - i);
                break;
            }
        }
        const int64_t cd[]{int64_t(rows), 81}, hd[]{int64_t(rows), 14}, sd[]{int64_t(rows)};
        CyfOrtValue* in[3]{}; CyfOrtValue* o[1]{};
        const char* names[]{"cells", "hand", "side"}; const char* outputs[]{"value"};
        bool ok = check(api->CreateTensorWithDataAsOrtValue(mem, cells.data(), cells.size(), cd, 2, 2, &in[0]))
            && check(api->CreateTensorWithDataAsOrtValue(mem, hand.data(), hand.size(), hd, 2, 2, &in[1]))
            && check(api->CreateTensorWithDataAsOrtValue(mem, side.data(), side.size(), sd, 1, 2, &in[2]));
        const CyfOrtValue* ci[]{in[0], in[1], in[2]};
        if (ok) {
            ort_runs.fetch_add(1, std::memory_order_relaxed); ort_rows.fetch_add(rows, std::memory_order_relaxed);
            ort_real.fetch_add(n, std::memory_order_relaxed);
            ok = check(api->Run(sess, nullptr, names, ci, 3, outputs, 1, o));
        }
        float* v = nullptr;
        if (ok) ok = check(api->GetTensorMutableData(o[0], reinterpret_cast<void**>(&v)));
        if (ok) for (size_t i = 0; i < n; ++i) { out[i] = v[i]; if (!std::isfinite(v[i])) ok = false; }
        for (auto x : in) if (x) api->ReleaseValue(x);
        if (o[0]) api->ReleaseValue(o[0]);
        if (!ok) ort_failed.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }
};

#include "cyf_value_cache.h"

struct BlendService {
    using Clock = std::chrono::steady_clock;
    struct Req { uint64_t key = 0, epoch = 0; FusionRaw raw; uint32_t job = DlValueCache::kNone, job_seq = 0, id_gen = 0; uint8_t tracked = 0; };
    struct StagedV { uint64_t key; float logit; uint8_t src; };   
    BlendConfig cfg; BlendBackend backend; BlendTable table;
    std::atomic<uint64_t> cost_per_batch_us{3000};
    std::atomic<bool> inference_budget_closed{false};
    std::mutex pack_mutex; std::condition_variable pack_cv;
    std::atomic<uint64_t> pack_wait_us{0},batch_rows{0};
    std::atomic<int> worker_pack_us{0},worker_budget_ms{0};
    uint64_t budget_rejected=0,queue_rejected=0,queue_highwater=0;
    std::string loaded_signature; bool ready = false;
#if defined(CYF_BLEND_ADMISSION_PROFILE)
    uint64_t admission_calls=0, admission_tsc=0;
#endif
#if defined(CYF_BLEND_LIFETIME_AUDIT)
    struct Life { uint64_t requested_go=0, committed_go=0; bool used=false; };
    std::unordered_map<uint64_t,Life> life;
    uint64_t life_go=0, life_commits=0, life_first_hits=0, life_same=0, life_next=0, life_later=0;
    void life_request(uint64_t k) { life[k].requested_go=life_go; }
    void life_hit(uint64_t k) {
        auto it=life.find(k); if(it==life.end() || !it->second.committed_go) return;
        auto& x=it->second;
        if(!x.used) { x.used=true; ++life_first_hits; }
        const auto age=life_go-x.requested_go;
        if(age==0) ++life_same; else if(age==1) ++life_next; else ++life_later;
    }
#endif
    std::vector<Req> ring; std::atomic<uint64_t> head{0}, tail{0};
    std::vector<uint64_t> requested; uint64_t req_mask = 0;
#if defined(CYF_BLEND_REPEAT_ONLY)
    std::vector<uint64_t> visited;
    uint64_t first_visits = 0;
#endif
#if defined(CYF_BLEND_REQUEST_AUDIT)
    std::unordered_map<uint64_t, uint64_t> request_audit;
    uint64_t repeat_requests = 0;
    void audit_request(uint64_t key) {
        auto& count = request_audit[key];
        repeat_requests += count != 0;
        ++count;
    }
#endif
    std::mutex staged_mutex; std::vector<StagedV> staged;
    std::atomic<uint64_t> epoch{0}; std::atomic<bool> accepting{false}, quit{false};
    std::unique_ptr<DlValueCache> dl;   
    bool dl_session = false;            
    bool dl_go = false;                 
    std::string dl_reason = "off";
    uint64_t go_new_rows = 0, dl_req_serial = 0;
    struct GoCounters {                 
        uint64_t calls = 0, capped = 0, filter_pos = 0, filter_neg = 0, hit = 0, join = 0, created = 0,
                 untracked = 0, waiter_full = 0, miss_nocreate = 0, dl_tsc = 0, staged_hit = 0;
        std::array<uint64_t, DlValueCache::SRC_NB> commit{}, use_first{}, use_all{};
    } gc;
    struct WorkerCounters {             
        std::atomic<uint64_t> claim_run{0}, claim_cancel{0}, claim_stale{0}, unclaim{0}, delivered{0},
            rej_late{0}, hist_staged{0}, hist_late{0}, repeat_rows{0}, unique_rows{0}, after_end_batches{0};
    } wc;
    std::atomic<bool> worker_audit{false};
    std::mutex audit_mutex; std::unordered_set<uint64_t> audit_rows;   
    struct UseSlot { uint64_t key = 0; uint8_t src = 0, used = 0; };
    std::vector<UseSlot> use_table; uint64_t use_overflow = 0;          
    uint64_t go_cpu0_us = 0, go_index = 0;
    struct Mark { uint64_t ort_runs, ort_rows, ort_real, ort_failed, claim_run, claim_cancel, claim_stale, unclaim,
                  delivered, rej_late, hist_staged, hist_late, repeat_rows, unique_rows, after_end; DlValueCache::Stats st; };
    Mark mark_begin{}, mark_end{};
    std::thread worker;
#if defined(CYF_BLEND_WORKER_CPU)
    std::atomic<uint64_t> worker_cpu_us{0}; 
#endif
#if defined(CYF_BLEND_WAKE)
    std::atomic<uint64_t> wake_sequence{0};
    void wake_worker() { wake_sequence.fetch_add(1, std::memory_order_release); wake_sequence.notify_one(); }
#endif
    uint64_t rule_id = 0;
    std::atomic<uint64_t> evals{0}, cnn_hits{0}, cnn_runs{0}, run_us{0}, snap_hits{0}, fallbacks{0},
        requests{0}, dropped{0}, deduped{0}, arrived{0}, batches{0}, infer_us{0}, discarded{0},
        verify_checks{0}, verify_mismatch{0};
    uint64_t merged = 0, rule_changes = 0, scrub_us = 0, pass_evals0 = 0, pass_hits0 = 0;
    std::vector<std::array<uint64_t, 4>> passes;   
    std::vector<std::pair<uint64_t, FusionRaw>> pend;
    uint64_t root_passes = 0, root_aborted = 0, root_changed = 0, root_cross_rule = 0, root_cross_pass = 0,
             root_rollbacks = 0, root_rollback_oldrule = 0, root_old_real = 0, corr_resets = 0;
    bool last_cross_rule = false; int64_t last_top_rule_lag = 0;

    ~BlendService() { stop_worker(); }
    std::string signature(const BlendConfig& c) const {
        char buf[256]; std::snprintf(buf, sizeof buf, "%d|%s|%d|%d|%d", c.mode, c.provider.c_str(), c.threads, c.batch, c.cache_mb);
        return std::string(buf) + "|" + c.model + "|fallback=" + std::to_string(c.fallback_only);
    }
    void stop_worker() {
        quit = true;
#if defined(CYF_BLEND_WAKE)
        if (worker.joinable()) wake_worker();   
#endif
        if (worker.joinable()) worker.join(); quit = false;
    }
    void prepare() {
        accepting = false;
        stop_worker();
        ++epoch;
        cfg = blend_config();
        dl_session = dl_go = false; dl_reason = "off";   
        if (dl) dl->reset_inflight();                   
        if (!cfg.mode) { ready = false; stop_worker(); if (cfg.dl_cache) dl_reason = "requires Cyf_Blend_Mode=2"; dl.reset(); return; }
        const bool need_model = !(cfg.mode == 2 && cfg.fallback_only);
        const std::string sig = signature(cfg);
        if (sig != loaded_signature) {
            stop_worker(); ready = false; backend.close(); loaded_signature.clear();
            if (need_model) {
                std::string why;
                if (!backend.open(cfg, why)) {
#if defined(CYF_BLEND_REQUIRED)
                    sync_cout << "Error: required GPU evaluator unavailable (" << why << ")" << sync_endl;
                    std::exit(EXIT_FAILURE);
#else
                    sync_cout << "info string cyf blend: model unavailable (" << why << "); evaluation stays NNUE" << sync_endl;
                    return;
#endif
                }
                if (cfg.provider == "tensorrt") {   
                    FusionRaw dummy{}; float value = 0; const auto t0 = Clock::now();
                    const bool w = backend.run(&dummy, 1, &value);
                    sync_cout << "info string cyf blend: trt warmup " << (w ? "ok" : "FAILED") << " ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count() << sync_endl;
                }
#if defined(CYF_BLEND_WARMUP)
                if (cfg.provider != "cpu") {   
                    FusionRaw dummy{}; float value = 0;
                    const auto start = Clock::now();
                    uint64_t warm_us[3]{};
                    for (int i = 0; i < 3; ++i) {
                      const auto run_start = Clock::now();
                      if (!backend.run(&dummy, 1, &value)) {
                        sync_cout << "info string cyf blend: warmup failed" << sync_endl;
#if defined(CYF_BLEND_REQUIRED)
                        std::exit(EXIT_FAILURE);
#endif
                        backend.close(); return;
                      }
                      warm_us[i] = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-run_start).count());
                    }
                    cost_per_batch_us.store(std::max<uint64_t>(1, std::max(warm_us[1], warm_us[2])), std::memory_order_relaxed);
                    sync_cout << "info string cyf blend: startup_warmup_us="
                              << std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count()
                              << " first_run_us=" << warm_us[0] << " second_run_us=" << warm_us[1]
                              << " third_run_us=" << warm_us[2] << " seeded_batch_us=" << cost_per_batch_us.load()
                              << sync_endl;
                }
#endif
            }
            table.resize(size_t(std::max(1, cfg.cache_mb)));
            loaded_signature = sig;
        }
        table.clear(); rule_id = 0;
        use_table.clear(); use_overflow = 0;   
#if defined(CYF_BLEND_LIFETIME_AUDIT)
        life.clear(); life_go=life_commits=life_first_hits=life_same=life_next=life_later=0;
#endif
#if defined(CYF_BLEND_TTSHADOW)
        cyf_tt_shadow().rule = 0;
#endif
        { std::lock_guard<std::mutex> lk(staged_mutex); staged.clear(); }
        pend.clear();
        ring.assign(size_t(std::max(16, cfg.queue)), Req{}); head = 0; tail = 0;
        requested.assign(size_t(1) << 20, 0); req_mask = requested.size() - 1;
        dl_configure();   
        if (cfg.mode == 2 && need_model && !cfg.det && !worker.joinable()) worker = std::thread([this] { loop(); });
        worker_pack_us=cfg.pack_wait_us;worker_budget_ms=cfg.infer_budget_ms;
        ready = true;
        sync_cout << "info string cyf blend: ready mode=" << cfg.mode << " provider=" << backend.provider
                  << " batch=" << backend.rows << " threads=" << backend.threads << " model=" << cfg.model
                  << " wn=" << cfg.wn << " wc=" << cfg.wc << " b=" << cfg.b << " fn=" << cfg.fn << " fb=" << cfg.fb
                  << " clip=" << cfg.clip << (cfg.fallback_only ? " fallback_only" : "") << (cfg.no_scrub ? " NO_SCRUB(negative control)" : "")
                  << (cfg.det ? " DETERMINISTIC(debug)" : "")
                  << sync_endl;
    }
    void dl_configure() {
        dl_session = false;
        if (!cfg.dl_cache) { dl.reset(); dl_reason = "off"; return; }
        const char* why = nullptr;
        if (cfg.mode != 2) why = "requires Cyf_Blend_Mode=2";
        else if (cfg.det) why = "unsupported with Cyf_Blend_Deterministic (main-thread inference)";
        else if (cfg.fallback_only) why = "no model (Cyf_Blend_FallbackOnly)";
        else if (!backend.sess) why = "model unavailable";
        else if (backend.identity.empty()) why = "model identity unavailable (unreadable model file)";
        if (why) { dl.reset(); dl_reason = why; sync_cout << "info string cyfdlcache: off (" << why << ")" << sync_endl; return; }
        if (!dl) dl = std::make_unique<DlValueCache>();
        const bool changed = dl->set_identity(backend.identity);
        dl->reset_inflight();
        dl_session = true; dl_reason = "on";
        mark_end = take_mark();
        sync_cout << "info string cyfdlcache: ready identity_changed=" << changed
                  << " completed_bytes=" << dl->completed_bytes() << " slots=" << dl->slot_count()
                  << " inflight_cap=" << DlValueCache::kMaxInflight << " inflight_bytes=" << DlValueCache::inflight_bytes()
                  << " waiter_cap=" << DlValueCache::kMaxWaiters << " waiter_bytes=" << DlValueCache::waiter_bytes()
                  << " completed_now=" << dl->snapshot().completed << " identity=" << dl->identity() << sync_endl;
    }
    Mark take_mark() const {
        Mark m{};
        m.ort_runs = backend.ort_runs.load(); m.ort_rows = backend.ort_rows.load(); m.ort_real = backend.ort_real.load();
        m.ort_failed = backend.ort_failed.load();
        m.claim_run = wc.claim_run.load(); m.claim_cancel = wc.claim_cancel.load(); m.claim_stale = wc.claim_stale.load();
        m.unclaim = wc.unclaim.load(); m.delivered = wc.delivered.load(); m.rej_late = wc.rej_late.load();
        m.hist_staged = wc.hist_staged.load(); m.hist_late = wc.hist_late.load();
        m.repeat_rows = wc.repeat_rows.load(); m.unique_rows = wc.unique_rows.load(); m.after_end = wc.after_end_batches.load();
        if (dl) m.st = dl->snapshot().st;
        return m;
    }
    static std::string mark_delta(const Mark& a, const Mark& b) {
        std::ostringstream os;
        os << " ort_runs=" << b.ort_runs - a.ort_runs << " ort_rows=" << b.ort_rows - a.ort_rows
           << " ort_real=" << b.ort_real - a.ort_real << " ort_failed=" << b.ort_failed - a.ort_failed
           << " claim_run=" << b.claim_run - a.claim_run << " claim_cancel=" << b.claim_cancel - a.claim_cancel
           << " claim_stale=" << b.claim_stale - a.claim_stale << " unclaim=" << b.unclaim - a.unclaim
           << " delivered=" << b.delivered - a.delivered << " rej_late=" << b.rej_late - a.rej_late
           << " hist_staged=" << b.hist_staged - a.hist_staged << " hist_late=" << b.hist_late - a.hist_late
           << " repeat_rows=" << b.repeat_rows - a.repeat_rows << " unique_rows=" << b.unique_rows - a.unique_rows
           << " batches_after_end=" << b.after_end - a.after_end
           << " dl_stored=" << b.st.stored - a.st.stored << " dl_present=" << b.st.store_present - a.st.store_present
           << " dl_evict=" << b.st.evictions - a.st.evictions << " dl_done_ok=" << b.st.done_ok - a.st.done_ok
           << " dl_done_fail=" << b.st.done_fail - a.st.done_fail << " dl_nonfinite=" << b.st.done_nonfinite - a.st.done_nonfinite
           << " dl_done_stale=" << b.st.done_stale - a.st.done_stale << " dl_rej_dead=" << b.st.rej_dead - a.st.rej_dead
           << " dl_rej_cancelled=" << b.st.rej_cancelled - a.st.rej_cancelled << " dl_expired=" << b.st.waiters_expired - a.st.waiters_expired
           << " dl_untracked_stored=" << b.st.untracked_stored - a.st.untracked_stored
           << " dl_lock_contended=" << b.st.lock_contended - a.st.lock_contended;
        return os.str();
    }
    static uint64_t thread_cpu_us() {
#if defined(_WIN32)
        FILETIME c{}, e{}, k{}, u{};
        if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u)) return 0;
        return ((uint64_t(k.dwHighDateTime) << 32 | k.dwLowDateTime) + (uint64_t(u.dwHighDateTime) << 32 | u.dwLowDateTime)) / 10;
#else
        return 0;
#endif
    }
    void use_commit(uint64_t k, uint8_t src) {
        if (use_table.empty()) use_table.assign(size_t(1) << 16, UseSlot{});
        const size_t mask = use_table.size() - 1; size_t i = size_t(k) & mask;
        for (size_t probe = 0; probe < 64; ++probe, i = (i + 1) & mask) {
            if (use_table[i].key == k) return;   
            if (!use_table[i].key) { use_table[i] = {k, src, 0}; ++gc.commit[src]; return; }
        }
        ++use_overflow;
    }
    void use_hit(uint64_t k) {
        if (use_table.empty()) return;
        const size_t mask = use_table.size() - 1; size_t i = size_t(k) & mask;
        for (size_t probe = 0; probe < 64; ++probe, i = (i + 1) & mask) {
            UseSlot& s = use_table[i];
            if (!s.key) return;
            if (s.key == k) { ++gc.use_all[s.src]; if (!s.used) { s.used = 1; ++gc.use_first[s.src]; } return; }
        }
    }
    Value clamp_eval(double v) const {
        return Value(std::clamp<long>(std::lround(v), -long(VALUE_MAX_EVAL - 1), long(VALUE_MAX_EVAL - 1)));
    }
    struct HetCorr {   
        std::vector<int16_t> stage, active;
        static uint32_t idx(const Position& pos) {
            const uint64_t h = uint64_t(pos.pawn_key()) ^ (uint64_t(pos.state()->hand_key) * 0x9E3779B97F4A7C15ull);
            return uint32_t((h ^ (h >> 29)) & 0xFFFF);
        }
        void ensure() { if (stage.empty()) { stage.assign(65536, 0); active.assign(65536, 0); } }
    } het;
    void het_stage(const Position& pos, Value b, Value f) {   
        het.ensure();
        int16_t& s = het.stage[HetCorr::idx(pos)];
        const int d = std::clamp(int(b) - int(f), -2000, 2000);
        s = int16_t(std::clamp(int(s) + (d - int(s)) * cfg.het_u / 256, -2000, 2000));
    }
    Value het_apply(const Position& pos, Value f) const {   
        if (het.active.empty()) return f;
        return clamp_eval(double(f) + double(cfg.het_w) * het.active[HetCorr::idx(pos)] / 128.0);
    }
    Value blend(Value nnue, float logit) const {
        const double n = std::clamp<double>(double(nnue), -cfg.clip, cfg.clip);
        const double c = std::clamp<double>(600.0 * double(logit), -cfg.clip, cfg.clip);
        return clamp_eval(cfg.wn * n + cfg.wc * c + 600.0 * cfg.b);
    }
    Value fallback(Value nnue) const {
        if (cfg.fn == 1.0 && cfg.fb == 0.0) return nnue;
        return clamp_eval(cfg.fn * std::clamp<double>(double(nnue), -cfg.clip, cfg.clip) + 600.0 * cfg.fb);
    }
    bool infer_now(const Position& pos, float& logit) {
        FusionRaw r; fusion_encode(pos, r);
        const auto t0 = Clock::now();
        const bool ok = backend.run(&r, 1, &logit);
        run_us += uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count());
        ++cnn_runs;
        return ok;
    }
    Value evaluate(const Position& pos, Value nnue, bool allow_request = true, bool defer_nnue = false) {
        ++evals;
        const uint64_t k = uint64_t(pos.key());
        float logit = 0.f;
        if (cfg.mode == 1) {
            if (table.get(k, logit)) ++cnn_hits;
            else {
                if (!infer_now(pos, logit)) { sync_cout << "info string cyf blend: inference failed; aborting" << sync_endl; std::abort(); }
                table.put(k, logit);
            }
            if (defer_nnue && cfg.wn != 0.0) nnue = exact_nnue(pos);
#if defined(CYF_BLEND_INCREMENTAL_ACC)
            else if (defer_nnue) Eval::NNUE::networks().feature_transformer.MaintainIncrementalAccumulator(pos);
#elif defined(CYF_BLEND_KEEP_ACC)
            else if (defer_nnue) Eval::NNUE::networks().feature_transformer.EnsureAccumulator(pos, false);
#endif
            return blend(nnue, logit);
        }
        if (!cfg.fallback_only && table.get(k, logit)) {
            ++snap_hits;
            if (cfg.dl_audit) use_hit(k);
#if defined(CYF_BLEND_LIFETIME_AUDIT)
            life_hit(k);
#endif
            if (defer_nnue && cfg.wn != 0.0) nnue = exact_nnue(pos);
#if defined(CYF_BLEND_INCREMENTAL_ACC)
            else if (defer_nnue) Eval::NNUE::networks().feature_transformer.MaintainIncrementalAccumulator(pos);
#elif defined(CYF_BLEND_KEEP_ACC)
            else if (defer_nnue) Eval::NNUE::networks().feature_transformer.EnsureAccumulator(pos, false);
#endif
            { const Value bv = blend(nnue, logit); het_stage(pos, bv, fallback(nnue)); return bv; }   
        }
        ++fallbacks;
#if defined(CYF_BLEND_EVAL_FIRST)
        if (defer_nnue) nnue = exact_nnue(pos);
#endif
        if (allow_request && !cfg.fallback_only) request(pos, k);
#if !defined(CYF_BLEND_EVAL_FIRST)
        if (defer_nnue) nnue = exact_nnue(pos);
#endif
        return het_apply(pos, fallback(nnue));   
    }
    void request(const Position& pos, uint64_t k) {
#if defined(CYF_BLEND_ADMISSION_PROFILE)
        struct AdmissionTimer {
            BlendService& s; uint64_t start=__rdtsc();
            ~AdmissionTimer() { ++s.admission_calls; s.admission_tsc+=__rdtsc()-start; }
        } admission_timer{*this};
#endif
        ++gc.calls;
#if defined(CYF_BLEND_GO_SNAPSHOT)
        if (requests.load(std::memory_order_relaxed) >= uint64_t(cfg.discovery_budget)) return;
#endif
        if(cfg.infer_budget_ms>0){
            if(inference_budget_closed.load(std::memory_order_relaxed)){++budget_rejected;return;}
            const auto predicted=(requests.load(std::memory_order_relaxed)/backend.rows+1)*cost_per_batch_us.load(std::memory_order_relaxed);
            if(predicted>uint64_t(cfg.infer_budget_ms)*1000){++budget_rejected;return;}
        }
        if (cfg.max_updates > 0 && passes.size() >= size_t(cfg.max_updates)) return;
        if (!accepting.load(std::memory_order_relaxed)) return;
        const uint64_t ep = epoch.load(std::memory_order_relaxed);
        const uint64_t tag = k + ep * 0x9E3779B97F4A7C15ULL;   
        const uint64_t request_index = k & req_mask;
#if defined(CYF_BLEND_REPEAT_ONLY)
        if (visited.empty()) visited.resize(requested.size());
        auto& visit = visited[request_index];
        if (visit != tag) { visit = tag; ++first_visits; return; }
#endif
#if defined(CYF_BLEND_REQUEST_2WAY)
        uint64_t& seen = requested[request_index & ~uint64_t(1)];
        uint64_t& older = requested[(request_index & ~uint64_t(1)) + 1];
        if (older == tag) { ++deduped; return; }
#else
        uint64_t& seen = requested[request_index];
#endif
        if (seen == tag) { ++deduped; return; }
        auto mark_seen = [&] {
#if defined(CYF_BLEND_REQUEST_2WAY)
            older = seen;
#endif
            seen = tag;
        };
        if (dl_go) { dl_request(pos, k, ep, mark_seen); return; }
        if (cfg.new_rows_per_go > 0 && go_new_rows >= uint64_t(cfg.new_rows_per_go) && !cfg.det) {
            mark_seen(); ++gc.capped; return;   
        }
        if (cfg.det) {
            if (pend.size() >= size_t(std::max(16, cfg.queue))) { ++dropped; return; }
#if defined(CYF_BLEND_REQUEST_2WAY)
            older = seen;
#endif
            seen = tag; pend.emplace_back(k, FusionRaw{}); fusion_encode(pos, pend.back().second); ++requests;
#if defined(CYF_BLEND_LIFETIME_AUDIT)
            life_request(k);
#endif
#if defined(CYF_BLEND_REQUEST_AUDIT)
            audit_request(k);
#endif
            return;
        }
        const uint64_t h = head.load(std::memory_order_relaxed);
        const auto queued=h-tail.load(std::memory_order_acquire);
        if(cfg.queue_batches>0 && queued>=uint64_t(cfg.queue_batches)*backend.rows){++queue_rejected;return;}
        queue_highwater=std::max(queue_highwater,queued+1);
        if (h - tail.load(std::memory_order_acquire) >= ring.size()) { ++dropped; return; }
#if defined(CYF_BLEND_REQUEST_2WAY)
        older = seen;
#endif
        seen = tag;
        Req& r = ring[h % ring.size()]; r.key = k; r.epoch = ep; fusion_encode(pos, r.raw);
        r.tracked = 0; r.job = DlValueCache::kNone;
        head.store(h + 1, std::memory_order_release); ++requests; ++go_new_rows;
        if(cfg.pack_wait_us>0 && h+1-tail.load(std::memory_order_acquire)>=backend.rows)pack_cv.notify_one();
#if defined(CYF_BLEND_LIFETIME_AUDIT)
        life_request(k);
#endif
#if defined(CYF_BLEND_WAKE)
        wake_worker();
#endif
#if defined(CYF_BLEND_REQUEST_AUDIT)
        audit_request(k);
#endif
    }
    template<class MarkSeen>
    void dl_request(const Position& pos, uint64_t k, uint64_t ep, MarkSeen&& mark_seen) {
        const bool capped = cfg.new_rows_per_go > 0 && go_new_rows >= uint64_t(cfg.new_rows_per_go);
        const bool maybe = dl->maybe(k);
        if (maybe) ++gc.filter_pos; else ++gc.filter_neg;
        if (!maybe && capped) { mark_seen(); ++gc.capped; return; }   
        const uint64_t h = head.load(std::memory_order_relaxed);
        const auto queued = h - tail.load(std::memory_order_acquire);
        const bool queue_full = cfg.queue_batches > 0 && queued >= uint64_t(cfg.queue_batches) * backend.rows;
        const bool ring_full = queued >= ring.size();
        const uint64_t t0 = __rdtsc();
        FusionRaw raw; fusion_encode(pos, raw);
        DlRow row; std::memcpy(row.b, &raw, sizeof row.b);
        const uint64_t dg = DlValueCache::digest(dl->namespace_seed(), row);
        DlValueCache::Waiter w; w.owner = 1; w.go = ep; w.key = k; w.req = ++dl_req_serial;
        float logit = 0.f; DlValueCache::Job job;
        const auto res = dl->request(row, dg, k, w, logit, job, !capped && !queue_full && !ring_full);
        gc.dl_tsc += __rdtsc() - t0;
        switch (res) {
        case DlValueCache::Result::Hit: {
            { std::lock_guard<std::mutex> lk(staged_mutex); staged.push_back({k, logit, DlValueCache::SRC_HIT}); }
            ++gc.hit; ++gc.staged_hit; mark_seen(); return;
        }
        case DlValueCache::Result::Joined: ++gc.join; mark_seen(); return;
        case DlValueCache::Result::WaiterFull: ++gc.waiter_full; mark_seen(); return;
        case DlValueCache::Result::Miss:
            if (capped) { ++gc.capped; ++gc.miss_nocreate; mark_seen(); }
            else if (queue_full) ++queue_rejected;   
            else ++dropped;
            return;
        case DlValueCache::Result::Created: case DlValueCache::Result::InflightFull: break;
        }
        queue_highwater = std::max(queue_highwater, queued + 1);
        Req& r = ring[h % ring.size()]; r.key = k; r.epoch = ep; r.raw = raw;
        if (res == DlValueCache::Result::Created) { r.tracked = 1; r.job = job.idx; r.job_seq = job.seq; ++gc.created; }
        else { r.tracked = 2; r.job = DlValueCache::kNone; r.id_gen = dl->identity_generation(); ++gc.untracked; }
        head.store(h + 1, std::memory_order_release); ++requests; ++go_new_rows;
        mark_seen();
        if (cfg.pack_wait_us > 0 && h + 1 - tail.load(std::memory_order_acquire) >= backend.rows) pack_cv.notify_one();
#if defined(CYF_BLEND_LIFETIME_AUDIT)
        life_request(k);
#endif
#if defined(CYF_BLEND_WAKE)
        wake_worker();
#endif
#if defined(CYF_BLEND_REQUEST_AUDIT)
        audit_request(k);
#endif
    }
    void loop() {
#if defined(CYF_BLEND_WORKER_CPU)
        auto thread_cpu = []() -> uint64_t {
            FILETIME created{}, exited{}, kernel{}, user{};
            if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) std::abort();
            return ((uint64_t(kernel.dwHighDateTime)<<32) + kernel.dwLowDateTime
                  + (uint64_t(user.dwHighDateTime)<<32) + user.dwLowDateTime) / 10;
        };
        const auto cpu_start = thread_cpu();
#endif
        std::vector<FusionRaw> raw(backend.rows); std::vector<uint64_t> keys(backend.rows), eps(backend.rows);
        std::vector<float> out(backend.rows);
        std::vector<uint8_t> trk(backend.rows); std::vector<DlValueCache::Job> jobs(backend.rows);
        std::vector<uint32_t> idg(backend.rows); std::vector<DlValueCache::Delivery> deliveries;
        auto live = [this](const DlValueCache::Waiter& w) {
            return accepting.load(std::memory_order_acquire) && w.go == epoch.load(std::memory_order_acquire);
        };
        auto drop_batch = [&](size_t n) {
            for (size_t j = 0; j < n; ++j) if (trk[j] == 1) { dl->unclaim(jobs[j]); ++wc.unclaim; }
            discarded += n;
        };
        uint64_t budget_epoch=~uint64_t(0),spent_us=0;
        while (!quit.load()) {
#if defined(CYF_BLEND_WAKE)
            const auto wake0 = wake_sequence.load(std::memory_order_acquire);
#endif
            const uint64_t t = tail.load(std::memory_order_relaxed);
            uint64_t h = head.load(std::memory_order_acquire);
            if (h == t) {
#if defined(CYF_BLEND_WAKE)
                if (!quit.load()) wake_sequence.wait(wake0, std::memory_order_acquire);
#else
                std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
                continue;
            }
            const int pack_delay=worker_pack_us.load();
            if(pack_delay>0 && h-t<backend.rows){
                const auto begin=Clock::now();
                std::unique_lock<std::mutex> lock(pack_mutex);
                pack_cv.wait_for(lock,std::chrono::microseconds(pack_delay),[&]{return quit.load() || !accepting.load() || head.load(std::memory_order_acquire)-t>=backend.rows;});
                pack_wait_us+=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-begin).count();
                h=head.load(std::memory_order_acquire);
            }
            size_t n = 0; uint64_t i = t; const uint64_t cur0 = epoch.load();
            for (; i < h && n < backend.rows; ++i) {
                const Req& r = ring[i % ring.size()];
                if (r.tracked == 1) {
                    const auto c = dl->claim({r.job, r.job_seq}, live);
                    if (c == DlValueCache::Claim::Cancelled) { ++wc.claim_cancel; continue; }
                    if (c == DlValueCache::Claim::Stale) { ++wc.claim_stale; continue; }
                    ++wc.claim_run;
                } else if (r.epoch != cur0) { ++discarded; continue; }   
                raw[n] = r.raw; keys[n] = r.key; eps[n] = r.epoch;
                trk[n] = r.tracked; jobs[n] = {r.job, r.job_seq}; idg[n] = r.id_gen; ++n;
            }
            tail.store(i, std::memory_order_release);
            if (!n) continue;
            if (!accepting.load(std::memory_order_acquire) || epoch.load() != cur0) {
                drop_batch(n);
                continue;
            }
            const auto t0 = Clock::now();
            if(budget_epoch!=cur0){budget_epoch=cur0;spent_us=0;}
            const int budget_ms=worker_budget_ms.load();
            if(budget_ms>0 && spent_us+cost_per_batch_us.load()>uint64_t(budget_ms)*1000){
                if(epoch.load()==cur0)inference_budget_closed=true;
                drop_batch(n);continue;
            }
#if defined(CYF_BLEND_TEST_WORKER_DELAY)
            sync_cout << "info string cyfblend_test_inflight" << sync_endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif
#if defined(CYF_GPU_ADMISSION)
            if(!CyfGpuAdmission::gate().take(0)){drop_batch(n);continue;}
#endif
            const bool ok = backend.run(raw.data(), n, out.data());
#if defined(CYF_GPU_ADMISSION)
            CyfGpuAdmission::gate().release();
#endif
            batch_rows+=n;
            const auto elapsed=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count());
            infer_us += elapsed;spent_us+=elapsed;
#if defined(CYF_BLEND_WORKER_CPU)
            worker_cpu_us=thread_cpu()-cpu_start;
#endif
            const auto prior=cost_per_batch_us.load(std::memory_order_relaxed);
            cost_per_batch_us.store(std::max<uint64_t>(1,(prior*3+elapsed)/4),std::memory_order_relaxed);
            ++batches;
            if (!accepting.load(std::memory_order_acquire)) ++wc.after_end_batches;   
            if (worker_audit.load(std::memory_order_relaxed)) {
                std::lock_guard<std::mutex> lk(audit_mutex);
                for (size_t j = 0; j < n; ++j) {
                    DlRow row; std::memcpy(row.b, &raw[j], sizeof row.b);
                    if (audit_rows.insert(DlValueCache::digest(0x6e63303161756474ull, row)).second) ++wc.unique_rows;
                    else ++wc.repeat_rows;
                }
            }
            if (!ok) {
                sync_cout << "info string cyf blend: worker inference failed" << sync_endl;
                for (size_t j = 0; j < n; ++j) if (trk[j] == 1) dl->complete(jobs[j], false, 0.f, live, deliveries);
                deliveries.clear();
                continue;
            }
            deliveries.clear();
            for (size_t j = 0; j < n; ++j) {
                if (trk[j] == 1) dl->complete(jobs[j], true, out[j], live, deliveries);
                else if (trk[j] == 2) {
                    DlRow row; std::memcpy(row.b, &raw[j], sizeof row.b);
                    dl->store_untracked(row, DlValueCache::digest(dl->namespace_seed(), row), keys[j], true, out[j], idg[j]);
                }
            }
            std::lock_guard<std::mutex> lk(staged_mutex);
            const uint64_t cur = epoch.load();
            for (size_t j = 0; j < n; ++j) {
                if (trk[j] == 1) continue;
                if (eps[j] != epoch.load() || !accepting.load()) { ++discarded; ++wc.hist_late; continue; }
                staged.push_back({keys[j], out[j], trk[j] == 2 ? uint8_t(DlValueCache::SRC_UNTRACKED) : uint8_t(DlValueCache::SRC_NEW)});
                ++arrived; ++wc.hist_staged;
            }
            for (const auto& d : deliveries) {
                if (d.w.go != cur || !accepting.load()) { ++wc.rej_late; continue; }
                staged.push_back({d.w.key, d.logit, d.w.src}); ++arrived; ++wc.delivered;
            }
        }
#if defined(CYF_BLEND_WORKER_CPU)
        worker_cpu_us = thread_cpu() - cpu_start;
#endif
    }
    template<class TT>
    bool advance_rule(TT& tt, int depth, bool at_go_start) {
        if (!ready || cfg.mode != 2 || cfg.fallback_only) return false;
#if defined(CYF_BLEND_GO_SNAPSHOT)
        if (!at_go_start) return false;
#endif
        if (!at_go_start && cfg.max_updates > 0 && passes.size() >= size_t(cfg.max_updates)) return false;
        std::vector<StagedV> got;
        if (cfg.det) {
            if (pend.size() < size_t(at_go_start && cfg.publish_partial ? 1 : std::max(1, cfg.min_new))) return false;
            got.reserve(pend.size());
            std::vector<FusionRaw> raw(backend.rows); std::vector<float> out(backend.rows);
            for (size_t s = 0; s < pend.size(); s += backend.rows) {
                const size_t n = std::min(backend.rows, pend.size() - s);
                for (size_t j = 0; j < n; ++j) raw[j] = pend[s + j].second;
                const auto t0 = Clock::now();
                if (!backend.run(raw.data(), n, out.data())) {
                    sync_cout << "info string cyf blend: inference failed; aborting" << sync_endl; std::abort();
                }
                infer_us += uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count());
                ++batches;
                for (size_t j = 0; j < n; ++j) got.push_back({pend[s + j].first, out[j], uint8_t(DlValueCache::SRC_NEW)});
            }
            arrived += got.size(); pend.clear();
        } else {
#if defined(CYF_BLEND_PERSISTENT_WORKER)
            std::unique_lock<std::mutex> lk(staged_mutex,std::try_to_lock);
            if(!lk.owns_lock())return false;
#else
            std::lock_guard<std::mutex> lk(staged_mutex);
#endif
            if (staged.size() < size_t(at_go_start && cfg.publish_partial ? 1 : std::max(1, cfg.min_new))) return false;
            got.swap(staged);
        }
        const uint64_t e = evals.load(), hsnap = snap_hits.load();
        if (!at_go_start) passes.push_back({uint64_t(depth), e - pass_evals0, hsnap - pass_hits0, uint64_t(got.size())});
#if defined(CYF_BLEND_STOP_AT_CAP)
        if (!at_go_start && cfg.max_updates > 0 && passes.size() >= size_t(cfg.max_updates)) {
            accepting.store(false, std::memory_order_release);
            ++epoch;
            std::lock_guard<std::mutex> lk(staged_mutex);
            discarded += staged.size();
            staged.clear();
        }
#endif
        pass_evals0 = e; pass_hits0 = hsnap;
        for (const auto& g : got) {
            table.put(g.key, g.logit);
            if (cfg.dl_audit) use_commit(g.key, g.src);
#if defined(CYF_BLEND_LIFETIME_AUDIT)
            auto& x=life[g.key]; x.committed_go=life_go; x.used=false; ++life_commits;
#endif
        }
        merged += got.size();
        if (!het.stage.empty()) het.active = het.stage;   
        if (!cfg.no_scrub) {
            const auto t0 = Clock::now();
            tt.scrub_values();
            scrub_us += uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count());
        }
        ++rule_id; ++rule_changes;
#if defined(CYF_BLEND_TTSHADOW)
        cyf_tt_shadow().rule = uint32_t(rule_id);
#endif
        return true;
    }
    Value peek(const Position& pos) {
        const Value nn = Eval::evaluate(pos);
        if (!ready) return nn;
        float logit = 0.f; const uint64_t k = uint64_t(pos.key());
        if (cfg.mode == 1) {
            if (!table.get(k, logit)) { FusionRaw r; fusion_encode(pos, r); backend.run(&r, 1, &logit); table.put(k, logit); }
            return blend(nn, logit);
        }
        return (!cfg.fallback_only && table.get(k, logit)) ? blend(nn, logit) : het_apply(pos, fallback(nn));   
    }
    void verify_tt_eval(const Position& pos, Value tt_eval) {
        ++verify_checks;
        if (peek(pos) != tt_eval) ++verify_mismatch;
    }
#if defined(CYF_BLEND_TTSHADOW)
    enum { MC_UNSHADOWED, MC_COLLISION_DIRECT, MC_COLLISION_INHERITED, MC_OLDER_RULE, MC_PROBCUT_EXCLUDED,
           MC_OTHER, MC_NB };
    std::array<std::array<uint64_t, MC_NB>, 3> mclass{};
    std::array<uint64_t, 3> checks_by_consumer{}, mismatch_by_consumer{};
    std::array<uint64_t, 16> mismatch_origin_site{}, mismatch_last_site{};
    uint64_t examples_printed = 0;
    void verify_tt_eval(const Position& pos, Value tt_eval, int consumer, const void* entry) {
        ++verify_checks; ++checks_by_consumer[consumer];
        const Value fresh = peek(pos);
        if (fresh == tt_eval) return;
        ++verify_mismatch; ++mismatch_by_consumer[consumer];
        const uint64_t k = uint64_t(pos.key());
        const CyfTTShadowEntry* x = cyf_tt_shadow().find(entry);
        int c;
        if (!x || !x->valid) c = MC_UNSHADOWED;
        else if (x->key != k) c = MC_COLLISION_DIRECT;
        else if (x->origin_key != k) c = MC_COLLISION_INHERITED;
        else if (x->origin_rule != cyf_tt_shadow().rule) c = MC_OLDER_RULE;
        else if (x->origin_site == CYF_TT_SITE_PROBCUT_EXCLUDED) c = MC_PROBCUT_EXCLUDED;
        else c = MC_OTHER;
        ++mclass[consumer][c];
        if (x && x->valid) { ++mismatch_origin_site[x->origin_site & 15]; ++mismatch_last_site[x->site & 15]; }
        if (examples_printed < 40) {
            ++examples_printed;
            sync_cout << "info string cyfblendmismatch consumer=" << consumer << " class=" << c << " key=" << k
                      << " tt_eval=" << int(tt_eval) << " fresh=" << int(fresh) << " diff=" << int(tt_eval) - int(fresh)
                      << " shadow_key=" << (x ? x->key : 0) << " origin_key=" << (x ? x->origin_key : 0)
                      << " shadow_eval=" << (x ? int(x->eval) : 0) << " site=" << (x ? int(x->site) : -1)
                      << " origin_site=" << (x ? int(x->origin_site) : -1) << " origin_rule=" << (x ? x->origin_rule : 0)
                      << " rule=" << cyf_tt_shadow().rule << sync_endl;
        }
    }
    std::string shadow_stats() const {
        std::ostringstream os;
        const char* names[MC_NB] = {"unshadowed", "collision_direct", "collision_inherited", "older_rule",
                                    "probcut_excluded", "other"};
        for (int u = 0; u < 3; ++u) {
            os << " consumer" << u << "_checks=" << checks_by_consumer[u] << " consumer" << u << "_mismatch=" << mismatch_by_consumer[u];
            for (int c = 0; c < MC_NB; ++c) os << " consumer" << u << "_" << names[c] << "=" << mclass[u][c];
        }
        os << " mismatch_origin_sites=";
        for (int s = 0; s < 16; ++s) if (mismatch_origin_site[s]) os << s << ":" << mismatch_origin_site[s] << ",";
        os << " mismatch_last_sites=";
        for (int s = 0; s < 16; ++s) if (mismatch_last_site[s]) os << s << ":" << mismatch_last_site[s] << ",";
        const auto& sh = cyf_tt_shadow();
        os << " shadow_writes=" << sh.writes << " shadow_inherited_writes=" << sh.inherited_writes
           << " shadow_probcut_excluded_writes=" << sh.probcut_excluded_writes
           << " shadow_probcut_excluded_skips=" << sh.probcut_excluded_skips
           << " shadow_unknown_site_writes=" << sh.unknown_site_writes;
        return os.str();
    }
#endif
    template<class RootMoves>
    void root_pass_end(const RootMoves& rms, int pass, bool aborted, bool changed, bool rollback) {
        ++root_passes; root_aborted += aborted; root_changed += changed; root_rollbacks += rollback;
        if (rms.empty()) return;
        const auto& top = rms[0];
        size_t cur = 0, old_rule = 0, old_pass = 0;
        for (size_t i = 1; i < rms.size(); ++i) {
            const auto& rm = rms[i];
            if (rm.score == -VALUE_INFINITE) continue;
            if (rm.cyfRule != rule_id) ++old_rule;
            else if (rm.cyfPass != pass) ++old_pass;
            else ++cur;
        }
        const bool top_real = top.score != -VALUE_INFINITE;
        const bool top_old_rule = top_real && top.cyfRule != rule_id;
        const bool top_old_pass = top_real && top.cyfRule == rule_id && top.cyfPass != pass;
        const bool cross_rule = !rollback && top_old_rule && cur > 0;
        const bool cross_pass = !rollback && top_old_pass && cur > 0;
        root_cross_rule += cross_rule; root_cross_pass += cross_pass; root_old_real += old_rule;
        root_rollback_oldrule += rollback && top_old_rule;
        last_cross_rule = cross_rule; last_top_rule_lag = int64_t(rule_id) - int64_t(top.cyfRule);
        if (!cfg.root_log) return;
        std::ostringstream os;
        os << "info string cyfblendroot pass=" << pass << " rule=" << rule_id << " changed=" << changed
           << " aborted=" << aborted << " rollback=" << rollback << " top=" << top.pv[0].to_usi_string()
           << " top_score=" << int(top.score) << " top_rule=" << top.cyfRule << " top_pass=" << top.cyfPass
           << " others_cur=" << cur << " others_oldrule=" << old_rule << " others_oldpass=" << old_pass
           << " cross_rule=" << cross_rule << " cross_pass=" << cross_pass << " moves=";
        for (size_t i = 0; i < std::min<size_t>(rms.size(), 6); ++i)
            os << rms[i].pv[0].to_usi_string() << ":" << int(rms[i].score) << ":" << rms[i].cyfRule << ":"
               << rms[i].cyfPass << ",";
        sync_cout << os.str() << sync_endl;
    }
    std::string root_stats() const {
        std::ostringstream os;
        os << " root_passes=" << root_passes << " root_aborted=" << root_aborted << " root_changed=" << root_changed
           << " root_cross_rule=" << root_cross_rule << " root_cross_pass=" << root_cross_pass
           << " root_old_rule_scores=" << root_old_real << " root_rollbacks=" << root_rollbacks
           << " root_rollback_oldrule=" << root_rollback_oldrule << " final_cross_rule=" << last_cross_rule
           << " final_top_rule_lag=" << last_top_rule_lag << " corr_resets=" << corr_resets
           << " root_fix=" << cfg.root_fix;
        return os.str();
    }
    template<class TT>
    void begin_go(TT& tt, size_t threads, bool ponder = false) {
        cfg.new_rows_per_go = blend_config().new_rows_per_go; cfg.dl_audit = blend_config().dl_audit;
        worker_audit.store(cfg.dl_audit, std::memory_order_relaxed);
        gc = GoCounters{}; go_new_rows = 0; ++go_index;
        dl_go = ready && dl_session && !ponder;   
        if (ready && cfg.stats && (dl_session || cfg.dl_audit)) {
            const Mark now = take_mark();
            sync_cout << "info string cyfdlcache between go=" << go_index << mark_delta(mark_end, now) << sync_endl;
            mark_begin = now;
        } else mark_begin = take_mark();
        go_cpu0_us = thread_cpu_us();
        cfg.infer_budget_ms=blend_config().infer_budget_ms;cfg.queue_batches=blend_config().queue_batches;
        cfg.publish_partial=blend_config().publish_partial;
        cfg.pack_wait_us=blend_config().pack_wait_us;
        worker_pack_us=cfg.pack_wait_us;worker_budget_ms=cfg.infer_budget_ms;
#if !defined(CYF_BLEND_PERSISTENT_WORKER)
        pack_wait_us=batch_rows=0;
#endif
        budget_rejected=queue_rejected=queue_highwater=0;table.writes=table.evictions=0;
        inference_budget_closed=false;
        cfg.verify = blend_config().verify; cfg.check = blend_config().check;
        cfg.root_fix = blend_config().root_fix; cfg.root_log = blend_config().root_log;
        cfg.reset_corr = blend_config().reset_corr;
        cfg.max_updates = blend_config().max_updates;
        verify_checks = verify_mismatch = 0;
        root_passes = root_aborted = root_changed = root_cross_rule = root_cross_pass = 0;
        root_rollbacks = root_rollback_oldrule = root_old_real = corr_resets = 0;
        last_cross_rule = false; last_top_rule_lag = 0;
#if defined(CYF_BLEND_TTSHADOW)
        mclass = {}; checks_by_consumer = {}; mismatch_by_consumer = {};
        mismatch_origin_site = {}; mismatch_last_site = {}; examples_printed = 0;
        cyf_tt_shadow().reset_counters();
#endif
        if (!ready) return;
#if defined(CYF_BLEND_WORKER_CPU)
#if !defined(CYF_BLEND_PERSISTENT_WORKER)
        worker_cpu_us = 0;
#endif
#endif
#if defined(CYF_BLEND_LIFETIME_AUDIT)
        ++life_go;
#endif
        if (threads != 1) {
            sync_cout << "info string cyf blend: experimental modes require Threads=1 (the rule may only change while no "
                         "search thread runs); refusing" << sync_endl;
            std::abort();
        }
        evals = cnn_hits = cnn_runs = run_us = snap_hits = fallbacks = requests = dropped = deduped = 0;
#if !defined(CYF_BLEND_PERSISTENT_WORKER)
        arrived=0;
#endif
        verify_checks = verify_mismatch = 0;
#if !defined(CYF_BLEND_PERSISTENT_WORKER)
        batches = infer_us = discarded = 0;
#endif
#if defined(CYF_BLEND_ADMISSION_PROFILE)
        admission_calls=admission_tsc=0;
#endif
#if defined(CYF_BLEND_REQUEST_AUDIT)
        request_audit.clear(); repeat_requests = 0;
#endif
        merged = rule_changes = scrub_us = 0; passes.clear(); pass_evals0 = pass_hits0 = 0;
#if defined(CYF_BLEND_REPEAT_ONLY)
        first_visits = 0;
#endif
        if (cfg.mode == 2) {
            advance_rule(tt, 0, true);
            accepting = true;
            if (!cfg.fallback_only && !cfg.det && !worker.joinable()) worker = std::thread([this] { loop(); });
        }
    }
    template<class RootMoves>
    void end_go(Position& root, const RootMoves& rootMoves) {
        if (ready && cfg.mode == 2) {
            if (dl_session) {
                std::lock_guard<std::mutex> lk(staged_mutex);
                accepting = false; ++epoch;
            } else { accepting = false; ++epoch; }
            pack_cv.notify_all();
#if !defined(CYF_BLEND_PERSISTENT_WORKER)
            stop_worker();
#endif
            if (dl_session) {
                const uint64_t cur = epoch.load();
                dl->expire([cur](const DlValueCache::Waiter& w) { return w.go != cur; });
            }
        }
        if (ready && cfg.stats && (dl_session || cfg.dl_audit)) dl_go_stats();
        if (cfg.check) pv_leaf_check(root, rootMoves);
        if (!ready) {
            if (cfg.verify || cfg.root_log) {
                std::ostringstream os;
                os << "info string cyfblend mode=0 verify_checks=" << verify_checks << " verify_mismatch=" << verify_mismatch;
                if (cfg.root_log) os << root_stats();
#if defined(CYF_BLEND_TTSHADOW)
                if (cfg.verify) os << shadow_stats();
#endif
                sync_cout << os.str() << sync_endl;
            }
            return;
        }
        if (!cfg.stats) return;
        const uint64_t e = evals.load(), hsnap = snap_hits.load();
        if (cfg.mode == 2 && e > pass_evals0) passes.push_back({999, e - pass_evals0, hsnap - pass_hits0, 0});
        std::ostringstream os;
#if defined(CYF_BLEND_PERSISTENT_WORKER)
        os << "info string cyfblend worker_counters_lifetime=1 worker_persistent=1 ";
        os << "mode=";
#else
        os << "info string cyfblend mode=";
#endif
        os << cfg.mode << " evals=" << e;
#if defined(CYF_BLEND_ADMISSION_PROFILE)
        os << " admission_calls=" << admission_calls << " admission_tsc=" << admission_tsc;
#endif
#if defined(CYF_BLEND_LIFETIME_AUDIT)
        os << " life_go=" << life_go << " life_commits=" << life_commits
           << " life_first_hits=" << life_first_hits << " life_same=" << life_same
           << " life_next=" << life_next << " life_later=" << life_later;
#endif
#if defined(CYF_BLEND_WORKER_CPU)
        os << " worker_cpu_us=" << worker_cpu_us;
#endif
#if defined(CYF_BLEND_REPEAT_ONLY)
        os << " first_visits=" << first_visits;
#endif
        if (cfg.mode == 1)
            os << " cnn_hits=" << cnn_hits << " cnn_runs=" << cnn_runs << " run_us=" << run_us;
        else
            os << " snap_hits=" << hsnap << " fallbacks=" << fallbacks << " requests=" << requests
               << " dropped=" << dropped << " deduped=" << deduped << " arrived=" << arrived
               << " discarded=" << discarded << " batches=" << batches << " infer_us=" << infer_us
               << " merged=" << merged << " rule_changes=" << rule_changes << " scrub_us=" << scrub_us
               << " rule_id=" << rule_id << " passes=";
        if (cfg.mode == 2) for (auto& p : passes) os << p[0] << ":" << p[1] << ":" << p[2] << ":" << p[3] << ",";
        os << " cost_per_batch_us=" << cost_per_batch_us << " budget_rejected=" << budget_rejected << " queue_rejected=" << queue_rejected << " queue_highwater=" << queue_highwater << " cache_writes=" << table.writes << " cache_evictions=" << table.evictions;
        os << " pack_wait_us=" << pack_wait_us << " batch_rows=" << batch_rows;
        if (cfg.verify) os << " verify_checks=" << verify_checks << " verify_mismatch=" << verify_mismatch;
        os << root_stats();
#if defined(CYF_BLEND_REQUEST_AUDIT)
        os << " request_unique=" << request_audit.size() << " request_repeats=" << repeat_requests;
#endif
#if defined(CYF_BLEND_TTSHADOW)
        if (cfg.verify) os << shadow_stats();
#endif
        sync_cout << os.str() << sync_endl;
    }
    void dl_go_stats() {
        const Mark now = take_mark();
        std::ostringstream os;
        os << "info string cyfdlcache go=" << go_index << " dl=" << dl_go << " session=" << dl_session
           << " audit=" << cfg.dl_audit << " cap=" << cfg.new_rows_per_go << " calls=" << gc.calls
           << " deduped=" << deduped << " capped=" << gc.capped << " filter_pos=" << gc.filter_pos
           << " filter_neg=" << gc.filter_neg << " hit=" << gc.hit << " join=" << gc.join << " created=" << gc.created
           << " untracked=" << gc.untracked << " waiter_full=" << gc.waiter_full << " miss_nocreate=" << gc.miss_nocreate
           << " dropped=" << dropped << " queue_rejected=" << queue_rejected << " new_rows=" << go_new_rows
           << " requests=" << requests << " dl_tsc=" << gc.dl_tsc << " staged_hit=" << gc.staged_hit
           << " evals=" << evals << " snap_hits=" << snap_hits << " fallbacks=" << fallbacks
           << " merged=" << merged << " rule_changes=" << rule_changes;
        static const char* nm[DlValueCache::SRC_NB] = {"new", "hit", "join", "untracked"};
        for (int s = 0; s < DlValueCache::SRC_NB; ++s) os << " commit_" << nm[s] << "=" << gc.commit[s];
        for (int s = 0; s < DlValueCache::SRC_NB; ++s) os << " use_first_" << nm[s] << "=" << gc.use_first[s];
        for (int s = 0; s < DlValueCache::SRC_NB; ++s) os << " use_all_" << nm[s] << "=" << gc.use_all[s];
        os << " use_overflow=" << use_overflow << mark_delta(mark_begin, now);
        if (dl) { const auto s = dl->snapshot(); os << " completed_now=" << s.completed << " inflight_now=" << s.inflight << " waiters_now=" << s.waiters; }
        os << " search_cpu_us=" << thread_cpu_us() - go_cpu0_us;
        sync_cout << os.str() << sync_endl;
        mark_end = now;
    }
    template<class RootMoves>
    void pv_leaf_check(Position& root, const RootMoves& rootMoves) {
        if (rootMoves.empty()) return;
        const auto& rm = rootMoves[0];
        std::deque<StateInfo> st; size_t done = 0; bool ok = true;
        for (Move m : rm.pv) {
            if (!m.is_ok() || !root.pseudo_legal_s<true>(m) || !root.legal(m)) { ok = false; break; }
            st.emplace_back(); root.do_move(m, st.back()); ++done;
        }
        std::ostringstream os;
        os << "info string cyfblendcheck pvlen=" << rm.pv.size() << " walked=" << done << " root=" << int(rm.score);
        if (ok && !root.checkers()) {
            const Value nn = Eval::evaluate(root);
            float logit = 0.f; bool have = ready && table.get(uint64_t(root.key()), logit);
            Value e = nn;
            if (ready && cfg.mode == 1) { if (!have) { have = infer_now(root, logit); table.put(uint64_t(root.key()), logit); } e = blend(nn, logit); }
            else if (ready && cfg.mode == 2) e = (have && !cfg.fallback_only) ? blend(nn, logit) : fallback(nn);
            const int sign = (done % 2) ? -1 : 1;
            os << " leaf_nnue=" << int(nn) << " leaf_cnn_cp=" << (have ? int(std::lround(600.0 * logit)) : 0)
               << " leaf_E=" << int(e) << " root_vs_leaf=" << (int(rm.score) - sign * int(e))
               << " match=" << (int(rm.score) == sign * int(e));
        } else os << " leaf=" << (ok ? "in_check" : "unwalkable");
        for (size_t i = done; i-- > 0;) root.undo_move(rm.pv[i]);
        sync_cout << os.str() << sync_endl;
    }
};
inline BlendService& blend_service() { static BlendService s; return s; }
inline bool blend_active() { return blend_service().ready; }

inline void blend_dl_debug(std::istream& is) {
    auto& s = blend_service(); std::string sub; is >> sub;
    std::ostringstream os;
    if (sub == "clear") {
        if (s.dl) s.dl->clear_completed();
        { std::lock_guard<std::mutex> lk(s.audit_mutex); s.audit_rows.clear(); }
        os << "info string cyfdlcache cleared completed=" << (s.dl ? s.dl->snapshot().completed : 0);
        sync_cout << os.str() << sync_endl; return;
    }
    if (sub == "dump") {
        size_t n = 8; is >> n;
        if (!s.dl) { sync_cout << "info string cyfdlcache dump_end n=0" << sync_endl; return; }
        static const char* hex = "0123456789abcdef";
        const auto entries = s.dl->dump(n);
        for (const auto& [row, logit] : entries) {
            uint32_t bits; std::memcpy(&bits, &logit, 4);
            std::ostringstream line; line << "info string cyfdlcache entry logit=" << std::setprecision(9) << logit << " bits=";
            for (int b = 28; b >= 0; b -= 4) line << hex[(bits >> b) & 15];
            line << " row=";
            for (uint8_t c : row.b) line << hex[c >> 4] << hex[c & 15];
            sync_cout << line.str() << sync_endl;
        }
        sync_cout << "info string cyfdlcache dump_end n=" << entries.size() << sync_endl;
        return;
    }
    const auto& d = blend_config();
    os << "info string cyfdlcache declared=" << d.dl_cache << " effective=" << s.dl_session << " reason=" << s.dl_reason
       << " audit=" << d.dl_audit << " new_rows_per_go=" << d.new_rows_per_go << " mode=" << s.cfg.mode
       << " provider=" << s.backend.provider << " shape=" << s.backend.rows << " ort_runs=" << s.backend.ort_runs
       << " ort_rows=" << s.backend.ort_rows << " ort_real=" << s.backend.ort_real << " ort_failed=" << s.backend.ort_failed;
    { std::lock_guard<std::mutex> lk(s.audit_mutex); os << " audit_rows=" << s.audit_rows.size(); }
    if (s.dl) {
        const auto sn = s.dl->snapshot(); const auto& t = sn.st;
        os << " completed=" << sn.completed << " slots=" << s.dl->slot_count() << " completed_bytes=" << s.dl->completed_bytes()
           << " inflight=" << sn.inflight << "/" << DlValueCache::kMaxInflight << " waiters=" << sn.waiters << "/" << DlValueCache::kMaxWaiters
           << " hits=" << t.hits << " joins=" << t.joins << " created=" << t.created << " inflight_full=" << t.inflight_full
           << " waiter_full=" << t.waiter_full << " stored=" << t.stored << " evictions=" << t.evictions
           << " done_stale=" << t.done_stale << " identity_changes=" << t.identity_changes << " clears=" << t.clears
           << " identity=" << s.dl->identity();
    }
    sync_cout << os.str() << sync_endl;
}

inline void blend_debug(Position& pos) {
    auto& s = blend_service();
    FusionRaw r; fusion_encode(pos, r);
    std::ostringstream os;
    const Value nn = Eval::evaluate(pos);
    os << "info string cyfblend key=" << uint64_t(pos.key()) << " side=" << int(pos.side_to_move())
       << " check=" << bool(pos.checkers()) << " nnue=" << int(nn);
    if (s.ready && s.backend.sess) {
        float logit = 0.f; s.infer_now(pos, logit);
        os << " cnn_logit=" << logit << " cnn_cp=" << 600.0 * logit << " E_blend=" << int(s.blend(nn, logit));
    }
    if (s.ready) os << " E_fallback=" << int(s.fallback(nn));
    os << " raw=";
    static const char* hex = "0123456789abcdef";
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&r);
    for (size_t i = 0; i < sizeof r; ++i) os << hex[b[i] >> 4] << hex[b[i] & 15];
    sync_cout << os.str() << sync_endl;
}
