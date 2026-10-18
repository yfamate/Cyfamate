

#if defined(USE_MATE_DFPN)





#if defined(DFPN64) || defined(DFPN32)

#include <mutex>
#include "../position.h"
#include "../thread.h"
#include "mate_move_picker.h"

namespace YaneuraOu {

#if defined (DFPN64)

namespace Mate::Dfpn64
{
	
	
	

	
	

	template <typename NodeCountType , bool MoveOrdering>
	struct alignas(32) Node
	{
		
		
		
		static constexpr NodeCountType DNPN_INF  = std::numeric_limits<NodeCountType>::max();
		static constexpr NodeCountType DNPN_MATE = std::numeric_limits<NodeCountType>::max() - 2000;

		
		static constexpr u8 CHILDNUM_NOT_INIT = std::numeric_limits<u8>::max();

		
		Node* children;

		
		
		
		
		
		
		NodeCountType pn, dn;

		
		Move lastMove;

		
		
		

		
		
		
		u8 child_num;

		
		
		bool repeated;

		u8 padding[2]; 

		
		template <bool or_node>
		void init(ExtMove m)
		{
			lastMove = m;

			if (MoveOrdering)
			{
				
				pn = -m.value + 30000; 
				dn = +m.value + 30000; 
			}
			else {
				pn = dn = 1;
			}

			children = nullptr;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void init_mate_move(Move move)
		{
			this->lastMove = move;
			this->template set_mate<true>();

			children = nullptr;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void set_mate(int ply = 0)
		{
			
			
			this->pn = or_node ? 0                             : NodeCountType(DNPN_INF - ply);
			this->dn = or_node ? NodeCountType(DNPN_INF - ply): 0;
		}

		
		template <bool or_node>
		void set_mated(int ply = 0)
		{
			this->pn = or_node ? NodeCountType(DNPN_INF - ply): 0;
			this->dn = or_node ? 0                             :NodeCountType(DNPN_INF - ply);
		}

		
		void set_nomate(int ply = 0)
		{
			this->pn = NodeCountType(DNPN_INF - ply);
			this->dn = 0; 
		}

		
		void set_child(u8 child_num , Node* children = nullptr)
		{
			this->child_num = child_num;
			this->children = children;
		}
	};

#endif 

#if defined (DFPN32)


namespace Mate::Dfpn32
{
	
	
	

	
	
	
	

	template <typename NodeCountType , bool MoveOrdering>
	struct alignas(16) Node
	{
		
		static constexpr NodeCountType NodeCountTypeNull = std::numeric_limits<NodeCountType>::max();

		
		
		
		static constexpr NodeCountType DNPN_INF  = std::numeric_limits<NodeCountType>::max();
		static constexpr NodeCountType DNPN_MATE = std::numeric_limits<NodeCountType>::max() - 2000;

		
		static constexpr u8 CHILDNUM_NOT_INIT = std::numeric_limits<u8>::max();

		
		static bool is_nullptr(NodeCountType n) { return n == NodeCountTypeNull; }

		
		
		NodeCountType children;

		
		
		
		
		
		
		NodeCountType pn, dn;

		
		
		u32 lastMove  : 23;

		
		
		u32 repeated  : 1;

		
		
		
		
		
		u32 child_num :  8;

		
		

		
		template <bool or_node>
		void init(ExtMove m)
		{
			lastMove = m.to_u32();

			if (MoveOrdering)
			{
				
				pn = -m.value + 30000; 
				dn = +m.value + 30000; 
			}
			else {
				pn = dn = 1;
			}

			
			children = NodeCountTypeNull;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void init_mate_move(Move move ,int ply = 0)
		{
			this->lastMove = move.to_u32();
			this->template set_mate<true>(ply);

			
			children = NodeCountTypeNull;
			child_num = CHILDNUM_NOT_INIT;
		}

		
		
		template <bool or_node>
		void set_mate(int ply = 0)
		{
			
			
			this->pn = or_node ? 0                             : NodeCountType(DNPN_INF - ply);
			this->dn = or_node ? NodeCountType(DNPN_INF - ply): 0;
		}

		
		template <bool or_node>
		void set_mated(int ply = 0)
		{
			this->pn = or_node ? NodeCountType(DNPN_INF - ply): 0;
			this->dn = or_node ? 0                             :NodeCountType(DNPN_INF - ply);
		}

		
		void set_nomate(int ply = 0)
		{
			this->pn = NodeCountType(DNPN_INF - ply);
			this->dn = 0; 
		}

		
		void set_child(u8 child_num , NodeCountType children = NodeCountTypeNull)
		{
			this->child_num = child_num;
			this->children = children;
		}
	};

#endif 

	

	
	
	

	
	
	template <typename NodeCountType , bool MoveOrdering>
	struct NodeManager
	{
		
		typedef Node<NodeCountType,MoveOrdering> NodeType;

		NodeManager() :node_index(0), nodes_num(0) {}

		
		void alloc(size_t size_) {
			
			release();

			this->nodes_num = (NodeCountType)size_;
			nodes = std::make_unique<NodeType[]>(size_);
		}

		
		void release() { nodes.reset(); }

		
		NodeCountType size() const { return nodes_num; }

		
		
		NodeType* new_node(size_t size = 1)
		{
			
			

			if (is_out_of_memory())
				return nullptr;

			NodeType* node = &nodes[node_index];
			node_index += (NodeCountType)size;
			return node;
		}

		
		
		
		void reset_counter() { node_index = 0; }

		
		bool is_out_of_memory() const {
			
			return node_index + MaxCheckMoves >= nodes_num;
		}

		
		int hashfull() const { return (int)((u64)node_index * 1000 / nodes_num); }


#if defined(DFPN32)
		
		
		
		

		
		NodeType* get_children(const NodeType* node) const
		{
			return node_index_to_node(node->children);
		}

		
		void set_children(NodeType* node, const NodeType* next) const
		{
			node->children = node_to_node_index(next);
		}

		
		
		NodeType* node_index_to_node(NodeCountType n) const {
			return ! NodeType::is_nullptr(n) ? &nodes[n] : nullptr;
		}

		
		
		NodeCountType node_to_node_index(const NodeType*node) const
		{
			
			return (node == nullptr) ? NodeType::NodeCountTypeNull : NodeCountType(node - nodes.get());
		}
#endif

#if defined(DFPN64)
		

		NodeType* get_children(const NodeType* node) const { return node->children; }
		void set_children(NodeType* node, const NodeType* next) const { node->children = next; }
		NodeType* node_index_to_node(NodeType* n) const { return n; }
		NodeType* node_to_node_index(NodeType* node) const { return node; }
#endif

	private:

		
		
		std::unique_ptr<Node<NodeCountType,MoveOrdering>[]> nodes;

		
		NodeCountType nodes_num;

		
		std::atomic<NodeCountType> node_index;

		
		
	};


	
	
	

	
	
	template <typename NodeCountType , bool MoveOrdering , bool WithHash >
	class MateDfpnPn : public Mate::Dfpn::MateDfpnSolverInterface
	{
	public:
		
		typedef Node<NodeCountType,MoveOrdering> NodeType;

		
		
		virtual void alloc(size_t size_mb) {
			
			release();

			
			size_t size = size_mb * 1024 * 1024;
			node_manager.alloc(size / sizeof(Node<NodeCountType,MoveOrdering>));
		}

		
		
		virtual void alloc_by_nodes_limit(size_t nodes_limit)
		{
			
			release();

			node_manager.alloc(nodes_limit);
		}

		
		
		
		
		virtual void set_hash_table(MateHashTable* hash_table)
		{
			this->hash_table = hash_table;
		}

		
		void release()
		{
			node_manager.release();
		}

		
		
		
		virtual void set_max_game_ply(int max_game_ply)
		{
			this->max_game_ply = max_game_ply;
		}

		
		
		
		virtual Move mate_dfpn(const Position& pos_, u64 nodes_limit)
		{
			
			auto& pos = *const_cast<Position*>(&pos_);

			nodes_searched = 0;
			this->nodes_limit = (NodeCountType)nodes_limit;
			bestmove = Move::none();

			
			node_manager.reset_counter();
			out_of_memory = false;

			
			ExpandRoot(pos);

			
			ParallelSearch(pos);

			
			if (current_root->pn == 0 && current_root->dn >= NodeType::DNPN_MATE)
			{
#if 0
				std::cout << "solved! ply = " << get_mate_ply() << ", nodes_searched = " << nodes_searched << std::endl;
				
#endif

				
				auto node = current_root;
				return pick_the_best<true, true, false>(node);
			}

			
			if (current_root->pn >= NodeType::DNPN_MATE && current_root->dn == 0)
				return Move::null();

			
			
			return Move::none();
		}

		
		
		virtual std::vector<Move> get_pv() const { return get_pv<true,false>(); }

		
		virtual std::vector<Move> get_current_pv() const { return get_pv<true,true>(); }
		
		
		
		
		std::vector<Move> get_unproof_pv() const { return get_pv<false,false>(); }

		
		int get_mate_ply() const {
			if (current_root->pn != 0 || current_root->dn < NodeType::DNPN_MATE)
				return -1; 
			return int(NodeType::DNPN_INF - current_root->dn);
		}

		
		virtual u64 get_nodes_searched() const {
			return (u64)nodes_searched;
		}

		
		virtual bool is_out_of_memory() const { return node_manager.is_out_of_memory(); }

		
		virtual int hashfull() const { return node_manager.hashfull(); }

	protected:

		
		
		
		template <bool proof , bool current>
		std::vector<Move> get_pv() const {
			std::vector<Move> pv;

			auto node = current_root;
			
			bool or_node = true;
			while (node) {
				
				
				
				Move move = or_node ? pick_the_best<true,proof,current>(node) : pick_the_best<false,proof,current>(node);
				if (move == Move::none())
					break;

				pv.push_back(move);
				or_node ^= true; 
			}
			return pv;
		}

		
		
		
		
		
		
		template <bool or_node, bool proof,bool current>
		Move pick_the_best(NodeType*& node) const
		{
			NodeType* children = node_manager.get_children(node);
			if ( children == nullptr || node->child_num == 0)
			{
				node = nullptr;
				return Move::none(); 
			}

			
			int child_num = node->child_num;

			int selected_index = 0;
			if (proof)
			{
				for (int i = 1;  i < child_num ; ++i)
					if (or_node)
					{
						if (current)
						{
							
							if (children[i].pn < children[selected_index].pn
								|| (children[i].pn == children[selected_index].pn && children[i].dn > children[selected_index].dn))
								selected_index = i;
						}
						else {
							
							
							if (children[i].dn > children[selected_index].dn)
								selected_index = i;
						}
					}
					else {
						if (current)
						{
							
							if (children[i].pn < children[selected_index].pn
								|| (children[i].pn == children[selected_index].pn && children[i].dn < children[selected_index].dn))
								selected_index = i;
						}
						else {
							
							
							if (children[i].dn < children[selected_index].dn)
								selected_index = i;
						}
					}
			}
			else {
				
				
				for (int i = 1;  i < child_num ; ++i)
					if (or_node)
					{
						if (children[i].pn > children[selected_index].pn)
							selected_index = i;
					}
					else {
						if (children[i].pn < children[selected_index].pn)
							selected_index = i;
					}
			}

			auto& selected = children[selected_index];
			node = &selected;

			return (Move)selected.lastMove;
		}

		
		void dump_tree(NodeType* node , std::string pv = "")
		{
			NodeType* children = node_manager.get_nodeptr(node);
			if (children == nullptr)
			{
				std::cout << pv << std::endl;
				return;
			}
			int child_num = node->child_num;
			if (child_num == 0)
			{
				std::cout << pv << std::endl;
				return;
			}

			bool found = false;
			for (int i = 0; i < child_num; ++i)
			{
				if (node->children[i].pn == 0)
				{
					found = true;
					
					auto pv2 = pv + "[" + std::to_string(node->dn) + "] " + to_usi_string(children[i].move);
					NodeType* next = node->children[i].children;
					dump_tree(next, pv2);
				}
			}

			if (!found)
				std::cout << "mate not found : " << pv << std::endl;
		}

		
		void ParallelSearch(Position& pos)
		{
			ParallelSearch<true>(pos, current_root , NodeType::DNPN_INF , NodeType::DNPN_INF);
		}

		
		
		
		template <bool or_node>
		void ParallelSearch(Position& pos , NodeType* node , NodeCountType second_pn , NodeCountType second_dn)
		{
			ASSERT_LV3(node->pn <= second_pn && node->dn <= second_dn);

			
			
			 while (
				    node->pn <= second_pn
				 && node->dn <= second_dn
				 && node->pn
				 && node->dn
				 && !out_of_memory
				 
				 && (!nodes_limit ||
						 ( MoveOrdering && (nodes_limit - nodes_searched > (std::max(current_root->pn, node->pn) >>16) )) ||
						 (!MoveOrdering && (nodes_limit - nodes_searched > (std::max(current_root->pn, node->pn)))))
				 
				 
				 
				 && !stop 
				)
			{
#if 0
				 
				 std::cout << pos << std::endl;
#endif

				u8 child_num = node->child_num;
				if (child_num == NodeType::CHILDNUM_NOT_INIT)
				{
					ExpandNode<or_node>(pos, node);
					

					continue;
				}

				NodeCountType second_pn2 = second_pn;
				NodeCountType second_dn2 = second_dn;
				auto best_child = select_the_best_child<or_node>(node, second_pn2, second_dn2);

				
				StateInfo si;
				Move m = pos.to_move(Move16(u16(u32(best_child->lastMove))));
				

				pos.do_move(m, si);

				
				ParallelSearch<!or_node>(pos, best_child ,second_pn2 , second_dn2);

				
				SummarizeNode<or_node>(node);

				pos.undo_move(m);

				
				if (WithHash && (node->dn == 0 || node->pn == 0) && !node->repeated)
				{
					Key key = pos.state()->board_key;
					auto entry = hash_table->first_entry(key, pos.side_to_move());

					
					bool is_mate = node->pn == 0; 
					auto ply = is_mate ? (u32)(NodeType::DNPN_INF - node->dn) : (u32)(NodeType::DNPN_INF - node->pn);
					entry->save(key, root_color , pos.state()->hand , is_mate , ply, m);

#if 0
					
					std::cout << pos << std::endl
							  << " is_mate = " << is_mate << std::endl
							  << " ply     = " << ply << std::endl
							  << " move    = " << m << std::endl;
#endif
				}

			 }
		}

		
		
		
		
		template <bool or_node>
		NodeType* select_the_best_child(NodeType* node,NodeCountType& second_pn,NodeCountType& second_dn)
		{
			auto children  = node_manager.get_children(node);
			u32 child_num = node->child_num;

			u32 selected_index = 0;
			NodeCountType pn2 = second_pn;
			NodeCountType dn2 = second_dn;

			if (or_node)
			{
				
				for (u32 i = 1; i < child_num; ++i)
					if (children[i].pn < children[selected_index].pn)
						selected_index = i;

				
				
				for (u32 i = 0; i < child_num; ++i)
					if (i != selected_index)
					{
						if (children[i].pn < pn2)
							pn2 = children[i].pn;

						
						
						
						
						
						

						
						if (children[i].dn < NodeType::DNPN_MATE)
							dn2 -= children[i].dn;
					}

				
				dn2 = std::min(dn2, second_dn);
			}
			else {
				
				for (u32 i = 1; i < child_num; ++i)
					if (children[i].dn < children[selected_index].dn)
						selected_index = i;

				for (u32 i = 0; i < child_num; ++i)
					if (i != selected_index)
					{
						if (children[i].dn < dn2)
							dn2 = children[i].dn;

						if (children[i].pn < NodeType::DNPN_MATE)
							pn2 -= children[i].pn;
					}

				pn2 = std::min(pn2, second_pn);
			}

			
			second_pn = pn2;
			second_dn = dn2;

			return &children[selected_index];
		}

		
		template <bool or_node>
		void SummarizeNode(NodeType* node)
		{
			NodeCountType pn = or_node ? NodeType::DNPN_INF : 0                 ;
			NodeCountType dn = or_node ? 0                  : NodeType::DNPN_INF;

			u32 child_num = node->child_num;
			ASSERT_LV3(child_num != NodeType::CHILDNUM_NOT_INIT);
			NodeType* children = node_manager.get_children(node); 

			for (u32 i = 0; i < child_num ; ++i)
			{
				
				NodeCountType p = children[i].pn;
				NodeCountType d = children[i].dn;

				

				if (p >= NodeType::DNPN_MATE) --p;
				if (d >= NodeType::DNPN_MATE) --d;

				
				if (or_node)
				{
					
					
					
					

					pn = std::min(pn, p);

					
					
					

					if (dn >= NodeType::DNPN_MATE || d >= NodeType::DNPN_MATE)
						dn = std::max(dn, d);
					else
						dn += d; 
				}
				else {
					
					
					if (pn >= NodeType::DNPN_MATE || p >= NodeType::DNPN_MATE)
						pn = std::max(pn, p);
					else
						pn += p; 

					dn = std::min(dn, d);
				}
			}
			node->pn = pn;
			node->dn = dn;

			if (WithHash)
			{
				
				if (node->pn == 0 || node->dn == 0)
				{
					
					
					

					
					
					


					if (or_node)
						if (node->pn == 0) {
							for (u32 i = 0; i < child_num; ++i)
								if (children[i].pn == 0) {
									node->repeated = children[i].repeated;
									break;
								}
						}
						else {
							bool repeated = false;
							for (u32 i = 0; i < child_num; ++i)
								repeated |= children[i].repeated;
							node->repeated = repeated;
						}
					else 
						if (node->dn == 0) {
							for (u32 i = 0; i < child_num; ++i)
								if (children[i].dn == 0) {
									node->repeated = children[i].repeated;
									break;
								}
						}
						else {
							bool repeated = false;
							for (u32 i = 0; i < child_num; ++i)
								repeated |= children[i].repeated;
							node->repeated = repeated;
						}
				}
				else {
					
					
				}
			}

		}

		
		void ExpandRoot(Position& pos)
		{
			

			current_root = new_node();
			
			
			ASSERT_LV3(current_root != nullptr);

			
			constexpr bool or_node = true;
			current_root->child_num = NodeType::CHILDNUM_NOT_INIT;
			ExpandNode<or_node>(pos, current_root);

			root_game_ply =  pos.game_ply();
			root_color    =  pos.side_to_move();
			root_pos      = &pos;
		}

		
		
		
		
		
		template <bool or_node>
		void ExpandNode(Position& pos, NodeType* node)
		{
			pos.in_check() ? ExpandNode<or_node,true>(pos, node) : ExpandNode<or_node,false>(pos, node);

			nodes_searched++;
		}

		template <bool or_node , bool INCHECK>
		void ExpandNode(Position& pos, NodeType* node)
		{
			ASSERT_LV3(node != nullptr && node->child_num == NodeType::CHILDNUM_NOT_INIT);

			if (WithHash)
			{
				

				Key key = pos.state()->board_key;
				auto entry = hash_table->first_entry(key, pos.side_to_move());

				
				bool proven = false;

				entry->lock();
				if (   entry->board_key  == (key & 0xffffffffffff) 
					&& entry->root_color == root_color
					)
				{
#if 0
					
					std::cout << pos << std::endl
						      << entry->get_move() << std::endl
							  << entry->get_hand() << std::endl;
#endif

					
					if (entry->is_mate) 
					{
						
						
						
						
						
						if (   ( or_node && hand_is_equal_or_superior(pos.state()->hand , entry->get_hand()))
							|| (!or_node && hand_is_equal_or_superior(entry->get_hand() , pos.state()->hand))
							)
						{
							node->template set_mate<true >(entry->ply);
							proven = true;

#if 0
							
							if (or_node)
							{
								Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
								dfpn.alloc(1024);
								Move move = dfpn.mate_dfpn(pos, 1000000);
								if (move == MOVE_NONE || move == MOVE_NULL)
								{
									
									std::cout << pos << std::endl;
								}
							}
#endif
						}
					}
					else { 
						
						
						
						
						
						if (   ( or_node && hand_is_equal_or_superior(entry->get_hand() , pos.state()->hand))
							|| (!or_node && hand_is_equal_or_superior(pos.state()->hand , entry->get_hand()))
							)
						{
							node->set_nomate(entry->ply);
							proven = true;

#if 0
							
							Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
							dfpn.alloc(1024);
							if (dfpn.mate_dfpn(pos, 1000000) != MOVE_NULL)
							{
								
								std::cout << pos << std::endl;
							}
#endif
						}
					}
				}
				
				Move move = entry->get_move();

				
				entry->unlock();

				if (proven)
				{
					
					
					if (node == current_root)
					{
						NodeType* child = new_node(1);
						
						if (child == nullptr)
							return;

						child->template init_mate_move<or_node>(move);
						node->set_child(1,node_manager.node_to_node_index(child));
					}
					else {
						
						
						node->set_child(0);
					}

					
					node->repeated = false;
					return;
				}

			}

			
			
			
			
			
			
			if (or_node && max_game_ply && max_game_ply < pos.game_ply())
			{
				
				node->set_child(0);
				node->set_nomate();
				if (WithHash)
					node->repeated = true;
				return;
			}

			int plies_from_root = pos.game_ply() - root_game_ply;
			auto rep = mate_repetition(pos, plies_from_root, MAX_REPETITION_PLY, or_node);

			

			
			switch (rep)
			{
			case MateRepetitionState::Mate:
				node->set_child(0);
				node->template set_mate<or_node>();
				if (WithHash)
					node->repeated = true;
				return;

			case MateRepetitionState::Mated:
				node->set_child(0);
				node->template set_mated<or_node>();
				if (WithHash)
					node->repeated = true;
				return;

			case MateRepetitionState::Unknown:
				break;
			}

#if 1
			
			
			if (or_node && !pos.in_check())
			{
				Move mate_move = Mate::mate_1ply(pos);
				if (mate_move != Move::none())
				{
					
					NodeType* child = new_node(1);
					
					if (child == nullptr)
						return;

					child->template init_mate_move<or_node>(mate_move);

					node->set_child(1,node_manager.node_to_node_index(child));
					node->template set_mate<or_node>(1 );

					if (WithHash)
						node->repeated = false;
					
					return;
				}
			}
#endif

			
			auto mp = MovePicker<or_node, INCHECK, true , MoveOrdering>(pos);
			u32 child_num = (u32)mp.size();

			if (!or_node && max_game_ply && max_game_ply < pos.game_ply() && child_num)
			{
				
				
				
				
				
				node->set_child(0);
				node->set_nomate();
				if (WithHash)
					node->repeated = true;
				return;
			}


			node->child_num = child_num;

			if (child_num == 0)
			{
				
				
				node->set_child(0);
				node->template set_mated<or_node>();
			}
			else {
				NodeType* children = new_node(child_num);
				
				if (children == nullptr)
					return;

				
				node->set_child(child_num , node_manager.node_to_node_index(children) );

				for (auto m : mp)
					(children++)->template init<or_node>(m);

				
				
				SummarizeNode<or_node>(node);
			}
		}

		
		NodeType* new_node(size_t size = 1) {
			auto node = node_manager.new_node(size);
			if (node == nullptr)
				out_of_memory = true;
			return node;
		}

	private:
		
		NodeType* current_root;
		Position* root_pos;
		int root_game_ply; 
		Color root_color;  

		
		NodeCountType nodes_limit;

		
		std::atomic<NodeCountType> nodes_searched;

		
		std::atomic<bool> out_of_memory;

		
		Move bestmove;

		
		
		
		int max_game_ply;

		
		MateHashTable* hash_table;

	private:
		
		NodeManager<NodeCountType,MoveOrdering> node_manager;
	};

} 

#endif 

} 

#endif 
