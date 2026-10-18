#pragma once
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace CyfCsa {

#if defined(_WIN32)
typedef UINT_PTR sock_t;
static const sock_t BAD_SOCK = (sock_t)(~0);
struct WsApi {
    HMODULE dll = nullptr;
    int (WINAPI* WSAStartup_)(WORD, void*) = nullptr;
    sock_t (WINAPI* socket_)(int, int, int) = nullptr;
    int (WINAPI* connect_)(sock_t, const void*, int) = nullptr;
    int (WINAPI* send_)(sock_t, const char*, int, int) = nullptr;
    int (WINAPI* recv_)(sock_t, char*, int, int) = nullptr;
    int (WINAPI* closesocket_)(sock_t) = nullptr;
    int (WINAPI* setsockopt_)(sock_t, int, int, const char*, int) = nullptr;
    void* (WINAPI* gethostbyname_)(const char*) = nullptr;
    unsigned short (WINAPI* htons_)(unsigned short) = nullptr;
    bool load() {
        if (dll) return true;
        dll = LoadLibraryA("ws2_32.dll");
        if (!dll) return false;
        WSAStartup_ = (decltype(WSAStartup_))GetProcAddress(dll, "WSAStartup");
        socket_ = (decltype(socket_))GetProcAddress(dll, "socket");
        connect_ = (decltype(connect_))GetProcAddress(dll, "connect");
        send_ = (decltype(send_))GetProcAddress(dll, "send");
        recv_ = (decltype(recv_))GetProcAddress(dll, "recv");
        closesocket_ = (decltype(closesocket_))GetProcAddress(dll, "closesocket");
        setsockopt_ = (decltype(setsockopt_))GetProcAddress(dll, "setsockopt");
        gethostbyname_ = (decltype(gethostbyname_))GetProcAddress(dll, "gethostbyname");
        htons_ = (decltype(htons_))GetProcAddress(dll, "htons");
        if (!WSAStartup_ || !socket_ || !connect_ || !send_ || !recv_ || !closesocket_ || !gethostbyname_ || !htons_) return false;
        char wsa[512];
        return WSAStartup_(MAKEWORD(2, 2), wsa) == 0;
    }
};
static WsApi& ws() { static WsApi w; return w; }
struct HostEnt { char* h_name; char** h_aliases; short h_addrtype; short h_length; char** h_addr_list; };
struct SockAddrIn { short sin_family; unsigned short sin_port; unsigned char sin_addr[4]; char sin_zero[8]; };
#else
typedef int sock_t;
static const sock_t BAD_SOCK = -1;
#endif

static sock_t sock_connect(const std::string& host, int port, std::string& err) {
#if defined(_WIN32)
    if (!ws().load()) { err = "ws2_32.dll load / WSAStartup failed"; return BAD_SOCK; }
    HostEnt* he = (HostEnt*)ws().gethostbyname_(host.c_str());
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) { err = "host not found: " + host; return BAD_SOCK; }
    sock_t s = ws().socket_(2 , 1 , 6 );
    if (s == BAD_SOCK) { err = "socket() failed"; return BAD_SOCK; }
    SockAddrIn a{}; a.sin_family = 2; a.sin_port = ws().htons_((unsigned short)port);
    std::memcpy(a.sin_addr, he->h_addr_list[0], 4);
    if (ws().connect_(s, &a, sizeof(a)) != 0) { ws().closesocket_(s); err = "connect failed"; return BAD_SOCK; }
    if (ws().setsockopt_) { int one = 1; ws().setsockopt_(s, 6 , 1 , (const char*)&one, sizeof(one)); }
    return s;
#else
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) { err = "host not found: " + host; return BAD_SOCK; }
    sock_t s = BAD_SOCK;
    for (addrinfo* p = res; p; p = p->ai_next) {
        s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        if (::connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(s); s = BAD_SOCK;
    }
    freeaddrinfo(res);
    if (s == BAD_SOCK) { err = "connect failed"; return BAD_SOCK; }
    int one = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return s;
#endif
}
static bool sock_send_all(sock_t s, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
#if defined(_WIN32)
        int n = ws().send_(s, data.data() + off, (int)(data.size() - off), 0);
#else
        ssize_t n = ::send(s, data.data() + off, data.size() - off, MSG_NOSIGNAL);
#endif
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}
static int sock_recv(sock_t s, char* buf, int len) {
#if defined(_WIN32)
    return ws().recv_(s, buf, len, 0);
#else
    return (int)::recv(s, buf, (size_t)len, 0);
#endif
}
static void sock_close(sock_t s) {
    if (s == BAD_SOCK) return;
#if defined(_WIN32)
    ws().closesocket_(s);
#else
    ::shutdown(s, SHUT_RDWR); ::close(s);
#endif
}

