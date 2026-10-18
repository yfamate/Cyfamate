
struct AutoConfig {
    std::atomic<bool> requested{false}, dump{false};
    std::atomic<int> test_delay_ms{0};
    std::atomic<uint64_t> version{1};      
    std::string model = "<empty>", ort = "<empty>";   
};
inline AutoConfig& auto_config() { static AutoConfig c; return c; }

inline uint64_t auto_thread_cycles() {
#if defined(_WIN32)
    ULONG64 c = 0; QueryThreadCycleTime(GetCurrentThread(), &c); return uint64_t(c);
#else
    return 0;
#endif
}

struct AutoBackend {
    static constexpr int kIntra = 4, kInter = 1;
    const CyfOrtApi* api = nullptr;
    CyfOrtEnv* env = nullptr; CyfOrtSession* sess = nullptr; CyfOrtMemoryInfo* mem = nullptr;
#if defined(_WIN32)
    HMODULE dll = nullptr;
#endif
    std::string library, version, model, tag;
    using CreateRO = CyfOrtStatus*(CYF_ORT_CALL*)(CyfOrtRunOptions**);
    using RunOpt = CyfOrtStatus*(CYF_ORT_CALL*)(CyfOrtRunOptions*);
    using InterOp = CyfOrtStatus*(CYF_ORT_CALL*)(CyfOrtSessionOptions*, int);
    ~AutoBackend() { close(); }
    bool check(CyfOrtStatus* st, std::string* why = nullptr) {
        if (!st) return true;
        if (why) *why = api->GetErrorMessage(st);
        api->ReleaseStatus(st); return false;
    }
    bool is_open() const { return sess != nullptr; }
    void close() {
        if (api) {
            if (sess) api->ReleaseSession(sess);
            if (mem) api->ReleaseMemoryInfo(mem);
            if (env) api->ReleaseEnv(env);
        }
        sess = nullptr; mem = nullptr; env = nullptr; api = nullptr;
#if defined(_WIN32)
        if (dll) FreeLibrary(dll);
        dll = nullptr;
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
    bool open(const std::string& model_path, const std::string& ort_path, std::string& why) {
        close();
#if defined(_WIN32)
        model = model_path;
        if (model.empty() || model == "<empty>") { why = "model_not_set"; return false; }
        if (!fusion_file_id(model)) { why = "model_unreadable"; return false; }
        std::vector<std::string> libs;
        if (!ort_path.empty() && ort_path != "<empty>") libs.push_back(ort_path); else libs = fusion_library_candidates();
        why = "no_loadable_onnxruntime";
        for (const auto& l : libs) {
            dll = LoadLibraryExA(l.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!dll) continue;
            auto base_fn = reinterpret_cast<const CyfOrtApiBase*(CYF_ORT_CALL*)()>(GetProcAddress(dll, "OrtGetApiBase"));
            const CyfOrtApiBase* base = base_fn ? base_fn() : nullptr;
            if (base && (api = base->GetApi(16))) { library = l; version = base->GetVersionString(); break; }
            FreeLibrary(dll); dll = nullptr; api = nullptr;
        }
        if (!api) return false;
        why = "session_rejected";
        if (!check(api->CreateEnv(2, "cyf-auto", &env), &why)) return false;
        CyfOrtSessionOptions* so = nullptr;
        if (!check(api->CreateSessionOptions(&so), &why)) return false;
        bool ok = check(api->SetIntraOpNumThreads(so, kIntra), &why)
               && check(reinterpret_cast<InterOp>(api->SetInterOpNumThreads)(so, kInter), &why)
               && check(api->SetSessionExecutionMode(so, 0), &why)              
               && check(api->SetSessionGraphOptimizationLevel(so, 99), &why)    
               && check(api->AddSessionConfigEntry(so, "session.intra_op.allow_spinning", "0"), &why);
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> path(std::max(1, length));
        ok = ok && length > 0 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model.c_str(), -1, path.data(), length);
        if (ok) ok = check(api->CreateSession(env, path.data(), so, &sess), &why);   
        api->ReleaseSessionOptions(so);
        if (!ok) return false;
        size_t inputs = 0, outputs = 0;
        why = "model_contract";   
        ok = check(api->SessionGetInputCount(sess, &inputs)) && inputs == 3
          && check(api->SessionGetOutputCount(sess, &outputs)) && outputs == 1
          && contract(true, 0, "cells", 2, {-1, 81}) && contract(true, 1, "hand", 2, {-1, 14})
          && contract(true, 2, "side", 2, {-1}) && contract(false, 0, "value", 1, {-1, 1})
          && check(api->CreateCpuMemoryInfo(0, -1, &mem));
        if (!ok) return false;
        char t[32]; std::snprintf(t, sizeof t, "%016llx", (unsigned long long)fusion_file_id(model));
        tag = std::string("cnn8m-fnv") + t;
        why = "ok";
        return true;
#else
        (void)model_path; (void)ort_path; why = "windows_only"; return false;
#endif
    }
    CyfOrtRunOptions* make_ro() {
        CyfOrtRunOptions* ro = nullptr;
        return check(reinterpret_cast<CreateRO>(api->CreateRunOptions)(&ro)) ? ro : nullptr;
    }
    void terminate(CyfOrtRunOptions* ro) { if (ro) check(reinterpret_cast<RunOpt>(api->RunOptionsSetTerminate)(ro)); }
    void release_ro(CyfOrtRunOptions* ro) { if (ro) api->ReleaseRunOptions(ro); }
    bool run(const FusionRaw* raw, size_t n, float* out, CyfOrtRunOptions* ro, std::string& why) {
        if (!sess || !n) { why = "no_session"; return false; }
        std::vector<uint8_t> cells(n * 81), hand(n * 14), side(n);
        for (size_t i = 0; i < n; ++i) {
            std::memcpy(cells.data() + i * 81, raw[i].cells, 81);
            std::memcpy(hand.data() + i * 14, raw[i].hand, 14); side[i] = raw[i].side;
        }
        const int64_t cd[]{int64_t(n), 81}, hd[]{int64_t(n), 14}, sd[]{int64_t(n)};
        CyfOrtValue* in[3]{}; CyfOrtValue* o[1]{};
        const char* names[]{"cells", "hand", "side"}; const char* outputs[]{"value"};
        bool ok = check(api->CreateTensorWithDataAsOrtValue(mem, cells.data(), cells.size(), cd, 2, 2, &in[0]), &why)
               && check(api->CreateTensorWithDataAsOrtValue(mem, hand.data(), hand.size(), hd, 2, 2, &in[1]), &why)
               && check(api->CreateTensorWithDataAsOrtValue(mem, side.data(), side.size(), sd, 1, 2, &in[2]), &why);
        const CyfOrtValue* ci[]{in[0], in[1], in[2]};
        if (ok) ok = check(api->Run(sess, ro, names, ci, 3, outputs, 1, o), &why);
        float* v = nullptr;
        if (ok) ok = check(api->GetTensorMutableData(o[0], reinterpret_cast<void**>(&v)), &why);
        if (ok) for (size_t i = 0; i < n; ++i) { out[i] = v[i]; if (!std::isfinite(v[i])) { ok = false; why = "nonfinite"; } }
        for (auto x : in) if (x) api->ReleaseValue(x);
        if (o[0]) api->ReleaseValue(o[0]);
        return ok;
    }
};

