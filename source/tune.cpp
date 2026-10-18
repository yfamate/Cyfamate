#include "tune.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

#include "usioption.h"

using std::string;

namespace YaneuraOu {

bool          Tune::update_on_last;
const Option* LastOption = nullptr;
OptionsMap*   Tune::options;
namespace {
std::map<std::string, int> TuneResults;

std::optional<std::string> on_tune(const Option& o) {

    if (!Tune::update_on_last || LastOption == &o)
        Tune::read_options();

    return std::nullopt;
}
}



void Tune::make_option(OptionsMap* opts, const string& n, int v, const SetRange& r) {

    
	
    if (r(v).first == r(v).second)
        return;

    if (TuneResults.count(n))
        v = TuneResults[n];

    opts->add(n, Option(v, r(v).first, r(v).second, on_tune));
    LastOption = &((*opts)[n]);

    
    

	

	std::cout << n << ","                                  
              << v << ","                                  
              << r(v).first << ","                         
              << r(v).second << ","                        
              << (r(v).second - r(v).first) / 20.0 << ","  
              << "0.0020" << std::endl;
}




string Tune::next(string& names, bool pop) {

    string name;

    do
    {
		
        string token = names.substr(0, names.find(','));

		
        if (pop)
            names.erase(0, token.size() + 1);

        std::stringstream ws(token);
        name += (ws >> token, token);  

    } while (std::count(name.begin(), name.end(), '(') - std::count(name.begin(), name.end(), ')'));
	

    return name;
}


template<>
void Tune::Entry<int>::init_option() {
	
    make_option(options, name, value, range);
}

template<>
void Tune::Entry<int>::read_option() {
	
    if (options->count(name))
        value = int((*options)[name]);
}



template<>
void Tune::Entry<Tune::PostUpdate>::init_option() {}

template<>
void Tune::Entry<Tune::PostUpdate>::read_option() {
    value();
}

}  






















namespace YaneuraOu {

void Tune::read_results() {  }

}  
