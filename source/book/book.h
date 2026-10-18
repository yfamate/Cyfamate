#ifndef _BOOK_H_INCLUDED_
#define _BOOK_H_INCLUDED_

#include "../types.h"
#include "../position.h"
#include "../misc.h"
#include "../usi.h"
#include "../testcmd/unit_test.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace YaneuraOu {

namespace Search {
struct LimitsType;
using  RootMoves = std::vector<RootMove>;
};


namespace Book
{
static const char* BookDBHeader2016_100 = "#YANEURAOU-DB2016 1.00";


struct ProbeResult {
    
    Move bestmove;
    
    
    Score bestscore = 0;

    
    Move pondermove;

	
    ProbeResult() :
        bestmove(Move::none()),
        pondermove(Move::none()) {}

	
    ProbeResult(Move move) :
        bestmove(move),
        pondermove(Move::none()) {}
};





struct BookMove
{
	
	

	Move16 move;   
	Move16 ponder; 

	

	int value;     
	int depth;     

	uint64_t move_count; 

	

	BookMove(Move16 move_, Move16 ponder_, int value_, int depth_, uint64_t move_count_)
		: move(move_), ponder(ponder_), value(value_), depth(depth_), move_count(move_count_){}

	
	
	static BookMove from_string(std::string line);

	
	bool operator == (const BookMove& rhs) const
	{
		return move == rhs.move;
	}

	
	
	bool operator < (const BookMove& rhs) const {
		return (move_count != rhs.move_count) ? (move_count > rhs.move_count ) : (value > rhs.value);
	}
};

static std::ostream& operator<<(std::ostream& os, BookMove c);


typedef std::vector<BookMove>::iterator BookMoveIterator;









struct BookMoves
{
	BookMoves() {}

	
	
	BookMoves(const BookMoves& bm) { sorted = bm.sorted; moves = bm.moves; }

	
	
	
	
	void insert(const BookMove& book_move,bool overwrite);

	
	
	
	void push_back(const BookMove& book_move);

	
	std::shared_ptr<BookMove> find_move(const Move16 move) const;

	
	
	
	size_t size() const { return moves.size(); }

	
	
	
	
	void sort_moves();

	
	void foreach(std::function<void(BookMove&)> f);

	

	
	

	const BookMove& operator[](size_t s) const { return moves[s]; }
	const BookMoveIterator begin() { return moves.begin(); }
	const BookMoveIterator end() { return moves.end(); }
	BookMoveIterator erase(BookMoveIterator start, BookMoveIterator last) { return moves.erase(start, last); }
	void clear() { moves.clear(); }

	

private:
	
	std::vector<BookMove> moves;

	
	
	
	bool sorted = false;

	
	std::recursive_mutex mutex_;
};

typedef std::shared_ptr<BookMoves> BookMovesPtr;


typedef std::unordered_map<std::string , BookMovesPtr > BookType;




struct MemoryBook
{
    
    void set_options(OptionsMap& o);

	
	
	
	
	BookMovesPtr find(const Position& pos);

	
	
	
	
	
	
	
	Tools::Result read_book(const std::string& filename, bool on_the_fly = false);

	
	
	
	
	
	Tools::Result write_book(const std::string& filename ) const;

	
	
	
	
	

	
	BookMovesPtr find(const std::string& sfen) const;

	
	
	
	void append(const std::string& sfen, const Book::BookMovesPtr ptr);

	
	
	
	
	void insert(const std::string& sfen, const BookMove& bp , bool overwrite = true);

	
	void merge(MemoryBook& book2);

	
	void foreach(std::function<void(const std::string& , const Book::BookMovesPtr)> f);

	
	size_t size() const { return ybb_memory_book ? static_cast<size_t>(ybb_record_count) : book_body.size(); }

protected:

	
	
	
	
	BookType book_body;

	
	std::recursive_mutex mutex_;

	
	
	
	std::string trim(std::string input) const;

	
	BookMovesPtr find_bookmoves_on_the_fly(std::string sfen);
	BookMovesPtr find_ybb_bookmoves_on_the_fly(const Position& pos);
	BookMovesPtr find_ybb_bookmoves_on_the_fly(const PackedSfen& target, uint16_t game_ply);
	BookMovesPtr find_ybb_bookmoves_in_memory(const PackedSfen& target, uint16_t game_ply);

	
	
	
	
	bool on_the_fly = false;

	
	
	bool ignoreBookPly = false;

	
	std::fstream fs;

	
	
	std::fstream ybb_index_fs;

	
	
	std::fstream ybb_moves_fs;

	
	std::vector<unsigned char> ybb_index_data;

	
	
	uint64_t ybb_moves_base = 0;

	
	uint64_t ybb_record_count = 0;

	
	
	uint64_t ybb_flags = 0;

	
	bool ybb_book = false;

	
	bool ybb_memory_book = false;

	
	std::string ybb_moves_name;

	
	
	
	
	std::string book_name;
	std::string pure_book_name; 

	
	OptionsMapRef options;
};


struct BookMoveSelector
{
    
	void add_options(OptionsMap& o);

    
    void set_options(OptionsMap& options);

	
	
	
	
	
	void read_book();

	
    
    ProbeResult probe(Position& pos, const Search::UpdateContext& updates);

protected:
	
	
	
	std::vector<std::unique_ptr<MemoryBook>> memory_books;

	
	std::vector<std::string> book_names;

	
	bool book_on_the_fly = false;
	bool ignoreBookPly = false;

	
	
	std::string get_book_name() const;

	
	std::vector<std::string> get_book_names() const;

	
	BookMovesPtr find_in_books(Position& pos);

	
	
	
	
	
	
	
	
	
	
	
	
    bool probe_impl(Position&                    rootPos,
                    bool                         isRoot,
                    const Search::UpdateContext& updates,
                    Move16&                      bestMove,
                    Move16&                      ponderMove,
                    Value&                       value,
                    bool                         forceHit = false);

	
	
	
    std::string pv_builder(Position& pos, const Search::UpdateContext& updates, Move16 m, int rest_ply);

	AsyncPRNG prng;

	
	OptionsMapRef options;
};


void UnitTest(Test::UnitTester& tester, IEngine& engine);
}


namespace BookTools
{











void feed_position_string(Position& pos, const std::string& root_sfen, std::deque<StateInfo>& si,
	const std::function<void(Position&, Move)>& position_callback = [](Position&, Move) {});





std::vector<std::string> get_start_sfens();



std::vector<std::string> get_next_sfens(std::string root_sfen);

} 
} 

#endif 
