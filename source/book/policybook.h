#ifndef POLICYBOOK_H_INCLUDED
#define POLICYBOOK_H_INCLUDED

#include "../config.h"

#if defined(USE_POLICY_BOOK)

#include <vector>
#include "../types.h"
#include "../extra/key128.h"
#include "../misc.h"

namespace YaneuraOu {

static_assert(HASH_KEY_BITS == 128 , "HASH_KEY_BITS must be 128");

#define POLICY_BOOK_DB_NAME           "eval/policy_book.db"
#define POLICY_BOOK_DB_BIN_NAME       "eval/policy_book.db.bin"
#define POLICY_BOOK_LEARN_DB_BIN_NAME "eval/policy_book-learn.db.bin"

#define POLICY_BOOK_HEADER            "#YANEURAOU-POLICY-DB2024 1.01"


struct MoveFreq
{
	Move16 move16; 
	u16    freq;   

	MoveFreq() { move16 = Move16::none(); freq = 0; }
	MoveFreq(Move16 m , u16 f) : move16(m), freq(f) {}
};

struct MoveFreq32
{
	Move16 move16; 
	u32    freq;   

	MoveFreq32() { move16 = Move16::none(); freq = 0; }
	MoveFreq32(Move16 m, u32 f) : move16(m), freq(f) {}
	MoveFreq32(std::pair<Move16,u32> p) : move16(p.first), freq(p.second) {}

};

constexpr int POLICY_BOOK_NUM = 3;

struct alignas(16) MoveFreq32Record
{
	MoveFreq32 move_freq32[POLICY_BOOK_NUM];

	float value = FLT_MAX;

	u16 overflow_check();
};

struct alignas(32) PolicyBookEntry
{
	HASH_KEY key;
	MoveFreq move_freq[POLICY_BOOK_NUM];

	float value = FLT_MAX;

	void from_move_freq32rec(const MoveFreq32Record& mf32r);
};

class PolicyBook
{
public:
	Tools::Result read_book();

	Tools::Result read_book_db(std::string path = POLICY_BOOK_DB_NAME);
	Tools::Result read_book_db_bin(std::string path = POLICY_BOOK_DB_BIN_NAME);

	Tools::Result write_book_db_bin(std::string path = POLICY_BOOK_DB_BIN_NAME);

	void merge_book(const PolicyBook& book);

	void garbage_book();

	void append_sfen_to_db_bin(const std::string& sfen);

	bool is_loaded() const { return book_body.size() != 0; }

	PolicyBookEntry* probe_policy_book(HASH_KEY key);

protected:
	void sort_book();

private:
	std::vector<PolicyBookEntry> book_body;
};

} 

#endif 
#endif
