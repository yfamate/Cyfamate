#include "search.h"
#include "evaluate.h"

namespace YaneuraOu {

Search::Worker::Worker(SharedState& sharedState,
#if STOCKFISH
                       std::unique_ptr<ISearchManager> sm,
                       size_t                          threadIdx,
                       size_t                          numaThreadId,
                       size_t                          numaTotalThreads,
                       NumaReplicatedAccessToken       token
#else
                       const ThreadIds& ids
#endif
) : 
	options(sharedState.options),
	threads(sharedState.threads),
#if STOCKFISH
	threadIdx(threadIdx),
    numaThreadIdx(numaThreadId),
    numaTotal(numaTotalThreads),
    numaAccessToken(token),
    manager(std::move(sm)),
#else
	threadIdx(ids.threadIdx),
    numaThreadIdx(ids.numaThreadIdx),
    numaTotal(ids.numaTotal),
    numaAccessToken(ids.numaAccessToken),
#endif
	tt(sharedState.tt)
{

}

#if 0
void Search::Worker::ensure_network_replicated() {
    (void) (networks[numaAccessToken]);
}
#endif




} 