struct Event { int src; std::string line; };   
class EventQueue {
    std::mutex m; std::condition_variable cv; std::deque<Event> q;
public:
    void push(int src, const std::string& l) { { std::lock_guard<std::mutex> g(m); q.push_back({src, l}); } cv.notify_all(); }
    bool pop(Event& e, int timeout_ms) {
        std::unique_lock<std::mutex> g(m);
        if (!cv.wait_for(g, std::chrono::milliseconds(timeout_ms), [&] { return !q.empty(); })) return false;
        e = q.front(); q.pop_front(); return true;
    }
    void clear_src(int src) { std::lock_guard<std::mutex> g(m); for (auto it = q.begin(); it != q.end();) it = (it->src == src) ? q.erase(it) : it + 1; }
};

class PipeInBuf : public std::streambuf {
    std::mutex m; std::condition_variable cv; std::string buf; bool closed = false; char ch = 0;
protected:
    int_type underflow() override {
        std::unique_lock<std::mutex> g(m);
        cv.wait(g, [&] { return !buf.empty() || closed; });
        if (buf.empty()) return traits_type::eof();
        ch = buf[0]; buf.erase(0, 1);
        setg(&ch, &ch, &ch + 1);
        return traits_type::to_int_type(ch);
    }
public:
    void write_line(const std::string& l) { { std::lock_guard<std::mutex> g(m); buf += l; buf += '\n'; } cv.notify_all(); }
    void close() { { std::lock_guard<std::mutex> g(m); closed = true; } cv.notify_all(); }
};
class TeeOutBuf : public std::streambuf {
    std::streambuf* orig; EventQueue* q; std::string line; std::mutex m; std::ofstream* log;
protected:
    int_type overflow(int_type c) override {
        if (c == traits_type::eof()) return 0;
        std::lock_guard<std::mutex> g(m);
        if (orig) orig->sputc((char)c);
        if (c == '\n') { if (!line.empty() && line.back() == '\r') line.pop_back(); q->push(1, line); if (log && log->good()) { *log << "< " << line << '\n'; } line.clear(); }
        else line.push_back((char)c);
        return c;
    }
    int sync() override { std::lock_guard<std::mutex> g(m); if (orig) orig->pubsync(); if (log) log->flush(); return 0; }
public:
    TeeOutBuf(std::streambuf* o, EventQueue* eq, std::ofstream* lg) : orig(o), q(eq), log(lg) {}
};

