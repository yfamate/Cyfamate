struct FusionConfig {
    bool adaptive_batch = false;
    int mode = 0; 
    int min_depth = 12, batch = 8, queue = 512, cache = 1024, wait_us = 0;
    int gap_permille = 150;
    int device = -1; 
    std::string model, ort_dll, provider, calibration;
};
inline FusionConfig& fusion_config() { static FusionConfig c; return c; }
inline size_t fusion_batch_rows(size_t real, size_t maximum, bool adaptive, const std::string& provider) {
    if (!adaptive || provider != "cuda") return maximum;
    size_t rows=1; while(rows<real && rows<maximum) rows*=2;
    return std::min(rows,maximum);
}
inline const char* fusion_env(const char* name, const char* fallback = "") {
    const auto& c = fusion_config();
    const std::string* selected = nullptr;
    if (std::strcmp(name,"CYFAMATE_FUSION_MODEL")==0) selected=&c.model;
    else if (std::strcmp(name,"CYFAMATE_ORT_DLL")==0) selected=&c.ort_dll;
    else if (std::strcmp(name,"CYFAMATE_FUSION_PROVIDER")==0) selected=&c.provider;
    else if (std::strcmp(name,"CYFAMATE_FUSION_CALIBRATION")==0) selected=&c.calibration;
    if(selected && !selected->empty() && *selected!="<empty>") return selected->c_str();
    const char* p = std::getenv(name); return p && *p ? p : fallback;
}
inline int fusion_device() {
    const int d = fusion_config().device;
    return d >= 0 ? d : std::atoi(fusion_env("CYFAMATE_FUSION_DEVICE", "0"));
}
inline uint64_t fusion_file_id(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return 0;
    uint64_t h = 14695981039346656037ULL; unsigned char buf[8192]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)))
        for (size_t i = 0; i < n; ++i) h = (h ^ buf[i]) * 1099511628211ULL;
    const bool ok = !std::ferror(f); std::fclose(f); return ok ? h : 0;
}
struct FusionRaw { uint8_t cells[81]{}, hand[14]{}, side = 0; };
static_assert(sizeof(FusionRaw) == 96);
struct FusionResult { float value = 0; std::vector<float> policy; };
struct FusionTicket {
    std::atomic<unsigned> consumers{0};
    std::atomic<bool> published{false};
    std::shared_ptr<const FusionResult> result;
    void publish(std::shared_ptr<const FusionResult> value) {
        result=std::move(value);
        published.store(true,std::memory_order_release);
    }
};
struct FusionLease {
    std::shared_ptr<FusionTicket> ticket;
    FusionLease() = default;
    explicit FusionLease(std::shared_ptr<FusionTicket> t):ticket(std::move(t)) {
        if(ticket) ticket->consumers.fetch_add(1,std::memory_order_relaxed);
    }
    FusionLease(const FusionLease&) = delete;
    FusionLease& operator=(const FusionLease&) = delete;
    FusionLease(FusionLease&& other) noexcept:ticket(std::move(other.ticket)) {}
    FusionLease& operator=(FusionLease&& other) noexcept {
        if(ticket||other.ticket) move_assign(other);
        return *this;
    }
    ~FusionLease(){if(ticket) release();}
    void reset() {if(ticket) release();}
    explicit operator bool() const {return bool(ticket);}
    const FusionResult* result() const {
        return ticket && ticket->published.load(std::memory_order_acquire) ? ticket->result.get() : nullptr;
    }
private:
    [[gnu::noinline]] void move_assign(FusionLease& other) noexcept {
        if(this!=&other){reset();ticket=std::move(other.ticket);}
    }
    [[gnu::noinline]] void release() noexcept {
        ticket->consumers.fetch_sub(1,std::memory_order_relaxed);ticket.reset();
    }
};
struct FusionCalibration {
    double na = 0, nb = 0, da = 0, db = 0; bool valid = false;
    bool load(const std::string& model, const std::string& nnue) {
        valid = false;
        FILE* f = std::fopen(fusion_env("CYFAMATE_FUSION_CALIBRATION"), "r");
        if (!f) return false;
        char magic[32]{}; unsigned long long mi = 0, ni = 0;
        const int n = std::fscanf(f, "%31s %llx %llx %lf %lf %lf %lf", magic,
                                  &mi, &ni, &na, &nb, &da, &db);
        std::fclose(f);
        valid = n == 7 && std::string(magic) == "CYFCAL1"
            && mi != 0 && ni != 0 && mi == fusion_file_id(model) && ni == fusion_file_id(nnue)
            && std::isfinite(na) && std::isfinite(nb) && std::isfinite(da) && std::isfinite(db)
            && na > 0 && da > 0;
        return valid;
    }
    static double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-std::clamp(x, -60.0, 60.0))); }
    double gap(int corrected_internal_value, float dl_logit) const {
        return valid ? std::abs(sigmoid(na * corrected_internal_value + nb)
                             - sigmoid(da * dl_logit + db)) : -1.0;
    }
};

