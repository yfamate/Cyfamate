#include "unit_test.h"

#include <iostream>
#include <sstream>
#include "../position.h"
#include "../usi.h"
#include "../search.h"
#include "../misc.h"
#include "../book/book.h"
#include "../tt.h"

using namespace std;
namespace YaneuraOu {

namespace Test
{



	UnitTester::UnitTester()
	{
		cout << "=== Start UnitTest ===" << endl;
	}

	UnitTester::~UnitTester()
	{
		cout << "=== Summary UnitTest ===" << endl;

		auto passed = test_count - errors.size();
		cout << "Summary : " << passed << " / " << test_count << " passed." << endl;

		if (errors.empty())
			cout << "-> Passed all UnitTests." << endl;
		else
		{
			cout << "Error List : " << endl;
			for (auto s : errors)
				cout << s << endl;
		}

		cout << "=== Finish UnitTest === " << endl;
	}

	void UnitTester::test(const std::string& message, bool b)
	{
		stringstream ss;
		ss << section_name() << " " << message;
		auto left = ss.str();

		string right = b ? "passed" : "failed";

		auto padding = std::max((size_t)0, 79 - left.size() - right.size());

		string mes = left + string(padding, '.') + right;

		cout << mes << endl;

		if (!b)
			errors.emplace_back(mes);

		++test_count;
	}

	void UnitTester::run(std::function<void(UnitTester&, IEngine& )> f)
	{
		if (before_run)
			before_run();

		f(*this, *engine);

		if (after_run)
			after_run();
	}

	const std::string UnitTester::section_name()
	{
		stringstream ss;

		ss << "[";
		bool first = true;
		for (auto section : sections)
		{
			if (!first)
				ss << "::";
			first = false;

			ss << section;
		}
		ss << "]";

		return ss.str();
	}



	void UnitTest(istringstream& is, IEngine& engine)
	{
		UnitTester tester;

		string token;
		s64 random_player_loop = 0; 
		while (is >> token)
		{
			if (token == "random_player_loop")
				is >> random_player_loop;
		}
		cout << "random_player_loop : " << random_player_loop << endl;

		tester.options.add("random_player_loop", Option(random_player_loop));


		tester.engine = &engine;

		tester.run(Book::UnitTest);

		tester.run(Bitboard::UnitTest);
		tester.run(Bitboard256::UnitTest);

		tester.run(Position::UnitTest);

		tester.run(TranspositionTable::UnitTest);

		tester.run(USIEngine::UnitTest);

		tester.run(Misc::UnitTest);


	}

} 
} 