struct AutoPayload { CyfRootHint::Msg msg; };

struct AutoService : public std::enable_shared_from_this<AutoService> {
    CyfAutoHint::Core<AutoPayload> core;
    AutoBackend backend;
    bool open_ok = false, quit_done = false;
    uint64_t open_version = 0, opens = 0, open_failures = 0, warm_rows_total = 0, native_uncollected = 0;
    std::string status = "never_enabled", why = "never_enabled";
    double create_ms = -1, warm1_ms = -1, warm2_ms = -1;
    int warm_rows = 0;
    struct Go {
        uint64_t gen = 0, key = 0, id = 0, search_cycles0 = 0, search_cycles = 0;
        int64_t t0_us = 0, end_us = -1;
        bool enabled = false;
        const char* status = "none";
        std::string reason;
    } go;
    uint64_t prev_id = 0;

    ~AutoService() { shutdown(nullptr); }

    static std::string conflicts() {
        std::string s;
        if (!CyfPreInfer::state().enabled.load()) s += s.empty() ? "Cyf_PreInfer_Observe" : ",Cyf_PreInfer_Observe";
        if (!CyfPreInfer::state().root_ready_requested.load()) s += s.empty() ? "Cyf_PreInfer_RootReady" : ",Cyf_PreInfer_RootReady";
        if (!CyfRootHint::state().enabled.load()) s += s.empty() ? "Cyf_Root_Hint" : ",Cyf_Root_Hint";
        if (!CyfRootHint::state().require_key.load()) s += s.empty() ? "Cyf_Root_Hint_RequireKey" : ",Cyf_Root_Hint_RequireKey";
        return s;
    }

