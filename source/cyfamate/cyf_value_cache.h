struct BlendTable {
    uint64_t writes=0, evictions=0, duplicates=0;
    struct Slot {
        std::atomic<uint64_t> key{0};
        std::atomic<float> logit{0.f};
        std::atomic<bool> valid{false};
    };
    std::unique_ptr<Slot[]> slots;
    size_t mask=0, count=0;
    void resize(size_t mb) {
        size_t n=2;
        while (n*2*sizeof(Slot) <= mb*1024*1024) n*=2;
        if (n!=count) { slots.reset(new Slot[n]); count=n; mask=n-1; }
        clear();
    }
    void clear() {
        for (size_t i=0; i<count; ++i) slots[i].valid.store(false, std::memory_order_relaxed);
    }
    bool get(uint64_t k, float& v) const {
        if (!count) return false;
        const size_t i=size_t(k)&mask;
        for (size_t j : {i, i^1}) {
            const Slot& s=slots[j];
            if (s.valid.load(std::memory_order_relaxed) && s.key.load(std::memory_order_relaxed)==k) {
                v=s.logit.load(std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }
    void put(uint64_t k, float v) {
        if (!count) return;
        const size_t i=size_t(k)&mask;
        Slot* target=nullptr;
        for (size_t j : {i, i^1}) {
            Slot& s=slots[j];
            if (!s.valid.load(std::memory_order_relaxed)) {
                if (!target) target=&s;
            } else if (s.key.load(std::memory_order_relaxed)==k) {
                ++duplicates;
                return;
            }
        }
        if (!target) { target=&slots[i]; ++evictions; }
        ++writes;
        target->key.store(k, std::memory_order_relaxed);
        target->logit.store(v, std::memory_order_relaxed);
        target->valid.store(true, std::memory_order_relaxed);
    }
    size_t occupied() const {
        size_t c=0;
        for (size_t i=0; i<count; ++i) c+=slots[i].valid.load(std::memory_order_relaxed);
        return c;
    }
};