static const char* CSA_PT[] = {"FU", "KY", "KE", "GI", "KI", "KA", "HI", "OU", "TO", "NY", "NK", "NG", "UM", "RY"};
static const char* SFEN_PT[] = {"P", "L", "N", "S", "G", "B", "R", "K", "+P", "+L", "+N", "+S", "+B", "+R"};
static int pt_index(const std::string& c) { for (int i = 0; i < 14; ++i) if (c == CSA_PT[i]) return i; return -1; }
static int promote_pt(int i) { return i <= 2 ? i + 8 : i == 3 ? 11 : i == 5 ? 12 : i == 6 ? 13 : i; }
static int unpromote_pt(int i) { return i == 8 ? 0 : i == 9 ? 1 : i == 10 ? 2 : i == 11 ? 3 : i == 12 ? 5 : i == 13 ? 6 : i; }
struct Board {
    int sq[10][10];        
    int hand[2][7];        
    int side = 0;          
    void clear() { std::memset(sq, 0, sizeof(sq)); std::memset(hand, 0, sizeof(hand)); side = 0; }
    void hirate() {
        clear();
        const char* back[9] = {"KY", "KE", "GI", "KI", "OU", "KI", "GI", "KE", "KY"};
        for (int f = 1; f <= 9; ++f) {
            sq[f][1] = -(pt_index(back[9 - f]) + 1); sq[f][9] = pt_index(back[9 - f]) + 1;
            sq[f][3] = -1; sq[f][7] = 1;
        }
        sq[8][2] = -(pt_index("HI") + 1); sq[2][2] = -(pt_index("KA") + 1);
        sq[2][8] = pt_index("HI") + 1; sq[8][8] = pt_index("KA") + 1;
    }
    bool row(int rank, const std::string& body) {
        for (int i = 0; i < 9; ++i) {
            std::string c = body.substr(i * 3, 3);
            int f = 9 - i;
            if (c == " * " || c.size() < 3) { sq[f][rank] = 0; continue; }
            int p = pt_index(c.substr(1, 2));
            if (p < 0) return false;
            sq[f][rank] = (c[0] == '+' ? 1 : -1) * (p + 1);
        }
        return true;
    }
    void hands(const std::string& l) {
        int col = l[1] == '+' ? 0 : 1;
        for (size_t i = 2; i + 4 <= l.size(); i += 4) {
            std::string pc = l.substr(i + 2, 2);
            if (pc == "AL") continue;
            int p = pt_index(pc);
            if (p >= 0 && p <= 6) hand[col][p]++;
            else if (p >= 0) hand[col][unpromote_pt(p)]++;
            int f = l[i] - '0', r = l[i + 1] - '0';
            if (f >= 1 && f <= 9 && r >= 1 && r <= 9) { sq[f][r] = (col == 0 ? 1 : -1) * (p + 1); if (p <= 6) hand[col][p]--; }
        }
    }
    std::string sfen() const {
        std::string s;
        for (int r = 1; r <= 9; ++r) {
            int empty = 0;
            for (int f = 9; f >= 1; --f) {
                int v = sq[f][r];
                if (!v) { ++empty; continue; }
                if (empty) { s += std::to_string(empty); empty = 0; }
                std::string p = SFEN_PT[std::abs(v) - 1];
                if (v < 0) for (auto& ch : p) ch = (char)std::tolower(ch);
                s += p;
            }
            if (empty) s += std::to_string(empty);
            if (r < 9) s += '/';
        }
        s += side == 0 ? " b " : " w ";
        std::string h;
        const int order[7] = {6, 5, 4, 3, 2, 1, 0};   
        for (int col = 0; col < 2; ++col)
            for (int k : order) {
                int n = hand[col][k];
                if (!n) continue;
                if (n > 1) h += std::to_string(n);
                std::string p = SFEN_PT[k];
                if (col == 1) for (auto& ch : p) ch = (char)std::tolower(ch);
                h += p;
            }
        s += h.empty() ? "-" : h;
        s += " 1";
        return s;
    }
    std::string apply_csa(const std::string& m) {
        if (m.size() < 7) return "";
        int col = m[0] == '+' ? 0 : 1;
        int ff = m[1] - '0', fr = m[2] - '0', tf = m[3] - '0', tr = m[4] - '0';
        int p = pt_index(m.substr(5, 2));
        if (p < 0 || tf < 1 || tf > 9 || tr < 1 || tr > 9) return "";
        std::string usi;
        auto sqs = [](int f, int r) { return std::to_string(f) + (char)('a' + r - 1); };
        if (ff == 0) {
            if (p > 6 || hand[col][p] <= 0) return "";
            hand[col][p]--;
            sq[tf][tr] = (col == 0 ? 1 : -1) * (p + 1);
            usi = std::string(SFEN_PT[p]) + "*" + sqs(tf, tr);
        } else {
            int from = sq[ff][fr];
            if (!from) return "";
            int fromPt = std::abs(from) - 1;
            bool promo = (fromPt != p) && (promote_pt(fromPt) == p);
            int cap = sq[tf][tr];
            if (cap) hand[col][unpromote_pt(std::abs(cap) - 1)]++;
            sq[ff][fr] = 0;
            sq[tf][tr] = (col == 0 ? 1 : -1) * (p + 1);
            usi = sqs(ff, fr) + sqs(tf, tr) + (promo ? "+" : "");
        }
        side = 1 - col;
        return usi;
    }
    std::string usi_to_csa(const std::string& u) const {
        std::string sign = side == 0 ? "+" : "-";
        if (u.size() >= 4 && u[1] == '*') {
            int p = -1;
            for (int i = 0; i < 7; ++i) if (std::string(1, (char)std::toupper(u[0])) == SFEN_PT[i]) p = i;
            if (p < 0) return "";
            return sign + "00" + std::string(1, u[2]) + std::to_string(u[3] - 'a' + 1) + CSA_PT[p];
        }
        if (u.size() < 4) return "";
        int ff = u[0] - '0', fr = u[1] - 'a' + 1, tf = u[2] - '0', tr = u[3] - 'a' + 1;
        if (ff < 1 || ff > 9 || fr < 1 || fr > 9 || tf < 1 || tf > 9 || tr < 1 || tr > 9) return "";
        int from = sq[ff][fr];
        if (!from) return "";
        int pt = std::abs(from) - 1;
        if (u.size() >= 5 && u[4] == '+') pt = promote_pt(pt);
        return sign + std::to_string(ff) + std::to_string(fr) + std::to_string(tf) + std::to_string(tr) + CSA_PT[pt];
    }
};

