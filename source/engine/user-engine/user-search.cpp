#include "../../config.h"

#if defined(USER_ENGINE)



#include "../../types.h"
#include "../../extra/all.h"

using YaneuraOu::Search::Worker;

namespace YaneuraOu {

namespace Search {

	class UserWorker : public Worker
	{
	public:

		UserWorker(SharedState& sharedState, const Search::ThreadIds& ids):
			
			Worker(sharedState, ids){ }

		
		
		virtual void clear() override
		{
			sync_cout << "UserWorker::clear" << sync_endl;
		}

		
		
		
		virtual void start_searching() override
		{
			sync_cout << "UserWorker::start_searching , position sfen = " << rootPos.sfen() << ", threadIdx = " << threadIdx << sync_endl;

			if (is_mainthread())
			{
				threads.start_searching();  

				Tools::sleep(1000);

				
				sync_cout << "bestmove resign" << sync_endl;
			}
		}

	};


} 

class UserEngine : public Engine
{
    
    virtual std::string get_engine_name() const override { return "my engine"; }
    virtual std::string get_engine_author() const override { return "myself"; }

	
	#if 0
	virtual void usi() override {
        sync_cout << "id user-engine\n"
                  << "author a user" << sync_endl;
	}
	#endif

	
	virtual void isready() override
	{
		sync_cout << "UserEngine::isready" << sync_endl;

		
		Engine::isready();
	}

	
	virtual void add_options() override
	{
		
		Engine::add_options();

		sync_cout << "UserEngine::add_options" << sync_endl;

		
		options.add("HogeOption", Option("hogehoge"));
	}

	
	virtual void user(std::istringstream& is) override
	{
		sync_cout << "UserEngine::user_cmd" << sync_endl;
	}

	
	virtual void resize_threads() override
	{
		

		
		threads.wait_for_search_finished();

		

		
		

		auto worker_factory = [&](Search::SharedState& sharedState,
								  const Search::ThreadIds& ids)
		{
            auto p = make_unique_large_page<Search::UserWorker>(
                
                sharedState, ids);

            return LargePagePtr<Worker>(p.release());  
        };

        threads.set(numaContext.get_numa_config(), {options, threads, tt, sharedHists  },
					updateContext, options["Threads"], worker_factory);

		

		
		threads.ensure_network_replicated();
	}
};

} 

using namespace YaneuraOu;

namespace {

	
	void engine_main()
	{
		
		auto engine = std::make_unique<UserEngine>();

		
		auto usi = std::make_unique<USIEngine>();
		usi->set_engine(*engine);  
		usi->enqueue_startup_commands(CommandLine::g);

		
		usi->loop();
	}

	
	static EngineFuncRegister r(engine_main, "UserEngine", 0);

} 


#endif 
