#include "../config.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>		
#include <limits>
#include <numeric>      

#include "book.h"
#include "apery_book.h"
#include "../position.h"
#include "../misc.h"
#include "../search.h"
#include "../thread.h"
#include "../usi.h"
#include "../movegen.h"

namespace YaneuraOu {
namespace Book
{
	std::ostream& operator<<(std::ostream& os, BookMove c)
	{
		os << "move " << c.move << " , ponder " << c.ponder << " , value " << c.value << " , depth " << c.depth;
		return os;
	}

	Move16 convert_move_from_apery(uint16_t apery_move) {
		Move16 m;

		const uint16_t to = apery_move & 0x7f;
		const uint16_t from = (apery_move >> 7) & 0x7f;
		const bool is_promotion = (apery_move & (1 << 14)) != 0;
		if (is_promotion)
			m =  make_move_promote16(static_cast<Square>(from), static_cast<Square>(to));
		else
		{
			const bool is_drop = ((apery_move >> 7) & 0x7f) >= SQ_NB;
			if (is_drop) {
				const uint16_t piece = from - SQ_NB + 1;
				m = make_move_drop16(static_cast<PieceType>(piece), static_cast<Square>(to));
			}
			else
				m = make_move16(static_cast<Square>(from), static_cast<Square>(to));
		}

		return Move16(m);
	};

	void BookMoves::push_back(const BookMove& book_move)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		moves.push_back(book_move);
		sorted = false; 
	}

	std::shared_ptr<BookMove> BookMoves::find_move(const Move16 m16) const
	{
		std::lock_guard<std::recursive_mutex> lock(const_cast<BookMoves*>(this)->mutex_);

		auto it = std::find_if(moves.begin(), moves.end(), [m16](const BookMove& book_move) { return book_move.move == m16; });
		return std::shared_ptr<BookMove>(it == moves.end() ? nullptr : new BookMove(*it));
	}

	void BookMoves::sort_moves()
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (sorted)
			return ;

		std::stable_sort(moves.begin(), moves.end());