struct SideTime { long long total_ms = -1, byoyomi_ms = -1, inc_ms = -1, least_ms = -1; };
struct Summary {
    std::string game_id, my_turn = "+", to_move = "+", name_b, name_w;
    long long unit_ms = 1000, total_ms = 0, byoyomi_ms = -1, inc_ms = 0, least_ms = 0;
    bool byoyomi_seen = false;
    SideTime side[2];
    Board start;
    std::vector<std::pair<std::string, long long>> init_moves;   
};
static long long parse_unit(const std::string& v) {
    std::string s = v; while (!s.empty() && s.back() == ' ') s.pop_back();
    double n = 1; size_t i = 0;
    while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) ++i;
    if (i) n = std::atof(s.substr(0, i).c_str());
    std::string u = s.substr(i);
    if (u == "msec") return (long long)(n + 0.5);
    if (u == "min") return (long long)(n * 60000 + 0.5);
    return (long long)(n * 1000 + 0.5);   
}

struct Config {
    std::string host, id, pw, options_file, log_dir = "csa_logs";
    std::string profile = "server", gamename;   
    int port = 4081, games = 0, keepalive_s = 30;
    bool ponder = true, x1 = false, utf8 = false, reconnect = true, comment = false;   
    std::vector<std::string> setoptions;
};

class Client {
    Config cfg; EventQueue q; PipeInBuf* in = nullptr; sock_t sock = BAD_SOCK;
    std::ofstream log; std::thread reader; std::atomic<bool> reader_live{false};
    std::chrono::steady_clock::time_point last_send;
    std::string pending_echo;          
    long long rem[2] = {0, 0}, inc_[2] = {0, 0}, byo[2] = {0, 0}, total0[2] = {0, 0};
    int moves_by[2] = {0, 0};

    static std::string now_str() {
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char b[32]; std::strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%S", std::localtime(&t)); return b;
    }
    void L(const std::string& s) { if (log.good()) { log << now_str() << " " << s << '\n'; log.flush(); } }
    void engine(const std::string& c) { L("E> " + c); in->write_line(c); }
    bool send(const std::string& l, bool quiet = false) {
        if (!quiet) L("S> " + l);
        last_send = std::chrono::steady_clock::now();
        return sock_send_all(sock, l + "\n");
    }
    void start_reader() {
        reader_live = true;
        reader = std::thread([this] {
            std::string acc; char buf[4096];
            for (;;) {
                int n = sock_recv(sock, buf, sizeof(buf));
                if (n <= 0) break;
                acc.append(buf, (size_t)n);
                size_t p;
                while ((p = acc.find('\n')) != std::string::npos) {
                    std::string l = acc.substr(0, p); acc.erase(0, p + 1);
                    if (!l.empty() && l.back() == '\r') l.pop_back();
                    q.push(0, l);
                }
            }
            reader_live = false; q.push(2, "");
        });
    }
    void stop_reader() { sock_close(sock); sock = BAD_SOCK; if (reader.joinable()) reader.join(); q.clear_src(0); q.clear_src(2); }

    bool server_line(std::string& out, int timeout_s = 0) {
        auto t0 = std::chrono::steady_clock::now();
        for (;;) {
            Event e;
            bool got = false;
            if (!pushback.empty()) { e = pushback.front(); pushback.pop_front(); got = true; }
            else got = q.pop(e, 500);
            if (got) {
                if (e.src == 2) return false;
                if (e.src == 0) { if (!e.line.empty()) L("S< " + e.line); out = e.line; return true; }
                continue;   
            }
            keepalive();
            if (timeout_s && std::chrono::steady_clock::now() - t0 > std::chrono::seconds(timeout_s)) { out.clear(); return true; }
        }
    }
    void keepalive() {
        if (cfg.keepalive_s <= 0 || sock == BAD_SOCK) return;
        if (std::chrono::steady_clock::now() - last_send > std::chrono::seconds(cfg.keepalive_s)) send("", true);
    }
    bool wait_engine(const std::string& prefix, std::string& out, int timeout_s) {
        auto t0 = std::chrono::steady_clock::now();
        for (;;) {
            Event e;
            if (q.pop(e, 500)) {
                if (e.src == 1 && e.line.rfind(prefix, 0) == 0) { out = e.line; return true; }
                if (e.src == 0 || e.src == 2) { pushback.push_back(e); continue; }
            } else keepalive();
            if (timeout_s && std::chrono::steady_clock::now() - t0 > std::chrono::seconds(timeout_s)) return false;
        }
    }
    std::deque<Event> pushback;   
    bool next_event(Event& e, int timeout_ms) {
        if (!pushback.empty()) { e = pushback.front(); pushback.pop_front(); return true; }
        return q.pop(e, timeout_ms);
    }

