#ifndef UNIT_TEST_H_INCLUDED
#define UNIT_TEST_H_INCLUDED

#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include "../usi.h"

namespace YaneuraOu {
namespace Test {



	class UnitTester
	{
	public:


		UnitTester();
		~UnitTester();

		void test(const std::string& message, bool b);

		struct UnitTesterSection
		{
			UnitTesterSection(UnitTester* t, const std::string& section_name) :tester(t) { tester->sections.push_back(section_name); }
			~UnitTesterSection() { tester->sections.pop_back(); }
			UnitTester* tester;
		};

		UnitTesterSection section(const std::string& section_name)
		{
			return UnitTesterSection(this, section_name);
		}

		void run(std::function<void(UnitTester&, IEngine& engine)> f);

		std::function<void()> before_run;
		std::function<void()> after_run;

		OptionsMap options;

		IEngine* engine;

	protected:
		std::vector<std::string> sections;

		std::vector<std::string> errors;

		int test_count = 0;

		const std::string section_name();
	};


	void UnitTest(std::istringstream& is, IEngine& engine);

} 
} 

#endif 
