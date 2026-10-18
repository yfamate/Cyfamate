#ifndef TIMEMAN_H_INCLUDED
#define TIMEMAN_H_INCLUDED

#include <cstdint>

#include "misc.h"

namespace YaneuraOu {

class OptionsMap;
enum Color : uint8_t;

namespace Search {
	struct LimitsType;
}







class TimeManagement {
   public:
    
    
    void add_options(OptionsMap& options);

    
    
	
    void init(const Search::LimitsType& limits, Color us, int ply, const OptionsMap& options
#if STOCKFISH
			  , double& originalTimeAdjust
    
#else
			  , int max_moves_to_draw
#endif
    );

    TimePoint optimum() const;
    TimePoint maximum() const;
    TimePoint minimum() const;  

    
    template<typename FUNC>
    TimePoint elapsed(FUNC nodes) const {
#if STOCKFISH
        return useNodesTime ? TimePoint(nodes()) : elapsed_time();
#else
        
        return elapsed_time();
#endif
    }

    
    
    TimePoint elapsed_time() const { return now() - startTime; };


	
    
    void clear() {}

#if STOCKFISH

    void advance_nodes_time(std::int64_t nodes);
    

#else

    

    
    
    TimePoint search_end;

    
	
    TimePoint round_up(TimePoint t);

    
	
	
    void set_search_end(TimePoint e);

    
    
	
	
    TimePoint elapsed_time_from_ponderhit() const { return now() - ponderhitTime; };
    TimePoint remain() const { return remain_time; }   

#endif


#if STOCKFISH
   private:
    TimePoint startTime;      
#else
    bool      isFinalPush;    
							  
    TimePoint startTime;      
    TimePoint ponderhitTime;  
                              
   private:
    TimePoint minimumTime;  
#endif

    TimePoint optimumTime;
    TimePoint maximumTime;

#if STOCKFISH

	std::int64_t availableNodes = -1;     
    bool         useNodesTime   = false;  
    

#else
    

    
    void init_(const Search::LimitsType& limits,
               Color                     us,
               int                       ply,
               const OptionsMap&         options,
               int                       max_moves_to_draw);

    
    TimePoint minimum_thinking_time;
    TimePoint network_delay;
    TimePoint remain_time;
    bool      round_up_to_fullsecond;

	
#if 0
    
	
    Search::LimitsType* lastcall_Limits;
    Color               lastcall_Us;
    int                 lastcall_Ply;
    OptionsMap*         lastcall_Opt;
#endif

#endif
};

}  

#endif  