    bool login() {
        if (cfg.utf8) send("'CSA encoding=UTF-8");
        std::string pw = cfg.pw;   
        if (cfg.profile == "floodgate" && !cfg.gamename.empty() && pw.find(',') == std::string::npos) pw = cfg.gamename + "," + pw;
        send("LOGIN " + cfg.id + " " + pw + (cfg.x1 ? " x1" : ""));
        std::string l;
        for (;;) {
            if (!server_line(l, 60)) return false;
            if (l.empty()) { L("login timeout"); return false; }
            if (l.rfind("LOGIN:", 0) == 0) return l.find(" OK") != std::string::npos;
        }
    }
    bool read_summary(Summary& s, bool& disconnected) {
        std::string l; bool in_pos = false; std::string in_time; int begun = 0;
        s.start.hirate(); bool board_seen = false;
        disconnected = false;
        for (;;) {
            if (!server_line(l)) { disconnected = true; return false; }
            if (l == "BEGIN Game_Summary") { begun = 1; continue; }
            if (!begun) continue;
            if (l == "END Game_Summary") break;
            if (l == "BEGIN Position") { in_pos = true; continue; }
            if (l == "END Position") { in_pos = false; continue; }
            if (l == "BEGIN Time") { in_time = "c"; continue; }
            if (l == "BEGIN Time+") { in_time = "+"; continue; }
            if (l == "BEGIN Time-") { in_time = "-"; continue; }
            if (l.rfind("END Time", 0) == 0) { in_time.clear(); continue; }
            auto val = [&](const std::string& k) { return l.substr(k.size()); };
            if (in_pos) {
                if (l == "PI") { s.start.hirate(); board_seen = true; }
                else if (l.size() > 2 && l[0] == 'P' && l[1] >= '1' && l[1] <= '9') { if (!board_seen) { s.start.clear(); board_seen = true; } s.start.row(l[1] - '0', l.substr(2)); }
                else if (l.rfind("P+", 0) == 0 || l.rfind("P-", 0) == 0) s.start.hands(l);
                else if (l == "+" || l == "-") s.start.side = l == "+" ? 0 : 1;
                else if ((l[0] == '+' || l[0] == '-') && l.size() >= 7) {
                    std::stringstream ss(l); std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        if (tok.size() >= 7 && (tok[0] == '+' || tok[0] == '-')) s.init_moves.push_back({tok, 0});
                        else if (!tok.empty() && (tok[0] == 'T' || tok[0] == 't') && !s.init_moves.empty()) s.init_moves.back().second = std::atoll(tok.c_str() + 1);
                    }
                }
                continue;
            }
            if (!in_time.empty()) {
                SideTime* st = in_time == "c" ? nullptr : &s.side[in_time == "+" ? 0 : 1];
                auto ms = [&](const std::string& v) { return (long long)(std::atof(v.c_str()) * s.unit_ms + 0.5); };
                if (l.rfind("Time_Unit:", 0) == 0) s.unit_ms = parse_unit(val("Time_Unit:"));
                else if (l.rfind("Total_Time:", 0) == 0) (st ? st->total_ms : s.total_ms) = ms(val("Total_Time:"));
                else if (l.rfind("Byoyomi:", 0) == 0) { (st ? st->byoyomi_ms : s.byoyomi_ms) = ms(val("Byoyomi:")); if (!st) s.byoyomi_seen = true; }
                else if (l.rfind("Increment:", 0) == 0) (st ? st->inc_ms : s.inc_ms) = ms(val("Increment:"));
                else if (l.rfind("Least_Time_Per_Move:", 0) == 0) (st ? st->least_ms : s.least_ms) = ms(val("Least_Time_Per_Move:"));
                continue;
            }
            if (l.rfind("Game_ID:", 0) == 0) s.game_id = val("Game_ID:");
            else if (l.rfind("Your_Turn:", 0) == 0) s.my_turn = val("Your_Turn:");
            else if (l.rfind("To_Move:", 0) == 0) s.to_move = val("To_Move:");
            else if (l.rfind("Name+:", 0) == 0) s.name_b = val("Name+:");
            else if (l.rfind("Name-:", 0) == 0) s.name_w = val("Name-:");
        }
        if (s.byoyomi_ms < 0) s.byoyomi_ms = (s.inc_ms > 0 || s.side[0].inc_ms > 0 || s.side[1].inc_ms > 0) ? 0 : 0;
        return true;
    }

    void account(int col, long long t_units, bool add_inc) {
        long long ms = t_units * unit_ms_;
        rem[col] = (std::max)(0LL, rem[col] - ms);
        if (add_inc) { rem[col] += inc_[col]; moves_by[col]++; }
        long long cap = total0[col] + inc_[col] * moves_by[col];
        if (add_inc && rem[col] > cap) rem[col] = cap;
    }
    long long unit_ms_ = 1000; int me_ = 0;
    std::string last_info;   
    std::string csa_comment(const Board& b0, const std::string& bm, const std::string& pm) const {
        long long score = 0; std::vector<std::string> pv;
        { std::stringstream ss(last_info); std::string t;
          while (ss >> t) {
              if (t == "cp") { ss >> score; }
              else if (t == "mate") { std::string m; ss >> m; long long n = std::atoll(m.c_str()); score = (m.find('-') == std::string::npos ? 1 : -1) * (100000 - std::llabs(n)); }
              else if (t == "pv") { while (ss >> t) pv.push_back(t); }
          } }
        if (pv.empty() || pv[0] != bm) { pv = {bm}; if (!pm.empty()) pv.push_back(pm); }
        std::string out = std::string("'* ") + (score >= 0 ? "+" : "") + std::to_string(score);
        Board b = b0;
        for (auto& u : pv) { std::string c = b.usi_to_csa(u); if (c.empty() || b.apply_csa(c).empty()) break; out += " " + c; }
        return out;
    }
    std::string go_args() const {
        std::string a = " btime " + std::to_string(rem[0]) + " wtime " + std::to_string(rem[1]);
        if (inc_[0] > 0 || inc_[1] > 0) a += " binc " + std::to_string(inc_[0]) + " winc " + std::to_string(inc_[1]);
        else a += " byoyomi " + std::to_string(byo[me_]);
        return a;
    }
    static long long parse_T(const std::string& l) {
        size_t p = l.find(",T");
        return p == std::string::npos ? 0 : std::atoll(l.c_str() + p + 2);
    }

    bool play(const Summary& s) {
        unit_ms_ = s.unit_ms;
        for (int c = 0; c < 2; ++c) {
            total0[c] = s.side[c].total_ms >= 0 ? s.side[c].total_ms : s.total_ms;
            inc_[c] = s.side[c].inc_ms >= 0 ? s.side[c].inc_ms : s.inc_ms;
            byo[c] = s.side[c].byoyomi_ms >= 0 ? s.side[c].byoyomi_ms : (std::max)(0LL, s.byoyomi_ms);
            rem[c] = total0[c]; moves_by[c] = 0;
        }
        int me = s.my_turn == "+" ? 0 : 1; me_ = me;
        Board b = s.start;
        std::string start_sfen = b.sfen();
        std::vector<std::string> usi_moves, kifu;
        for (auto& im : s.init_moves) {               
            int col = im.first[0] == '+' ? 0 : 1;
            std::string u = b.apply_csa(im.first);
            if (u.empty()) { L("bad init move " + im.first); return true; }
            usi_moves.push_back(u); kifu.push_back(im.first + ",T" + std::to_string(im.second));
            account(col, im.second, false);
        }
        std::string kpath = cfg.log_dir + "/" + (s.game_id.empty() ? std::string("game") : s.game_id) + ".csa";
        std::ofstream kf(kpath);
        kf << "V2.2\nN+" << s.name_b << "\nN-" << s.name_w << "\n$EVENT:" << s.game_id << "\n";
        kf << "' start sfen " << start_sfen << "\n";

        send("AGREE");
        std::string l;
        for (;;) {
            if (!server_line(l)) return false;
            if (l.rfind("START:", 0) == 0) break;
            if (l.rfind("REJECT:", 0) == 0) { L("game rejected"); return true; }
        }
        engine("usinewgame");
        auto position = [&](const std::vector<std::string>& mv) {
            std::string c = "position sfen " + start_sfen;
            if (!mv.empty()) { c += " moves"; for (auto& m : mv) c += " " + m; }
            return c;
        };
        enum { IDLE, THINK, PONDER } state = IDLE; last_info.clear();
        std::string ponder_move, result;
        auto think = [&] { engine(position(usi_moves)); engine("go" + go_args()); state = THINK; };
        auto stop_and_drain = [&] {
            if (state == IDLE) return;
            engine("stop"); std::string bm; wait_engine("bestmove", bm, 60); state = IDLE;
        };
        if (b.side == me) think();
        bool over = false;
        while (!over) {
            Event e;
            if (!next_event(e, 500)) { keepalive(); continue; }
            if (e.src == 2) { stop_and_drain(); engine("gameover lose"); return false; }
            if (e.src == 1) {
                if (state == THINK && e.line.rfind("info", 0) == 0 && e.line.find(" pv ") != std::string::npos) last_info = e.line;
                if (e.line.rfind("bestmove", 0) != 0 || state != THINK) continue;
                L("E< " + e.line);
                std::stringstream ss(e.line); std::string tok, mv, pm;
                ss >> tok >> mv; if (ss >> tok && tok == "ponder") ss >> pm;
                state = IDLE;
                if (mv == "resign") { send("%TORYO"); continue; }
                if (mv == "win") { send("%KACHI"); continue; }
                std::string cm = b.usi_to_csa(mv);
                if (cm.empty()) { L("cannot convert bestmove " + mv + " -> resign"); send("%TORYO"); continue; }
                pending_echo = cm; ponder_move = (cfg.ponder ? pm : "");
                send(cfg.comment ? cm + "," + csa_comment(b, mv, pm) : cm);
                last_info.clear();
                continue;
            }
            l = e.line;
            if (!l.empty()) L("S< " + l);
            if (l.empty()) continue;
            if (l[0] == '+' || l[0] == '-') {
                std::string cm = l.substr(0, 7); long long T = parse_T(l); int col = l[0] == '+' ? 0 : 1;
                if (col == me) {
                    if (cm != pending_echo) { L("unexpected own echo " + l + " (not counted)"); continue; }
                    pending_echo.clear();
                }
                std::string u = b.apply_csa(cm);
                if (u.empty()) { L("bad move from server " + l); continue; }
                usi_moves.push_back(u); kifu.push_back(l); kf << l << "\n"; kf.flush();
                account(col, T, true);
                if (col == me) {
                    if (!ponder_move.empty()) {
                        auto pv = usi_moves; pv.push_back(ponder_move);
                        engine(position(pv)); engine("go ponder" + go_args()); state = PONDER;
                    }
                } else {
                    if (state == PONDER && u == ponder_move) { engine("ponderhit"); state = THINK; }
                    else { stop_and_drain(); think(); }
                    ponder_move.clear();
                }
                continue;
            }
            if (l[0] == '%') { kf << l << "\n"; continue; }   
            if (l[0] == '#') {
                kf << "'" << l << "\n";
                if (l == "#WIN" || l == "#LOSE" || l == "#DRAW" || l == "#CENSORED" || l == "#CHUDAN") {
                    result = l == "#WIN" ? "win" : l == "#LOSE" ? "lose" : "draw";
                    over = true;
                }
                continue;
            }
        }
        stop_and_drain();
        engine("gameover " + result);
        kf.close();
        L("game over " + result + " kifu " + kpath + " rem b " + std::to_string(rem[0]) + " w " + std::to_string(rem[1]));
        return true;
    }

