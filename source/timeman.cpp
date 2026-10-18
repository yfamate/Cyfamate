#include "config.h"

#if defined(USE_TIME_MANAGEMENT)

#include "misc.h"
#include "search.h"
#include "thread.h"
#include "usi.h"
#include "timeman.h"

namespace YaneuraOu {

namespace {



const int MoveHorizon = 160;


PRNG prng;

} 



void TimeManagement::add_options(OptionsMap& options) {

		
#if defined(YANEURAOU_ENGINE_DEEP)
    
    int time_margin = 400;
#else
    int time_margin = 120;
#endif

    
    
    
    options.add("NetworkDelay", Option(time_margin, 0, 10000));

    
    
    
    options.add("NetworkDelay2", Option(time_margin + 1000, 0, 10000));

    
    options.add("MinimumThinkingTime", Option(2000, 1, 100000));

    
    
    
    options.add("SlowMover", Option(100, 1, 1000));

    
    
    
    
    
    options.add("Cyf_TM_Fischer_Horizon", Option(0, 0, 400));
    options.add("Cyf_TM_Inc_Spend", Option(100, 0, 100));
    options.add("Cyf_TM_Fischer_Cap", Option(0, 0, 100));

	
    options.add("RoundUpToFullSecond", true);

}

void TimeManagement::init(const Search::LimitsType& limits,
                          Color               us,
                          int                 ply,
                          const OptionsMap&   options
#if STOCKFISH
                          ,
                          double& originalTimeAdjust

#else
                          ,
                          int max_moves_to_draw
#endif

) {

    
    
    startTime = limits.startTime;

	
#if 0
    
    lastcall_Limits = &limits;
    lastcall_Us     = us;
    lastcall_Ply    = ply;
    lastcall_Opt    = const_cast<OptionsMap*>(&options);
#endif

    init_(limits, us, ply, options, max_moves_to_draw);
}





void TimeManagement::init_(const Search::LimitsType& limits,
                           Color               us,
                           int                 ply,
                           const OptionsMap&   options, int max_moves_to_draw) {

#if STOCKFISH
	TimePoint npmsec = TimePoint(options["nodestime"]);
	

    
    
    startTime    = limits.startTime;

	useNodesTime = npmsec != 0;

    if (limits.time[us] == 0)
        return;

	TimePoint npmsec = Options["nodestime"];

	
	
	
	
	if (npmsec)
	{
		
		if (!availableNodes)
			availableNodes = npmsec * limits.time[us];
		
		
		limits.time[us] = TimePoint(availableNodes);
		for (auto c : COLOR)
		{
			limits.inc[c] *= npmsec;
			limits.byoyomi[c] *= npmsec;
		}
		limits.rtime *= npmsec;
		limits.npmsec = npmsec;

		
		
	}
#endif

	
	
    network_delay = (TimePoint) options["NetworkDelay"];

	
	round_up_to_fullsecond = options["RoundUpToFullSecond"];

	
	
    startTime = ponderhitTime = limits.startTime;
    search_end                = 0;

	
	
	
    remain_time =
      limits.time[us]
      + limits.byoyomi[us]  -(TimePoint) options["NetworkDelay2"];

	
	
	remain_time = std::max(remain_time, (TimePoint) (round_up_to_fullsecond ? 100 : 1) );

	
    minimum_thinking_time = (TimePoint) options["MinimumThinkingTime"];

	
	
	
	int slowMover = (int)options["SlowMover"];

	if (limits.rtime)
	{
		
		
		

		auto r = limits.rtime;
#if 1
		
		if (ply)
			r += (int)prng.rand((int)std::min(r * 0.5f, r * 10.0f / (ply)));
#endif

		remain_time = minimumTime = optimumTime = maximumTime = r;
		return;
	}

	
	
	if (limits.movetime)
	{
		remain_time = minimumTime = optimumTime = maximumTime = limits.movetime;
		return;
	}

	
	bool time_forfeit = limits.inc[us] == 0 && limits.byoyomi[us] == 0;

	
	
	
	
	
	int move_horizon;
	if (time_forfeit)
		move_horizon = MoveHorizon + 40 - std::min(ply , 40);
	else
		
		move_horizon = MoveHorizon + 20 - std::min(ply , 80);

	
	
	
    const int MTG = std::min(max_moves_to_draw - ply + 2, move_horizon) / 2;

	if (MTG <= 0)
	{
		
		sync_cout << "info string Error! : MaxMovesToDraw is too small." << sync_endl;
		
        minimumTime = optimumTime = maximumTime = 500;
        return;
	}
	if (MTG == 1)
	{
		
		minimumTime = optimumTime = maximumTime = remain_time;
		return;
	}

	

	{
		
		
		minimumTime = std::max(minimum_thinking_time - network_delay,
							   (TimePoint)(round_up_to_fullsecond ? 1000 : 1));

		
		optimumTime = maximumTime = remain_time;

		
		
		

		
		TimePoint remain_estimate = limits.time[us]
			+ limits.inc[us] * MTG
			
			+ limits.byoyomi[us] * MTG;

		
		if (round_up_to_fullsecond)
			remain_estimate -= (MTG + 1) * 1000;
		remain_estimate = std::max(remain_estimate, TimePoint(0));

		
		TimePoint t1 = minimumTime + remain_estimate / MTG;

		
		const bool pyf_fischer = limits.inc[us] > 0 && limits.byoyomi[us] == 0;
		const int  pyf_h   = (int)options["Cyf_TM_Fischer_Horizon"];
		const int  pyf_sp  = (int)options["Cyf_TM_Inc_Spend"];
		if (pyf_fischer && (pyf_h > 0 || pyf_sp != 100))
		{
			const int mtg_main = (pyf_h > 0) ? std::max(1, std::min(MTG, pyf_h)) : MTG;
			TimePoint main_part = limits.time[us];
			if (round_up_to_fullsecond)
				main_part -= (MTG + 1) * 1000;
			main_part = std::max(main_part, TimePoint(0));
			t1 = minimumTime + main_part / mtg_main + limits.inc[us] * pyf_sp / 100;
		}

		

		
		
		float max_ratio = 5.0f;

		
		if (time_forfeit)
		{
			
			
			
			max_ratio = std::min(max_ratio, std::max(float(limits.time[us]) / (60 * 1000), 1.0f));
		}
		TimePoint t2 = minimumTime + (int)(remain_estimate * max_ratio / MTG);

		
		
		t2 = std::min(t2, (TimePoint)(remain_estimate * 0.3));

		
		optimumTime = std::min(t1, optimumTime) * slowMover / 100;

		
		if (pyf_fischer)
		{
			const int pyf_cap = (int)options["Cyf_TM_Fischer_Cap"];
			if (pyf_cap > 0)
				optimumTime = std::min(optimumTime, (TimePoint)(remain_time * pyf_cap / 100));
		}
		maximumTime = std::min(t2, maximumTime);

		
		
        if (options["USI_Ponder"] && !options["Stochastic_Ponder"])
			optimumTime += optimumTime * ((int)options["Cyf_TM_Ponder_Bonus"] - 100) / 100;   

	}

	
    isFinalPush = false;
	if (limits.byoyomi[us])
	{
		
		
        if (limits.time[us] < (int) (limits.byoyomi[us] * 1.2))
        {
            minimumTime = optimumTime = maximumTime = limits.byoyomi[us] + limits.time[us];
			
            isFinalPush                             = true;
        }
	}

	
	minimumTime = std::min(round_up(minimumTime), remain_time);
	optimumTime = std::min(         optimumTime , remain_time);
	maximumTime = std::min(round_up(maximumTime), remain_time);
}



TimePoint TimeManagement::round_up(TimePoint t0) {
	if (round_up_to_fullsecond)
	{
		

		
		auto t = std::max(((t0 + 999) / 1000) * 1000, minimum_thinking_time);

		
		t = t - network_delay;

		
		if (t < t0)
			t += 1000;

		
		t = std::min(t, remain_time);
		return t;

	} else {
		

		
		auto t = std::max(t0, minimum_thinking_time);

		
		t = t - network_delay;

		
		t = std::min(t, remain_time);
		return t;
	}
};


void TimeManagement::set_search_end(TimePoint e) {
    

    
    TimePoint t1 = e + startTime - ponderhitTime;

	
	
    TimePoint t2 = isFinalPush ? minimum() : minimum() + startTime - ponderhitTime;

	
    
    search_end = round_up(std::max(t1, t2)) + ponderhitTime - startTime;
}

TimePoint TimeManagement::minimum() const { return minimumTime; }
TimePoint TimeManagement::optimum() const { return optimumTime; }
TimePoint TimeManagement::maximum() const { return maximumTime; }


} 

#endif