		sorted = true;
	}

	BookMove BookMove::from_string(std::string line)
	{
		Move16 move, ponder;

		std::string move_str, ponder_str;
		int value = 0;
		int depth = 0;
		u64 move_count = 1;



		Parser::LineScanner scanner(line);
		move_str   = scanner.get_text();
		ponder_str = scanner.get_text();


		value = (int)scanner.get_number(value);
		depth = (int)scanner.get_number(depth);
		move_count = (u64)scanner.get_number(move_count);


		move   = (move_str   == "none" || move_str   == "None" || move_str   == "resign") ? Move16::none() : USIEngine::to_move16(move_str  );
		ponder = (ponder_str == "none" || ponder_str == "None" || ponder_str == "resign") ? Move16::none() : USIEngine::to_move16(ponder_str);

		return BookMove(move,ponder,value,depth,move_count);
	}

	void BookMoves::insert(const BookMove& bp, bool overwrite)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		for (auto& b : moves)
			if (b == bp)
			{
				if (overwrite)
				{
					auto move_count = b.move_count;
					b = bp;
					b.move_count += move_count;

					sorted = false; 
				}
				return;
			}

		moves.push_back(bp);

		sorted = false; 
	}

	void MemoryBook::merge(MemoryBook& book2)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		book2.foreach([&](const std::string& sfen, const Book::BookMovesPtr book_moves)
		{
			this->append(sfen, book_moves);
		});
	}

	void BookMoves::foreach(std::function<void(BookMove&)> f)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		for(auto& it : moves)
			f(it);
	}

	void MemoryBook::insert(const std::string& sfen, const BookMove& bp , bool overwrite)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		auto it = book_body.find(sfen);
		if (it == book_body.end())
		{
			BookMovesPtr move_list(new BookMoves());
			move_list->push_back(bp);
			book_body[sfen] = move_list;
		}
		else {
			auto& book_moves = *it->second;
			book_moves.insert(bp, overwrite);
		}
	}

	void MemoryBook::foreach(std::function<void(const std::string& , const BookMovesPtr)> f)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		for(auto& it : book_body)
			f(it.first,it.second);
	}


	static std::unique_ptr<AperyBook> apery_book;
	static const constexpr char* kAperyBookName = "book.bin";
	static constexpr std::array<char, 16> YbbMagic = {
		'Y', 'A', 'N', 'E', '-', 'B', 'I', 'N',
		'B', 'O', 'O', 'K', '-', 'V', '1', '\0',
	};
	static constexpr uint64_t YbbHeaderSize = 32;
	static constexpr uint64_t YbbIndexRecordSize = 44;
	static constexpr uint64_t YbbFlagMoveDepth = 1;
	static constexpr uint64_t YbbKnownFlags = YbbFlagMoveDepth;

	static bool ends_with(const std::string& text, const std::string& suffix)
	{
		return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
	}

	static bool is_ybb_book(const std::string& filename)
	{
		return ends_with(filename, ".ybb");
	}

	static bool is_db_book(const std::string& filename)
	{
		return ends_with(filename, ".db");
	}

	static std::string ybb_book_name_from_db_name(const std::string& db_filename)
	{
		if (!ends_with(db_filename, ".db"))
			return std::string();
		std::string ybb_filename = db_filename;
		ybb_filename.resize(ybb_filename.size() - std::string(".db").size());
		ybb_filename += ".ybb";
		return ybb_filename;
	}

	static std::string resolve_book_filename_with_ybb_fallback(const std::string& filename)
	{
		if (Path::Exists(filename))
			return filename;

		const auto ybb_filename = ybb_book_name_from_db_name(filename);
		if (!ybb_filename.empty() && Path::Exists(ybb_filename))
			return ybb_filename;

		return filename;
	}

	static std::string book_name_without_extension(const std::string& filename)
	{
		if (is_db_book(filename))
			return filename.substr(0, filename.size() - std::string(".db").size());
		if (is_ybb_book(filename))
			return filename.substr(0, filename.size() - std::string(".ybb").size());
		return std::string();
	}

	static std::string priority_book_filename(const std::string& stem, int index, const std::string& extension)
	{
		auto number = std::to_string(index);
		while (number.size() < 3)
			number = "0" + number;
		return stem + "-" + number + extension;
	}

	static std::string resolve_priority_book_filename(const std::string& base_filename, int index)
	{
		const auto stem = book_name_without_extension(base_filename);
		if (stem.empty())
			return std::string();

		const auto primary_extension   = is_ybb_book(base_filename) ? std::string(".ybb") : std::string(".db");
		const auto secondary_extension = is_ybb_book(base_filename) ? std::string(".db") : std::string(".ybb");
		const auto primary_filename    = priority_book_filename(stem, index, primary_extension);
		const auto secondary_filename  = priority_book_filename(stem, index, secondary_extension);

		if (Path::Exists(primary_filename))
		{
			if (Path::Exists(secondary_filename))
				sync_cout << "info string priority book file exists twice. use : " << primary_filename << sync_endl;
			return primary_filename;
		}
		if (Path::Exists(secondary_filename))
			return secondary_filename;

		return std::string();
	}

	static bool read_u16_le(std::istream& is, uint16_t& value)
	{
		std::array<unsigned char, 2> bytes{};
		is.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
		if (!is)
			return false;
		value = uint16_t(bytes[0] | (bytes[1] << 8));
		return true;
	}

	static bool read_u64_le(std::istream& is, uint64_t& value)
	{
		std::array<unsigned char, 8> bytes{};
		is.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
		if (!is)
			return false;
		value = 0;
		for (int i = 7; i >= 0; --i)
		{
			value <<= 8;
			value |= bytes[size_t(i)];
		}
		return true;
	}

	static bool read_file_to_memory(const std::string& filename, std::vector<unsigned char>& data)
	{
		std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
		if (!file)
			return false;

		const auto size = file.tellg();
		if (size < std::streampos(0))
			return false;

		data.resize(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		if (!data.empty())
			file.read(reinterpret_cast<char*>(data.data()), data.size());
		return bool(file) || data.empty();
	}

	static bool read_u16_le_from_memory(const std::vector<unsigned char>& data, uint64_t offset, uint16_t& value)
	{
		if (offset > data.size() || data.size() - offset < 2)
			return false;
		value = uint16_t(data[size_t(offset)] | (data[size_t(offset + 1)] << 8));
		return true;
	}

	static bool read_u64_le_from_memory(const std::vector<unsigned char>& data, uint64_t offset, uint64_t& value)
	{
		if (offset > data.size() || data.size() - offset < 8)
			return false;

		value = 0;
		for (int i = 7; i >= 0; --i)
		{
			value <<= 8;
			value |= data[size_t(offset + i)];
		}
		return true;
	}

	struct YbbIndexEntry
	{
		PackedSfen packed_sfen{};
		uint64_t moves_offset = 0;
		uint16_t ply = 0;
		uint16_t move_count = 0;
	};

	static uint64_t ybb_move_record_size(uint64_t flags)
	{
		return (flags & YbbFlagMoveDepth) ? 6 : 4;
	}

	static bool ybb_index_size(uint64_t record_count, uint64_t& index_size)
	{
		if (record_count > (std::numeric_limits<uint64_t>::max() - YbbHeaderSize) / YbbIndexRecordSize)
			return false;
		index_size = YbbHeaderSize + record_count * YbbIndexRecordSize;
		return true;
	}

	static bool read_ybb_header(std::istream& is, uint64_t& record_count, uint64_t& flags)
	{
		std::array<char, 16> magic{};
		is.read(magic.data(), magic.size());
		if (!is || magic != YbbMagic)
			return false;
		if (!read_u64_le(is, record_count))
			return false;
		if (!read_u64_le(is, flags))
			return false;
		return (flags & ~YbbKnownFlags) == 0;
	}

	static bool read_ybb_index_entry(std::istream& is, YbbIndexEntry& entry)
	{
		is.read(reinterpret_cast<char*>(entry.packed_sfen.data), 32);
		if (!is)
			return false;
		if (!read_u64_le(is, entry.moves_offset))
			return false;
		if (!read_u16_le(is, entry.ply))
			return false;
		if (!read_u16_le(is, entry.move_count))
			return false;
		return true;
	}

	static bool read_ybb_header_from_memory(const std::vector<unsigned char>& data, uint64_t& record_count, uint64_t& flags)
	{
		if (data.size() < YbbHeaderSize)
			return false;
		if (std::memcmp(data.data(), YbbMagic.data(), YbbMagic.size()) != 0)
			return false;
		if (!read_u64_le_from_memory(data, 16, record_count))
			return false;
		if (!read_u64_le_from_memory(data, 24, flags))
			return false;
		if ((flags & ~YbbKnownFlags) != 0)
			return false;
		uint64_t index_size = 0;
		if (!ybb_index_size(record_count, index_size))
			return false;
		if (data.size() < index_size)
			return false;
		return true;
	}

	static bool read_ybb_index_entry_from_memory(const std::vector<unsigned char>& data, uint64_t record_index, YbbIndexEntry& entry)
	{
		if (record_index > (std::numeric_limits<uint64_t>::max() - YbbHeaderSize) / YbbIndexRecordSize)
			return false;
		const uint64_t offset = YbbHeaderSize + record_index * YbbIndexRecordSize;
		if (offset > data.size() || data.size() - offset < YbbIndexRecordSize)
			return false;

		std::memcpy(entry.packed_sfen.data, data.data() + size_t(offset), 32);
		if (!read_u64_le_from_memory(data, offset + 32, entry.moves_offset))
			return false;
		if (!read_u16_le_from_memory(data, offset + 40, entry.ply))
			return false;
		if (!read_u16_le_from_memory(data, offset + 42, entry.move_count))
			return false;
		return true;
	}

	static bool read_ybb_index_entry_at(std::fstream& index_fs, uint64_t record_index, YbbIndexEntry& entry)
	{
		index_fs.clear();
		index_fs.seekg(std::streamoff(YbbHeaderSize + record_index * YbbIndexRecordSize), std::ios::beg);
		if (!index_fs)
			return false;
		return read_ybb_index_entry(index_fs, entry);
	}

	static int compare_packed_sfen(const PackedSfen& lhs, const PackedSfen& rhs)
	{
		return std::memcmp(lhs.data, rhs.data, 32);
	}

	static BookMovesPtr read_ybb_moves(std::istream& moves_fs, const YbbIndexEntry& entry, uint64_t flags, uint64_t moves_base)
	{
		BookMovesPtr book_moves(new BookMoves());
		moves_fs.clear();
		moves_fs.seekg(std::streamoff(moves_base + entry.moves_offset), std::ios::beg);
		if (!moves_fs)
			return BookMovesPtr();
		for (uint16_t i = 0; i < entry.move_count; ++i)
		{
			uint16_t move16_value = 0;
			uint16_t eval_value = 0;
			uint16_t depth_value = 0;
			if (!read_u16_le(moves_fs, move16_value) || !read_u16_le(moves_fs, eval_value))
				return BookMovesPtr();
			if ((flags & YbbFlagMoveDepth) && !read_u16_le(moves_fs, depth_value))
				return BookMovesPtr();
			auto move16 = Move16(move16_value);
			auto value = int(int16_t(eval_value));
			auto depth = int(depth_value);
			book_moves->push_back(BookMove(move16, Move16::none(), value, depth, 0));
		}
		book_moves->sort_moves();
		return book_moves;
	}

	static BookMovesPtr read_ybb_moves_from_memory(const std::vector<unsigned char>& moves_data, const YbbIndexEntry& entry, uint64_t flags, uint64_t moves_base)
	{
		if (moves_base > moves_data.size() || entry.moves_offset > moves_data.size() - moves_base)
			return BookMovesPtr();
		const uint64_t move_record_size = ybb_move_record_size(flags);
		const uint64_t moves_size = uint64_t(entry.move_count) * move_record_size;
		const uint64_t absolute_moves_offset = moves_base + entry.moves_offset;
		if (moves_data.size() - absolute_moves_offset < moves_size)
			return BookMovesPtr();

		BookMovesPtr book_moves(new BookMoves());
		for (uint16_t i = 0; i < entry.move_count; ++i)
		{
			const uint64_t offset = absolute_moves_offset + uint64_t(i) * move_record_size;
			uint16_t move16_value = 0;
			uint16_t eval_value = 0;
			uint16_t depth_value = 0;
			if (!read_u16_le_from_memory(moves_data, offset, move16_value) || !read_u16_le_from_memory(moves_data, offset + 2, eval_value))
				return BookMovesPtr();
			if ((flags & YbbFlagMoveDepth) && !read_u16_le_from_memory(moves_data, offset + 4, depth_value))
				return BookMovesPtr();
			auto move16 = Move16(move16_value);
			auto value = int(int16_t(eval_value));
			auto depth = int(depth_value);
			book_moves->push_back(BookMove(move16, Move16::none(), value, depth, 0));
		}
		book_moves->sort_moves();
		return book_moves;
	}
	void MemoryBook::set_options(OptionsMap& options)
	{
		this->options.set_ref(options);
	}

	std::string MemoryBook::trim(std::string input) const
	{
		return options["IgnoreBookPly"] ? StringExtension::trim_number(input) : StringExtension::trim(input);
	}

	Tools::Result MemoryBook::read_book(const std::string& filename, bool on_the_fly_)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		bool ignore_book_ply_ = options["IgnoreBookPly"];
		if (this->book_name == filename && this->on_the_fly == on_the_fly_ && this->ignoreBookPly == ignore_book_ply_)
			return Tools::Result::Ok();

		this->book_name = "";
		this->pure_book_name = "";

		book_body.clear();
		this->on_the_fly = false;
		this->ybb_book = false;
		this->ybb_memory_book = false;
		this->ybb_record_count = 0;
		this->ybb_flags = 0;
		this->ybb_moves_base = 0;
		this->ybb_moves_name.clear();
		this->ybb_index_data.clear();
		if (ybb_index_fs.is_open())
			ybb_index_fs.close();
		if (ybb_moves_fs.is_open())
			ybb_moves_fs.close();
		this->ignoreBookPly = ignore_book_ply_;

		auto pure_filename = Path::GetFileName(filename);
		auto actual_filename = filename;
		auto actual_pure_filename = pure_filename;

		if (pure_filename == "no_book")
		{
			this->book_name = filename;
			this->pure_book_name = pure_filename;
			return Tools::Result::Ok();
		}

		if (pure_filename == kAperyBookName) {
			apery_book = std::unique_ptr<AperyBook>(new AperyBook(filename));
		}
		else {
			actual_filename = resolve_book_filename_with_ybb_fallback(filename);
			actual_pure_filename = Path::GetFileName(actual_filename);
			if (actual_filename != filename)
				sync_cout << "info string book file fallback : " << filename << " -> " << actual_filename << sync_endl;

			const bool ybb_book_file = is_ybb_book(actual_pure_filename);

			if (on_the_fly_)
			{
				if (ybb_book_file)
				{
					ybb_index_fs.open(actual_filename, std::ios::in | std::ios::binary);
					if (ybb_index_fs.fail())
					{
						sync_cout << "info string Error! : can't read file : " + actual_filename << sync_endl;
						return Tools::Result(Tools::ResultCode::FileNotFound);
					}
					ybb_moves_fs.open(actual_filename, std::ios::in | std::ios::binary);
					if (ybb_moves_fs.fail())
					{
						sync_cout << "info string Error! : can't read file : " + actual_filename << sync_endl;
						return Tools::Result(Tools::ResultCode::FileNotFound);
					}

					if (!read_ybb_header(ybb_index_fs, ybb_record_count, ybb_flags))
					{
						sync_cout << "info string Error! : invalid ybb file : " << actual_filename << sync_endl;
						return Tools::Result(Tools::ResultCode::FileReadError);
					}

					if (!ybb_index_size(ybb_record_count, ybb_moves_base))
					{
						sync_cout << "info string Error! : invalid ybb file : " << actual_filename << sync_endl;
						return Tools::Result(Tools::ResultCode::FileReadError);
					}

					this->ybb_book = true;
					this->ybb_moves_name = actual_filename;
					this->on_the_fly = true;
					this->book_name = filename;
					this->pure_book_name = actual_pure_filename;
					return Tools::Result::Ok();
				}

				if (fs.is_open())
					fs.close();

				fs.open(actual_filename, std::ios::in);
				if (fs.fail())
				{
					sync_cout << "info string Error! : can't read file : " + actual_filename << sync_endl;
					return Tools::Result(Tools::ResultCode::FileNotFound);
				}

				this->on_the_fly = true;
				this->book_name = filename;
				return Tools::Result::Ok();
			}

			sync_cout << "info string read book file : " << actual_filename << sync_endl;

			if (ybb_book_file)
			{
				if (!read_file_to_memory(actual_filename, ybb_index_data))
				{
					sync_cout << "info string Error! : can't read file : " + actual_filename << sync_endl;
					return Tools::Result(Tools::ResultCode::FileNotFound);
				}

				uint64_t record_count = 0;
				uint64_t flags = 0;
				if (!read_ybb_header_from_memory(ybb_index_data, record_count, flags))
				{
					sync_cout << "info string Error! : invalid ybb file : " << actual_filename << sync_endl;
					return Tools::Result(Tools::ResultCode::FileReadError);
				}

				if (!ybb_index_size(record_count, ybb_moves_base))
				{
					sync_cout << "info string Error! : invalid ybb file : " << actual_filename << sync_endl;
					return Tools::Result(Tools::ResultCode::FileReadError);
				}

				this->ybb_memory_book = true;
				this->ybb_record_count = record_count;
				this->ybb_flags = flags;
				this->ybb_moves_name = actual_filename;
				this->book_name = filename;
				this->pure_book_name = actual_pure_filename;

				sync_cout << "info string read book done. number of positions = " << size() << sync_endl;

				return Tools::Result::Ok();
			}

			SystemIO::TextReader reader;
			reader.SetTrim(true);
			reader.SkipEmptyLine(true);

			auto result = reader.Open(actual_filename);
			if (result.is_not_ok())
			{
				sync_cout << "info string Error! : can't read file : " + actual_filename << sync_endl;
				return result; 
			}

			std::string sfen;

#if 0
			string last_sfen;
			int last_sfen_ply = 0;
			bool ignore_book_ply = options["IgnoreBookPly"];
#endif

			bool ignoreBookPly = options["IgnoreBookPly"];

			Tools::ProgressBar progress(reader.GetSize());

			std::string line;
			while(reader.ReadLine(line).is_ok())
			{
				progress.check(reader.GetFilePos());

				if (line.length() >= 1 && line[0] == '#')
					continue;

				if (line.length() >= 2 && line.substr(0, 2) == "//")
					continue;

				if (line.length() >= 5 && line.substr(0, 5) == "sfen ")
				{

					sfen = line.substr(5); 
					if (ignoreBookPly)
						StringExtension::trim_number_inplace(sfen); 

#if 0
					if (ignore_book_ply)
					{
						int ply = StringExtension::to_int(StringExtension::mid(line, sfen.length() + 5), 0);


						if (last_sfen != sfen)
							last_sfen_ply = int_max;
						else if (last_sfen_ply < ply)
							sfen = "";

						last_sfen_ply = std::min(last_sfen_ply,ply); 
						last_sfen = sfen;
					}
#endif

					continue;
				}

				if (sfen.size() == 0)
					continue;

				insert(sfen, BookMove::from_string(line));





			}
		}

		this->book_name = filename;
		this->pure_book_name = actual_pure_filename;

		sync_cout << "info string read book done. number of positions = " << size() << sync_endl;

		return Tools::Result::Ok();
	}

	Tools::Result MemoryBook::write_book(const std::string& filename ) const
	{
		std::lock_guard<std::recursive_mutex> lock(const_cast<MemoryBook*>(this)->mutex_);

		if (ybb_memory_book)
		{
			sync_cout << "info string Error! : ybb memory book cannot be written by MemoryBook::write_book(). use convert_ybb_to_db.py." << sync_endl;
			return Tools::Result(Tools::ResultCode::FileWriteError);
		}



		SystemIO::TextWriter writer;
		if (writer.Open(filename).is_not_ok())
			return Tools::Result(Tools::ResultCode::FileOpenError);

		std::cout << "write " + filename << std::endl;

		writer.WriteLine(BookDBHeader2016_100);

		std::vector<std::pair<std::string, BookMovesPtr> > vectored_book;


		std::unordered_map<std::string, int> book_ply;

		for (auto& it : book_body)
		{
			if (it.second->size() == 0)
				continue;
			vectored_book.push_back(it);
		}


		u64 counter = 0;
		Tools::ProgressBar progress(vectored_book.size() * 2);

		{
			Position pos;

			for (auto& it : vectored_book)
			{
				StateInfo si;
				pos.set(it.first, &si);
				auto sfen = pos.sfen();
				it.first = sfen;

				auto sfen_left = StringExtension::trim_number(sfen); 
				int ply = StringExtension::to_int(sfen.substr(sfen_left.length()), 0);

				auto it2 = book_ply.find(sfen_left);
				if (it2 == book_ply.end())
					book_ply[sfen_left] = ply; 
				else
					it2->second = std::min(it2->second, ply); 

				progress.check(++counter);
			}
		}

		std::sort(vectored_book.begin(), vectored_book.end(),
			[](const std::pair<std::string, BookMovesPtr>&lhs, const std::pair<std::string, BookMovesPtr>&rhs) {
			return lhs.first < rhs.first;
		});

		for (auto& it : vectored_book)
		{

			auto sfen = it.first;
			auto sfen_left = StringExtension::trim_number(sfen); 
			int ply = StringExtension::to_int(sfen.substr(sfen_left.length()), 0);
			if (book_ply[sfen_left] != ply)
				continue;


			writer.WriteLine("sfen " + it.first ); 

			auto& move_list = *it.second;

			move_list.sort_moves();

			for (auto& bp : move_list)
				if (writer.WriteLine(to_usi_string(bp.move) + ' ' + to_usi_string(bp.ponder) + ' '
					+ std::to_string(bp.value) + " " + std::to_string(bp.depth) + " " + std::to_string(bp.move_count)).is_not_ok())
					return Tools::Result(Tools::ResultCode::FileWriteError);

			progress.check(++counter);
		}

		writer.Close();

		return Tools::Result::Ok();
	}

	BookMovesPtr MemoryBook::find(const std::string& sfen) const
	{
		std::lock_guard<std::recursive_mutex> lock(const_cast<MemoryBook*>(this)->mutex_);

		auto it = book_body.find(trim(sfen));
		return it == book_body.end() ? BookMovesPtr() : it->second;
	}

	void MemoryBook::append(const std::string& sfen, const Book::BookMovesPtr ptr)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		book_body[sfen] = ptr;
	}

	BookMovesPtr make_flipped_bookmoves(BookMovesPtr pt)
	{
		BookMovesPtr entry(new BookMoves());
		pt->foreach([&](BookMove& bm)
			{
				BookMove flip_book_move(flip_move(bm.move), flip_move(bm.ponder), bm.value , bm.depth , bm.move_count);
				entry->push_back(flip_book_move);
			}
		);
		entry->sort_moves();
		return entry;
	};

	BookMovesPtr MemoryBook::find_bookmoves_on_the_fly(std::string sfen)
	{
		BookMovesPtr pml_entry(new BookMoves());

		sfen = trim(sfen);


		fs.clear();
		fs.seekg(0, std::ios::beg);
		auto file_start = fs.tellg();

		fs.clear();
		fs.seekg(0, std::ios::end);

		auto file_size = s64(fs.tellg() - file_start);


		auto next_sfen = [&](s64 seek_from , s64& last_pos)
		{
			std::string line;

			seek_from = std::max( s64(0), seek_from - 2);

			fs.clear();
			fs.seekg(seek_from , std::fstream::beg);


			std::getline(fs, line);

			last_pos = seek_from + (s64)line.size() + 1;

			while (std::getline(fs, line))
			{
				last_pos += s64(line.size()) + 1;

				if (!line.compare(0, 4, "sfen"))
				{
					return trim(line.substr(5));
				}
			}
			return std::string();
		};


		s64 s = 0, e = file_size, m , last_pos;

		while (true)
		{
			m = (s + e) / 2;

			auto sfen2 = next_sfen(m, last_pos);
			if (sfen2 == "" || sfen < sfen2) {

				e = m;

			} else if (sfen > sfen2) {


				s = last_pos;

			} else {
				break;
			}

			if (s + 40 > e)
			{
				if ( next_sfen(s, last_pos) == sfen)
					break;

				return BookMovesPtr();
			}

		}



		while (!fs.eof())
		{
			std::string line;
			std::getline(fs, line);

			if (line.length() >= 1 && line[0] == '#')
				continue;

			if (line.length() >= 2 && line.substr(0, 2) == "//")
				continue;

			if (  (line.length() >= 5 && line.substr(0, 5) == "sfen ")
				|| line.length() == 0 )
			{
				break;
			}

			pml_entry->push_back(BookMove::from_string(line));
		}
		pml_entry->sort_moves();
		return pml_entry;
	}

	BookMovesPtr MemoryBook::find_ybb_bookmoves_on_the_fly(const Position& pos)
	{
		PackedSfen target{};
		const_cast<Position&>(pos).sfen_pack(target);
		return find_ybb_bookmoves_on_the_fly(target, uint16_t(pos.game_ply()));
	}

	BookMovesPtr MemoryBook::find_ybb_bookmoves_on_the_fly(const PackedSfen& target, uint16_t game_ply)
	{
		if (!ybb_book || !ybb_index_fs.is_open() || !ybb_moves_fs.is_open())
			return BookMovesPtr();

		uint64_t left  = 0;
		uint64_t right = ybb_record_count;
		while (left < right)
		{
			const uint64_t middle = left + (right - left) / 2;
			YbbIndexEntry entry;
			if (!read_ybb_index_entry_at(ybb_index_fs, middle, entry))
				return BookMovesPtr();

			const int compare = compare_packed_sfen(target, entry.packed_sfen);
			if (compare < 0)
				right = middle;
			else if (compare > 0)
				left = middle + 1;
			else
			{
				if (!ignoreBookPly && entry.ply != game_ply)
					return BookMovesPtr();
				return read_ybb_moves(ybb_moves_fs, entry, ybb_flags, ybb_moves_base);
			}
		}

		return BookMovesPtr();
	}

	BookMovesPtr MemoryBook::find_ybb_bookmoves_in_memory(const PackedSfen& target, uint16_t game_ply)
	{
		if (!ybb_memory_book)
			return BookMovesPtr();

		uint64_t left  = 0;
		uint64_t right = ybb_record_count;
		while (left < right)
		{
			const uint64_t middle = left + (right - left) / 2;
			YbbIndexEntry entry;
			if (!read_ybb_index_entry_from_memory(ybb_index_data, middle, entry))
				return BookMovesPtr();

			const int compare = compare_packed_sfen(target, entry.packed_sfen);
			if (compare < 0)
				right = middle;
			else if (compare > 0)
				left = middle + 1;
			else
			{
				if (!ignoreBookPly && entry.ply != game_ply)
					return BookMovesPtr();
				return read_ybb_moves_from_memory(ybb_index_data, entry, ybb_flags, ybb_moves_base);
			}
		}

		return BookMovesPtr();
	}

	BookMovesPtr MemoryBook::find(const Position& pos)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (pure_book_name == "no_book")
			return BookMovesPtr();

		if (pure_book_name == kAperyBookName) {

			BookMovesPtr pml_entry(new BookMoves());

			const auto& entries = apery_book->get_entries(pos);
			int64_t sum_count = 0;
			for (const auto& entry : entries) {
				sum_count += entry.count;
			}


			for (const auto& entry : entries) {

				Move16 theMove16 = convert_move_from_apery(entry.fromToPro);
				Move16 thePonder = Move16::none();
#if 0
				Move   theMove   = pos.to_move(theMove16);
				if (pos.pseudo_legal_s<true>(theMove) && pos.legal(theMove))
				{
					StateInfo si;
					Position* thePos = const_cast<Position*>(&pos);
					thePos->do_move(theMove,si);
					const auto& entries2 = apery_book->get_entries(pos);
					if (entries2.size())
						thePonder = convert_move_from_apery(entries2[0].fromToPro); 
					thePos->undo_move(theMove);
				}
#endif
				BookMove book_pos(theMove16 , thePonder , entry.score, 256, entry.count);
				pml_entry->insert(book_pos,true);
			}
			pml_entry->sort_moves();

			return 	pml_entry;
		}
		else {

			if (!on_the_fly && !ybb_memory_book && book_body.size() == 0)
				return BookMovesPtr();

			BookType::iterator it;


			if (on_the_fly)
			{
				if (ybb_book)
				{
					PackedSfen target{};
					const_cast<Position&>(pos).sfen_pack(target);
					auto entry = find_ybb_bookmoves_on_the_fly(target, uint16_t(pos.game_ply()));
					if (entry == nullptr && options["FlippedBook"])
					{
						target.flip();
						entry = find_ybb_bookmoves_on_the_fly(target, uint16_t(pos.game_ply()));
						if (entry != nullptr)
							entry = make_flipped_bookmoves(entry);
					}
					return entry;
				}

				auto sfen = pos.sfen();
				auto entry = find_bookmoves_on_the_fly(sfen);
				if (entry == nullptr)
				{
					if (options["FlippedBook"])
					{
						entry = find_bookmoves_on_the_fly(Position::sfen_to_flipped_sfen(sfen));
						if (entry != nullptr)
							entry = make_flipped_bookmoves(entry);
					}
				}
				return entry;

			} else {

				if (ybb_memory_book)
				{
					PackedSfen target{};
					const_cast<Position&>(pos).sfen_pack(target);
					auto entry = find_ybb_bookmoves_in_memory(target, uint16_t(pos.game_ply()));
					if (entry == nullptr && options["FlippedBook"])
					{
						target.flip();
						entry = find_ybb_bookmoves_in_memory(target, uint16_t(pos.game_ply()));
						if (entry != nullptr)
							entry = make_flipped_bookmoves(entry);
					}
					return entry;
				}

				auto sfen = pos.sfen();
				it = book_body.find(trim(sfen));
				if (it != book_body.end())
				{
					it->second->sort_moves();
					return BookMovesPtr(it->second);
				}

				if (options["FlippedBook"])
				{
					it = book_body.find(trim(Position::sfen_to_flipped_sfen(sfen)));
					if (it != book_body.end())
					{
						return make_flipped_bookmoves(it->second);
					}
				}

				return BookMovesPtr();
			}
		}
	}


	void BookMoveSelector::set_options(OptionsMap& o) {
        options.set_ref(o);
		for (auto& memory_book : memory_books)
			memory_book->set_options(o);
    }

	void BookMoveSelector::add_options(OptionsMap& o)
	{
        set_options(o);
		const bool book_options_v2 = options.book_options_v2();

		options.add("USI_OwnBook", Option(true));

		if (!book_options_v2)
			options.add("NarrowBook", Option(false));

        options.add("BookMoves", Option(book_options_v2 ? 200 : 16, 0, 10000));

        options.add("BookIgnoreRate", Option(0, 0, 100));



		std::vector<std::string> book_list = { "no_book" , "standard_book.db"
			, "yaneura_book1.db" , "yaneura_book2.db" , "yaneura_book3.db", "yaneura_book4.db"
			, "user_book1.db", "user_book2.db", "user_book3.db", "book.bin" };

#if !defined(__EMSCRIPTEN__)
		options.add("BookFile", Option(book_list, book_list[0]));
#else
        options.add("BookFile", Option(book_list, book_list[0]));
#endif

#if !defined(__EMSCRIPTEN__)
        options.add("BookDir", Option("book"));
#else
        options.add("BookDir", Option("."));
#endif


		if (book_options_v2)
		{
			options.add("BookEvalBlackDiff", Option(0, 0, 99999));
			options.add("BookEvalWhiteDiff", Option(0, 0, 99999));
		}
		else
			options.add("BookEvalDiff", Option(30, 0, 99999));
        options.add("BookEvalBlackLimit", Option(0, -99999, 99999));
        options.add("BookEvalWhiteLimit", Option(-140, -99999, 99999));
		if (book_options_v2)
		{
			options.add("BookDepthBlackLimit", Option(0, 0, 99999));
			options.add("BookDepthWhiteLimit", Option(5, 0, 99999));
		}
		else
			options.add("BookDepthLimit", Option(16, 0, 99999));

        options.add("BookOnTheFly", Option(book_options_v2));

		if (!book_options_v2)
			options.add("ConsiderBookMoveCount", Option(false));

        options.add("BookPvMoves", Option(8, 1, MAX_PLY));

        options.add("IgnoreBookPly", Option(book_options_v2));

        options.add("FlippedBook", Option(true));
	}

	void BookMoveSelector::read_book()
	{
		const auto new_book_names    = get_book_names();
		const bool new_on_the_fly    = bool(options["BookOnTheFly"]);
		const bool new_ignoreBookPly = bool(options["IgnoreBookPly"]);

		if (book_names == new_book_names
			&& book_on_the_fly == new_on_the_fly
			&& ignoreBookPly == new_ignoreBookPly)
			return;

		memory_books.clear();
		book_names.clear();
		book_on_the_fly = new_on_the_fly;
		ignoreBookPly = new_ignoreBookPly;

		for (const auto& book_name : new_book_names)
		{
			auto memory_book = std::unique_ptr<MemoryBook>(new MemoryBook());
			memory_book->set_options(options.get_ref());

			if (memory_book->read_book(book_name, new_on_the_fly).is_ok())
			{
				memory_books.push_back(std::move(memory_book));
				book_names.push_back(book_name);
			}
		}
	}

	std::string BookMoveSelector::get_book_name() const
	{
        std::string abs_book_dir =
            Path::Combine(Directory::GetBinaryFolder(), std::string(options["BookDir"]));
		return Path::Combine( abs_book_dir , std::string(options["BookFile"]));
	}

	std::vector<std::string> BookMoveSelector::get_book_names() const
	{
		const auto base_book_name = get_book_name();
		std::vector<std::string> names;

		for (int index = 0;; ++index)
		{
			const auto priority_book_name = resolve_priority_book_filename(base_book_name, index);
			if (priority_book_name.empty())
				break;
			names.push_back(priority_book_name);
		}

		names.push_back(base_book_name);
		return names;
	}

	BookMovesPtr BookMoveSelector::find_in_books(Position& pos)
	{
		for (auto& memory_book : memory_books)
		{
			auto book_moves = memory_book->find(pos);
			if (book_moves != nullptr && book_moves->size() != 0)
				return book_moves;
		}

		return BookMovesPtr();
	}


    std::string BookMoveSelector::pv_builder(Position&                    pos,
                                                const Search::UpdateContext& updates,
                                                Move16                       m16,
                                                int                          rest_ply) {
        ASSERT_LV3(rest_ply > 0);

        std::string result = "";

        auto add_space_check = [&result]() {
            if (result != "")
                result += " ";
        };

        Move m = pos.to_move(m16);

        if (pos.pseudo_legal_s<true>(m) && pos.legal(m))
        {
            StateInfo si;
            pos.do_move(m, si);

            result = m16.to_usi_string();
            --rest_ply;

            auto rep = pos.is_repetition(MAX_PLY);
            if (rep != REPETITION_NONE)
            {
                add_space_check();
                result += to_usi_string(rep);
            }
            else
            {

                Move16 bestMove16, ponderMove16;
                Value  value;
                if (probe_impl(pos, false, updates, bestMove16, ponderMove16, value,
                                true ))
                {

                    std::string result2;
                    if (rest_ply >= 1)
                    {
                        result2 = pv_builder(pos, updates, bestMove16, rest_ply);

                        if (result2.empty())
                        {
                            if (rest_ply >= 1 && ponderMove16.to_u16() != MOVE_NONE)
                            {
                                add_space_check();
                                result += " " + ponderMove16.to_usi_string();
                            }
                        }
                        else
                        {
                            add_space_check();
                            result += result2;
                        }

                    }
                }
            }
            pos.undo_move(m);
        }
        return result;
    }

	bool BookMoveSelector::probe_impl(Position& rootPos, bool isRoot, const Search::UpdateContext& updates , Move16& bestMove , Move16& ponderMove , Value& value, bool forceHit)
	{
		if (!forceHit)
		{
			if ((int)options["BookIgnoreRate"] > (int)prng.rand(100)) {
				return false;
			}

			int book_ply = (int)options["BookMoves"];
			if (!forceHit && rootPos.game_ply() > book_ply)
				return false;
		}

		auto it = find_in_books(rootPos);
		if (it == nullptr || it->size()==0)
			return false;





		auto move_list = *it;

		{
			auto it_end = std::remove_if(move_list.begin(), move_list.end(), [&](Book::BookMove& m) {
				Move move = rootPos.to_move(m.move);
				bool legal =  rootPos.pseudo_legal_s<true>(move) && rootPos.legal(move);

				if (!legal)
				{
					sync_cout << "info string Error! : Illegal Move In Book DB : move = " << move
							  << " , sfen = " << rootPos.sfen() << sync_endl;

				}
#if 0
				else {
					legal &= rootPos.pseudo_legal(move);
				}
#endif

				return !legal;
				});
			move_list.erase(it_end, move_list.end());
		}

		const u64 move_count_total = std::accumulate(move_list.begin(), move_list.end(), (u64)0, [](u64 acc, BookMove& b) { return acc + b.move_count; });
		const bool has_move_count = move_count_total != 0;

		const bool book_options_v2 = options.book_options_v2();

        if (isRoot)
        {

            int pv_moves = (int) options["BookPvMoves"];

            for (size_t i = 0; i < move_list.size(); ++i)
            {
                auto& it = move_list[i];


                std::string prob_str;
                if (has_move_count)
                {
                    double             prob = 100 * it.move_count / double(move_count_total);
                    std::ostringstream oss;
                    oss << std::fixed << " (" << std::setprecision(2) << prob << "%)";
                    prob_str = oss.str();
                }

                YaneuraOu::Search::InfoFull info;

                info.multiPV   = i + 1;
                info.score     = Score::from_internal_value(it.value);
                info.depth     = it.depth;
                std::string pv = pv_builder(rootPos, updates, it.move, pv_moves) + prob_str;
                info.pv        = pv;
                info.selDepth  = 0;
                info.nodes     = 0;
                info.nps       = 0;
                info.timeMs    = 0;
                info.hashfull  = 0;

                updates.onUpdateFull(info);
            }
        }



		if (forceHit)
		{
			auto value_limit = move_list[0].value;

			auto it_end = std::remove_if(move_list.begin(), move_list.end(), [&](Book::BookMove & m) { return m.value < value_limit; });
			move_list.erase(it_end, move_list.end());

		} else {

			bool narrowBook = book_options_v2 ? false : bool(options["NarrowBook"]);

			if (narrowBook && has_move_count)
			{
				auto n = move_list.size();

				auto it_end = std::remove_if(move_list.begin(), move_list.end(), [&](Book::BookMove & m)
					{ return ((double)m.move_count / move_count_total) < 0.1; });
				move_list.erase(it_end, move_list.end());

                if (n != move_list.size())
                    updates.onUpdateString(
                        std::string_view("NarrowBook : " + std::to_string(n) + " moves to "
                        + std::to_string(move_list.size()) + " moves."));
			}

			if (move_list.size() == 0)
				return false;


			const auto depth_limit_name = book_options_v2
				? (rootPos.side_to_move() == BLACK ? std::string("BookDepthBlackLimit") : std::string("BookDepthWhiteLimit"))
				: std::string("BookDepthLimit");
			auto depth_limit = int(options[depth_limit_name]);


			if (  (depth_limit != 0 && move_list[0].depth < depth_limit )
				)
			{
                updates.onUpdateString(std::string_view(
                    "info string " + depth_limit_name + " is lower than the depth of this node."));
				move_list.clear();
			}
			else {
				const auto eval_diff_name = book_options_v2
					? (rootPos.side_to_move() == BLACK ? std::string("BookEvalBlackDiff") : std::string("BookEvalWhiteDiff"))
					: std::string("BookEvalDiff");
				auto eval_diff = int(options[eval_diff_name]);
				auto value_limit1 = move_list[0].value - eval_diff;
				auto stm_string = (rootPos.side_to_move() == BLACK) ? "BookEvalBlackLimit" : "BookEvalWhiteLimit";
				auto value_limit2 = (int)options[stm_string];
				auto value_limit = std::max(value_limit1, value_limit2);

				auto n = move_list.size();

				auto it_end = std::remove_if(move_list.begin(), move_list.end(), [&](Book::BookMove & m) { return m.value < value_limit; });
				move_list.erase(it_end, move_list.end());


				if (n != move_list.size())
                    updates.onUpdateString(std::string_view(eval_diff_name + " = " + std::to_string(eval_diff)
						+ " , " + stm_string + " = " + std::to_string(value_limit2)
						+ " , " + std::to_string(n) + " moves to " + std::to_string(move_list.size()) + " moves."));
			}
		}
		if (move_list.size() == 0)
			return false;

		{

			auto bestBookMove = move_list[prng.rand(move_list.size())];

			const bool consider_book_move_count =
				options.book_options_v2() ? false : bool(options["ConsiderBookMoveCount"]);
			if (forceHit || consider_book_move_count)
			{


				u64 sum = 0;
				for (auto &move : move_list)
					sum += move.move_count;

				u64 sum_move_counts = 0;
				for (auto &move : move_list)
				{
					u64 move_count = (sum == 0) ? 1 : move.move_count; 
					sum_move_counts += move_count;
					if (sum_move_counts != 0 
						&& prng.rand(sum_move_counts) < move_count)
						bestBookMove = move;
				}
			}

			bestMove   = bestBookMove.move;
			ponderMove = bestBookMove.ponder;
			value      = Value(bestBookMove.value);

			if (!ponderMove.is_ok())
			{
				Move best = rootPos.to_move(bestMove);
				if (rootPos.pseudo_legal_s<true>(best) && rootPos.legal(best))
				{
					StateInfo si;
					rootPos.do_move(best,si);

					auto it = find_in_books(rootPos);
					if (it != nullptr && it->size())
						ponderMove = (*it)[0].move;

					rootPos.undo_move(best);
				}
			}

			return true;
		}

		return false;
	}

	ProbeResult BookMoveSelector::probe(Position& pos, const Search::UpdateContext& updates)
	{
        ProbeResult result;

		if (!options["USI_OwnBook"])
			return result;

		Move16 bestMove16, ponderMove16;
		Value value;
		if (probe_impl(pos, true, updates, bestMove16, ponderMove16, value))
		{
            Move bestMove = pos.to_move(bestMove16);


            result.bestmove   = bestMove;
            result.bestscore  = Score::from_internal_value(value);

            StateInfo si;
            pos.do_move(bestMove, si);
            Move ponderMove = pos.to_move(ponderMove16);
            if (pos.pseudo_legal_s<true>(ponderMove) && pos.legal(ponderMove))
                result.pondermove = ponderMove;
            pos.undo_move(bestMove);
        }

		return result;
	}


	void UnitTest(Test::UnitTester& tester, IEngine& engine)
	{
#if 0
		auto s1 = tester.section("Book");

		auto& limits = Search::Limits;

		Position pos;
		StateInfo si;

		auto hirate_init = [&] { pos.set_hirate(&si); };

		{
			auto s2 = tester.section("probe");
			MemoryBook book;

			limits.generate_all_legal_moves = true;

			tester.test("pawn's unpromoted move", true);
		}
#endif

		auto s1 = tester.section("BookTools");

		Position pos;
		std::string root_sfen = "startpos moves 7g7f 3c3d 6g6f 8b3b 8h7g 5a6b 2h8h 6b7b 8g8f 3d3e 8f8e 3e3f 3i2h 3f3g+ 2h3g 3a4b 4i3h P*3f 3g2h 4b3c 6i5h 3c4d 7i6h 1c1d P*3g 7a8b";
		std::deque<StateInfo> si;
		BookTools::feed_position_string(pos, root_sfen, si, [](Position&,Move){});

		std::string moves1 = "1g1f 2g2f 3g3f 4g4f 5g5f 6f6e 7f7e 8e8d 9g9f 1i1h 9i9h 2h3i 6h6g 6h7i 7g8f 7g9e 8h7h 8h8f 8h8g 8h9h 3h3i 3h4h 5h4h 5h6g 5i4h 5i4i 5i6i";
		std::string moves2 = std::string();
		for(auto m : MoveList<LEGAL_ALL>(pos))
			moves2 += (moves2.empty() ? "" : " ") + to_usi_string(Move(m));

		tester.test("feed_position_string" , moves1 == moves2);
	}
}


