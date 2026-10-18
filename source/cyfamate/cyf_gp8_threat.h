#pragma once
#if defined(__AVX512VBMI__) && !defined(CYF_THREAT_ENUM_NO_V8) && !defined(CYF_THREAT_ENUM_V5)
#define CYF_THREAT_ENUM_V8 1
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace YaneuraOu {
namespace Cyfamate {
namespace Gp8 {

constexpr int kMaxStacks = 9;
inline int stacks = 5;
inline double bounds[kMaxStacks - 1] = {0.4, 0.6, 0.8, 1.0, 0.0, 0.0, 0.0, 0.0};
inline std::string arch_string;          
inline std::uint32_t top_hash = 0;       
inline bool threat_enabled = false;      
inline bool fixed_gp5 = true;            
inline bool hm = false;
constexpr int kHmRows = 45 * 1629;

struct LoadedState {
    int stacks_;
    double bounds_[kMaxStacks - 1];
    std::string arch_string_;
    std::uint32_t top_hash_;
    bool threat_enabled_, fixed_gp5_, hm_;

    static LoadedState save() {
        LoadedState s{stacks, {}, arch_string, top_hash, threat_enabled, fixed_gp5, hm};
        std::copy(std::begin(bounds), std::end(bounds), s.bounds_);
        return s;
    }
    void restore() const {
        stacks = stacks_;
        std::copy(std::begin(bounds_), std::end(bounds_), bounds);
        arch_string = arch_string_;
        top_hash = top_hash_;
        threat_enabled = threat_enabled_;
        fixed_gp5 = fixed_gp5_;
        hm = hm_;
    }
};

constexpr std::uint32_t kTagGp5 = 0x35475000u;    
constexpr std::uint32_t kTagGpk = 0x004B5047u;    
constexpr std::uint32_t kTagH15 = 0x35314831u;    
constexpr std::uint32_t kTagThreat = 0x54524854u; 
constexpr std::uint32_t kTagHm = 0x00004D48u;     

struct ParsedArch {
    int ft_out = 0, h1 = 0, h2 = 0, stacks = 0;
    std::vector<double> bounds;
    bool threat = false, mirror_off = false, hm = false;
    int threat_dims = 0, threat_profile = 0;
    int l1_main_scale = 0;      
    bool ft_scale_254 = false;  
    std::string error;
};

inline bool parse_number(const std::string& s, std::size_t& p, int& out) {
    std::size_t q = p;
    while (q < s.size() && s[q] >= '0' && s[q] <= '9') ++q;
    if (q == p) return false;
    out = std::stoi(s.substr(p, q - p));
    p = q;
    return true;
}

inline ParsedArch parse_arch(const std::string& a) {
    ParsedArch r;
    const std::string prefix = "ModelType=SFNNWithoutPsqt;Features=HalfKA2(Friend)[131949->";
    const std::string prefix_hm = "ModelType=SFNNWithoutPsqt;Features=HalfKA2_hm(Friend)[73305->";
    std::size_t p;
    if (a.compare(0, prefix.size(), prefix) == 0) p = prefix.size();
    else if (a.compare(0, prefix_hm.size(), prefix_hm) == 0) { r.hm = true; p = prefix_hm.size(); }
    else { r.error = "not a Cyfamate HalfKA2 file"; return r; }
    if (!parse_number(a, p, r.ft_out) || a.compare(p, 3, "x2]") != 0) { r.error = "bad Features token"; return r; }
    p += 3;
    const std::size_t net = a.find(",Network=", p);
    if (net == std::string::npos) { r.error = "no Network token"; return r; }
    std::size_t t = p;
    while (t < net) {
        if (a[t] != ',') { r.error = "bad token separator"; return r; }
        ++t;
        std::size_t e = a.find(',', t);
        if (e == std::string::npos || e > net) e = net;
        const std::string tok = a.substr(t, e - t);
        if (tok.compare(0, 7, "Threat=") == 0) { r.threat = true; r.threat_dims = std::stoi(tok.substr(7)); }
        else if (tok.compare(0, 14, "ThreatProfile=") == 0) r.threat_profile = std::stoi(tok.substr(14));
        else if (tok == "ThreatMirror=off") r.mirror_off = true;
        else if (tok == "ThreatMirror=hm") r.mirror_off = false;
        else { r.error = "unknown token " + tok; return r; }
        t = e;
    }
    p = net + 9;
    const std::string nprefix = "SFNN_HALFKA2_";
    if (a.compare(p, nprefix.size(), nprefix) != 0) { r.error = "bad Network name"; return r; }
    p += nprefix.size();
    int w2 = 0;
    if (!parse_number(a, p, w2) || a[p] != '_') { r.error = "bad Network width"; return r; }
    ++p;
    if (!parse_number(a, p, r.h1) || a[p] != '_') { r.error = "bad Network h1"; return r; }
    ++p;
    if (!parse_number(a, p, r.h2) || a[p] != '_') { r.error = "bad Network h2"; return r; }
    ++p;
    int n_name = 0;
    if (!parse_number(a, p, n_name) || a.compare(p, 3, "GP{") != 0) { r.error = "bad Network stacks"; return r; }
    p += 3;
    if (a.compare(p, 11, "LayerStack=") != 0) { r.error = "no LayerStack"; return r; }
    p += 11;
    if (!parse_number(a, p, r.stacks) || a.compare(p, 11, ";Bucket=GP(") != 0) { r.error = "no Bucket=GP("; return r; }
    p += 11;
    const std::size_t close = a.find(')', p);
    if (close == std::string::npos) { r.error = "bad Bucket list"; return r; }
    std::size_t q = close + 1;
    while (q < a.size() && a[q] == ';') {
        std::size_t e = a.find_first_of(";}", q + 1);
        if (e == std::string::npos) { r.error = "bad attribute"; return r; }
        const std::string attr = a.substr(q + 1, e - q - 1);
        if (attr.compare(0, 12, "L1MainScale=") == 0) r.l1_main_scale = std::stoi(attr.substr(12));
        else if (attr == "FTScale=254") r.ft_scale_254 = true;
        else { r.error = "unknown attribute " + attr; return r; }
        q = e;
    }
    if (q + 1 != a.size() || a[q] != '}') { r.error = "bad Network tail"; return r; }
    std::string list = a.substr(p, close - p);
    std::size_t s = 0;
    while (s < list.size()) {
        std::size_t e = list.find(',', s);
        if (e == std::string::npos) e = list.size();
        r.bounds.push_back(std::strtod(list.substr(s, e - s).c_str(), nullptr));
        s = e + 1;
    }
    if (w2 != r.ft_out || n_name != r.stacks || int(r.bounds.size()) + 1 != r.stacks) { r.error = "Network / LayerStack / bounds disagree"; return r; }
    if (r.stacks < 1 || r.stacks > kMaxStacks) { r.error = "unsupported stack count"; return r; }
    for (std::size_t i = 0; i < r.bounds.size(); ++i)
        if (!(r.bounds[i] > 0.0 && r.bounds[i] <= 1.5) || (i > 0 && r.bounds[i] <= r.bounds[i - 1])) { r.error = "bad boundary list"; return r; }
    if (r.threat && (r.threat_dims != 33408 || r.threat_profile != 3)) { r.error = "only step-attacker threat (33408, profile 3) is implemented"; return r; }
    if (r.threat && !r.mirror_off) { r.error = "threat block must be ThreatMirror=off for this engine"; return r; }
    return r;
}

inline bool is_fixed_gp5(const ParsedArch& r) {
    return !r.threat && !r.hm && r.ft_out == 1024 && r.h2 == 64 && (r.h1 == 7 || r.h1 == 15) && r.stacks == 5
        && r.bounds.size() == 4 && r.bounds[0] == 0.4 && r.bounds[1] == 0.6 && r.bounds[2] == 0.8 && r.bounds[3] == 1.0;
}

inline std::uint32_t expected_top_hash(const ParsedArch& r, std::uint32_t base) {
    std::uint32_t h = base ^ kTagGp5;
    if (!is_fixed_gp5(r)) h ^= kTagGpk;
    if (r.h1 == 15) h ^= kTagH15;
    if (r.threat) h ^= kTagThreat;
    if (r.hm) h ^= kTagHm;
    return h;
}

namespace Threat {

constexpr int kDims = 33408;
constexpr int kMaxActive = 128;
constexpr int kClasses = 9;

struct List {
    std::uint32_t v[kMaxActive];
    int n = 0;
    void push(int idx) { if (n < kMaxActive) v[n++] = std::uint32_t(idx); }
};
constexpr int kPatterns = 14;
constexpr int kAttacksPerColor[kClasses] = {72, 324, 112, 328, 416, 816, 1296, 1104, 1552};

#if defined(CYF_EVAL_PROFILE)
struct EnumProf { std::uint64_t collect_cyc = 0, bb_cyc = 0, enum_cyc = 0, index_cyc = 0, prefetch_cyc = 0,
                  calls = 0, changed_sq = 0, cand = 0, full_enum_cyc = 0, full_sort_cyc = 0, full_n = 0; };
inline EnumProf& enum_prof() { static EnumProf p; return p; }
#endif

inline int class_of(PieceType pt) {
    switch (pt) {
    case PAWN: return 0;
    case LANCE: return 1;
    case KNIGHT: return 2;
    case SILVER: return 3;
    case GOLD: case PRO_PAWN: case PRO_LANCE: case PRO_KNIGHT: case PRO_SILVER: return 4;
    case BISHOP: return 5;
    case ROOK: return 6;
    case HORSE: return 7;
    case DRAGON: return 8;
    default: return -1;   
    }
}
inline bool is_step(int cls) { return cls == 0 || cls == 2 || cls == 3 || cls == 4; }
inline bool excluded_attacker(int ac) { return ac == 1 || ac >= 5; }   

struct Tables {
    std::int32_t pair_base[2][kClasses][2][kClasses];
    std::int32_t from_offset[kPatterns][81];
    std::uint8_t attack_order[kPatterns][81][81];
    std::int32_t step_from_offset[8][81];
    std::uint8_t step_order[8][81][24];
    int dims = 0;
};
inline constexpr std::int8_t kStepSlot[kClasses] = {0, -1, 1, 2, 3, -1, -1, -1, -1};

inline void empty_board_targets(int cls, int white, int from, std::vector<int>& out) {
    out.clear();
    const int file = from / 9, rank = from % 9;
    const int fwd = white ? 1 : -1;
    auto step = [&](int df, int dr) {
        const int f = file + df, r = rank + dr;
        if (f >= 0 && f < 9 && r >= 0 && r < 9) out.push_back(f * 9 + r);
    };
    auto ray = [&](int df, int dr) {
        int f = file + df, r = rank + dr;
        while (f >= 0 && f < 9 && r >= 0 && r < 9) { out.push_back(f * 9 + r); f += df; r += dr; }
    };
    switch (cls) {
    case 0: step(0, fwd); break;
    case 1: ray(0, fwd); break;
    case 2: step(-1, 2 * fwd); step(1, 2 * fwd); break;
    case 3: step(-1, fwd); step(0, fwd); step(1, fwd); step(-1, -fwd); step(1, -fwd); break;
    case 4: step(-1, fwd); step(0, fwd); step(1, fwd); step(-1, 0); step(1, 0); step(0, -fwd); break;
    case 5: ray(-1, -1); ray(-1, 1); ray(1, -1); ray(1, 1); break;
    case 6: ray(-1, 0); ray(1, 0); ray(0, -1); ray(0, 1); break;
    case 7: ray(-1, -1); ray(-1, 1); ray(1, -1); ray(1, 1); step(-1, 0); step(1, 0); step(0, -1); step(0, 1); break;
    case 8: ray(-1, 0); ray(1, 0); ray(0, -1); ray(0, 1); step(-1, -1); step(-1, 1); step(1, -1); step(1, 1); break;
    default: break;
    }
    std::sort(out.begin(), out.end());
}

inline int pattern_id(int cls, int oriented_white) { return (oriented_white && cls <= 4) ? kClasses + cls : cls; }

inline const Tables& tables() {
    static const Tables t = [] {
        Tables tb{};
        std::vector<int> targets;
        for (int pat = 0; pat < kPatterns; ++pat) {
            const int cls = pat < kClasses ? pat : pat - kClasses;
            const int white = pat >= kClasses;
            std::memset(tb.attack_order[pat], 0xff, sizeof(tb.attack_order[pat]));
            int cumulative = 0;
            for (int from = 0; from < 81; ++from) {
                tb.from_offset[pat][from] = cumulative;
                empty_board_targets(cls, white, from, targets);
                for (std::size_t k = 0; k < targets.size(); ++k)
                    tb.attack_order[pat][from][targets[k]] = std::uint8_t(k);
                cumulative += int(targets.size());
            }
        }
        int cumulative = 0;
        for (int as = 0; as < 2; ++as)
            for (int ac = 0; ac < kClasses; ++ac)
                for (int ds = 0; ds < 2; ++ds)
                    for (int dc = 0; dc < kClasses; ++dc) {
                        if (excluded_attacker(ac)) { tb.pair_base[as][ac][ds][dc] = -1; continue; }
                        tb.pair_base[as][ac][ds][dc] = cumulative;
                        cumulative += kAttacksPerColor[ac];
                    }
        tb.dims = cumulative;
        std::memset(tb.step_order, 0xff, sizeof(tb.step_order));
        for (int pat = 0; pat < kPatterns; ++pat) {
            const int cls = pat < kClasses ? pat : pat - kClasses;
            if (!is_step(cls)) continue;
            const int slot = (pat >= kClasses ? 4 : 0) + kStepSlot[cls];
            for (int from = 0; from < 81; ++from) {
                tb.step_from_offset[slot][from] = tb.from_offset[pat][from];
                for (int to = 0; to < 81; ++to) {
                    const std::uint8_t ord = tb.attack_order[pat][from][to];
                    if (ord == 0xff) continue;
                    const int d = to - from + 11;
                    tb.step_order[slot][from][d] = ord;   
                }
            }
        }
        return tb;
    }();
    return t;
}

struct Edge {
    std::uint8_t from, to;
    std::int8_t a_class, t_class;
    std::uint8_t a_white, t_white;
};

inline int index_of(const Edge& e, Color perspective) {
    const Tables& t = tables();
    const int pw = perspective == WHITE;
    const int as = e.a_white != pw, ds = e.t_white != pw;
    const int base = t.pair_base[as][e.a_class][ds][e.t_class];
    if (base < 0) return -1;
    const int oriented_white = e.a_white != pw;   
    const int from_n = pw ? 80 - e.from : e.from;
    const int to_n = pw ? 80 - e.to : e.to;
    if (is_step(e.a_class)) {   
        const int slot = (oriented_white ? 4 : 0) + kStepSlot[e.a_class];
        return base + t.step_from_offset[slot][from_n] + t.step_order[slot][from_n][to_n - from_n + 11];
    }
    const int pat = pattern_id(e.a_class, oriented_white);
    const int ord = t.attack_order[pat][from_n][to_n];
    return base + t.from_offset[pat][from_n] + ord;
}

inline Bitboard step_effect(int cls, Color c, Square s) {
    switch (cls) {
    case 0: return pawnEffect(c, s);
    case 2: return knightEffect(c, s);
    case 3: return silverEffect(c, s);
    case 4: return goldEffect(c, s);
    default: return Bitboard(ZERO);
    }
}

template <typename F>
inline void for_each_edge(const Position& pos, F f) {
    const Bitboard targets = pos.pieces() ^ (pos.pieces(BLACK, KING) | pos.pieces(WHITE, KING));
    for (Color c : {BLACK, WHITE}) {
        for (int cls : {0, 2, 3, 4}) {
            Bitboard bb = cls == 0 ? pos.pieces(c, PAWN) : cls == 2 ? pos.pieces(c, KNIGHT)
                        : cls == 3 ? pos.pieces(c, SILVER) : pos.pieces(c, GOLDS);
            while (bb) {
                const Square s = bb.pop();
                Bitboard t = step_effect(cls, c, s) & targets;
                while (t) {
                    const Square to = t.pop();
                    const Piece tp = pos.piece_on(to);
                    f(Edge{std::uint8_t(s), std::uint8_t(to), std::int8_t(cls), std::int8_t(class_of(type_of(tp))),
                           std::uint8_t(c == WHITE), std::uint8_t(color_of(tp) == WHITE)});
                }
            }
        }
    }
}

inline bool bona_board(int bp, Square& sq, int& cls, Color& col) {
    if (bp < int(Eval::fe_hand_end) || bp >= int(Eval::f_king)) return false;
    const int idx = bp - int(Eval::fe_hand_end);
    const int plane = idx / 81;
    static constexpr std::int8_t kClassOfPlane[9] = {0, 1, 2, 3, 4, 5, 7, 6, 8};   
    sq = Square(idx % 81);
    col = (plane & 1) ? WHITE : BLACK;
    cls = kClassOfPlane[plane / 2];
    return true;
}

inline const std::vector<int>& startpos_golden() {
    static const std::vector<int> g = {1315, 1316, 1399, 1400, 5215, 5381, 10643, 10746,
                                       19015, 19016, 19099, 19100, 24672, 25162, 31045, 31148};
    return g;
}


struct Fast {
    std::int8_t cls_of_piece[PIECE_NB];        
    std::uint8_t rev_attack[PIECE_NB][24];     
    const Bitboard* step_eff[PIECE_NB];        
    Bitboard step_reach[SQ_NB];                
    Piece piece_of_plane[18];                  
};
inline const Fast& fast() {
    static const Fast f = [] {
        Fast t{};
        for (int p = 0; p < PIECE_NB; ++p) {
            const Piece pc = Piece(p);
            const PieceType pt = type_of(pc);
            const int cls = (pt == NO_PIECE_TYPE || pt == KING || pt == QUEEN) ? -1 : class_of(pt);
            t.cls_of_piece[p] = std::int8_t(cls);
            t.step_eff[p] = nullptr;
            if (cls >= 0 && is_step(cls)) {
                const Color c = color_of(pc);
                switch (cls) {
                case 0: t.step_eff[p] = &BB_Table::PawnEffectBB[0][c]; break;
                case 2: t.step_eff[p] = &BB_Table::KnightEffectBB[0][c]; break;
                case 3: t.step_eff[p] = &BB_Table::SilverEffectBB[0][c]; break;
                default: t.step_eff[p] = &BB_Table::GoldEffectBB[0][c]; break;
                }
                constexpr int kCentre = 4 * 9 + 4;
                Bitboard eff = step_effect(cls, c, Square(kCentre));
                while (eff) {
                    const Square to = eff.pop();
                    t.rev_attack[p][int(to) - kCentre + 11] = 1;
                }
            }
        }
        for (int s = 0; s < SQ_NB; ++s)
            t.step_reach[s] = kingEffect(Square(s)) | knightEffect(BLACK, Square(s)) | knightEffect(WHITE, Square(s));
        static constexpr PieceType kTypeOfPlane[9] = {PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP, HORSE, ROOK, DRAGON};
        for (int plane = 0; plane < 18; ++plane)
            t.piece_of_plane[plane] = Piece(int(kTypeOfPlane[plane / 2]) + ((plane & 1) ? int(PIECE_WHITE) : 0));
        return t;
    }();
    return f;
}

inline void emit2(const Tables& t, const Fast& f, Piece ap, Piece tp, int from, int to, List* out) {
    const int ac = f.cls_of_piece[ap], tc = f.cls_of_piece[tp];
    const int aw = ap >> 4, tw = tp >> 4;   
    const int slot_b = (aw ? 4 : 0) + kStepSlot[ac];
    out[BLACK].push(t.pair_base[aw][ac][tw][tc] + t.step_from_offset[slot_b][from] + t.step_order[slot_b][from][to - from + 11]);
    const int as = aw ^ 1, ds = tw ^ 1;
    const int slot_w = (as ? 4 : 0) + kStepSlot[ac];
    const int fn = 80 - from;
    out[WHITE].push(t.pair_base[as][ac][ds][tc] + t.step_from_offset[slot_w][fn] + t.step_order[slot_w][fn][from - to + 11]);
}
inline void emit1(const Tables& t, const Fast& f, Piece ap, Piece tp, int from, int to, Color perspective, List& out) {
    const int ac = f.cls_of_piece[ap], tc = f.cls_of_piece[tp];
    const int pw = perspective == WHITE;
    const int as = (ap >> 4) ^ pw, ds = (tp >> 4) ^ pw;
    const int slot = (as ? 4 : 0) + kStepSlot[ac];
    const int fn = pw ? 80 - from : from;
    const int d = pw ? from - to + 11 : to - from + 11;
    out.push(t.pair_base[as][ac][ds][tc] + t.step_from_offset[slot][fn] + t.step_order[slot][fn][d]);
}

struct ChangedSq {
    Square sq[6];
    Piece before[6], after[6];
    int n = 0;
    int find(Square s) const {
        for (int i = 0; i < n; ++i) if (sq[i] == s) return i;
        return -1;
    }
    int add(const Position& pos, Square s) {
        const int i = find(s);
        if (i >= 0) return i;
        sq[n] = s;
        after[n] = pos.piece_on(s);
        before[n] = after[n];
        return n++;
    }
};

inline bool bona_square(const Fast& f, int bp, Square& sq, Piece& pc) {
    if (bp < int(Eval::fe_hand_end) || bp >= int(Eval::f_king)) return false;
    const int idx = bp - int(Eval::fe_hand_end);
    sq = Square(idx % 81);
    pc = f.piece_of_plane[idx / 81];
    return true;
}

inline void collect_changed_pieces(const Fast& f, const Position& pos, const Eval::DirtyPiece* const* plies, int nplies, ChangedSq& ch) {
    ch.n = 0;
    for (int k = nplies - 1; k >= 0; --k) {
        const Eval::DirtyPiece* dp = plies[k];
        for (int i = 0; i < dp->dirty_num; ++i) {
            Square s; Piece pc;
            if (bona_square(f, int(dp->changed_piece[i].new_piece.fb), s, pc)) { const int j = ch.add(pos, s); ch.before[j] = NO_PIECE; }
        }
        for (int i = 0; i < dp->dirty_num; ++i) {
            Square s; Piece pc;
            if (bona_square(f, int(dp->changed_piece[i].old_piece.fb), s, pc)) { const int j = ch.add(pos, s); ch.before[j] = pc; }
        }
    }
}

inline void changed_indices(const Position& pos, const Eval::DirtyPiece* const* plies, int nplies, List removed[2], List added[2]) {
    const Tables& t = tables();
    const Fast& f = fast();
#if defined(CYF_THREAT_SUBPROF)
    auto& ep = enum_prof();
    const std::uint64_t t0 = __rdtsc();
#endif
    ChangedSq ch;
    collect_changed_pieces(f, pos, plies, nplies, ch);
#if defined(CYF_THREAT_SUBPROF)
    const std::uint64_t t1 = __rdtsc();
    ep.collect_cyc += t1 - t0;
    ep.changed_sq += std::uint64_t(ch.n);
#endif
    if (ch.n == 0) return;
    const Bitboard kings = pos.pieces(BLACK, KING) | pos.pieces(WHITE, KING);
    const Bitboard nonking_now = pos.pieces() ^ kings;
    Bitboard chmask(ZERO);
    for (int i = 0; i < ch.n; ++i) chmask |= Bitboard(ch.sq[i]);
    Bitboard nonking_before = chmask.andnot(nonking_now);
    for (int i = 0; i < ch.n; ++i)
        if (f.cls_of_piece[ch.before[i]] >= 0) nonking_before |= Bitboard(ch.sq[i]);
    for (int i = 0; i < ch.n; ++i) {
        const Square s = ch.sq[i];
        const Piece bp = ch.before[i], ap = ch.after[i];
        const int btc = f.cls_of_piece[bp], atc = f.cls_of_piece[ap];
        if (btc >= 0 || atc >= 0) {
            Bitboard cand = chmask.andnot(f.step_reach[s] & nonking_now);
            while (cand) {
                const Square sp = cand.pop();
                const Piece p = pos.piece_on(sp);
#if defined(CYF_THREAT_SUBPROF)
                ++ep.cand;
#endif
                if (!f.rev_attack[p][int(s) - int(sp) + 11]) continue;
                if (btc >= 0) emit2(t, f, p, bp, sp, s, removed);
                if (atc >= 0) emit2(t, f, p, ap, sp, s, added);
            }
        }
        if (const Bitboard* eff = f.step_eff[bp]) {
            Bitboard tg = eff[2 * int(s)] & nonking_before;
            while (tg) {
                const Square to = tg.pop();
                const int j = ch.find(to);
                emit2(t, f, bp, j >= 0 ? ch.before[j] : pos.piece_on(to), s, to, removed);
            }
        }
        if (const Bitboard* eff = f.step_eff[ap]) {
            Bitboard tg = eff[2 * int(s)] & nonking_now;
            while (tg) {
                const Square to = tg.pop();
                emit2(t, f, ap, pos.piece_on(to), s, to, added);
            }
        }
    }
#if defined(CYF_THREAT_SUBPROF)
    ep.enum_cyc += __rdtsc() - t1;
#endif
}

#if defined(CYF_THREAT_ENUM_V8)
struct NbTable {
    alignas(16) std::uint8_t sq[SQ_NB][16];       
    alignas(64) std::uint32_t amask[SQ_NB][16];   
    std::uint8_t n[SQ_NB];
};
inline const NbTable& nb_table() {
    static const NbTable t = [] {
        NbTable nt{};
        const Fast& f = fast();
        for (int s = 0; s < SQ_NB; ++s) {
            int k = 0;
            Bitboard r = f.step_reach[s];
            while (r) {
                const Square sp = r.pop();
                nt.sq[s][k] = std::uint8_t(sp);
                std::uint32_t m = 0;
                for (int p = 0; p < 32; ++p)
                    if (p < PIECE_NB && f.rev_attack[p][s - int(sp) + 11]) m |= 1u << p;
                nt.amask[s][k] = m;
                ++k;
            }
            nt.n[s] = std::uint8_t(k);
            for (; k < 16; ++k) { nt.sq[s][k] = std::uint8_t(SQ_NB); nt.amask[s][k] = 0; }
        }
        return nt;
    }();
    return t;
}
inline void changed_indices_v8(const Position& pos, const Eval::DirtyPiece* const* plies, int nplies, List removed[2], List added[2]) {
    const Tables& t = tables();
    const Fast& f = fast();
    const NbTable& nt = nb_table();
    ChangedSq ch;
    collect_changed_pieces(f, pos, plies, nplies, ch);
    if (ch.n == 0) return;
    const Bitboard kings = pos.pieces(BLACK, KING) | pos.pieces(WHITE, KING);
    const Bitboard nonking_now = pos.pieces() ^ kings;
    Bitboard chmask(ZERO);
    for (int i = 0; i < ch.n; ++i) chmask |= Bitboard(ch.sq[i]);
    Bitboard nonking_before = chmask.andnot(nonking_now);
    for (int i = 0; i < ch.n; ++i)
        if (f.cls_of_piece[ch.before[i]] >= 0) nonking_before |= Bitboard(ch.sq[i]);
    const std::uint8_t* board = reinterpret_cast<const std::uint8_t*>(pos.piece_array().data());
    static_assert(sizeof(Piece) == 1);
    const __m512i b_lo = _mm512_loadu_si512(board);                                        
    const __m512i b_hi = _mm512_maskz_loadu_epi8((__mmask64(1) << (SQ_NB + 1 - 64)) - 1, board + 64);   
    const __m512i one = _mm512_set1_epi32(1);
    for (int i = 0; i < ch.n; ++i) {
        const Square s = ch.sq[i];
        const Piece bp = ch.before[i], ap = ch.after[i];
        const int btc = f.cls_of_piece[bp], atc = f.cls_of_piece[ap];
        if (btc >= 0 || atc >= 0) {
            const __m512i idx = _mm512_castsi128_si512(_mm_load_si128(reinterpret_cast<const __m128i*>(nt.sq[s])));
            const __m512i occ = _mm512_cvtepu8_epi32(_mm512_castsi512_si128(_mm512_permutex2var_epi8(b_lo, idx, b_hi)));
            const __m512i hit = _mm512_srlv_epi32(_mm512_load_si512(nt.amask[s]), occ);
            std::uint32_t lanes = _mm512_test_epi32_mask(hit, one);   
            while (lanes) {
                const int j = __builtin_ctz(lanes); lanes &= lanes - 1;
                const Square sp = Square(nt.sq[s][j]);
                if (chmask.test(sp)) continue;   
                const Piece p = Piece(board[sp]);
                if (btc >= 0) emit2(t, f, p, bp, sp, s, removed);
                if (atc >= 0) emit2(t, f, p, ap, sp, s, added);
            }
        }
        if (const Bitboard* eff = f.step_eff[bp]) {
            Bitboard tg = eff[2 * int(s)] & nonking_before;
            while (tg) {
                const Square to = tg.pop();
                const int j = ch.find(to);
                emit2(t, f, bp, j >= 0 ? ch.before[j] : pos.piece_on(to), s, to, removed);
            }
        }
        if (const Bitboard* eff = f.step_eff[ap]) {
            Bitboard tg = eff[2 * int(s)] & nonking_now;
            while (tg) {
                const Square to = tg.pop();
                emit2(t, f, ap, pos.piece_on(to), s, to, added);
            }
        }
    }
}
#endif

inline void all_indices(const Position& pos, Color perspective, List& out) {
    const Tables& t = tables();
    const Fast& f = fast();
    out.n = 0;
#if defined(CYF_THREAT_SUBPROF)
    auto& ep = enum_prof();
    const std::uint64_t t0 = __rdtsc();
#endif
    const Bitboard nonking = pos.pieces() ^ (pos.pieces(BLACK, KING) | pos.pieces(WHITE, KING));
    for (Color c : {BLACK, WHITE}) {
        Bitboard steppers = pos.pieces(c, PAWN) | pos.pieces(c, KNIGHT) | pos.pieces(c, SILVER) | pos.pieces(c, GOLDS);
        while (steppers) {
            const Square s = steppers.pop();
            const Piece ap = pos.piece_on(s);
            Bitboard tg = f.step_eff[ap][2 * int(s)] & nonking;
            while (tg) {
                const Square to = tg.pop();
                emit1(t, f, ap, pos.piece_on(to), s, to, perspective, out);
            }
        }
    }
#if defined(CYF_THREAT_SUBPROF)
    const std::uint64_t t1 = __rdtsc();
#endif
    std::sort(out.v, out.v + out.n);
#if defined(CYF_THREAT_SUBPROF)
    ep.full_enum_cyc += t1 - t0;
    ep.full_sort_cyc += __rdtsc() - t1;
    ++ep.full_n;
#endif
}

#if defined(CYF_THREAT_ENUM_V5)

struct Changed {
    Square sq[6];
    std::int8_t bcls[6], acls[6];
    Color bcol[6], acol[6];
    int n = 0;
    int find(Square s) const {
        for (int i = 0; i < n; ++i) if (sq[i] == s) return i;
        return -1;
    }
    int add(const Position& pos, Square s) {
        const int i = find(s);
        if (i >= 0) return i;
        const Piece p = pos.piece_on(s);
        sq[n] = s;
        if (p == NO_PIECE) { acls[n] = -2; acol[n] = BLACK; }
        else { acls[n] = std::int8_t(class_of(type_of(p))); acol[n] = color_of(p); }
        bcls[n] = acls[n]; bcol[n] = acol[n];
        return n++;
    }
};

struct ClassBB {
    Bitboard cls[2][5];
    Bitboard nonking;
};

template <bool Before, typename F>
inline void edges_of(const Position& pos, const Changed& ch, const ClassBB& bb, Bitboard chmask, F f) {
    const Bitboard not_ch = ~chmask;
    for (int i = 0; i < ch.n; ++i) {
        const Square s = ch.sq[i];
        const int c = Before ? ch.bcls[i] : ch.acls[i];
        if (c < 0) continue;   
        const Color co = Before ? ch.bcol[i] : ch.acol[i];
        if (is_step(c)) {
            Bitboard t = step_effect(c, co, s) & bb.nonking;   
            while (t) {
                const Square to = t.pop();
                int tc; Color tco;
                const int j = ch.find(to);
                if (j >= 0) { tc = Before ? ch.bcls[j] : ch.acls[j]; tco = Before ? ch.bcol[j] : ch.acol[j]; }
                else { const Piece p = pos.piece_on(to); tc = class_of(type_of(p)); tco = color_of(p); }
                f(Edge{std::uint8_t(s), std::uint8_t(to), std::int8_t(c), std::int8_t(tc), std::uint8_t(co == WHITE), std::uint8_t(tco == WHITE)});
            }
        }
        for (Color ac : {BLACK, WHITE}) {
            for (int k : {0, 2, 3, 4}) {
                Bitboard att = bb.cls[ac][k] & step_effect(k, ~ac, s) & not_ch;
                while (att) {
                    const Square sp = att.pop();
                    f(Edge{std::uint8_t(sp), std::uint8_t(s), std::int8_t(k), std::int8_t(c), std::uint8_t(ac == WHITE), std::uint8_t(co == WHITE)});
                }
            }
        }
    }
}

inline void collect_changed(const Position& pos, const Eval::DirtyPiece* const* plies, int nplies, Changed& ch) {
    ch.n = 0;
    for (int k = nplies - 1; k >= 0; --k) {
        const Eval::DirtyPiece* dp = plies[k];
        for (int i = 0; i < dp->dirty_num; ++i) {
            Square s; int c; Color co;
            if (bona_board(int(dp->changed_piece[i].new_piece.fb), s, c, co)) { const int j = ch.add(pos, s); ch.bcls[j] = -2; }
        }
        for (int i = 0; i < dp->dirty_num; ++i) {
            Square s; int c; Color co;
            if (bona_board(int(dp->changed_piece[i].old_piece.fb), s, c, co)) { const int j = ch.add(pos, s); ch.bcls[j] = std::int8_t(c); ch.bcol[j] = co; }
        }
    }
}

inline void changed_edges(const Position& pos, const Eval::DirtyPiece* const* plies, int nplies,
                          std::vector<Edge>& removed, std::vector<Edge>& added) {
    removed.clear(); added.clear();
#if defined(CYF_THREAT_SUBPROF)
    auto& ep = enum_prof();
    const std::uint64_t t0 = __rdtsc();
#endif
    Changed ch;
    collect_changed(pos, plies, nplies, ch);
#if defined(CYF_THREAT_SUBPROF)
    const std::uint64_t t1 = __rdtsc();
    ep.collect_cyc += t1 - t0;
    ep.changed_sq += std::uint64_t(ch.n);
#endif
    if (ch.n == 0) return;
    ClassBB now;
    for (Color c : {BLACK, WHITE}) {
        now.cls[c][0] = pos.pieces(c, PAWN);
        now.cls[c][1] = Bitboard(ZERO);
        now.cls[c][2] = pos.pieces(c, KNIGHT);
        now.cls[c][3] = pos.pieces(c, SILVER);
        now.cls[c][4] = pos.pieces(c, GOLDS);
    }
    now.nonking = pos.pieces() ^ (pos.pieces(BLACK, KING) | pos.pieces(WHITE, KING));
    ClassBB before = now;
    Bitboard chmask(ZERO);
    for (int i = 0; i < ch.n; ++i) {
        const Bitboard m(ch.sq[i]);
        chmask |= m;
        if (ch.acls[i] >= 0) {
            before.nonking &= ~m;
            if (is_step(ch.acls[i])) before.cls[ch.acol[i]][ch.acls[i]] &= ~m;
        }
        if (ch.bcls[i] >= 0) {
            before.nonking |= m;
            if (is_step(ch.bcls[i])) before.cls[ch.bcol[i]][ch.bcls[i]] |= m;
        }
    }
#if defined(CYF_THREAT_SUBPROF)
    const std::uint64_t t2 = __rdtsc();
    ep.bb_cyc += t2 - t1;
#endif
    edges_of<true>(pos, ch, before, chmask, [&](const Edge& e) { removed.push_back(e); });
    edges_of<false>(pos, ch, now, chmask, [&](const Edge& e) { added.push_back(e); });
#if defined(CYF_THREAT_SUBPROF)
    ep.enum_cyc += __rdtsc() - t2;
#endif
}
#endif  

}  
}  
}  
}  
