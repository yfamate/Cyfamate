#include "../config.h"

#if defined(USE_POLICY_BOOK)

#include <numeric> 

#include "policybook.h"
#include "../misc.h"
#include "../position.h"
#include "../thread.h"
#include "../usi.h"
#include "../book/book.h"
#include "../eval/deep/nn.h"


u16 MoveFreq32Record::overflow_check()
{
	u32 total = 0;
	for (auto& mf : move_freq32)
		total += mf.freq;

	
	
	while (total >= UINT16_MAX)
	{
		total /= 2;
		for (auto& mf : move_freq32 )
			mf.freq /= 2;
	}
	return u16(total);
}


void PolicyBookEntry::from_move_freq32rec(const MoveFreq32Record& mf32r)
{
	for (size_t i = 0; i < POLICY_BOOK_NUM; ++i)
	{
		move_freq[i].move16 =     mf32r.move_freq32[i].move16 ;
		move_freq[i].freq   = u16(mf32r.move_freq32[i].freq  );
	}
	value = mf32r.value;
}





Tools::Result PolicyBook::read_book_db(std::string path)
{
	SystemIO::TextReader   reader;
	auto result = reader.Open(path);

	if (result.is_ok())
	{
		

		sync_cout << "info string read " << path << sync_endl;

		u64 counter = 0;
		std::string sfen, moves_str;
		Position pos;
		StateInfo si;
		reader.ReadLine(sfen);
		if (sfen != POLICY_BOOK_HEADER)
		{
			sync_cout << "info string Error! invalid policy book header" << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
		while (true)
		{
			if (reader.ReadLine(sfen).is_not_ok())
				break;
			pos.set(sfen, &si, Threads.main());

			reader.ReadLine(moves_str);
			auto moves = StringExtension::split(moves_str);

			
			moves.emplace_back("");
			moves.emplace_back("");

			
			
			
			
			
			
			

			
			MoveFreq32Record mf32r;

			for (size_t i = 0 , j = 0 ; ; i += 2)
			{
				auto& movestr = moves[i];
				if (movestr == "")
					break;
				if (movestr == "value")
					mf32r.value = StringExtension::to_float(moves[i + 1], FLT_MAX);
				else if (movestr == "eval")
					mf32r.value = Eval::dlshogi::cp_to_value(StringExtension::to_int(moves[i + 1], 0), 300);
				else {
					
					
					
					if (j >= POLICY_BOOK_NUM)
						continue;

					Move16 move16 = USI::to_move16(movestr);

					
					
					if (!pos.pseudo_legal_s<false>(pos.to_move(move16)))
						continue;

					mf32r.move_freq32[j].move16 = move16;
					mf32r.move_freq32[j].freq   = StringExtension::to_int(moves[i + 1], 1);
					j++;
				}
			}

			
			if (mf32r.overflow_check() == 0)
				continue;

			PolicyBookEntry pbe;

			pbe.key = pos.hash_key();
			pbe.from_move_freq32rec(mf32r);

			book_body.push_back(pbe);

			if (++counter % 100000 == 0)
				sync_cout << "info string read " << counter << sync_endl;
		}

		sync_cout << "info string read " << counter << "..done." << sync_endl;

		
		

		
		garbage_book();

		
	}

	return result;
}


Tools::Result PolicyBook::read_book_db_bin(std::string path)
{
	SystemIO::BinaryReader bin_reader;
	auto result = bin_reader.Open(path);
	if (result.is_ok())
	{
		
		size_t bin_size = bin_reader.GetSize();

		
		size_t num_of_records = bin_size / sizeof(PolicyBookEntry);

		sync_cout << "info string read " << path
			<< " , " << num_of_records << " records." << sync_endl;

		book_body.resize(num_of_records);
		result = bin_reader.Read(book_body.data(), bin_size);
		if (result.is_ok())
			sync_cout << "info string read done." << sync_endl;
		else
			sync_cout << "info string Error! : read error." << sync_endl;
	}
	return result;
}


Tools::Result PolicyBook::read_book()
{

#if !defined(ENABLE_POLICY_BOOK_LEARN)
	
	if (!is_loaded())
	{
		
		if (read_book_db_bin().is_ok())
			return Tools::Result::Ok();

		auto result = read_book_db();
		if (result.is_ok())
		{
			
			return write_book_db_bin();
		}
		return result;
	}
	return Tools::Result::Ok();

#else
	

	
	Tools::Result result = read_book_db_bin();
	if (result.is_not_ok())
	{
		result = read_book_db();
		if (result.is_ok())
		{
			
			result = write_book_db_bin();
		}
	}

	
	if (result.is_not_ok())
		result = read_book_db_bin(POLICY_BOOK_LEARN_DB_BIN_NAME);
	else {
		PolicyBook pb;
		result = pb.read_book_db_bin(POLICY_BOOK_LEARN_DB_BIN_NAME);
		
		if (result.is_ok())
			merge_book(pb);
	}
	return result; 

#endif
}




void PolicyBook::sort_book()
{
	sync_cout << "info string sorting the policy book." << sync_endl;
	
	std::sort(book_body.begin(), book_body.end(), [](PolicyBookEntry& x, PolicyBookEntry& y)
		{ return x.key < y.key; });

	
	for (size_t i = 0; i < book_body.size() - 1; ++i)
		if (book_body[i].key == book_body[i + 1].key)
		{
			sync_cout << "info string warning! PolicyBookEntry.key is duplicated." << sync_endl;
			break;
		}
}


void PolicyBook::merge_book(const PolicyBook& book)
{
	sync_cout << "info string merge the policy book. : "
		<< book_body.size() << " records + " << book.book_body.size() << " records." << sync_endl;

	
	book_body.insert(book_body.end(), book.book_body.begin(), book.book_body.end());

	
	garbage_book();

	
	sync_cout << "..done. " << book_body.size() << " records." << sync_endl;
}


void PolicyBook::garbage_book()
{
	
	std::sort(book_body.begin(), book_body.end(), [](PolicyBookEntry& x, PolicyBookEntry& y)
		{ return x.key < y.key; });

	
	size_t read_cursor = 0, write_cursor = 0;
	for (; read_cursor < book_body.size(); )
	{
		
		if (read_cursor < book_body.size() - 1
			&& book_body[read_cursor].key == book_body[read_cursor + 1].key)
		{
			
			std::unordered_map<Move16, u32> counter;

			HASH_KEY key = book_body[read_cursor].key;
			

			
			float value = FLT_MAX;

			for (; read_cursor < book_body.size() && key == book_body[read_cursor].key; ++read_cursor)
			{
				auto& mf = book_body[read_cursor].move_freq;
				for (int k = 0; k < POLICY_BOOK_NUM && mf[k].move16 != Move16::none(); ++k)
					counter[mf[k].move16] += mf[k].freq;
				float v = book_body[read_cursor].value;
				if (v != FLT_MAX)
					value = v;
			}
			

			
			std::vector<MoveFreq32> sorted_counter(counter.begin(), counter.end());

			
			std::sort(sorted_counter.begin(), sorted_counter.end(),
				[](const MoveFreq32& a, const MoveFreq32& b) {
					return a.freq > b.freq; 
				});

			
			book_body[write_cursor].key = key;
			book_body[write_cursor].value = value;

			MoveFreq32Record mf32r;
			for (size_t k = 0; k < POLICY_BOOK_NUM; ++k)
				mf32r.move_freq32[k] = (k < sorted_counter.size()) ? sorted_counter[k] : MoveFreq32();

			
			mf32r.overflow_check();

			book_body[write_cursor].from_move_freq32rec(mf32r);

			
		}
		else {
			if (write_cursor != read_cursor)
				book_body[write_cursor] = book_body[read_cursor];
			

			read_cursor++;
		}
		write_cursor++;
	}
	
	book_body.resize(write_cursor);
}



Tools::Result PolicyBook::write_book_db_bin(std::string path)
{
	SystemIO::BinaryWriter bin_writer;
	auto result = bin_writer.Open(path);
	if (result.is_ok())
	{
		size_t write_size = book_body.size() * sizeof(PolicyBookEntry);
		sync_cout << "info string write " << path << " , write size = " << write_size << " bytes." << sync_endl;

		result = bin_writer.Write(book_body.data(), write_size);
		sync_cout << "info string ..done!" << sync_endl;
	}
	return result;
}



PolicyBookEntry* PolicyBook::probe_policy_book(HASH_KEY key)
{
	PolicyBookEntry pbe;
	pbe.key = key;

	
	auto it = std::lower_bound(book_body.begin(), book_body.end(), pbe,
		[](const PolicyBookEntry& a, const PolicyBookEntry& b) {
			return a.key < b.key;
		});

	

	
	return (it != book_body.end() && it->key == key) ? &*it : nullptr;
}



void PolicyBook::append_sfen_to_db_bin(const std::string& sfen)
{
	Position pos;
	StateList si;
	std::vector<PolicyBookEntry> entries;

	BookTools::feed_position_string(pos, sfen, si, [&](Position& p, Move m) {
		
		
		if (m == Move::none() || !pos.pseudo_legal_s<false>(m))
			return;
		PolicyBookEntry entry;
		entry.key = p.hash_key();
		entry.move_freq[0] = MoveFreq(m.to_move16(), 1);
		entries.push_back(entry);
		});

	
	SystemIO::BinaryWriter writer;
	writer.Open(POLICY_BOOK_LEARN_DB_BIN_NAME, true);
	auto result = writer.Write(entries.data(), sizeof(PolicyBookEntry) * entries.size());
	sync_cout << "info string append " << POLICY_BOOK_LEARN_DB_BIN_NAME << ". status = " << result.to_string() << sync_endl;
}

#if 0

void merge_test()
{
	PolicyBook book1;
	book1.read_book_db_bin("C:/Users/yaneen/largefile/YaneuraOuBookWork/policy_book_2020-2024.20241204.db.bin");

	Position pos;
	StateInfo si;
	pos.set_hirate(&si);
	auto key = pos.hash_key();

	{
		auto ptr = book1.probe_policy_book(key);
		if (ptr != nullptr)
			for (int i = 0; i < POLICY_BOOK_NUM; ++i)
			{
				auto& mr = ptr->move_freq[i];
				sync_cout << "move = " << mr.move16 << " , ratio = " << mr.freq << sync_endl;
			}
	}

	PolicyBook book2;
	book2.read_book_db_bin("C:/Users/yaneen/largefile/YaneuraOuBookWork/policy_book_2020-2024.20241204.db.bin");
	book1.merge_book(book2);

	{
		auto ptr = book1.probe_policy_book(key);
		if (ptr != nullptr)
			for (int i = 0; i < POLICY_BOOK_NUM; ++i)
			{
				auto& mr = ptr->move_freq[i];
				sync_cout << "move = " << mr.move16 << " , ratio = " << mr.freq << sync_endl;
			}
	}

}
#endif

#endif 