namespace BookTools
{
	void feed_position_string(Position& pos, const std::string& root_sfen, std::deque<StateInfo>& si,
		const std::function<void(Position&,Move)>& position_callback)
	{
		auto feed_next = [](Parser::LineScanner& iss)
		{
			return iss.get_text();
		};

		auto feed_sfen = [&feed_next](Parser::LineScanner& iss)
		{
			std::stringstream sfen;

			while(true)
			{
				std::string token;

				sfen << feed_next(iss);

				token = feed_next(iss);
				if (token != "w" && token != "b")
					break;
				sfen << " " << token;

				sfen << " " << feed_next(iss);

				sfen <<  " " << feed_next(iss);

				break;
			}
			return sfen.str();
		};

		si.clear();
		si.emplace_back(StateInfo()); 

		Parser::LineScanner iss(root_sfen);
		std::string token;
		do {
			token = feed_next(iss);
			if (token == "sfen")
			{
				auto sfen = feed_sfen(iss);
				pos.set(sfen, &si.back());
			}
			else if (token == "startpos")
			{
				pos.set_hirate(&si.back());
			}
		} while (token == "startpos" || token == "sfen" || token == "moves" );

		while (token != "")
		{
			Move move = USIEngine::to_move(pos, token);
			if (move == Move::none())
				break;

			if (!move.is_ok())
				break;

			position_callback(pos, move);

			si.emplace_back(StateInfo());
			pos.do_move(move, si.back());

			token = feed_next(iss);
		}

		position_callback(pos, Move::none());
	}

	std::vector<std::string> get_start_sfens()
	{
		std::vector<std::string> start_sfens = {
			 "sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1" ,
			 "sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1" ,
			 "sfen 1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
			 "sfen 4k4/9/9/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w 3p 1",
		};

		return start_sfens;
	}

	std::vector<std::string> get_next_sfens(std::string root_sfen)
	{
		Position pos;
		std::deque<StateInfo> si;
		feed_position_string(pos, root_sfen, si);
		StateInfo si2;
		std::vector<std::string> sfens;

		for (auto m : MoveList<LEGAL_ALL>(pos))
		{
			pos.do_move(m, si2);
			sfens.emplace_back("sfen " + pos.sfen());
			pos.undo_move(m);
		}

		return sfens;
	}

} 
} 
