
#include "misc.h"
#include "thread.h"
#include "usi.h"

namespace YaneuraOu {

bool CaseInsensitiveLess::operator()(const std::string& s1, const std::string& s2) const {

	return std::lexicographical_compare(
		s1.begin(), s1.end(), s2.begin(), s2.end(),
		[](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); });
}

void OptionsMap::add_info_listener(InfoListener&& message_func) { info = std::move(message_func); }

void OptionsMap::setoption(std::istringstream& is) {
	std::string token, name, value;

	is >> token;  

	while (is >> token && token != "value")
		name += (name.empty() ? "" : " ") + token;

	while (is >> token)
		value += (value.empty() ? "" : " ") + token;

	if (options_map.count(name))
		options_map[name] = value;
	else
		sync_cout << "No such option: " << name << sync_endl;
}

const Option& OptionsMap::operator[](const std::string& name) const {
	auto it = options_map.find(name);
#if STOCKFISH
	assert(it != options_map.end());
#else
	if (it == options_map.end())
	{
		sync_cout << "Error : Options[" << name << "] , not found." << sync_endl;
		Tools::exit();
	}
#endif

	return it->second;
}


void OptionsMap::add(const std::string& name, const Option& option) {
	if (!options_map.count(name))
	{

		static size_t insert_order = 0;

		options_map[name] = option;

		options_map[name].parent = this;
		options_map[name].idx = insert_order++;
	}
	else
	{

		std::cerr << "Option \"" << name << "\" was already added!" << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

std::size_t OptionsMap::count(const std::string& name) const { return options_map.count(name); }

Option::Option(const OptionsMap* map) :
	parent(map) {
}

Option::Option(const char* v, OnChange f) :
	type("string"),
	min(0),
	max(0),
	on_change(std::move(f)) {
	defaultValue = currentValue = v;
}

Option::Option(bool v, OnChange f) :
	type("check"),
	min(0),
	max(0),
	on_change(std::move(f)) {
	defaultValue = currentValue = (v ? "true" : "false");
}

Option::Option(OnChange f) :
	type("button"),
	min(0),
	max(0),
	on_change(std::move(f)) {
}

#if STOCKFISH
Option::Option(int v, int minv, int maxv, OnChange f) :
#else
Option::Option(int64_t v, int64_t minv, int64_t maxv, OnChange f) :
#endif
	type("spin"),
	min(minv),
	max(maxv),
	on_change(std::move(f)) {
	defaultValue = currentValue = std::to_string(v);
}

#if STOCKFISH

Option::Option(const char* v, const char* cur, OnChange f) :
	type("combo"),
	min(0),
	max(0),
	on_change(std::move(f)) {
	defaultValue = v;
	currentValue = cur;
}
#else

Option::Option(const std::vector<std::string>& list, const std::string& cur, OnChange f) :
	type("combo"),
	min(0),
	max(0),
	on_change(std::move(f)) {

    std::string s = cur;
    for (auto l : list)
        s += " var " + l;
    defaultValue = s;
	currentValue = cur;
}
#endif

Option::operator int64_t() const {
	ASSERT_LV1(type == "check" || type == "spin");
#if STOCKFISH
	return (type == "spin" ? std::stoi(currentValue) : currentValue == "true");
#else
	return (type == "spin" ? std::stoll(currentValue) : currentValue == "true");
#endif
}

Option::operator std::string() const {
#if STOCKFISH
    assert(type == "string");
#else
	ASSERT_LV1(type == "string" || type == "combo");
#endif
	return currentValue;
}

bool Option::operator==(const char* s) const {
	ASSERT_LV1(type == "combo");
    return !CaseInsensitiveLess()(currentValue, s) && !CaseInsensitiveLess()(s, currentValue);
}

bool Option::operator!=(const char* s) const { return !(*this == s); }



Option& Option::operator=(const std::string& v) {

#if STOCKFISH
    assert(!type.empty());
#else
	ASSERT_LV1(!type.empty());

	if (fixed)
		return *this;

	if (type == "spin" && !v.empty())
	{
		try { size_t consumed = 0; (void)std::stoll(v, &consumed);
            if (consumed != v.size()) throw std::invalid_argument("trailing spin characters"); }
		catch (const std::exception&)
		{
			sync_cout << "info string Error! : spin value is not a number, value = `" << v << "`" << sync_endl;
			return *this;
		}
	}
#endif

	if ((type != "button" && type != "string" && v.empty())
		|| (type == "check" && v != "true" && v != "false")
#if STOCKFISH
		|| (type == "spin" && (std::stoi(v) < min || std::stoi(v) > max)))
#else
		|| (type == "spin" && (std::stoll(v) < min || std::stoll(v) > max)))
#endif
		return *this;

	if (type == "combo")
	{
		OptionsMap         comboMap;  
		std::string        token;
		std::istringstream ss(defaultValue);
#if STOCKFISH
		while (ss >> token)
			comboMap.add(token, Option());
		if (!comboMap.count(v) || v == "var")
			return *this;
#else
        while (ss >> token)
            if (token != "var" && !comboMap.count(token))
                comboMap.add(token, Option());

		if (!comboMap.count(v))
        {
            sync_cout << "info string Error! : combo value not found, value = `" << v
                      << "`, values = " << defaultValue << sync_endl;
            return *this;
        }
#endif
	}

	if (type == "string")
		currentValue = v == "<empty>" ? "" : v;

	else if (type != "button")
		currentValue = v;

	if (on_change)
	{
		const auto ret = on_change(*this);

		if (ret && parent != nullptr && parent->info != nullptr)
			parent->info(ret);
	}

	return *this;
}

std::ostream& operator<<(std::ostream& os, const OptionsMap& om) {
	for (size_t idx = 0; idx < om.options_map.size(); ++idx)
		for (const auto& it : om.options_map)
			if (it.second.idx == idx)
			{
				const Option& o = it.second;
				os << "\noption name " << it.first << " type " << o.type;

				if (o.type == "check" || o.type == "combo")
					os << " default " << o.defaultValue;

				else if (o.type == "string")
				{
					std::string defaultValue = o.defaultValue.empty() ? "<empty>" : o.defaultValue;
					os << " default " << defaultValue;
				}

				else if (o.type == "spin")
#if STOCKFISH
                    os << " default " << stoi(o.defaultValue) << " min " << o.min << " max "
#else
					os << " default " << stoll(o.defaultValue) << " min " << o.min << " max "
#endif
					<< o.max;

				break;
			}

	return os;
}


#if 0

	static void on_logger(const Option& o) { start_logger(o); }


	std::string last_eval_dir;

#if defined(__EMSCRIPTEN__) && defined(EVAL_NNUE)
	std::string last_eval_file;
#endif

	std::ostream& operator<<(std::ostream& os, const OptionsMap& om)
	{
		for (size_t idx = 0; idx < om.size(); ++idx)
			for (const auto& it : om)
				if (it.second.idx == idx)
				{
					const Option& o = it.second;
					os << "option name " << it.first << " type " << o.type;

					if (o.type == "string" || o.type == "check" || o.type == "combo")
						os << " default " << o.defaultValue;

					if (o.type == "spin")
						os << " default " << int64_t(stoll(o.defaultValue))
						<< " min " << o.min
						<< " max " << o.max;

					if (o.list.size())
						for (auto v : o.list)
							os << " var " << v;

					os << std::endl;

					break;
				}

		return os;
	}


	bool load_eval_finished = false;

	void set_engine_options(const std::string& options)
	{
		auto v = StringExtension::Split(options, ";");
		for (auto line : v)
			build_option(std::string(line));
	}

#endif


void OptionsMap::read_engine_option_profile(const std::string& filename)
{
	SystemIO::TextReader reader;
	if (reader.Open(filename).is_not_ok())
		return;

	std::string line;
	while (reader.ReadLine(line).is_ok())
	{
		line = StringExtension::trim(line);

		if (line.empty() || line[0] == '#')
			continue;
		if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
			continue;

		const auto line2 = StringExtension::Replace(line, '=', ' ');
		Parser::LineScanner scanner(line2);
		const std::string key = scanner.get_text();

		if (!StringExtension::stricmp(key, "BOOK_OPTIONS_V2"))
		{
			book_options_version = 2;
			continue;
		}

		if (!StringExtension::stricmp(key, "BOOK_OPTIONS"))
		{
			const std::string value = scanner.get_text();
			if (!StringExtension::stricmp(value, "V2") || value == "2")
				book_options_version = 2;
			else if (!StringExtension::stricmp(value, "V1") || value == "1")
				book_options_version = 1;
		}
	}
}

void OptionsMap::read_engine_options(const std::string& filename)
{
	SystemIO::TextReader reader;
	if (reader.Open(filename).is_not_ok())
		return;

	sync_cout << "info string read engine options, path = " << filename << sync_endl;

	std::string line;
	while (reader.ReadLine(line).is_ok())
		build_option(line);
}

void OptionsMap::build_option(const std::string& line)
{



	auto& Options = options_map;

	const auto& line2 = StringExtension::Replace(line, '=', ' ');

	Parser::LineScanner scanner(line2);
	std::string token = scanner.get_text();

	std::string option_name, option_value;

	if (token != "option")
	{
		if (token == "")
			return;

		auto it = Options.find(token);
		if (it == Options.end())
		{
			std::cout << "Error : option name not found : " << token << std::endl;
			return;
		}

		option_name  = token;
		option_value = scanner.get_text();
	}
	else {

		std::string name, value, option_type;
		int64_t min_value = 0, max_value = 1;
		std::vector<std::string> combo_list;
		while (!scanner.eol())
		{
			std::string token = scanner.get_text();
			if (token == "name") name = scanner.get_text();
			else if (token == "type") option_type = scanner.get_text();
			else if (token == "default") value = scanner.get_text();
			else if (token == "min") min_value = stoll(scanner.get_text());
			else if (token == "max") max_value = stoll(scanner.get_text());
			else if (token == "var") {
				auto varText = scanner.get_text();
				combo_list.push_back(varText);
			}
			else {
				std::cout << "Error : invalid command: " << token << std::endl;
			}
		}

		if (Options.count(name) == 0)
		{
			std::cout << "Error : option name not found : " << name << std::endl;
			return;
		}

		option_name  = name;
		option_value = value;
	}

	Options[option_name] = option_value;
	Options[option_name].fixed = true; 

	sync_cout << "info string engine option override. name = " << option_name << " , value = " << option_value << sync_endl;

}


std::string OptionsMap::set_option_if_exists(const std::string& option_name, const std::string& option_value)
{
	for (auto& o : options_map)
	{
		if (!StringExtension::stricmp(option_name, o.first))
		{
			options_map[o.first] = option_value;
			return std::string("Options[") + o.first + "] = " + option_value;
		}
	}
	return std::string("No such option: ") + option_name;
}

std::pair<const std::string, const Option&> OptionsMap::get_option_by_idx(size_t idx) const
{
	for (const auto& o : options_map)
		if (o.second.idx == idx)
			return o;

	assert(false);
	return *options_map.begin(); 
}

std::string OptionsMap::get_option(const std::string& option_name)
{
	bool all = option_name == "";


	size_t max_key_length = 0;
	for (const auto& o : options_map) {
		max_key_length = std::max(max_key_length, o.first.length());
	}

	std::string result;
	for (size_t idx = 0; idx < options_map.size(); ++idx)
	{
		auto it = get_option_by_idx(idx);

		if ((!StringExtension::stricmp(option_name, it.first)) || all)
		{
			result += "Options[" + it.first + "]"
				+ std::string(max_key_length - it.first.length() + 1, ' ')
				+ "= "
				+ std::string(it.second) + "\n";

			if (!all)
				return result;
		}
	}
	if (!all)
		result += "No such option: " + option_name + "\n";

	return result;
}

} 