    bool job(const CyfAutoHint::Request& r, CyfAutoHint::Report& rep, AutoPayload& p, const std::function<bool()>& cancelled) {
        using CyfAutoHint::now_us; using CyfAutoHint::Final;
        const uint64_t c0 = auto_thread_cycles(); const int64_t t0 = now_us();
        StateInfo si; Position pos;
        if (pos.set(r.sfen, &si)) { rep.final = Final::snapshot_error; rep.why = "sfen"; return false; }
        if (uint64_t(pos.key()) != r.key) { rep.final = Final::snapshot_error; rep.why = "key"; return false; }
        const int64_t t1 = now_us(); rep.snapshot_us = t1 - t0;
        const MoveList<LEGAL_ALL> ml(pos);
        const size_t n = ml.size();
        if (!n) { rep.final = Final::snapshot_error; rep.why = "no_legal"; return false; }
        std::vector<FusionRaw> raw(n); std::vector<std::string> usi(n); std::vector<char> mated(n);
        int64_t enc = 0; size_t i = 0;
        for (Move m : ml) {
            StateInfo st; pos.do_move(m, st);
            mated[i] = MoveList<LEGAL_ALL>(pos).size() == 0;
            const int64_t a = now_us(); fusion_encode(pos, raw[i]); enc += now_us() - a;
            pos.undo_move(m);
            usi[i] = m.to_usi_string(); ++i;
        }
        const int64_t t2 = now_us();
        rep.encode_us = enc; rep.children_us = (t2 - t1) - enc; rep.rows = int(n);
        rep.mated = int(std::count(mated.begin(), mated.end(), 1));
        if (cancelled()) { rep.final = Final::terminated; rep.why = "before_infer"; return false; }
        std::vector<float> out(n);
        CyfOrtRunOptions* ro = backend.make_ro();
        if (!ro) { rep.final = Final::run_error; rep.why = "run_options"; return false; }
        core.set_canceller([this, ro] { backend.terminate(ro); });
        std::string why_run;
        const bool ok = backend.run(raw.data(), n, out.data(), ro, why_run);
        core.set_canceller(nullptr);
        backend.release_ro(ro);
        const int64_t t3 = now_us(); rep.infer_us = t3 - t2;
        if (!ok) {
            rep.run_terminated = cancelled();
            rep.final = rep.run_terminated ? Final::terminated : Final::run_error; rep.why = why_run;
            rep.worker_cycles = auto_thread_cycles() - c0; return false;
        }
        for (int d = r.test_delay_ms; d > 0 && !cancelled(); --d)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));   
        const int64_t t4 = now_us(); rep.delay_us = t4 - t3;
        if (cancelled()) { rep.final = Final::terminated; rep.why = "after_infer"; rep.worker_cycles = auto_thread_cycles() - c0; return false; }
        std::vector<size_t> idx(n);
        for (size_t k = 0; k < n; ++k) idx[k] = k;
        std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return usi[a] < usi[b]; });
        auto& msg = p.msg;
        msg.gen = r.gen; msg.key = r.key; msg.has_key = true; msg.apply = 1; msg.kind = "value";
        msg.model = backend.tag; msg.native = true;
        const bool dump = r.dump;
        uint64_t h = 14695981039346656037ULL;
        std::string body;
        for (size_t k : idx) {
            const double v = mated[k] ? std::numeric_limits<double>::infinity() : -double(out[k]);
            int s;
            if (mated[k]) s = 1000000;
            else s = int(std::clamp(std::nearbyint(v * 1000.0), -1000000.0, 1000000.0));   
            msg.moves.emplace_back(usi[k], s);
            const std::string piece = (body.empty() ? "" : " ") + usi[k] + " " + std::to_string(s);
            for (char c : piece) h = (h ^ uint8_t(c)) * 1099511628211ULL;
            body += piece;
            if (dump) {
                char f[40]; if (mated[k]) std::snprintf(f, sizeof f, "inf"); else std::snprintf(f, sizeof f, "%.9g", v);
                rep.dump += " " + usi[k] + " " + std::to_string(s) + " " + f;
            }
        }
        rep.body_fnv = h;
        msg.recv_us = CyfRootHint::now_us();
        rep.post_us = now_us() - t4;
        rep.worker_cycles = auto_thread_cycles() - c0;
        return true;
    }

    void start_worker() {
        core.start([owner = shared_from_this()](const CyfAutoHint::Request& r, CyfAutoHint::Report& rep, AutoPayload& p,
                          const std::function<bool()>& cancelled) { return owner->job(r, rep, p, cancelled); },
                   [](AutoPayload&& p, const CyfAutoHint::Request&) {
                       p.msg.recv_us = CyfRootHint::now_us();
                       CyfRootHint::deliver(std::move(p.msg));
                   });
    }

    bool warmup(std::string& w) {
        StateInfo si; Position pos;
        if (pos.set(StartSFEN, &si)) { w = "warmup_sfen"; return false; }
        std::vector<FusionRaw> raw;
        for (Move m : MoveList<LEGAL_ALL>(pos)) { StateInfo st; pos.do_move(m, st); raw.emplace_back(); fusion_encode(pos, raw.back()); pos.undo_move(m); }
        std::vector<float> out(raw.size());
        for (int k = 0; k < 2; ++k) {
            const int64_t t = CyfAutoHint::now_us();
            if (!backend.run(raw.data(), raw.size(), out.data(), nullptr, w)) return false;
            (k ? warm2_ms : warm1_ms) = (CyfAutoHint::now_us() - t) / 1000.0;
            warm_rows_total += raw.size();
        }
        warm_rows = int(raw.size());
        return true;
    }

    void prepare_isready() {
        auto& c = auto_config();
        const bool requested = c.requested.load();
        if (core.detached()) { status = "error"; why = "worker_stuck"; print_ready(); return; }
        if (!requested && !open_ok && !core.running_thread()) return;   
        if (requested && (!open_ok || open_version != c.version.load())) {
            if (core.running_thread() && !core.shutdown(2000)) { status = "error"; why = "worker_stuck"; print_ready(); return; }
            backend.close(); open_ok = false;
            const uint64_t v = c.version.load();
            const int64_t t = CyfAutoHint::now_us();
            bool ok = backend.open(c.model, c.ort, why);
            create_ms = (CyfAutoHint::now_us() - t) / 1000.0;
            if (ok) ok = warmup(why);
            if (ok) { start_worker(); open_ok = true; open_version = v; ++opens; status = "ready"; why = "ok"; }
            else { backend.close(); ++open_failures; status = "error"; }
        } else if (!requested) status = open_ok ? "idle_session" : status;
        else status = "ready";
        print_ready();
    }
    void print_ready() {
        const auto& c = auto_config();
        sync_cout << "info string cyfauto_ready v=1 requested=" << int(c.requested.load()) << " status=" << status
                  << " why=" << why << " library=" << (backend.library.empty() ? "none" : backend.library)
                  << " ort_version=" << (backend.version.empty() ? "none" : backend.version)
                  << " model=" << c.model << " tag=" << (backend.tag.empty() ? "none" : backend.tag)
                  << " provider=cpu intra=" << AutoBackend::kIntra << " inter=" << AutoBackend::kInter
                  << " execution=sequential graph_opt=all intra_spin=0 create_ms=" << create_ms
                  << " warm_rows=" << warm_rows << " warm1_ms=" << warm1_ms << " warm2_ms=" << warm2_ms
                  << " opens=" << opens << " open_failures=" << open_failures << " config_version=" << c.version.load()
                  << " worker=" << int(core.running_thread()) << " conflicts=" << (conflicts().empty() ? "none" : conflicts())
                  << sync_endl;
    }

    void begin_go(uint64_t gen, uint64_t key, size_t threads, int multipv, bool ponder, bool all_root_moves) {
        go = Go{}; go.gen = gen; go.key = key; go.t0_us = CyfAutoHint::now_us(); go.search_cycles0 = auto_thread_cycles();
        go.enabled = auto_config().requested.load();
        if (!go.enabled) { go.status = "off"; return; }
        go.status = "skipped";
        if (core.detached()) go.reason = "worker_stuck";
        else if (!open_ok || open_version != auto_config().version.load() || !core.running_thread()) go.reason = "not_ready";
        else if (const std::string c = conflicts(); !c.empty()) go.reason = "config_conflict:" + c;
        else if (ponder) go.reason = "ponder_unsupported";
        else if (threads != 1) go.reason = "threads_unsupported";
        else if (multipv != 1) go.reason = "multipv_unsupported";
        else if (!all_root_moves) go.reason = "searchmoves_unsupported";
        else { go.status = "armed"; go.reason = "no_root_ready"; core.begin_go(gen, key); }
    }
    void root_ready(const Position& pos, uint64_t gen, const CyfPreInfer::BudgetResult& b) {
        if (std::strcmp(go.status, "armed") != 0 || gen != go.gen) return;
        go.status = "skipped";
        if (b.status == CyfPreInfer::BudgetStatus::insufficient) { go.reason = "budget_insufficient"; return; }
        if (b.status == CyfPreInfer::BudgetStatus::unknown) { go.reason = "budget_unknown"; return; }
        CyfAutoHint::Request r; r.gen = gen; r.key = uint64_t(pos.key()); r.ply = pos.game_ply(); r.sfen = pos.sfen();
        r.test_delay_ms = auto_config().test_delay_ms.load(); r.dump = auto_config().dump.load();
        r.t_request_us = CyfAutoHint::now_us();
        uint64_t id = 0;
        const auto s = core.submit(std::move(r), &id);
        if (s == CyfAutoHint::Submit::queued) { go.status = "requested"; go.reason = "root_ready"; go.id = id; }
        else go.reason = s == CyfAutoHint::Submit::busy ? "busy" : "not_ready";
    }
    void end_go(uint64_t gen) {
        if (!go.enabled || gen != go.gen) return;
        if (std::strcmp(go.status, "armed") == 0) go.status = "skipped";
        go.search_cycles = auto_thread_cycles() - go.search_cycles0;
        core.end_go(gen);
        go.end_us = CyfAutoHint::now_us();
        auto& s = CyfRootHint::state();
        std::lock_guard<std::mutex> lk(s.mtx);
        if (s.pending && s.msg.native && s.msg.gen == gen) { s.pending = false; ++native_uncollected; }
    }
    void print_go(bool muted) {
        if (!go.enabled || muted) return;
        auto ms = [&](int64_t t) { return t < 0 ? std::string("na") : std::to_string((t - go.t0_us) / 1000.0); };
        auto us = [](int64_t v) { return v < 0 ? std::string("na") : std::to_string(v); };
        const auto rep = go.id ? core.report(go.id) : std::nullopt;
        const auto prev = prev_id ? core.report(prev_id) : std::nullopt;
        const auto t = core.totals();
        std::ostringstream o;
        o << "info string cyfauto v=1 gen=" << go.gen << " key=" << go.key << " status=" << go.status << " reason=" << go.reason
          << " id=" << go.id << " epoch=" << core.epoch() << " config_version=" << open_version;
        if (rep) {
            const bool fin = rep->final != CyfAutoHint::Final::none;
            o << " state_at_end=" << (fin ? "finished" : rep->t_start_us >= 0 ? "running" : "waiting")
              << " final=" << CyfAutoHint::name(rep->final) << " why=" << (rep->why.empty() ? "none" : rep->why)
              << " rows=" << rep->rows << " mated=" << rep->mated
              << " t_request_ms=" << ms(rep->t_request_us) << " t_start_ms=" << ms(rep->t_start_us)
              << " t_done_ms=" << ms(rep->t_done_us) << " t_publish_ms=" << ms(rep->t_publish_us)
              << " t_cancel_ms=" << ms(rep->t_cancel_us) << " t_go_end_ms=" << ms(go.end_us)
              << " snapshot_us=" << us(rep->snapshot_us) << " children_us=" << us(rep->children_us)
              << " encode_us=" << us(rep->encode_us) << " infer_us=" << us(rep->infer_us)
              << " delay_us=" << us(rep->delay_us) << " post_us=" << us(rep->post_us)
              << " cancel_requested=" << int(rep->cancel_requested) << " canceller_called=" << int(rep->canceller_called)
              << " run_terminated=" << int(rep->run_terminated) << " worker_cycles=" << rep->worker_cycles
              << " body_fnv=" << rep->body_fnv;
        }
        o << " search_cycles=" << go.search_cycles << " prev_id=" << prev_id
          << " prev_final=" << (prev ? CyfAutoHint::name(prev->final) : "none")
          << " prev_done_after_cancel_ms=" << (prev && prev->t_cancel_us >= 0 && prev->t_done_us >= 0
                                                  ? std::to_string((prev->t_done_us - prev->t_cancel_us) / 1000.0) : std::string("na"))
          << " tot_submitted=" << t.submitted << " tot_published=" << t.published << " tot_terminated=" << t.terminated
          << " tot_expired=" << t.expired << " tot_gen_changed=" << t.gen_changed << " tot_epoch_changed=" << t.epoch_changed
          << " tot_busy=" << t.skipped_busy << " tot_dropped_unstarted=" << t.dropped_unstarted << " tot_replaced=" << t.replaced
          << " tot_run_error=" << t.run_error << " tot_snapshot_error=" << t.snapshot_error << " tot_duplicate=" << t.duplicate
          << " tot_cancel_requests=" << t.cancel_requests << " tot_canceller_calls=" << t.canceller_calls
          << " tot_finished_after_go_end=" << t.finished_after_go_end << " native_uncollected=" << native_uncollected
          << " worker=" << int(core.running_thread()) << " busy_now=" << int(core.busy());
        sync_cout << o.str() << sync_endl;
        if (go.id) prev_id = go.id;
    }
    void dump_after_bestmove(bool muted) {
        if (!go.enabled || muted || !go.id || !auto_config().dump.load()) return;
        const auto rep = core.report(go.id);
        if (!rep || rep->dump.empty()) return;
        sync_cout << "info string cyfauto_body v=1 gen=" << go.gen << " key=" << go.key << " id=" << go.id
                  << " final=" << CyfAutoHint::name(rep->final) << " n=" << rep->rows << " order=usi_asc body_fnv=" << rep->body_fnv
                  << " fields=usi,score,view moves" << rep->dump << sync_endl;
    }
    void invalidate() { if (core.running_thread()) core.invalidate(); }
    void shutdown(bool* printed) {
        if (quit_done) return;
        quit_done = true;
        int64_t waited = 0;
        const bool had = core.running_thread();
        const bool joined = core.shutdown(2000, &waited);
        if (joined) backend.close();
        if (printed && (had || open_ok)) {
            sync_cout << "info string cyfauto_quit v=1 had_worker=" << int(had) << " joined=" << int(joined)
                      << " wait_us=" << waited << " released=" << int(joined) << " detached=" << int(core.detached()) << sync_endl;
            *printed = true;
        }
        open_ok = false;
    }
};
struct AutoServiceOwner {
    std::shared_ptr<AutoService> service = std::make_shared<AutoService>();
    ~AutoServiceOwner() { service->shutdown(nullptr); }
};
inline AutoService& auto_service() { static AutoServiceOwner owner; return *owner.service; }