public:
    explicit Client(const Config& c) : cfg(c) {}
    EventQueue& queue() { return q; }
    std::ofstream& logfile() { return log; }
    void open_log() {
#if defined(_WIN32)
        CreateDirectoryA(cfg.log_dir.c_str(), nullptr);
#else
        std::string mk = "mkdir -p '" + cfg.log_dir + "'"; if (std::system(mk.c_str())) {}
#endif
        log.open(cfg.log_dir + "/csa_protocol.log", std::ios::app);
    }
    void attach_input(PipeInBuf* p) { in = p; }
    void run() {
        std::string l;
        engine("usi");
        if (!wait_engine("usiok", l, 60)) { L("engine: no usiok"); engine("quit"); return; }
        if (!cfg.options_file.empty()) {
            std::ifstream f(cfg.options_file); std::string o;
            while (std::getline(f, o)) { if (!o.empty() && o.back() == '\r') o.pop_back(); if (o.rfind("setoption", 0) == 0) engine(o); }
        }
        for (auto& kv : cfg.setoptions) {
            size_t p = kv.find('=');
            if (p != std::string::npos) engine("setoption name " + kv.substr(0, p) + " value " + kv.substr(p + 1));
        }
        engine(std::string("setoption name USI_Ponder value ") + (cfg.ponder ? "true" : "false"));
        engine("isready");
        if (!wait_engine("readyok", l, 600)) { L("engine: no readyok"); engine("quit"); return; }
        pushback.clear();
        int played = 0, backoff = 10;
        for (;;) {
            std::string err;
            sock = sock_connect(cfg.host, cfg.port, err);
            if (sock == BAD_SOCK) {
                L("connect " + cfg.host + ":" + std::to_string(cfg.port) + " failed: " + err);
                if (!cfg.reconnect) break;
                std::this_thread::sleep_for(std::chrono::seconds(backoff)); continue;
            }
            start_reader();
            L("connected " + cfg.host + ":" + std::to_string(cfg.port));
            if (!login()) { L("LOGIN failed"); stop_reader(); if (!cfg.reconnect) break; std::this_thread::sleep_for(std::chrono::seconds(backoff)); continue; }
            bool alive = true;
            while (alive) {
                Summary s; bool disc = false;
                if (!read_summary(s, disc)) { alive = false; break; }
                alive = play(s);
                if (alive) ++played;
                if (cfg.games && played >= cfg.games) break;
            }
            if (alive) send("LOGOUT");
            stop_reader();
            if ((cfg.games && played >= cfg.games) || !cfg.reconnect) break;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        L("client done, games " + std::to_string(played));
        engine("quit");
    }
};

