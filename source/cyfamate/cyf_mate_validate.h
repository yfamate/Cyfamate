#pragma once
struct NativeMateProof {
    int remaining = 2048, visited = 0, max_game_ply = 100000;
    int64_t deadline = 0;
    const std::atomic<bool>* cancelled = nullptr;
    bool expired() const {
        return (cancelled && cancelled->load(std::memory_order_relaxed))
            || (deadline && CyfZyf::now_us() >= deadline);
    }
    int walk(Position& p, Color attacker, int depth, int ply, Move first = Move::none()) {
        if (expired() || remaining <= 0) return -1;
        --remaining; ++visited;
        if (p.game_ply() > max_game_ply || p.is_repetition(ply) != REPETITION_NONE) return -1;
        const bool attack = p.side_to_move() == attacker;
        if (!attack && !p.checkers()) return -1;
        const MoveList<LEGAL_ALL> moves(p);
        if (moves.size() == 0) return !attack && p.checkers() ? 0 : -1;
        if (depth <= 0 || ply >= MAX_PLY - 1) return -1;
        int longest = 0;
        for (auto em : moves) {
            Move m = em;
            if (attack && ((first != Move::none() && m != first) || !p.gives_check(m))) continue;
            if (expired()) return -1;
            StateInfo st;
            p.do_move(m, st);
            int child = walk(p, attacker, depth - 1, ply + 1);
            p.undo_move(m); 
            if (expired()) return -1;
            if (attack) { if (child >= 0) return child + 1; }
            else { if (child < 0) return -1; longest = std::max(longest, child + 1); }
        }
        return attack ? -1 : longest;
    }
};
