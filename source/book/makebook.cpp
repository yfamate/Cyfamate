#include "../config.h"

#if defined (ENABLE_MAKEBOOK_CMD)

#include "book.h"
#include "../misc.h"
#include "../position.h"
#include <sstream>

using namespace std;
namespace YaneuraOu {
namespace Book
{



    int makebook2025(istringstream& is, const string& token, const OptionsMap& options);


    void makebook(IEngine& engine, istringstream& is)
	{
        engine.isready();
		Tools::ProgressBar::enable(true);

		string token;
		is >> token;

        if (makebook2025(is, token, engine.get_options()))
		{
			Tools::ProgressBar::enable(false);
			return;
		}


		cout << "usage" << endl;
		cout << "> makebook peta_shock book.db user_book1.db" << endl;
		Tools::ProgressBar::enable(false);
	}

} 
} 

#endif 
