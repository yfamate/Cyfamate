
struct DlRow { uint8_t b[96]; };
static_assert(sizeof(DlRow) == 96, "the encoded row is exactly the model input bytes");

struct DlSha256 {
    uint32_t h[8]{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    uint8_t buf[64]{}; size_t fill = 0; uint64_t bits = 0;
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void block(const uint8_t* p) {
        static const uint32_t k[64]={
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = uint32_t(p[4*i])<<24 | uint32_t(p[4*i+1])<<16 | uint32_t(p[4*i+2])<<8 | p[4*i+3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
            const uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t t1 = hh + (rotr(e,6) ^ rotr(e,11) ^ rotr(e,25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const uint32_t t2 = (rotr(a,2) ^ rotr(a,13) ^ rotr(a,22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    void update(const void* data, size_t n) {
        const uint8_t* p = static_cast<const uint8_t*>(data); bits += uint64_t(n) * 8;
        while (n) {
            const size_t take = std::min(n, size_t(64) - fill);
            std::memcpy(buf + fill, p, take); fill += take; p += take; n -= take;
            if (fill == 64) { block(buf); fill = 0; }
        }
    }
    std::string hex() {
        const uint64_t total = bits; const uint8_t one = 0x80, zero = 0;
        update(&one, 1);
        while (fill != 56) update(&zero, 1);
        uint8_t len[8]; for (int i = 0; i < 8; ++i) len[i] = uint8_t(total >> (56 - 8*i));
        update(len, 8);
        static const char* x = "0123456789abcdef"; std::string out;
        for (uint32_t v : h) for (int s = 28; s >= 0; s -= 4) out += x[(v >> s) & 15];
        return out;
    }
    static std::string file(const std::string& path, uint64_t* bytes = nullptr) {
        FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return {};
        DlSha256 s; std::vector<uint8_t> chunk(65536); size_t n; uint64_t total = 0;
        while ((n = std::fread(chunk.data(), 1, chunk.size(), f))) { s.update(chunk.data(), n); total += n; }
        const bool ok = !std::ferror(f); std::fclose(f);
        if (bytes) *bytes = total;
        return ok ? s.hex() : std::string();
    }
};

class DlValueCache {
public:
    static constexpr size_t kMaxCompletedBytes = size_t(8) << 20;
    static constexpr size_t kMaxInflight = 256, kMaxWaiters = 512, kWays = 4;
    static constexpr uint32_t kNone = 0xffffffffu;
    enum class Result : uint8_t { Hit, Joined, Created, InflightFull, WaiterFull, Miss };
    enum class Claim : uint8_t { Run, Cancelled, Stale };
    enum Src : uint8_t { SRC_NEW = 0, SRC_HIT = 1, SRC_JOIN = 2, SRC_UNTRACKED = 3, SRC_NB = 4 };
    struct Waiter {          
        uint64_t go = 0, key = 0, req = 0; int64_t deadline_us = 0;   
        uint32_t owner = 0, next = kNone; uint8_t src = SRC_NEW, cancelled = 0;
    };
    struct Job { uint32_t idx = kNone, seq = 0; };
    struct Delivery { Waiter w; float logit; };
    struct Stats {           
        uint64_t hits = 0, joins = 0, created = 0, inflight_full = 0, waiter_full = 0;
        uint64_t claim_run = 0, claim_cancelled = 0, claim_stale = 0;
        uint64_t done_ok = 0, done_fail = 0, done_nonfinite = 0, done_stale = 0;
        uint64_t stored = 0, store_present = 0, evictions = 0, untracked_stored = 0;
        uint64_t delivered_candidates = 0, rej_dead = 0, rej_cancelled = 0, waiters_expired = 0, waiters_cancelled = 0;
        uint64_t identity_changes = 0, clears = 0, lock_contended = 0;
    };

    explicit DlValueCache(size_t completed_bytes = kMaxCompletedBytes) { allocate(completed_bytes); }

    static uint64_t digest(uint64_t seed, const DlRow& r) {
        uint64_t h = seed ^ 0x9E3779B97F4A7C15ull;
        for (int i = 0; i < 12; ++i) {
            uint64_t w; std::memcpy(&w, r.b + 8 * i, 8);
            h ^= w * 0xff51afd7ed558ccdull; h = (h << 31 | h >> 33) * 0xc4ceb9fe1a85ec53ull;
        }
        h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33;
        return h ? h : 1;   
    }
    static uint64_t seed_of(const std::string& s) {
        uint64_t h = 14695981039346656037ull;
        for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
        return h;
    }

    bool set_identity(const std::string& identity) {
        std::lock_guard<std::mutex> lk(m);
        if (identity == ident) return false;
        ident = identity; seed = seed_of(identity); ++idgen; ++st.identity_changes;
        clear_completed_locked(); drop_inflight_locked();
        return true;
    }
    const std::string& identity() const { return ident; }
    uint64_t namespace_seed() const { return seed; }
    uint32_t identity_generation() const { std::lock_guard<std::mutex> lk(m); return idgen; }
    void clear_completed() { std::lock_guard<std::mutex> lk(m); clear_completed_locked(); ++st.clears; }
    void reset_inflight() { std::lock_guard<std::mutex> lk(m); drop_inflight_locked(); }

    bool maybe(uint64_t hint) const {
        const uint64_t bit = mix(hint) & filter_mask;
        return (filter[bit >> 6].load(std::memory_order_relaxed) >> (bit & 63)) & 1;
    }

    Result request(const DlRow& row, uint64_t dg, uint64_t hint, const Waiter& who, float& logit, Job& job,
                   bool allow_create = true) {
        std::unique_lock<std::mutex> lk(m, std::try_to_lock);
        if (!lk.owns_lock()) { lk.lock(); ++st.lock_contended; }
        if (Slot* s = find_completed(dg, row)) { logit = s->logit; s->stamp = ++clock; ++st.hits; return Result::Hit; }
        const uint32_t f = find_inflight(dg, row);
        if (f != kNone) {
            if (!add_waiter(f, who, SRC_JOIN)) { ++st.waiter_full; return Result::WaiterFull; }
            ++st.joins; job = {f, flight[f].seq}; return Result::Joined;
        }
        if (!allow_create) return Result::Miss;
        uint32_t freei = kNone;
        for (uint32_t i = 0; i < kMaxInflight; ++i) if (flight[i].state == FREE) { freei = i; break; }
        if (freei == kNone) { ++st.inflight_full; return Result::InflightFull; }
        if (free_waiter == kNone) { ++st.waiter_full; return Result::WaiterFull; }
        Flight& x = flight[freei];
        x.state = QUEUED; x.digest = dg; x.row = row; x.hint = hint; x.idgen = idgen; x.seq = ++seqctr;
        x.head = kNone; x.waiters = 0;
        add_waiter(freei, who, SRC_NEW); ++inflight_n; ++st.created;
        set_filter(hint);
        job = {freei, x.seq}; return Result::Created;
    }
    template<class Live> Claim claim(Job j, Live&& live) {
        std::lock_guard<std::mutex> lk(m);
        if (!valid_job(j) || flight[j.idx].state != QUEUED) { ++st.claim_stale; return Claim::Stale; }
        for (uint32_t w = flight[j.idx].head; w != kNone; w = wait[w].next)
            if (!wait[w].cancelled && live(wait[w])) { flight[j.idx].state = RUNNING; ++st.claim_run; return Claim::Run; }
        ++st.claim_cancelled; free_job_locked(j.idx, true); return Claim::Cancelled;
    }
    void unclaim(Job j) {
        std::lock_guard<std::mutex> lk(m);
        if (valid_job(j)) { ++st.claim_cancelled; free_job_locked(j.idx, true); }
    }
    template<class Live> void complete(Job j, bool ok, float logit, Live&& live, std::vector<Delivery>& out) {
        std::lock_guard<std::mutex> lk(m);
        if (!valid_job(j)) { ++st.done_stale; return; }   
        Flight& x = flight[j.idx];
        const bool good = ok && std::isfinite(logit);
        if (!ok) ++st.done_fail; else if (!good) ++st.done_nonfinite; else ++st.done_ok;
        if (good) store_locked(x.digest, x.row, x.hint, logit);
        for (uint32_t w = x.head; w != kNone; w = wait[w].next) {
            const Waiter& a = wait[w];
            if (a.cancelled) { ++st.rej_cancelled; continue; }
            if (!good || !live(a)) { ++st.rej_dead; continue; }
            out.push_back({a, logit}); ++st.delivered_candidates;
        }
        free_job_locked(j.idx, false);
    }
    void store_untracked(const DlRow& row, uint64_t dg, uint64_t hint, bool ok, float logit, uint32_t id_gen) {
        std::lock_guard<std::mutex> lk(m);
        if (id_gen != idgen || !ok || !std::isfinite(logit)) return;
        ++st.untracked_stored; store_locked(dg, row, hint, logit);
    }
    bool cancel_waiter(uint32_t owner, uint64_t req) {
        std::lock_guard<std::mutex> lk(m);
        for (uint32_t i = 0; i < kMaxInflight; ++i) if (flight[i].state != FREE)
            for (uint32_t w = flight[i].head; w != kNone; w = wait[w].next)
                if (wait[w].owner == owner && wait[w].req == req && !wait[w].cancelled) {
                    wait[w].cancelled = 1; ++st.waiters_cancelled; return true;
                }
        return false;
    }
    template<class Dead> size_t expire(Dead&& dead) {
        std::lock_guard<std::mutex> lk(m); size_t n = 0;
        for (uint32_t i = 0; i < kMaxInflight; ++i) {
            if (flight[i].state == FREE) continue;
            uint32_t* link = &flight[i].head;
            while (*link != kNone) {
                const uint32_t w = *link;
                const bool cancelled = wait[w].cancelled;
                if (cancelled || dead(wait[w])) {
                    *link = wait[w].next; release_waiter(w); --flight[i].waiters; n += !cancelled;
                } else link = &wait[w].next;
            }
        }
        st.waiters_expired += n; return n;
    }

    struct Snapshot { Stats st; size_t completed = 0, inflight = 0, waiters = 0; };
    Snapshot snapshot() const {
        std::lock_guard<std::mutex> lk(m);
        Snapshot s; s.st = st; s.completed = completed_n; s.inflight = inflight_n; s.waiters = waiters_n; return s;
    }
    size_t slot_count() const { return slots.size(); }
    size_t completed_bytes() const { return slots.capacity() * sizeof(Slot) + filter_words * sizeof(uint64_t); }
    static constexpr size_t inflight_bytes() { return kMaxInflight * sizeof(Flight); }
    static constexpr size_t waiter_bytes() { return kMaxWaiters * sizeof(Waiter); }
    std::vector<std::pair<DlRow, float>> dump(size_t n) const {
        std::lock_guard<std::mutex> lk(m); std::vector<std::pair<DlRow, float>> out;
        for (size_t i = 0; i < slots.size() && out.size() < n; ++i)
            if (slots[i].digest) out.push_back({slots[i].row, slots[i].logit});
        return out;
    }

private:
    struct Slot { uint64_t digest = 0; uint32_t stamp = 0; float logit = 0; DlRow row{}; };
    static_assert(sizeof(Slot) == 112, "slot accounting");
    enum State : uint8_t { FREE = 0, QUEUED = 1, RUNNING = 2 };
    struct Flight { uint64_t digest = 0, hint = 0; uint32_t idgen = 0, seq = 0, head = kNone, waiters = 0; uint8_t state = FREE; DlRow row{}; };

    mutable std::mutex m;
    std::vector<Slot> slots; size_t bucket_mask = 0, completed_n = 0; uint32_t clock = 0;
    std::unique_ptr<std::atomic<uint64_t>[]> filter; size_t filter_words = 0; uint64_t filter_mask = 0;
    std::array<Flight, kMaxInflight> flight{}; std::array<Waiter, kMaxWaiters> wait{};
    uint32_t free_waiter = kNone, seqctr = 0; size_t inflight_n = 0, waiters_n = 0;
    std::string ident; uint64_t seed = 0; uint32_t idgen = 1;
    Stats st;

    static uint64_t mix(uint64_t x) { x ^= x >> 31; x *= 0x7fb5d329728ea185ull; x ^= x >> 27; x *= 0x81dadef4bc2dd44dull; return x ^ (x >> 33); }
    void allocate(size_t bytes) {
        bytes = std::min(bytes, kMaxCompletedBytes);
        size_t bits = 4096; while (bits * 2 <= bytes / 4) bits *= 2;
        filter_words = bits / 64; filter_mask = bits - 1;
        const size_t left = bytes > filter_words * 8 ? bytes - filter_words * 8 : 0;
        size_t buckets = 1; while (buckets * 2 * kWays * sizeof(Slot) <= left) buckets *= 2;
        if (buckets * kWays * sizeof(Slot) > left) buckets = 0;
        slots.assign(buckets * kWays, Slot{}); slots.shrink_to_fit(); bucket_mask = buckets ? buckets - 1 : 0;
        filter.reset(new std::atomic<uint64_t>[filter_words]);
        for (size_t i = 0; i < filter_words; ++i) filter[i].store(0, std::memory_order_relaxed);
        free_waiter = kNone;
        for (uint32_t i = kMaxWaiters; i-- > 0;) { wait[i] = Waiter{}; wait[i].next = free_waiter; free_waiter = i; }
    }
    void set_filter(uint64_t hint) {
        const uint64_t bit = mix(hint) & filter_mask;
        filter[bit >> 6].fetch_or(uint64_t(1) << (bit & 63), std::memory_order_relaxed);
    }
    void clear_completed_locked() {
        for (auto& s : slots) s = Slot{};
        completed_n = 0;
        for (size_t i = 0; i < filter_words; ++i) filter[i].store(0, std::memory_order_relaxed);
        for (const auto& f : flight) if (f.state != FREE) set_filter(f.hint);   
    }
    void drop_inflight_locked() {
        for (uint32_t i = 0; i < kMaxInflight; ++i) if (flight[i].state != FREE) free_job_locked(i, true);
    }
    Slot* find_completed(uint64_t dg, const DlRow& r) {
        if (slots.empty()) return nullptr;
        const size_t b = size_t(dg & bucket_mask) * kWays;
        for (size_t w = 0; w < kWays; ++w) {
            Slot& s = slots[b + w];
            if (s.digest == dg && std::memcmp(s.row.b, r.b, sizeof r.b) == 0) return &s;
        }
        return nullptr;
    }
    uint32_t find_inflight(uint64_t dg, const DlRow& r) const {
        for (uint32_t i = 0; i < kMaxInflight; ++i)
            if (flight[i].state != FREE && flight[i].digest == dg && std::memcmp(flight[i].row.b, r.b, sizeof r.b) == 0) return i;
        return kNone;
    }
    void store_locked(uint64_t dg, const DlRow& r, uint64_t hint, float logit) {
        if (slots.empty()) return;
        if (find_completed(dg, r)) { ++st.store_present; return; }   
        const size_t b = size_t(dg & bucket_mask) * kWays;
        Slot* target = nullptr;
        for (size_t w = 0; w < kWays; ++w) if (!slots[b + w].digest) { target = &slots[b + w]; break; }
        if (!target) {
            target = &slots[b];
            for (size_t w = 1; w < kWays; ++w) if (slots[b + w].stamp < target->stamp) target = &slots[b + w];
            ++st.evictions;
        } else ++completed_n;
        target->digest = dg; target->row = r; target->logit = logit; target->stamp = ++clock;
        set_filter(hint); ++st.stored;
    }
    bool valid_job(Job j) const {
        return j.idx < kMaxInflight && flight[j.idx].state != FREE && flight[j.idx].seq == j.seq && flight[j.idx].idgen == idgen;
    }
    bool add_waiter(uint32_t f, const Waiter& who, uint8_t src) {
        if (free_waiter == kNone) return false;
        const uint32_t w = free_waiter; free_waiter = wait[w].next;
        wait[w] = who; wait[w].src = src; wait[w].cancelled = 0;
        wait[w].next = flight[f].head; flight[f].head = w; ++flight[f].waiters; ++waiters_n;
        return true;
    }
    void release_waiter(uint32_t w) { wait[w] = Waiter{}; wait[w].next = free_waiter; free_waiter = w; --waiters_n; }
    void free_job_locked(uint32_t i, bool count_dead) {
        for (uint32_t w = flight[i].head; w != kNone;) {
            const uint32_t nx = wait[w].next;
            if (count_dead && !wait[w].cancelled) ++st.rej_dead;
            release_waiter(w); w = nx;
        }
        flight[i] = Flight{}; --inflight_n;
    }
};