inline const char* fusion_provider_entry(const std::string& provider) {
    return provider == "dml"  ? "OrtSessionOptionsAppendExecutionProvider_DML"
         : provider == "cuda" ? "OrtSessionOptionsAppendExecutionProvider_CUDA" : "";
}
#if defined(_WIN32)
inline bool fusion_has_nvidia() { 
    HMODULE h = LoadLibraryA("nvcuda.dll");
    if (!h) return false;
    FreeLibrary(h); return true;
}
inline std::string fusion_exe_dir() {
    char buf[MAX_PATH]; const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return {};
    const std::string p(buf, n); const size_t cut = p.find_last_of("\\/");
    return cut == std::string::npos ? std::string() : p.substr(0, cut + 1);
}
inline std::string fusion_python_ort() { 
    FILE* pipe = _popen("python -c \"import onnxruntime,os,sys;"
                        "sys.stdout.write(os.path.join(os.path.dirname(onnxruntime.__file__),"
                        "'capi','onnxruntime.dll'))\" 2>NUL", "r");
    if (!pipe) return {};
    char buf[1024]; const size_t n = std::fread(buf, 1, sizeof buf - 1, pipe);
    _pclose(pipe); buf[n] = 0;
    return n > 4 ? std::string(buf, n) : std::string();
}
inline std::vector<std::string> fusion_library_candidates() {
    std::vector<std::string> out;
    auto push = [&out](std::string p) {
        if (!p.empty() && std::find(out.begin(), out.end(), p) == out.end()) out.push_back(std::move(p));
    };
    const std::string given = fusion_env("CYFAMATE_ORT_DLL");
    if (!given.empty()) { push(given); return out; } 
    push("onnxruntime.dll");                         
    const std::string dir = fusion_exe_dir();
    if (!dir.empty()) {
        push(dir + "ort\\cuda\\onnxruntime.dll");    
        push(dir + "ort\\dml\\onnxruntime.dll");
        push(dir + ".learning-venv\\Lib\\site-packages\\onnxruntime\\capi\\onnxruntime.dll");
    }
    push(fusion_python_ort());
    return out;
}
inline bool fusion_dir_has_cuda_runtime(const std::string& dir) {
    static const char* patterns[] = {"cudnn64_*.dll", "cublas64_*.dll", "cublasLt64_*.dll", "cufft64_*.dll",
                                     "cudart64_*.dll", "nvinfer_*.dll"};
    for (const char* pattern : patterns) {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((dir + "\\" + pattern).c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) { FindClose(h); return true; }
    }
    return false;
}
inline void fusion_cuda_runtime_hunt(const std::string& library) {
    std::vector<std::string> dirs;
    const size_t cut = library.find_last_of("\\/");
    if (cut != std::string::npos) {
        const std::string base = library.substr(0, cut);
        dirs.push_back(base);
        std::string up = base;
        for (int i = 0; i < 2 && !up.empty(); ++i) {
            const size_t c = up.find_last_of("\\/");
            up = c == std::string::npos ? std::string() : up.substr(0, c);
        }
        if (!up.empty()) {
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((up + "\\nvidia\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.')
                        dirs.push_back(up + "\\nvidia\\" + fd.cFileName + "\\bin");
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
        }
    }
    const char* cuda_path = std::getenv("CUDA_PATH");
    if (cuda_path && *cuda_path) dirs.push_back(std::string(cuda_path) + "\\bin");
    const char* trt = std::getenv("CYF_TRT_DIR");
    if (trt && *trt) dirs.push_back(trt);
    {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA("C:\\Program Files\\NVIDIA\\TensorRT-*", &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    dirs.push_back(std::string("C:\\Program Files\\NVIDIA\\") + fd.cFileName + "\\lib");
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    std::string prefix;
    for (const auto& d : dirs) if (fusion_dir_has_cuda_runtime(d)) prefix += d + ";";
    if (prefix.empty()) return;
    std::vector<char> path(32767);
    const DWORD n = GetEnvironmentVariableA("PATH", path.data(), DWORD(path.size()));
    SetEnvironmentVariableA("PATH", (prefix + (n && n < path.size() ? path.data() : "")).c_str());
    std::fprintf(stderr, "cyf fusion: CUDA runtime search path += %s\n", prefix.c_str());
}
#endif

struct FusionBackend {
    const CyfOrtApi* api = nullptr;
    CyfOrtEnv* env = nullptr; CyfOrtSession* sess = nullptr;
    CyfOrtMemoryInfo* mem = nullptr;
#if defined(_WIN32)
    HMODULE dll = nullptr;
#endif
    std::string provider, library;
    bool check(CyfOrtStatus* st) {
        if (!st) return true;
        std::fprintf(stderr, "cyf fusion ORT: %s\n", api->GetErrorMessage(st));
        api->ReleaseStatus(st); return false;
    }
    ~FusionBackend() { close(); }
    void close() {
        if (api) {
            if (sess) api->ReleaseSession(sess);
            if (mem) api->ReleaseMemoryInfo(mem);
            if (env) api->ReleaseEnv(env);
        }
        sess = nullptr; mem = nullptr; env = nullptr; api = nullptr; library.clear();
#if defined(_WIN32)
        if (dll) FreeLibrary(dll); dll = nullptr;
#endif
    }
    bool tensor_contract(bool input, size_t idx, const char* name, int type,
                         const std::vector<int64_t>& dims) {
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
    bool open(const std::string& model) {
        close();
#if defined(_WIN32)
        const std::string requested = fusion_env("CYFAMATE_FUSION_PROVIDER", "auto");
        std::vector<std::string> providers;
        if (requested == "auto") {
            if (fusion_has_nvidia()) providers = {"cuda", "dml"};
            else                     providers = {"dml", "cuda"};
        } else providers = {requested};
        const std::vector<std::string> libraries = fusion_library_candidates();
        std::vector<std::string> rejected;
        for (const auto& p : providers) {
            if (!*fusion_provider_entry(p)) {
                std::fprintf(stderr, "cyf fusion: unknown provider '%s' (auto/cuda/dml)\n", p.c_str());
                continue;
            }
            for (const auto& l : libraries) {
                std::string why;
                if (open_with(model, l, p, why)) {
                    std::fprintf(stderr, "cyf fusion: provider=%s library=%s\n", p.c_str(), l.c_str());
                    return true;
                }
                close();
                rejected.push_back(p + " " + l + ": " + why);
            }
        }
        std::fprintf(stderr, "cyf fusion: no provider/onnxruntime combination available (requested=%s)\n",
                     requested.c_str());
        for (const auto& r : rejected) std::fprintf(stderr, "cyf fusion:   %s\n", r.c_str());
        return false;
#else
        (void)model; return false;
#endif
    }
    bool open_with(const std::string& model, const std::string& library, const std::string& want,
                   std::string& why) {
        provider = want; this->library = library; why = "ok";
#if defined(_WIN32)
        if (want == "cuda") fusion_cuda_runtime_hunt(library);
        dll = LoadLibraryExA(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!dll) { why = "library not loadable"; return false; }
        using Append = CyfOrtStatus*(CYF_ORT_CALL*)(CyfOrtSessionOptions*, int);
        auto append = reinterpret_cast<Append>(GetProcAddress(dll, fusion_provider_entry(want)));
        if (!append) { why = std::string("built without ") + want; return false; }
        auto base = reinterpret_cast<const CyfOrtApiBase*(CYF_ORT_CALL*)()>(GetProcAddress(dll, "OrtGetApiBase"));
        if (!base || !(api = base()->GetApi(16))) { why = "onnxruntime API below 16"; return false; }
        why = "provider or model rejected by onnxruntime";
        if (!check(api->CreateEnv(2, "cyf-fusion", &env))) return false;
        CyfOrtSessionOptions* so = nullptr;
        if (!check(api->CreateSessionOptions(&so))) return false;
        bool ok = check(api->SetIntraOpNumThreads(so, 1));
        ok = ok && check(api->SetSessionExecutionMode(so, 0))
                && check(api->DisableMemPattern(so));
        ok = ok && check(append(so, fusion_device()));
        ok = ok && check(api->AddSessionConfigEntry(so, "session.disable_cpu_ep_fallback", "1"));
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> path(std::max(1, length));
        ok = ok && length > 0 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                   model.c_str(), -1, path.data(), length);
        if (ok) ok = check(api->CreateSession(env, path.data(), so, &sess));
        api->ReleaseSessionOptions(so);
        if (!ok) return false;
        why = "model does not match the required input/output contract";
        size_t inputs = 0, outputs = 0;
        ok = check(api->SessionGetInputCount(sess, &inputs)) && inputs == 3
          && check(api->SessionGetOutputCount(sess, &outputs)) && outputs == 2
          && tensor_contract(true, 0, "cells", 2, {-1,81})
          && tensor_contract(true, 1, "hand", 2, {-1,14})
          && tensor_contract(true, 2, "side", 2, {-1})
          && tensor_contract(false, 0, "value", 1, {-1,1})
          && tensor_contract(false, 1, "policy", 1, {-1,13689})
          && check(api->CreateCpuMemoryInfo(0, -1, &mem));
        if (!ok) std::fprintf(stderr, "cyf fusion: requires raw Cyfnet dynamic batch value/logit + policy contract\n");
        if (ok) why = "ok";
        return ok;
#else
        (void)model; (void)library; (void)want; why = "windows only"; return false;
#endif
    }
    bool run(const std::vector<FusionRaw>& raw, std::vector<std::shared_ptr<const FusionResult>>& result,
             size_t real_rows=std::numeric_limits<size_t>::max()) {
        const size_t n = raw.size(); if (!n || !sess) return false;
        std::vector<uint8_t> cells(n*81), hand(n*14), side(n);
        for (size_t i=0; i<n; ++i) {
            std::memcpy(cells.data()+i*81, raw[i].cells, 81);
            std::memcpy(hand.data()+i*14, raw[i].hand, 14); side[i]=raw[i].side;
        }
        const int64_t cd[]{int64_t(n),81}, hd[]{int64_t(n),14}, sd[]{int64_t(n)};
        CyfOrtValue* in[3]{}; CyfOrtValue* out[2]{};
        const char* names[]{"cells","hand","side"}; const char* outputs[]{"value","policy"};
        bool ok = check(api->CreateTensorWithDataAsOrtValue(mem,cells.data(),cells.size(),cd,2,2,&in[0]))
            && check(api->CreateTensorWithDataAsOrtValue(mem,hand.data(),hand.size(),hd,2,2,&in[1]))
            && check(api->CreateTensorWithDataAsOrtValue(mem,side.data(),side.size(),sd,1,2,&in[2]));
        const CyfOrtValue* ci[]{in[0],in[1],in[2]};
        if (ok) ok=check(api->Run(sess,nullptr,names,ci,3,outputs,2,out));
        float *values=nullptr,*policies=nullptr;
        if (ok) ok=check(api->GetTensorMutableData(out[0],reinterpret_cast<void**>(&values)))
                  && check(api->GetTensorMutableData(out[1],reinterpret_cast<void**>(&policies)));
        result.clear();
        if (ok) for (size_t i=0;i<std::min(n,real_rows);++i) {
            auto r=std::make_shared<FusionResult>(); r->value=values[i];
            r->policy.assign(policies+i*13689,policies+(i+1)*13689);
            if (!std::isfinite(r->value) || !std::all_of(r->policy.begin(),r->policy.end(),
                                    [](float v){return std::isfinite(v);})) {ok=false; break;}
            result.push_back(std::move(r));
        }
        for (auto v:in) if(v) api->ReleaseValue(v);
        for (auto v:out) if(v) api->ReleaseValue(v);
        return ok;
    }
};

struct FusionService {
    using Clock=std::chrono::steady_clock;
    struct Request {uint64_t key, epoch; FusionRaw raw; Clock::time_point sent;
                    std::shared_ptr<FusionTicket> ticket;};
    struct Slot {uint64_t key=0, epoch=0; bool pending=false; std::shared_ptr<FusionTicket> ticket;};
    struct Counters {
        std::atomic<uint64_t> submitted{0}, duplicate{0}, busy{0}, full{0}, completed{0},
            hits{0}, batches{0}, latency_us{0}, infer_us{0}, eligible{0}, timely{0},
            policy_selected{0}, value_selected{0}, changed{0}, expired{0}, failures{0},
            nodes{0}, offered{0}, closed{0}, cancelled{0}, late{0}, live_completed{0}, gpu_rows{0};
        void reset() {submitted=0;duplicate=0;busy=0;full=0;completed=0;hits=0;batches=0;
            latency_us=0;infer_us=0;eligible=0;timely=0;policy_selected=0;value_selected=0;
            changed=0;expired=0;failures=0;nodes=0;offered=0;closed=0;
            cancelled=0;late=0;live_completed=0;gpu_rows=0;}
    } stats;
    FusionBackend backend; FusionCalibration calibration; FusionConfig cfg;
    std::vector<std::unique_ptr<FusionBackend>> batch_backends;
    std::mutex mutex; std::condition_variable cv; std::deque<Request> queue;
    std::vector<Slot> table; std::thread worker;
    bool quit=false, accepting=false; uint64_t epoch=0;
    std::atomic<bool> ready{false}; bool running=false;
    ~FusionService() {stop();}
    void stop() {
        ready=false;
        {std::lock_guard<std::mutex> lk(mutex);quit=true;accepting=false;queue.clear();}
        cv.notify_one(); if(worker.joinable())worker.join(); batch_backends.clear();backend.close(); running=false;
    }
    void prepare(const std::string& nnue) { 
        stop(); cfg=fusion_config(); if(!cfg.mode)return;
        const std::string model=fusion_env("CYFAMATE_FUSION_MODEL");
        calibration.load(model,nnue);
        if(cfg.mode==3 && !calibration.valid) {
            std::fprintf(stderr,"cyf fusion: mode 3 refused: missing/mismatched CYFCAL1 calibration\n");return;
        }
        if(!backend.open(model)){std::fprintf(stderr,"cyf fusion: GPU/model unavailable; guidance disabled\n");backend.close();return;}
        std::vector<FusionRaw> warm(cfg.batch); std::vector<std::shared_ptr<const FusionResult>> out;
        if(!backend.run(warm,out)){backend.close();return;}
        if(cfg.adaptive_batch && backend.provider=="cuda") {
            batch_backends.resize(cfg.batch);
            for(size_t n=1;n<size_t(cfg.batch);n*=2) {
                auto b=std::make_unique<FusionBackend>();
                std::string why;
                if(!b->open_with(model,backend.library,"cuda",why)) {
                    std::fprintf(stderr,"cyf fusion: batch backend %zu failed: %s\n",n,why.c_str());
                    batch_backends.clear();backend.close();return;
                }
                warm.resize(n);
                if(!b->run(warm,out)){batch_backends.clear();backend.close();return;}
                batch_backends[n]=std::move(b);
            }
        }
        table.assign(cfg.cache,Slot{}); quit=false; ready=true; running=true;
        worker=std::thread([this]{loop();});
        std::fprintf(stderr,"cyf fusion: ready mode=%d provider=%s device=%d batch=%d model=%s\n",
                     cfg.mode,backend.provider.c_str(),fusion_device(),cfg.batch,model.c_str());
    }
    void begin() { 
        std::lock_guard<std::mutex> lk(mutex); ++epoch; queue.clear();stats.reset();
        accepting=ready.load();
        for(auto& s:table) {
            if(s.ticket && s.ticket->published.load(std::memory_order_acquire)) s.epoch=epoch;
            else s=Slot{};
        }
    }
    void end() {
        std::lock_guard<std::mutex> lk(mutex); accepting=false;queue.clear();
        if(!cfg.mode)return;
        sync_cout << "info string fusion mode=" << cfg.mode << " ready=" << ready.load()
          << " nodes=" << stats.nodes << " offered=" << stats.offered << " closed=" << stats.closed
          << " submitted=" << stats.submitted << " completed=" << stats.completed
          << " hits=" << stats.hits << " eligible=" << stats.eligible << " timely=" << stats.timely
          << " policy=" << stats.policy_selected << " value=" << stats.value_selected
          << " changed=" << stats.changed << " busy=" << stats.busy << " full=" << stats.full
          << " duplicate=" << stats.duplicate << " expired=" << stats.expired
          << " cancelled=" << stats.cancelled << " late=" << stats.late
          << " live_completed=" << stats.live_completed
          << " batches=" << stats.batches << " latency_us=" << stats.latency_us
          << " infer_us=" << stats.infer_us << " gpu_rows=" << stats.gpu_rows
          << " failures=" << stats.failures << sync_endl;
    }
    template<class Encode>
    FusionLease visit(uint64_t key, Encode encode) {
        ++stats.offered;
        if(!ready.load(std::memory_order_relaxed))return {};
        std::unique_lock<std::mutex> lk(mutex,std::try_to_lock);
        if(!lk.owns_lock()){++stats.busy;return {};}
        if(!accepting){++stats.closed;return {};}
        Slot& s=table[key%table.size()];
        if(s.epoch==epoch && s.key==key) {
            if(s.ticket && s.ticket->published.load(std::memory_order_acquire)){
                ++stats.hits;return FusionLease(s.ticket);
            }
            if(s.pending){++stats.duplicate;return FusionLease(s.ticket);}
        }
        if(queue.size()>=size_t(cfg.queue)){++stats.full;return {};}
        if(s.pending){++stats.busy;return {};}
        auto ticket=std::make_shared<FusionTicket>();
        FusionLease lease(ticket); 
        Request request{key,epoch,{},Clock::now(),ticket}; encode(request.raw);
        s={key,epoch,true,ticket};queue.push_back(std::move(request));++stats.submitted;
        lk.unlock();cv.notify_one();return lease;
    }
    void take_batch(std::vector<Request>& batch) {
        while(!queue.empty() && batch.size()<size_t(cfg.batch)) {
            Request r=std::move(queue.front());queue.pop_front();
            if(r.epoch!=epoch){++stats.expired;continue;}
            if(r.ticket->consumers.load(std::memory_order_relaxed)==0) {
                auto& s=table[r.key%table.size()];
                if(s.ticket==r.ticket) s=Slot{};
                ++stats.cancelled;continue;
            }
            batch.push_back(std::move(r));
        }
    }
    void loop() {
        for(;;) {
            std::vector<Request> batch;
            {std::unique_lock<std::mutex> lk(mutex);
             cv.wait(lk,[this]{return quit||!queue.empty();});if(quit)return;
             const auto deadline=queue.front().sent+std::chrono::microseconds(cfg.wait_us);
             cv.wait_until(lk,deadline,[this]{return quit||queue.size()>=size_t(cfg.batch)||!accepting;});
             if(quit)return;
             take_batch(batch);}
            if(batch.empty())continue;
            std::vector<FusionRaw> raw;for(auto& r:batch)raw.push_back(r.raw);
            raw.resize(fusion_batch_rows(batch.size(),cfg.batch,cfg.adaptive_batch,backend.provider));
            std::vector<std::shared_ptr<const FusionResult>> out;
            auto& selected=(raw.size()<batch_backends.size() && batch_backends[raw.size()])
                ? *batch_backends[raw.size()] : backend;
            const auto start=Clock::now();const bool ok=selected.run(raw,out,batch.size());const auto end=Clock::now();
            std::lock_guard<std::mutex> lk(mutex);
            if(batch.front().epoch!=epoch){stats.expired+=batch.size();continue;}
            if(!accepting)continue;
            ++stats.batches;stats.gpu_rows+=raw.size();stats.infer_us+=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
            if(!ok){++stats.failures;ready=false;accepting=false;queue.clear();continue;}
            for(size_t i=0;i<batch.size();++i){auto& r=batch[i];auto& s=table[r.key%table.size()];
                if(s.key!=r.key||s.epoch!=r.epoch||s.ticket!=r.ticket){++stats.expired;continue;}
                if(r.ticket->consumers.load(std::memory_order_relaxed)>0) ++stats.live_completed;
                s.pending=false;r.ticket->publish(out[i]);++stats.completed;
                stats.latency_us+=std::chrono::duration_cast<std::chrono::microseconds>(end-r.sent).count();}
        }
    }
};
inline FusionService& fusion_service(){static FusionService s;return s;}

inline int fusion_square(Square sq){return int(rank_of(sq))*9+8-int(file_of(sq));}
inline int fusion_label(Move m){
    static const int hand_index[8]={-1,0,1,2,3,5,6,4};
    const int to=fusion_square(m.to_sq());
    return m.is_drop()?13122+hand_index[int(m.move_dropped_piece())]*81+to
                     :(fusion_square(m.from_sq())*81+to)*2+int(m.is_promote());
}
inline void fusion_encode(const Position& pos,FusionRaw& out){
    static const uint8_t ids[16]={0,1,2,3,4,6,7,5,8,9,10,11,12,13,14,0};
    for(Square sq=SQ_ZERO;sq<SQ_NB;++sq){Piece p=pos.piece_on(sq);
        out.cells[fusion_square(sq)]=p==NO_PIECE?0:ids[int(type_of(p))]+14*int(color_of(p));}
    static const PieceType pts[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};
    for(int c=0;c<2;++c)for(int i=0;i<7;++i)out.hand[c*7+i]=uint8_t(hand_count(pos.hand_of(Color(c)),pts[i]));
    out.side=uint8_t(pos.side_to_move());
}
struct FusionHint {
    Move move=Move::none();bool arrived=false,disagrees=false,used=false;
    FusionLease lease;
};
inline void fusion_poll(const Position& pos,FusionHint& hint,int corrected,bool late=true){
    if(hint.arrived)return;
    const auto* result=hint.lease.result();
    if(!result)return;
    auto& service=fusion_service();
    hint.arrived=true;if(late)++service.stats.late;
    float best=-std::numeric_limits<float>::infinity();
    for(auto em:MoveList<LEGAL>(pos)){Move m=em;const float p=result->policy[fusion_label(m)];
        if(p>best){best=p;hint.move=m;}}
    hint.disagrees=service.calibration.gap(corrected,result->value)>=service.cfg.gap_permille/1000.0;
}
inline FusionHint fusion_visit(const Position& pos,uint64_t key,int depth,int corrected){
    FusionHint hint;auto& service=fusion_service();
    if(!service.cfg.mode)return hint;
    ++service.stats.nodes;
    if(bench_guard()||depth<service.cfg.min_depth)return hint;
    hint.lease=service.visit(key,[&](FusionRaw& r){fusion_encode(pos,r);});
    fusion_poll(pos,hint,corrected,false);
    return hint;
}
inline int fusion_depth(FusionHint& hint,Move move,bool quiet,int baseline,int full){
    auto& service=fusion_service();if(!service.cfg.mode||!quiet||baseline>=full)return baseline;
    ++service.stats.eligible;if(hint.arrived)++service.stats.timely;
    if(hint.used||move!=hint.move)return baseline;
    ++service.stats.policy_selected;if(hint.disagrees)++service.stats.value_selected;
    if(service.cfg.mode==1||(service.cfg.mode==3&&!hint.disagrees))return baseline;
    hint.used=true;++service.stats.changed;return std::min(full,baseline+1);
}
