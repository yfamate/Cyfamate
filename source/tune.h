#ifndef TUNE_H_INCLUDED
#define TUNE_H_INCLUDED

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>  
#include <utility>
#include <vector>

namespace YaneuraOu {

class OptionsMap;


using Range    = std::pair<int, int>;  



using RangeFun = Range(int);





inline Range default_range(int v) { return v > 0 ? Range(0, 2 * v) : Range(2 * v, 0); }


struct SetRange {

    

    explicit SetRange(RangeFun f) :
        
        
        fun(f) {}

    SetRange(int min, int max) :
        fun(nullptr),
        range(min, max) {}

    
    Range operator()(int v) const { return fun ? fun(v) : range; }

    RangeFun* fun;
    Range     range;
};

#define SetDefaultRange SetRange(default_range)





























class Tune {

    using PostUpdate = void();  

    Tune() { read_results(); }
    Tune(const Tune&)           = delete;
    void operator=(const Tune&) = delete;
    void read_results();

	
    static Tune& instance() {
        static Tune t;
        return t;
    }  

    
	
	
    struct EntryBase {
        virtual ~EntryBase()       = default;
        virtual void init_option() = 0;
        virtual void read_option() = 0;
    };

	
    template<typename T>
    struct Entry: public EntryBase {

		
        static_assert(!std::is_const_v<T>, "Parameter cannot be const!");

		
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, PostUpdate>,
                      "Parameter type not supported!");

		
        Entry(const std::string& n, T& v, const SetRange& r) :
            name(n),
            value(v),
            range(r) {}

		void operator=(const Entry&) = delete;  

		
        void init_option() override;

        
        
        void read_option() override;

		
        std::string name;

		
        T&          value;

		
        SetRange    range;
    };

    
    
    
	
    

	
    static std::string next(std::string& names, bool pop = true);

    int add(const SetRange&, std::string&&) { return 0; }

    template<typename T, typename... Args>
    int add(const SetRange& range, std::string&& names, T& value, Args&&... args) {
        list.push_back(std::unique_ptr<EntryBase>(new Entry<T>(next(names), value, range)));
        return add(range, std::move(names), args...);
    }

    
    

	

	template<typename T, size_t N, typename... Args>
    int add(const SetRange& range, std::string&& names, T (&value)[N], Args&&... args) {
        for (size_t i = 0; i < N; i++)
            add(range, next(names, i == N - 1) + "[" + std::to_string(i) + "]", value[i]);
        return add(range, std::move(names), args...);
    }

    
    

    template<typename... Args>
    int add(const SetRange&, std::string&& names, SetRange& value, Args&&... args) {
        return add(value, (next(names), std::move(names)), args...);
    }

	
    
    static void make_option(OptionsMap* options, const std::string& n, int v, const SetRange& r);

	
    std::vector<std::unique_ptr<EntryBase>> list;

   public:
    template<typename... Args>
    static int add(const std::string& names, Args&&... args) {
        return instance().add(SetDefaultRange, names.substr(1, names.size() - 2),
                              args...);  
    }

	
	
    static void init(OptionsMap& o) {
        options = &o;
        for (auto& e : instance().list)
            e->init_option();
        read_options();
    }  
       

	
	
	
    static void read_options() {
        for (auto& e : instance().list)
            e->read_option();
    }

    static bool        update_on_last;
    static OptionsMap* options;
};

template<typename... Args>
constexpr void tune_check_args(Args&&...) {
    static_assert((!std::is_fundamental_v<Args> && ...), "TUNE macro arguments wrong");
}







#define STRINGIFY(x) #x



#define UNIQUE2(x, y) x##y





#define UNIQUE(x, y) UNIQUE2(x, y)  






#define TUNE(...) \
    int UNIQUE(p, __LINE__) = []() -> int { \
        tune_check_args(__VA_ARGS__); \
        return Tune::add(STRINGIFY((__VA_ARGS__)), __VA_ARGS__); \
    }();






#define UPDATE_ON_LAST() bool UNIQUE(p, __LINE__) = Tune::update_on_last = true

}  

#endif  
