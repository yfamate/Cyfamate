#ifndef __FUKAURAOU_ENGINE_H_INCLUDED__
#define __FUKAURAOU_ENGINE_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "../../engine.h"
#include "../../book/book.h"
#include "../../usioption.h"
#include "../../numa.h"

#include "dlshogi_types.h"
#include "dlshogi_searcher.h"
#include "SearchOptions.h"

using YaneuraOu::Search::Worker;

namespace dlshogi {



class FukauraOuEngine;

class FukauraOuWorker : public Worker {
   public:

	FukauraOuWorker(Search::SharedState&     sharedState,
                    const Search::ThreadIds& ids,
                    DlshogiSearcher&         searcher,
                    FukauraOuEngine&         engine);


	
	virtual void pre_start_searching() override;

	
    virtual void start_searching() override;

	
	void parallel_search();

	virtual ~FukauraOuWorker();

	
    DlshogiSearcher& searcher;

	
	FukauraOuEngine& engine;
};

class FukauraOuEngine: public YaneuraOu::Engine {
   public:
    FukauraOuEngine();

    
    virtual void add_options() override;

    
    virtual void isready() override;

	
    virtual void set_ponderhit(bool b) override;

	
    virtual std::string get_engine_name() const override;

    
    virtual std::string get_engine_author() const override;

	
    DlshogiSearcher searcher;

   protected:
    
    void add_nn_options();

	
	void init_gpu();

	
    std::vector<int> get_thread_settings();

};  

} 

#endif 

#endif 
