#ifndef __NODE_H_INCLUDED__
#define __NODE_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include <thread>
#include "../../position.h"
#include "../../movegen.h"
#include "dlshogi_types.h"

namespace dlshogi {

using namespace YaneuraOu;

struct Node;
class NodeGarbageCollector;






struct ChildNode {
    ChildNode() :
        move_count(0),
        win((WinType) 0),
        nnrate(0.0f) {}

    ChildNode(Move move) :
        move(move),
        move_count(0),
        win((WinType) 0),
        nnrate(0.0f) {}

    
    ChildNode(ChildNode&& o) noexcept :
        move(o.move),
        move_count(0),
        win((WinType) o.win),
        nnrate(o.nnrate) {}

    
    ChildNode& operator=(ChildNode&& o) noexcept {
        move       = o.move;
        move_count = (NodeCountType) o.move_count;
        win        = (WinType) o.win;
        nnrate     = (float) o.nnrate;
        return *this;
    }

    

    
    
    
    
    static bool IsMoveWin(Move m) { return m.to_u32() & VALUE_WIN; }
    static bool IsMoveLose(Move m) { return m.to_u32() & VALUE_LOSE; }
    static bool IsMoveDraw(Move m) { return m.to_u32() & VALUE_DRAW; }

    
    
    bool IsWin() const { return IsMoveWin(move); }
    bool IsLose() const { return IsMoveLose(move); }
    bool IsDraw() const { return IsMoveDraw(move); }
    void SetWin() { move = Move(move.to_u32() | VALUE_WIN); }
    void SetLose() { move = Move(move.to_u32() | VALUE_LOSE); }
    void SetDraw() { move = Move(move.to_u32() | VALUE_DRAW); }
    

    
    
    Move move;
	
    Move getMove() const { return Move(move.to_u32() & 0xffffff); }

    
    float nnrate;

    
    
    std::atomic<NodeCountType> move_count;

    
    
    std::atomic<WinType> win;
};



struct Node {
    Node() :
        move_count(NOT_EXPANDED),
        win(0),
        visited_nnrate(0.0f),
        child_num(0),
        dfpn_checked(0),
        dfpn_proven_unsolvable(0)  {}

    
    Node* CreateChildNode(int i) { return (child_nodes[i] = std::make_unique<Node>()).get(); }

    
    void CreateSingleChildNode(const Move move) {
        child_num = 1;
        child     = std::make_unique<ChildNode[]>(1);
        child[0]  = move;
    }

    
    
    
    void ExpandNode(const Position* pos, bool generate_all) {
        

        if (generate_all)
            
            expand_node<LEGAL_ALL>(pos);
        else
            
            expand_node<LEGAL>(pos);
    }

    
    void InitChildNodes() { child_nodes = std::make_unique<std::unique_ptr<Node>[]>(child_num); }

    
    
    
    
    
    
    
    
    
    
    Node* ReleaseChildrenExceptOne(NodeGarbageCollector* gc, const Move move);

    
    
    
    
    
    bool IsEvaled() const { return move_count != NOT_EXPANDED; }

    
    
    void SetEvaled() { move_count = 0; }

    

    
    std::atomic<NodeCountType> move_count;

    
    
    
    
    
    std::atomic<WinType> win;

    
    
    
    
    std::atomic<float> visited_nnrate;

    
    ChildNumType child_num;

    
    
    std::unique_ptr<ChildNode[]> child;

    
    
    
    std::unique_ptr<std::unique_ptr<Node>[]> child_nodes;

#if defined(USE_POLICY_BOOK)
    
    
    float policy_book_value = FLT_MAX;
#endif

    
    bool dfpn_checked;            
    bool dfpn_proven_unsolvable;  
    

    
    static std::mutex mtx_dfpn;

   private:
    
    template<GenType T>
    void expand_node(const Position* pos) {
        MoveList<T> ml(*pos);

        child            = std::make_unique<ChildNode[]>(ml.size());
        auto* child_node = child.get();
        for (auto m : ml)
            (child_node++)->move = m;

        
        child_num = (ChildNumType) ml.size();
    }
};



class NodeTree {
   public:
    NodeTree(NodeGarbageCollector* gc) :
        gc(gc) {}

    
    ~NodeTree() { DeallocateTree(); }

    
    
    
    
    
    
    bool ResetToPosition(const std::string& game_root_sfen, const std::vector<Move>& moves);

    
    Node* GetCurrentHead() const { return current_head; }

   private:
    
    void DeallocateTree();

    
    Node* current_head = nullptr;

    
    
    std::unique_ptr<Node> game_root_node;

    
    
    std::string game_root_sfen;

    
    NodeGarbageCollector* gc;
};







class NodeGarbageCollector {
   public:
    
    NodeGarbageCollector() :
        current_thread_id(-1),
        gc_thread(std::thread([this]() { Worker(); })) {}

    
    const int kGCIntervalMs = 100;

    
    
    
    void AddToGcQueue(std::unique_ptr<Node> node) {
        if (!node)
            return;

        std::lock_guard<std::mutex> lock(gc_mutex);
        subtrees_to_gc.emplace_back(std::move(node));
    }

    ~NodeGarbageCollector() {
        
        stop.store(true);
        gc_thread.join();
    }

    

    
    
    
    
    void set_thread_id(size_t thread_id) { next_thread_id = (int) thread_id; }

   private:
    
    void GarbageCollect() {
        while (!stop.load())
        {

            
            std::unique_ptr<Node> node_to_gc;
            {
                
                
                std::lock_guard<std::mutex> lock(gc_mutex);
                if (subtrees_to_gc.empty())
                    return;
                node_to_gc = std::move(subtrees_to_gc.back());
                
                
                

                subtrees_to_gc.pop_back();
            }
        }
    }

    
    void Worker() {
        
        while (!stop.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kGCIntervalMs));
            GarbageCollect();

            

            
            if (current_thread_id != next_thread_id)
            {
                current_thread_id = next_thread_id.load();

                
                
            }
        };
    }

    
    mutable std::mutex gc_mutex;

    
    
    std::vector<std::unique_ptr<Node>> subtrees_to_gc;

    
    std::atomic<bool> stop{false};
    
    std::thread gc_thread;

    

    
    

    std::atomic<int> current_thread_id;
    std::atomic<int> next_thread_id;
};

}  

#endif 

#endif 