static PipeInBuf g_in;
static std::unique_ptr<TeeOutBuf> g_out;
static std::unique_ptr<Client> g_client;
static std::thread g_thread;

static bool parse_args(int argc, char* argv[], Config& c) {
    if (argc < 3 || std::string(argv[1]) != "csa") return false;
    const std::string mode = argv[2];
    int first_kv = 7;
    if (mode == "server") {
        if (argc < 7) return false;
        c.host = argv[3]; c.port = std::atoi(argv[4]); c.id = argv[5]; c.pw = argv[6];
    } else if (mode == "floodgate" || mode == "dr") {
        c.profile = mode; first_kv = 3;
        if (mode == "floodgate") { c.host = "wdoor.c.u-tokyo.ac.jp"; c.port = 4081; c.gamename = "floodgate-300-10F"; c.comment = true; c.games = 0; c.reconnect = true; }
        else { c.host = "160.16.80.27"; c.port = 4081; c.games = 1; c.reconnect = true; }
        std::ifstream lf("csa_login.txt"); std::string l;
        while (std::getline(lf, l)) { std::stringstream ss(l); std::string pr, id, pw; if (ss >> pr >> id >> pw && pr == mode) { c.id = id; c.pw = pw; } }
    } else return false;
    for (int i = first_kv; i < argc; ++i) {
        std::string a = argv[i]; size_t p = a.find('=');
        if (p == std::string::npos) continue;
        std::string k = a.substr(0, p), v = a.substr(p + 1);
        if (k == "options") c.options_file = v;
        else if (k == "ponder") c.ponder = v != "0";
        else if (k == "x1") c.x1 = v != "0";
        else if (k == "utf8") c.utf8 = v != "0";
        else if (k == "games") c.games = std::atoi(v.c_str());
        else if (k == "reconnect") c.reconnect = v != "0";
        else if (k == "keepalive") c.keepalive_s = std::atoi(v.c_str());
        else if (k == "log") c.log_dir = v;
        else if (k == "setoption") c.setoptions.push_back(v);
        else if (k == "id") c.id = v;
        else if (k == "pw") c.pw = v;
        else if (k == "host") c.host = v;
        else if (k == "port") c.port = std::atoi(v.c_str());
        else if (k == "gamename") c.gamename = v;
        else if (k == "comment") c.comment = v != "0";
    }
    return true;
}
static bool start_if_requested(int argc, char* argv[]) {
    Config c;
    if (argc >= 2 && std::string(argv[1]) == "csa" && !parse_args(argc, argv, c)) {
        std::cerr << "usage: " << argv[0] << " csa server <host> <port> <id> <password> [k=v ...]\n"
                  << "       " << argv[0] << " csa floodgate [id=ID pw=PW gamename=floodgate-300-10F ...]  (wdoor.c.u-tokyo.ac.jp:4081, continuous, comments)\n"
                  << "       " << argv[0] << " csa dr [id=ID pw=PW ...]  (denryu-sen 160.16.80.27:4081, 1 game, plain password)\n"
                  << "  k=v: options=FILE ponder=0|1 x1=0|1 utf8=0|1 games=N reconnect=0|1 keepalive=S log=DIR setoption=Name=Value host= port= comment=0|1\n"
                  << "  floodgate / dr id + pw: id= pw= or ./csa_login.txt lines '<floodgate|dr> <id> <pw>' (machine-local)\n";
        std::exit(2);
    }
    if (argc < 2 || std::string(argv[1]) != "csa") return false;
    if (c.id.empty() || c.pw.empty()) { std::cerr << "csa: no login id / password (id= pw= or csa_login.txt)\n"; std::exit(2); }
    g_client.reset(new Client(c));
    g_client->open_log();
    g_out.reset(new TeeOutBuf(std::cout.rdbuf(), &g_client->queue(), nullptr));   
    std::cout.rdbuf(g_out.get());
    std::cin.rdbuf(&g_in);
    g_client->attach_input(&g_in);
    g_thread = std::thread([] { g_client->run(); });
    g_thread.detach();
    return true;
}

}  
