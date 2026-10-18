#ifndef USI_OPTION_H_INCLUDED
#define USI_OPTION_H_INCLUDED

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <cstdint>

namespace YaneuraOu {




struct CaseInsensitiveLess {
	bool operator() (const std::string&, const std::string&) const;
};

class OptionsMap;




class Option {
   public:
    
    using OnChange = std::function<std::optional<std::string>(const Option&)>;

    Option(const OptionsMap*);
    Option(OnChange = nullptr);

    
    Option(bool v, OnChange = nullptr);

    
    Option(const char* v, OnChange = nullptr);

    
#if STOCKFISH
	Option(int v, int minv, int maxv, OnChange = nullptr);
#else
	
    Option(int64_t v, int64_t minv, int64_t maxv, OnChange = nullptr);
#endif

    
#if STOCKFISH
    
    
    Option(const char* v, const char* cur, OnChange = nullptr);
#else
	
    Option(const std::vector<std::string>& list, const std::string& cur, OnChange = nullptr);
#endif

    Option& operator=(const std::string&);

    
    
    operator int64_t() const;

    operator std::string() const;
    bool operator==(const char*) const;
    bool operator!=(const char*) const;

    friend std::ostream& operator<<(std::ostream&, const OptionsMap&);

    int operator<<(const Option&) = delete;

    

    
    
    bool fixed = false;

   private:
    friend class OptionsMap;
    friend class Engine;
    friend class Tune;

    
    
    
    std::string defaultValue, currentValue, type;

    
    
    int64_t min, max;

    
    
    
    
    
    size_t idx;

    
    OnChange on_change;

    
    const OptionsMap* parent = nullptr;
};




class OptionsMap {
   public:
    using InfoListener = std::function<void(std::optional<std::string>)>;

    OptionsMap()                             = default;
    OptionsMap(const OptionsMap&)            = delete;
    OptionsMap(OptionsMap&&)                 = delete;
    OptionsMap& operator=(const OptionsMap&) = delete;
    OptionsMap& operator=(OptionsMap&&)      = delete;

	
	
    void add_info_listener(InfoListener&&);

    
    void setoption(std::istringstream&);

    
    
    const Option& operator[](const std::string&) const;

    
    void add(const std::string& option_name, const Option& option);

    
    
    std::size_t count(const std::string& option_name) const;

#if !STOCKFISH

    

    
    
    
    void read_engine_option_profile(const std::string& filename);

    
    bool book_options_v2() const { return book_options_version == 2; }

    
    
    
    
    void read_engine_options(const std::string& filename);

    
    
    std::string get_option(const std::string& option_name);

    
    
    std::string set_option_if_exists(const std::string& option_name,
                                     const std::string& option_value);

    
    
    std::pair<const std::string, const Option&> get_option_by_idx(size_t idx) const;

#endif

   private:
    friend class Engine;
    friend class Option;

    
    
    friend std::ostream& operator<<(std::ostream&, const OptionsMap&);

    
    
    

    using OptionsStore = std::map<std::string, Option, CaseInsensitiveLess>;

    OptionsStore options_map;
    InfoListener info;

    
    int book_options_version = 1;

#if !STOCKFISH
    
    
    
    
    
    
    
    void build_option(const std::string& line);
#endif
};





class OptionsMapRef
{
public:
	
	void set_ref(OptionsMap& o) { options = &o; }
	OptionsMap& get_ref() const { return *options; }

	

	const Option& operator[](const std::string& option_name) const { return (*options)[option_name]; };
	void add(const std::string& option_name, const Option& option) { return (*options).add(option_name, option); }
	std::size_t count(const std::string& option_name) const { return (*options).count(option_name); }
	bool book_options_v2() const { return (*options).book_options_v2(); }

private:
	OptionsMap* options = nullptr;
};

} 

#endif  
