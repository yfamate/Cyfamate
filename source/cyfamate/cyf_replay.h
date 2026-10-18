
inline uint64_t replay_mix(uint64_t x) {
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	return x ^ (x >> 31);
}

struct Replay {
	enum Kind { None, Lmr, Research, Placebo };
	bool enabled = false;
	Kind kind = None;
	int iter = 1, offset = 2;
	std::string path, move;

	bool resolved = false;
	uint64_t key = 0;
	int ply = 0;
	Move target = Move::none();
	uint64_t watch[2] = {0, 0};
	std::string watch_path;

	uint64_t trace = 0, trace_at_trigger = 0, nodes_at_trigger = 0, eligible = 0, applied = 0;
	bool triggered = false;
	std::vector<std::pair<Move, int>> log[2];   

	static const char* name(Kind k) {
		return k == Lmr ? "lmr" : k == Research ? "research" : k == Placebo ? "placebo" : "none";
	}

	bool set(std::string spec, std::string& err) {
		enabled = false;
		if (spec.empty() || spec == "<empty>" || spec == "off")
			return true;
		Replay r;
		std::stringstream ss(spec);
		std::string item;
		while (std::getline(ss, item, ',')) {
			auto eq = item.find('=');
			if (eq == std::string::npos) { err = "item without '=': " + item; return false; }
			const std::string k = item.substr(0, eq), v = item.substr(eq + 1);
			if (k == "kind") {
				if (v == "none") r.kind = None;
				else if (v == "lmr") r.kind = Lmr;
				else if (v == "research") r.kind = Research;
				else if (v == "placebo") r.kind = Placebo;
				else { err = "unknown kind " + v; return false; }
			}
			else if (k == "iter") r.iter = std::atoi(v.c_str());
			else if (k == "offset") r.offset = std::atoi(v.c_str());
			else if (k == "path") r.path = v;
			else if (k == "move") r.move = v;
			else { err = "unknown key " + k; return false; }
		}
		if (r.iter < 1) { err = "iter must be >= 1"; return false; }
		if ((r.kind == Lmr || r.kind == Research) && r.move.empty()) { err = "lmr/research need move="; return false; }
		if (r.path.find('.') != std::string::npos) { err = "path supports at most one move"; return false; }
		if (r.kind == Placebo && r.offset <= 0) { err = "placebo needs offset > 0"; return false; }
		*this = r;
		enabled = true;
		return true;
	}

	static uint64_t key_after(const Position& root, Move m) {
		Position p;
		StateInfo si, st;
		p.set(root.sfen(), &si);
		p.do_move(m, st);
		return uint64_t(p.key());
	}

	void begin_go(const Position& root, bool single) {
		trace = 0x243F6A8885A308D3ULL;
		trace_at_trigger = nodes_at_trigger = eligible = applied = 0;
		triggered = false;
		log[0].clear(); log[1].clear();
		resolved = true;
		ply = 0; key = 0; target = Move::none();
		if (!single) {
			sync_cout << "info string Error! : Cyf_Replay needs Threads 1 and Cyf_Mate_Enable false" << sync_endl;
			enabled = false;
			return;
		}
		if (kind != Lmr && kind != Research)
			return;
		Position p;
		StateInfo si, st;
		p.set(root.sfen(), &si);
		if (!path.empty()) {
			const Move m = USIEngine::to_move(p, path);
			if (!m.is_ok()) { resolved = false; return; }
			p.do_move(m, st);
			ply = 1;
		}
		key = uint64_t(p.key());
		target = USIEngine::to_move(p, move);
		resolved = target.is_ok();
	}

	uint64_t begin_iteration(const Position& root, Move first, int depth, uint64_t nodes) {
		watch[0] = uint64_t(root.key());
		watch[1] = first.is_ok() ? key_after(root, first) : 0;
		watch_path = first.is_ok() ? USIEngine::move(first) : "-";
		log[0].clear(); log[1].clear();
		if (depth != iter || triggered)
			return 0;
		triggered = true;
		trace_at_trigger = trace;
		nodes_at_trigger = nodes;
		sync_cout << "info string cyf_trigger iter=" << depth << " nodes=" << nodes
		          << " trace=" << hex(trace) << sync_endl;
		return kind == Placebo ? uint64_t(offset) : 0;
	}

	void visit(uint64_t k, int depth, int alpha, int beta, int ply_, uint64_t nodes) {
		trace = replay_mix(trace ^ k);
		trace = replay_mix(trace ^ (uint64_t(uint32_t(depth)) | uint64_t(uint32_t(ply_)) << 32));
		trace = replay_mix(trace ^ (uint64_t(uint32_t(alpha)) | uint64_t(uint32_t(beta)) << 32));
		trace = replay_mix(trace ^ nodes);
	}

	bool at_lmr(int ply_, uint64_t k, Move m, int& d, int newDepth) {
		if (ply_ < 2 && k == watch[ply_] && d < newDepth) {
			bool seen = false;
			for (auto& e : log[ply_])
				if (e.first == m) { e.second = newDepth - d; seen = true; }
			if (!seen)
				log[ply_].emplace_back(m, newDepth - d);
		}
		if (!triggered || !resolved || ply_ != ply || m != target || k != key)
			return false;
		++eligible;
		if (kind == Lmr && d < newDepth) {
			++d;
			++applied;
		}
		return kind == Research;
	}

	void end_iteration(int depth, uint64_t nodes, Move best, int score) {
		sync_cout << "info string cyf_iter depth=" << depth << " nodes=" << nodes << " trace=" << hex(trace)
		          << " best=" << USIEngine::move(best) << " score=" << score << sync_endl;
		for (int p = 0; p < 2; ++p) {
			std::stringstream ss;
			ss << "info string cyf_lmr depth=" << depth << " ply=" << p << " path=" << (p ? watch_path : "-");
			for (auto& e : log[p])
				ss << ' ' << USIEngine::move(e.first) << ':' << e.second;
			sync_cout << ss.str() << sync_endl;
		}
	}

	void end_go(uint64_t nodes, const std::string& bestmove) {
		sync_cout << "info string cyf_replay kind=" << name(kind) << " iter=" << iter
		          << " path=" << (path.empty() ? "-" : path) << " move=" << (move.empty() ? "-" : move)
		          << " resolved=" << resolved << " triggered=" << triggered
		          << " trigger_nodes=" << nodes_at_trigger << " trigger_trace=" << hex(trace_at_trigger)
		          << " eligible=" << eligible << " applied=" << applied
		          << " nodes=" << nodes << " final_trace=" << hex(trace) << " bestmove=" << bestmove << sync_endl;
	}

	static std::string hex(uint64_t v) {
		char b[17];
		std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
		return b;
	}
};
inline Replay& replay() { static Replay r; return r; }
