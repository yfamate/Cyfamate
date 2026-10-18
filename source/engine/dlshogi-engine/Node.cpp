#include "Node.h"
#if defined(YANEURAOU_ENGINE_DEEP)
#include "../../misc.h"

namespace dlshogi {


	std::mutex Node::mtx_dfpn;

	Node* Node::ReleaseChildrenExceptOne(NodeGarbageCollector* gc, const Move move)
	{
		if (child_num > 0 && child_nodes) {
			bool found = false;

			for (int i = 0; i < child_num; ++i)
			{
				auto& uct_child  = child[i];
				auto& child_node = child_nodes[i];
                if (uct_child.move == move)
                {
					found = true;
					if (!child_node)
	                    child_node = std::make_unique<Node>();

					if (i != 0) {
						child[0]       = std::move(uct_child);
						child_nodes[0] = std::move(child_node);
					}
				}
				else {
					if (child_node)
						gc->AddToGcQueue(std::move(child_node));
				}
			}

			if (found) {
				child_num = 1;
				return child_nodes[0].get();
			}
			else {
				CreateSingleChildNode(move);
				InitChildNodes();
				return (child_nodes[0] = std::make_unique<Node>()).get();
			}
		}
		else {
			CreateSingleChildNode(move);
			InitChildNodes();
			return (child_nodes[0] = std::make_unique<Node>()).get();
		}
	}


	bool NodeTree::ResetToPosition(const std::string& game_root_sfen , const std::vector<Move>& moves)
	{

		if (game_root_node && this->game_root_sfen != game_root_sfen)
		{
			DeallocateTree();
			this->game_root_sfen = game_root_sfen;
		}

		if (!game_root_node) {
			game_root_node = std::make_unique<Node>();
			current_head   = game_root_node.get();
		}

		Node* old_head = current_head;

		Node* prev_head = nullptr;

		current_head = game_root_node.get();

		bool seen_old_head = (game_root_node.get() == old_head);

		for (const auto& move : moves) {
			prev_head = current_head;
			
			current_head = current_head->ReleaseChildrenExceptOne(gc, move);
			
			seen_old_head |= (old_head == current_head);
		}

		if (!seen_old_head && current_head != old_head)
		{
			if (prev_head)
			{
				ASSERT_LV3(prev_head->child_num == 1);
				auto& prev_uct_child_node = prev_head->child_nodes[0];
				gc->AddToGcQueue(std::move(prev_uct_child_node));
				prev_uct_child_node = std::make_unique<Node>();
				current_head = prev_uct_child_node.get();
			}
			else {
				DeallocateTree();
			}
		}

		return seen_old_head;
	}

	void NodeTree::DeallocateTree()
	{
		gc->AddToGcQueue(std::move(game_root_node));

		game_root_node = std::make_unique<Node>();
		current_head = game_root_node.get();
	}

} 

#endif 
