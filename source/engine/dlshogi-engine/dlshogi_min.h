#ifndef __DLSHOGI_MIN_H_INCLUDED__
#define __DLSHOGI_MIN_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include <vector>
#include "../../types.h"


namespace YaneuraOu {
namespace dlshogi {

	void GetSearchResult(std::vector<std::pair<Move, float>>& result);

	struct SfenNode
	{
		SfenNode(){}
		SfenNode(const std::string& sfen_, u64 nodes_):
			sfen(sfen_), nodes(nodes_) {}

		std::string sfen;
		u64 nodes;

		bool operator <(const SfenNode& rhs) const
		{
			return nodes > rhs.nodes;
		}
	};
	typedef std::vector<SfenNode> SfenNodeList;

	void GetTopVisitedNodes(size_t n, SfenNodeList& sfens, bool same_color);

	u64 nodes_visited();

} 
} 


#endif 

#endif 
