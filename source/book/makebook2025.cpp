#include "../types.h"

#if defined (ENABLE_MAKEBOOK_CMD)


















#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <limits>
#include <random>
#include <utility> 
#include <new>

#include "book.h"
#include "../thread.h"
#include "../position.h"
#include "../movegen.h"
#include "../misc.h"

using namespace std;
namespace YaneuraOu {






template <typename T>
class SmallVector {
public:
	SmallVector() noexcept : data(nullptr), count(0), capacity(0) {

		
		
		reserve(4);
	}

	
	SmallVector(const SmallVector& other)
		: data(nullptr), count(0), capacity(0) {
		if (other.count > 0) {
			reserve(other.count);
			for (uint16_t i = 0; i < other.count; ++i) {
				new (&data[i]) T(other.data[i]);
			}
			count = other.count;
		}
	}

	
	SmallVector& operator=(const SmallVector& other) {
		if (this != &other) {
			clear();
			release();
			if (other.count > 0) {
				reserve(other.count);
				for (uint16_t i = 0; i < other.count; ++i) {
					new (&data[i]) T(other.data[i]);
				}
				count = other.count;
			}
		}
		return *this;
	}

	
	SmallVector(SmallVector&& other) noexcept
		: data(nullptr), count(0), capacity(0) {
		swap(other);
	}

	
	SmallVector& operator=(SmallVector&& other) noexcept {
		if (this != &other) {
			clear();
			release();
			swap(other);
		}
		return *this;
	}

	~SmallVector() noexcept {
		clear();
		release();
	}

	void push_back(const T& value) {
		emplace_back(value);
	}

	template <typename... Args>
	void emplace_back(Args&&... args) {
		if (count == capacity) {
			increase_capacity();
		}
		new (data + count) T(std::forward<Args>(args)...);
		++count;
	}

	void erase(T* position) {
		position->~T();
		for (T* it = position; it != data + count - 1; ++it) {
			new (it) T(std::move(*(it + 1)));
			(it + 1)->~T();
		}
		--count;
	}

	T& operator[](size_t idx) {
		return data[idx];
	}

	const T& operator[](size_t idx) const {
		return data[idx];
	}

	size_t size() const noexcept {
		return count;
	}

	T* begin() noexcept { return data; }
	T* end() noexcept { return data + count; }
	const T* begin() const noexcept { return data; }
	const T* end() const noexcept { return data + count; }

	void clear() noexcept {
		for (uint16_t i = 0; i < count; ++i) {
			data[i].~T();
		}
		count = 0;
	}

	void swap(SmallVector& other) noexcept {
		using std::swap;
		swap(data, other.data);
		swap(count, other.count);
		swap(capacity, other.capacity);
	}

private:
	void increase_capacity() {
		size_t new_capacity = capacity == 0 ? 1 : capacity * 2;
		if (new_capacity > UINT16_MAX) {
			std::cout << "Error! : SmallVector exceeds maximum capacity" << std::endl;
			Tools::exit();
		}
		reserve(static_cast<uint16_t>(new_capacity));
	}

	void reserve(uint16_t new_capacity) {
		T* new_data = reinterpret_cast<T*>(new char[new_capacity * sizeof(T)]);
		for (uint16_t i = 0; i < count; ++i) {
			new (&new_data[i]) T(std::move(data[i]));
			data[i].~T();
		}
		release();
		data = new_data;
		capacity = new_capacity;
	}

	void release() noexcept {
		delete[] reinterpret_cast<char*>(data);
		data = nullptr;
		capacity = 0;
	}

	T* data;
	uint16_t count;
	uint16_t capacity;
};



template <typename T>
void swap(SmallVector<T>& a, SmallVector<T>& b) noexcept {
    a.swap(b);
}

namespace MakeBook2025
{
	
	string SFEN_TEMP_FILENAME = "sfen_tmp.txt";

	
	
	typedef u32 BookNodeIndex;
	
	const BookNodeIndex BookNodeIndexNull = numeric_limits<BookNodeIndex>::max();

	
	const int BOOK_VALUE_INF  = numeric_limits<s16>::max();

	
	
	
	const int BOOK_VALUE_NONE = numeric_limits<s16>::min();

	
	const int BOOK_VALUE_MAX  =  VALUE_MATE;
	const int BOOK_VALUE_MIN  = -VALUE_MATE;

	
	const u16 BOOK_DEPTH_MAX = 9999;
	
	
	
	
	const u16 BOOK_DEPTH_PERPUTUAL_CHECKED = BOOK_DEPTH_MAX - 1;
	const u16 BOOK_DEPTH_PERPUTUAL_CHECK   = BOOK_DEPTH_MAX - 2;

	
	
	const u16 BOOK_MAX_PLY = 256;

	static constexpr std::array<char, 16> YBB_MAGIC = {
		'Y', 'A', 'N', 'E', '-', 'B', 'I', 'N',
		'B', 'O', 'O', 'K', '-', 'V', '1', '\0',
	};
	static constexpr u64 YBB_HEADER_SIZE = 32;
	static constexpr u64 YBB_INDEX_RECORD_SIZE = 44;
	static constexpr u64 YBB_FLAG_MOVE_DEPTH = 1;
	static constexpr u64 YBB_KNOWN_FLAGS = YBB_FLAG_MOVE_DEPTH;

	struct YbbIndexEntry
	{
		PackedSfen packed_sfen{};
		u64 moves_offset = 0;
		u16 ply = 0;
		u16 move_count = 0;
	};

	static bool ends_with(const string& text, const string& suffix)
	{
		return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
	}

	static bool is_ybb_book(const string& filename)
	{
		return ends_with(filename, ".ybb");
	}

	static bool is_db_book(const string& filename)
	{
		return ends_with(filename, ".db");
	}

	static string ybb_book_name_from_db_name(const string& db_filename)
	{
		if (!ends_with(db_filename, ".db"))
			return string();
		string ybb_filename = db_filename;
		ybb_filename.resize(ybb_filename.size() - string(".db").size());
		ybb_filename += ".ybb";
		return ybb_filename;
	}

	static string resolve_book_filename_with_ybb_fallback(const string& filename)
	{
		if (Path::Exists(filename))
			return filename;

		const auto ybb_filename = ybb_book_name_from_db_name(filename);
		if (!ybb_filename.empty() && Path::Exists(ybb_filename))
			return ybb_filename;

		return filename;
	}

	static bool read_u16_le(istream& is, u16& value)
	{
		array<unsigned char, 2> bytes{};
		is.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
		if (!is)
			return false;
		value = u16(bytes[0] | (bytes[1] << 8));
		return true;
	}

	static bool read_u64_le(istream& is, u64& value)
	{
		array<unsigned char, 8> bytes{};
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

	static bool write_u16_le(ostream& os, u16 value)
	{
		array<unsigned char, 2> bytes{
			static_cast<unsigned char>(value & 0xff),
			static_cast<unsigned char>((value >> 8) & 0xff),
		};
		os.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		return bool(os);
	}

	static bool write_u64_le(ostream& os, u64 value)
	{
		array<unsigned char, 8> bytes{};
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			bytes[i] = static_cast<unsigned char>(value & 0xff);
			value >>= 8;
		}
		os.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		return bool(os);
	}

	static bool read_ybb_header(istream& is, u64& record_count, u64& flags)
	{
		array<char, 16> magic{};
		is.read(magic.data(), magic.size());
		if (!is || magic != YBB_MAGIC)
			return false;
		if (!read_u64_le(is, record_count))
			return false;
		if (!read_u64_le(is, flags))
			return false;
		return (flags & ~YBB_KNOWN_FLAGS) == 0;
	}

	static bool ybb_index_size(u64 record_count, u64& index_size)
	{
		if (record_count > (std::numeric_limits<u64>::max() - YBB_HEADER_SIZE) / YBB_INDEX_RECORD_SIZE)
			return false;
		index_size = YBB_HEADER_SIZE + record_count * YBB_INDEX_RECORD_SIZE;
		return true;
	}

	static bool read_ybb_index_entry(istream& is, YbbIndexEntry& entry)
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

	static bool write_ybb_header(ostream& os, u64 record_count, u64 flags)
	{
		os.write(YBB_MAGIC.data(), YBB_MAGIC.size());
		return bool(os) && write_u64_le(os, record_count) && write_u64_le(os, flags);
	}

	static bool write_ybb_index_entry(ostream& os, const YbbIndexEntry& entry)
	{
		os.write(reinterpret_cast<const char*>(entry.packed_sfen.data), 32);
		return bool(os)
			&& write_u64_le(os, entry.moves_offset)
			&& write_u16_le(os, entry.ply)
			&& write_u16_le(os, entry.move_count);
	}

	
	struct ValueDepth
	{
		ValueDepth()
			: value(BOOK_VALUE_NONE), depth(0) {}

		ValueDepth(s16 value, u16 depth)
			: value(value) , depth(depth) {}

		
		bool operator==(const ValueDepth& v) const { return value==v.value && depth==v.depth;}
		bool operator!=(const ValueDepth& v) const { return !(*this==v); }

		
		bool operator > (const ValueDepth& v) const
		{
			
			if (this->value != v.value)
				return this->value > v.value;

			
			if (this->depth == BOOK_DEPTH_PERPUTUAL_CHECK)
				return false;
			if (v.depth == BOOK_DEPTH_PERPUTUAL_CHECK)
				return true;

			
			

			if (this->value >= 0)
				return this->depth < v.depth;
			else
				return this->depth > v.depth;
		}

		s16 value;
		
		u16 depth;
	};

	std::ostream& operator<<(std::ostream& os, ValueDepth vd)
	{
		os << "( " << vd.value << ", " << vd.depth << ")";
		return os;
	}

	
	
	
	struct BookMove
	{
		
		BookMove(Move16 move, s16 value, u16 depth) :
			move(move), vd(ValueDepth(value, depth)), leaf(true) {
		}

		
		BookMove(Move16 move, ValueDepth vd):
			move(move), vd(vd), leaf(true) { }

		
		BookMove(Move16 move, BookNodeIndex next) :
			move(move), next(next), leaf(false) {
		}

		
		Move16 move;

		
		bool leaf;

		union
		{
			
			
			ValueDepth vd;

			
			
			BookNodeIndex next;
		};

		
	};

	
	
	struct BookNode
	{
		
		
		
		SmallVector<BookMove> moves;

		
		
		
		
		
		
		
		ValueDepth vd;

		
		
		u8 color;

		
		
		
		bool const_node;

		
		
		bool checked;

		
		
		bool check_loop;

		

		
	};

	
	class PetaShock
	{
	public:
		
		void make_book(istringstream& is, string book_dir)
		{
			
			if (initialize(is, book_dir).is_not_ok())
				return;

			
			if (read_book().is_not_ok())
				return;

			
			if (convergence_check().is_not_ok())
				return;

			
			remove_const_nodes();

			
			extract_check_loop();

			
			init_cycle_nodes();

			
			propagate_all_nodes();

			
			if (write_peta_shock_book(writebook_path, book_nodes).is_not_ok())
				return;

			
			output_result();

			if (!fast)
				std::remove(sfen_temp_path.c_str());
		}

	protected:

		

		
		Tools::Result initialize(istringstream& is, string book_dir)
		{
			

			
			
			if (HASH_KEY_BITS < 128)
			{
				std::cout << "WARNING! : HASH_KEY_BITS = " << HASH_KEY_BITS << " is too short." << endl;
				std::cout << "    Rebuild with a set HASH_KEY_BITS == 128 or 256." << endl;
			}

			is >> readbook_path >> writebook_path;
			if (readbook_path.empty() || writebook_path.empty())
			{
				cout << "Error! : makebook peta_shock requires readbook and writebook paths." << endl;
				return Tools::ResultCode::FileMismatch;
			}

			
			string token;
			shrink = fast = false;
			while (is >> token)
			{
				if (token == "shrink")
					shrink = true;

				else if (token == "fast")
					fast = true;
			}

			string BOOK_DIR = book_dir;

			readbook_path  = Path::Combine(BOOK_DIR, readbook_path);
			writebook_path = Path::Combine(BOOK_DIR, writebook_path);
			sfen_temp_path = writebook_path + "." + SFEN_TEMP_FILENAME;

			const auto actual_readbook_path = resolve_book_filename_with_ybb_fallback(readbook_path);
			if (actual_readbook_path != readbook_path)
			{
				cout << "readbook fallback   : " << readbook_path << " -> " << actual_readbook_path << endl;
				readbook_path = actual_readbook_path;
			}

			const bool read_is_db = is_db_book(readbook_path);
			const bool write_is_db = is_db_book(writebook_path);
			const bool read_is_ybb = is_ybb_book(readbook_path);
			const bool write_is_ybb = is_ybb_book(writebook_path);
			if (!((read_is_db && write_is_db) || (read_is_ybb && write_is_ybb)))
			{
				cout << "Error! : makebook peta_shock supports only .db -> .db or .ybb -> .ybb." << endl;
				cout << "        readbook_path  : " << readbook_path << endl;
				cout << "        writebook_path : " << writebook_path << endl;
				return Tools::ResultCode::FileMismatch;
			}

			cout << "[ PetaShock makebook CONFIGURATION ]" << endl;

			cout << "readbook_path      : " << readbook_path << endl;
			cout << "writebook_path     : " << writebook_path << endl;

			cout << "shrink             : " << shrink << endl;
			cout << "fast               : " << fast << endl;

			

			drawValueTable[REPETITION_DRAW][BLACK] = 0;
			drawValueTable[REPETITION_DRAW][WHITE] = 0;

			

			converged_moves    = 0;
			const_nodes        = 0;
			in_check_counter   = 0;
			check_loop_counter = 0;

			

			original_sfens.clear();
			check_loop_nodes.clear();

			return Tools::Result::Ok();
		}

		
		Tools::Result read_book()
		{
			
			Tools::ProgressBar progress;

			cout << "Read a book DB      : " << endl;

			
			

			if (is_ybb_book(readbook_path))
			{
				ifstream index_reader(readbook_path, ios::binary);
				if (!index_reader)
				{
					sync_cout << "info string Error! : can't read file : " + readbook_path << sync_endl;
					return Tools::ResultCode::FileNotFound;
				}
				ifstream moves_reader(readbook_path, ios::binary);
				if (!moves_reader)
				{
					sync_cout << "info string Error! : can't read file : " + readbook_path << sync_endl;
					return Tools::ResultCode::FileNotFound;
				}

				u64 noe = 0;
				u64 ybb_flags = 0;
				if (!read_ybb_header(index_reader, noe, ybb_flags))
				{
					sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
					return Tools::ResultCode::FileReadError;
				}
				u64 moves_base = 0;
				if (!ybb_index_size(noe, moves_base))
				{
					sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
					return Tools::ResultCode::FileReadError;
				}

				cout << "Number Of Elements : " << noe << endl;
				book_nodes.reserve(size_t(noe));
				hashkey_to_index.reserve(size_t(noe));
				if (fast)
					original_sfens.reserve(size_t(noe));

				SystemIO::TextWriter sfen_writer;
				if (!fast)
				{
					if (sfen_writer.Open(sfen_temp_path).is_not_ok())
					{
						sync_cout << "info string Error! : can't write file : " + sfen_temp_path << sync_endl;
						return Tools::ResultCode::FileOpenError;
					}
				}

				progress.reset(noe == 0 ? 0 : noe - 1);
				Position pos;

				for (u64 i = 0; i < noe; ++i)
				{
					progress.check(i);

					YbbIndexEntry entry;
					if (!read_ybb_index_entry(index_reader, entry))
					{
						if (!fast)
							sfen_writer.Close();
						sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
						return Tools::ResultCode::FileReadError;
					}
					string sfen = Position::sfen_unpack(entry.packed_sfen);
					StringExtension::trim_number_inplace(sfen);
					sfen += " " + std::to_string(entry.ply);

					if (fast)
						original_sfens.push_back(sfen);
					else
						sfen_writer.WriteLine(sfen);

					StringExtension::trim_number_inplace(sfen);
					Color stm = (sfen.find('w') != std::string::npos) ? WHITE : BLACK;
					string white_sfen = stm == WHITE ? sfen : Position::sfen_to_flipped_sfen(sfen);

					StateInfo si;
					pos.set(white_sfen, &si);
					Key white_hash_key = pos.key();

					auto book_node_index = BookNodeIndex(book_nodes.size());
					hashkey_to_index[white_hash_key] = book_node_index;

					bool checked = pos.checkers();
					book_nodes.emplace_back(BookNode());
					auto& book_node = book_nodes.back();

					book_node.color       = stm;
					book_node.checked     = checked;
					book_node.check_loop  = checked;
					in_check_counter     += checked;

					moves_reader.clear();
					moves_reader.seekg(std::streamoff(moves_base + entry.moves_offset), ios::beg);
					if (!moves_reader)
					{
						if (!fast)
							sfen_writer.Close();
						sync_cout << "info string Error! : invalid ybb moves area : " << readbook_path << sync_endl;
						return Tools::ResultCode::FileReadError;
					}

					for (u16 move_index = 0; move_index < entry.move_count; ++move_index)
					{
						u16 move16_value = 0;
						u16 eval_value = 0;
						if (!read_u16_le(moves_reader, move16_value) || !read_u16_le(moves_reader, eval_value))
						{
							if (!fast)
								sfen_writer.Close();
							sync_cout << "info string Error! : invalid ybb moves area : " << readbook_path << sync_endl;
							return Tools::ResultCode::FileReadError;
						}
						if (ybb_flags & YBB_FLAG_MOVE_DEPTH)
						{
							
							u16 ignored_depth;
							if (!read_u16_le(moves_reader, ignored_depth))
							{
								if (!fast)
									sfen_writer.Close();
								sync_cout << "info string Error! : invalid ybb moves area : " << readbook_path << sync_endl;
								return Tools::ResultCode::FileReadError;
							}
						}
						Move16 move16 = Move16(move16_value);
						auto value = (s16)std::clamp((int)(s16)eval_value, BOOK_VALUE_MIN, BOOK_VALUE_MAX);

						if (book_node.color == WHITE)
							move16 = flip_move(move16);

						
						book_node.moves.emplace_back(BookMove(move16, value, 0));
					}
				}

				if (!fast)
					sfen_writer.Close();

				return Tools::Result::Ok();
			}

			SystemIO::TextReader reader;
			
			reader.SetTrim(true);
			reader.SkipEmptyLine(true);

			auto result = reader.Open(readbook_path);
			if (result.is_not_ok())
			{
				sync_cout << "info string Error! : can't read file : " + readbook_path << sync_endl;
				return Tools::ResultCode::FileNotFound;
			}

			
			std::string line;

			
			while (reader.ReadLine(line).is_ok())
			{
				auto line_no = reader.GetLineNumber();
				if (line_no == 1)
				{
					
					if (line.length() < 1 || line[0] != '#' || line != YaneuraOu::Book::BookDBHeader2016_100)
						cout << "WARNING : illegal YaneuraOu Book header 2016" << endl;
				}
				else if (line_no == 2)
				{
					
					
					
					if (line.length() >= 1 && line[0] == '#')
					{
						
						auto line2 = line.substr(2);
						auto splited = StringExtension::Split(line2, ",");
						for (auto command : splited)
						{
							auto splited2 = StringExtension::Split(command, ":");
							if (splited2.size() >= 1)
							{
								auto& token = splited2[0];
								if (token == "NOE" && splited2.size() == 2) 
								{
									size_t noe = StringExtension::to_int(string(splited2[1]), 0);
									cout << "Number Of Elements : " << noe << endl;

									
									book_nodes.reserve(noe);
									hashkey_to_index.reserve(noe);
									if (fast)
										original_sfens.reserve(noe);
								}
							}
						}
					}
				}
				else
					break;
			}
			reader.Close();
			reader.Open(readbook_path);

			progress.reset(reader.GetSize() - 1);

			
			SystemIO::TextWriter sfen_writer;
			if (!fast)
			{
				if (sfen_writer.Open(sfen_temp_path).is_not_ok())
				{
					sync_cout << "info string Error! : can't write file : " + sfen_temp_path << sync_endl;
					return Tools::ResultCode::FileOpenError;
				}
			}

			Position pos;

			while (reader.ReadLine(line).is_ok())
			{
				progress.check(reader.GetFilePos());

				
				if (line.length() >= 1 && line[0] == '#')
					continue;

				
				if (line.length() >= 2 && line.substr(0, 2) == "//")
					continue;

				
				if (line.length() >= 5 && line.substr(0, 5) == "sfen ")
				{
					string sfen = line.substr(5); 

					
					
					if (fast)
						original_sfens.push_back(sfen);
					else
						sfen_writer.WriteLine(sfen);

					
					StringExtension::trim_number_inplace(sfen);

					
					
					Color stm = (sfen.find('w') != std::string::npos) ? WHITE : BLACK;

					
					
					string white_sfen = stm == WHITE ? sfen : Position::sfen_to_flipped_sfen(sfen);

					
					StateInfo si;
					pos.set(white_sfen, &si);
					Key white_hash_key = pos.key();
					
					

					auto book_node_index = BookNodeIndex(book_nodes.size()); 
					hashkey_to_index[white_hash_key] = book_node_index;

					
					bool checked = pos.checkers();

					book_nodes.emplace_back(BookNode());
					auto& book_node = book_nodes.back();

					book_node.color        = stm; 
					book_node.checked      = checked;
					book_node.check_loop   = checked;
					in_check_counter      += checked;

					
					continue;
				}

				
				
				auto& book_node = book_nodes.back();

				

				Parser::LineScanner scanner(line);
				auto move_str   = scanner.get_text();
				auto ponder_str = scanner.get_text(); 
				auto value      = (s16)std::clamp((int)scanner.get_number(0), BOOK_VALUE_MIN, BOOK_VALUE_MAX);
				Move16 move16 = (move_str == "none" || move_str == "None" || move_str == "resign") ? Move16::none() : USIEngine::to_move16(move_str);
				

				
				
				if (book_node.color == WHITE)
					move16 = flip_move(move16);

				
				book_node.moves.emplace_back(BookMove(move16, value, 0));
				
			}
			if (!fast)
				sfen_writer.Close();

			return Tools::Result::Ok();
		}

		
		Tools::Result convergence_check()
		{
			cout << "Convergence Check   :" << endl;

			
			
			SystemIO::TextReader sfen_reader;
			if (!fast)
			{
				if (sfen_reader.Open(sfen_temp_path).is_not_ok())
				{
					sync_cout << "info string Error! : can't read file : " + sfen_temp_path << sync_endl;
					return Tools::ResultCode::FileOpenError;
				}
			}

			Tools::ProgressBar progress;
			progress.reset(book_nodes.size() - 1);

			Position pos;

			
			
			

			for (BookNodeIndex i = 0; i < BookNodeIndex(book_nodes.size()); ++i)
			{
				auto& book_node = book_nodes[i];

				StateInfo si, si2;
				string sfen;
				if (fast)
					sfen = original_sfens[i];
				else
				{
					if (sfen_reader.ReadLine(sfen).is_not_ok())
					{
						sync_cout << "info string Error! : can't read sfen temp file : " + sfen_temp_path << sync_endl;
						return Tools::ResultCode::FileReadError;
					}
				}

				
				
				if (book_node.color == WHITE)
					sfen = Position::sfen_to_flipped_sfen(sfen);

				pos.set(sfen, &si);
				ASSERT_LV3(pos.side_to_move() == BLACK);

				
				SmallVector<BookMove> book_moves;
				std::swap(book_node.moves, book_moves); 

				
				for (auto move : MoveList<LEGAL_ALL>(pos))
				{
					
					Key next_hash = pos.key_after(move);

					auto it = hashkey_to_index.find(next_hash);
					if (it != hashkey_to_index.end())
					{
						

						BookNodeIndex next_book_node_index = it->second;
						Move16        move16               = move.to_move16();
						BookMove      book_move(move16, next_book_node_index);

						
						
						
						if (std::find_if(book_moves.begin(), book_moves.end(), [&](const auto& bm) { return bm.move == move16; }) == book_moves.end())
							converged_moves++;

						book_node.moves.emplace_back(book_move);
					}
				}

				
				book_node.const_node = book_node.moves.size() == 0;

				
				for (auto& book_move : book_moves)
				{
					Move16 move = book_move.move;
					if (move == Move16::none())
						continue;

					
					
					
					if (std::find_if(book_node.moves.begin(), book_node.moves.end(), [&](auto& book_move) { return book_move.move == move; }) == book_node.moves.end())
						
						book_node.moves.emplace_back(book_move);
				}

				progress.check(i);
			}
			if (!fast)
				sfen_reader.Close();

			
			return Tools::Result::Ok();
		}

		
		ValueDepth make_vd_for_parent(ValueDepth vd)
		{
			
			return ValueDepth(-vd.value, std::min(u16(vd.depth + 1), BOOK_DEPTH_MAX));
		}

		
		ValueDepth bestvd_for_parent(const BookNode& node)
		{
			ValueDepth best(-BOOK_VALUE_INF, BOOK_DEPTH_MAX);

			for (const auto& book_move : node.moves)
			{
				
				
				
				const auto& vd = book_move.leaf ? book_move.vd : book_nodes[book_move.next].vd;
				if (vd > best)
					best = vd;
			}

			return make_vd_for_parent(best);
		}

		
		
		
		u64 remove_const_nodes_once()
		{
			

			

			
			u64 node_count = 0;
			for (BookNodeIndex book_node_index = 0; book_node_index < BookNodeIndex(book_nodes.size()); ++book_node_index)
			{
				auto& node = book_nodes[book_node_index];

				
				if (node.const_node)
					continue;

				
				
				for (auto& move : node.moves)
					if (!move.leaf && !book_nodes[move.next].const_node)
						goto Next;

				

				
				node.vd = bestvd_for_parent(node);
				node.const_node = true;
				node_count ++;

			Next: ;
			}
			return node_count;
		}

		
		void remove_const_nodes()
		{
			for (auto& node : book_nodes)
				node.const_node = false;

			cout << "Retrograde Analysis : Step I   -> delete nodes with zero out-degree." << endl;

			Tools::ProgressBar progress;
			progress.reset(BOOK_MAX_PLY);

			for(int i = 0; i < BOOK_MAX_PLY; ++i)
			{
				u64 count = remove_const_nodes_once();
				const_nodes += count;

				if (count == 0)
					break;
					
					

				progress.check(i);
			}

			progress.check(BOOK_MAX_PLY);
		}

		
		void extract_check_loop()
		{
			

			cout << "Retrograde Analysis : Step II  -> Extract perpetual check repetition loops." << endl;

			
			
			
			
			
			

			Tools::ProgressBar progress;
			progress.reset(BOOK_MAX_PLY - 1);

			unordered_set<BookNodeIndex> check_loop_nodes_set;

			for(int i = 0; i < BOOK_MAX_PLY ; ++i)
			{
				
				u64 updated = 0;
				for (auto& node : book_nodes)
				{
					if (!node.check_loop)
						continue;

					
					for (auto& move : node.moves)
					{
						if (move.leaf)
							continue;

						auto& next_node = book_nodes[move.next];
						for (auto& move2 : next_node.moves)
						{
							if (move2.leaf)
								continue;

							auto& next_next_node = book_nodes[move2.next];
							if (next_next_node.check_loop)
								goto Next;
						}
					}
					
					
					node.check_loop = false;
					updated++;
					continue;

				Next:;
				}
				progress.check(i);
				if (updated == 0)
					break;
			}

			
			for (BookNodeIndex i = 0; i < BookNodeIndex(book_nodes.size()) ; ++i)
			{
				auto& node = book_nodes[i];
				if (node.check_loop)
					check_loop_nodes_set.insert(BookNodeIndex(i));
			}

			
			check_loop_counter = check_loop_nodes_set.size();

			
			
			
			

			std::vector<BookNodeIndex> nodes(check_loop_nodes_set.begin(), check_loop_nodes_set.end());
			for (auto index: nodes)
			{
				auto& node = book_nodes[index];

				
				for (auto& move : node.moves)
				{
					if (move.leaf)
						continue;

					auto next_node_index = move.next;
					auto& next_node = book_nodes[next_node_index];
					for (auto& move2 : next_node.moves)
					{
						if (move2.leaf)
							continue;

						auto& next_next_node = book_nodes[move2.next];
						if (next_next_node.check_loop)
						{
							
							next_node.check_loop = true;
							check_loop_nodes_set.insert(next_node_index);
						}
					}
				}
			}

			
			check_loop_nodes.reserve(check_loop_nodes_set.size());
			std::copy(check_loop_nodes_set.begin(), check_loop_nodes_set.end(), std::back_inserter(check_loop_nodes));

			progress.check(BOOK_MAX_PLY);
		}

		
		void init_cycle_nodes()
		{
			cout << "Retrograde Analysis : Step III -> Initialize the cycle nodes." << endl;

			Tools::ProgressBar progress;
			progress.reset(book_nodes.size() - 1);

			
			
			for (size_t i = 0 ; i < book_nodes.size() ; ++i)
			{
				auto& node = book_nodes[i];
				if (!node.const_node)
				{
					if (!node.check_loop)
						
						node.vd = ValueDepth(0, BOOK_DEPTH_MAX);
					else
						
						node.vd = ValueDepth(node.checked ? BOOK_VALUE_MIN : BOOK_VALUE_MAX, BOOK_DEPTH_MAX);
				}
				progress.check(i);
			}
		}

		
		void dump_node(BookNodeIndex index)
		{
			cout << "--- BookNodes[" << index << "] ---" << endl;
			const auto& node = book_nodes[index];

			cout << "const_node = " << node.const_node << ", check_loop = " << node.check_loop << endl;
			cout << "vd = " << node.vd << endl;

			for (auto& move : node.moves)
			{
				cout << "is leaf = " << move.leaf << ", move = " << to_usi_string(move.move) << " : ";
				if (move.leaf)
					cout << "vd = " << move.vd << endl;
				else
				{
					cout << "next index = " << move.next << ", book_nodes[next].vd = ";
					const auto& next_node = book_nodes[move.next];
					cout << next_node.vd << endl;
				}
			}
		}

		
		
		
		
		u64 propagate_all_nodes_once()
		{
			
			u64 nodes_count = 0;
			for (BookNodeIndex i = 0 ; i < BookNodeIndex(book_nodes.size()) ; ++i)
			{
				auto& node = book_nodes[i];

				
				
				if (node.const_node || node.check_loop)
					continue;

				auto best = bestvd_for_parent(node);

				
				
				if (best.depth > BOOK_MAX_PLY)
					best.depth = BOOK_DEPTH_MAX;

				
				nodes_count += node.vd != best;

				
				node.vd = best;
			}

			

			return nodes_count;
		}

		
		ValueDepth dfs_for_check_loop_node(BookNodeIndex node_index, unordered_set<BookNodeIndex>& trajectory)
		{
			auto& node = book_nodes[node_index];

			
			if (!node.check_loop)
				return node.vd;

			
			if (trajectory.count(node_index))
			{
				
				
				if (node.checked)
					return ValueDepth(-BOOK_VALUE_MAX, BOOK_DEPTH_PERPUTUAL_CHECK);
				else
					return ValueDepth(+BOOK_VALUE_MAX, BOOK_DEPTH_PERPUTUAL_CHECKED);
			}

			

			trajectory.insert(node_index);

			ValueDepth best(-BOOK_VALUE_INF, 0);

			for (const auto& move : node.moves)
			{
				
				
				
				const auto vd = move.leaf ? move.vd : dfs_for_check_loop_node(move.next, trajectory);
				if (vd > best)
					best = vd;
			}

			trajectory.erase(node_index);

			return make_vd_for_parent(best);
		}

		
		void dfs_for_check_loop_nodes()
		{
			
			

			unordered_set<BookNodeIndex> trajectory; 
			for (auto node_index : check_loop_nodes)
			{
				auto& node = book_nodes[node_index];
				node.vd = dfs_for_check_loop_node(node_index, trajectory);
			}
		}


		
		
		void propagate_all_nodes()
		{
			cout << "Retrograde Analysis : Step IV  -> Propagate the eval to the parents of all nodes." << endl;

			Tools::ProgressBar progress;
			progress.reset(BOOK_MAX_PLY + 100);

			
			for (int ply = 0; ply < BOOK_MAX_PLY + 100; ++ply)
			{
				
				u64 updating_nodes = propagate_all_nodes_once();

				
				dfs_for_check_loop_nodes();

				progress.check(ply);

				
				if (updating_nodes == 0)
					break;
			}
			progress.check(BOOK_MAX_PLY + 100);
		}

		SmallVector<BookMove> make_output_moves(BookNode& book_node)
		{
			
			SmallVector<BookMove> moves;
			for (auto& move : book_node.moves)
				if (move.leaf)
					moves.emplace_back(move);
				else
				{
					auto& next_node = book_nodes[move.next];
					auto vd = next_node.vd;
					
					
					
					if (next_node.check_loop && next_node.checked)
						vd.depth = BOOK_DEPTH_PERPUTUAL_CHECK;
					moves.emplace_back(BookMove(move.move, vd));
				}

			
			std::sort(moves.begin(), moves.end(),
				[](const BookMove& x, const BookMove& y) {
					return x.vd > y.vd;
				});

			SmallVector<BookMove> output_moves;
			if (moves.size() == 0)
				return output_moves;

			for(size_t i = 0 ; i < moves.size() ; ++i)
			{
				auto move = moves[i];

				
				if (shrink && moves[0].vd.value != move.vd.value)
					continue;

				
				
				

				
				
				
				
				
				
				

				if (i > 0 && moves[0].vd.value == move.vd.value && moves[0].vd.depth != move.vd.depth)
					move.vd.value--;

				output_moves.emplace_back(move);
			}

			return output_moves;
		}

		
		
		Tools::Result write_peta_shock_book(std::string writebook_path, std::vector<BookNode>& book_nodes)
		{
			

			
			
			
			HashKey2Index().swap(this->hashkey_to_index);

			if (is_ybb_book(writebook_path))
				return write_peta_shock_ybb_book(writebook_path, book_nodes);

			return write_peta_shock_db_book(writebook_path, book_nodes);
		}

		Tools::Result write_peta_shock_db_book(std::string writebook_path, std::vector<BookNode>& book_nodes)
		{
			
			Tools::ProgressBar progress;

			

			cout << "Write to a book DB  : " << endl;

			SystemIO::TextWriter writer;
			if (writer.Open(writebook_path).is_not_ok())
			{
				cout << "Error! : open file error , path = " << writebook_path << endl;
				return Tools::ResultCode::FileOpenError;
			}

			progress.reset(book_nodes.size() == 0 ? 0 : book_nodes.size() - 1);

			
			writer.WriteLine(YaneuraOu::Book::BookDBHeader2016_100);
			writer.WriteLine("# NOE:" + to_string(book_nodes.size()));

			SystemIO::TextReader sfen_reader;
			if (!fast)
			{
				if (sfen_reader.Open(sfen_temp_path).is_not_ok())
				{
					sync_cout << "info string Error! : can't read file : " + sfen_temp_path << sync_endl;
					return Tools::ResultCode::FileOpenError;
				}
			}

			for(BookNodeIndex i = 0 ; i < BookNodeIndex(book_nodes.size()) ; ++i)
			{
				auto& book_node = book_nodes[i];
				string sfen;
				if (fast)
					sfen = original_sfens[i];
				else
				{
					if (sfen_reader.ReadLine(sfen).is_not_ok())
					{
						sync_cout << "info string Error! : can't read sfen temp file : " + sfen_temp_path << sync_endl;
						return Tools::ResultCode::FileReadError;
					}
				}

				if (writer.WriteLine("sfen " + sfen).is_not_ok())
					return Tools::ResultCode::FileWriteError;
				writer.Flush(); 

				const auto moves = make_output_moves(book_node);
				for(const auto& move : moves)
				{
					
					
					Move16 m16 = (book_node.color == WHITE) ? flip_move(move.move) : move.move;
					if (writer.WriteLine(to_usi_string(m16) + " none " + to_string(move.vd.value) + " " + to_string(move.vd.depth)).is_not_ok())
						return Tools::ResultCode::FileWriteError;
				}

				progress.check(i);
			}
			if (!fast)
				sfen_reader.Close();

			cout << "write " + writebook_path << endl;
			return Tools::Result::Ok();
		}

		Tools::Result write_peta_shock_ybb_book(std::string writebook_path, std::vector<BookNode>& book_nodes)
		{
			cout << "Write to a book DB  : " << endl;

			ifstream index_reader(readbook_path, ios::binary);
			if (!index_reader)
			{
				sync_cout << "info string Error! : can't read file : " + readbook_path << sync_endl;
				return Tools::ResultCode::FileNotFound;
			}

			u64 input_record_count = 0;
			u64 input_flags = 0;
			if (!read_ybb_header(index_reader, input_record_count, input_flags))
			{
				sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
				return Tools::ResultCode::FileReadError;
			}

			u64 input_moves_base = 0;
			if (!ybb_index_size(input_record_count, input_moves_base) || input_record_count != u64(book_nodes.size()))
			{
				sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
				return Tools::ResultCode::FileReadError;
			}

			u64 index_size = 0;
			if (!ybb_index_size(u64(book_nodes.size()), index_size))
				return Tools::ResultCode::FileWriteError;

			ofstream writer(writebook_path, ios::binary | ios::out | ios::trunc);
			if (!writer)
			{
				cout << "Error! : open file error , path = " << writebook_path << endl;
				return Tools::ResultCode::FileOpenError;
			}

			constexpr u64 output_flags = YBB_FLAG_MOVE_DEPTH;
			if (!write_ybb_header(writer, u64(book_nodes.size()), output_flags))
				return Tools::ResultCode::FileWriteError;

			u64 index_offset = YBB_HEADER_SIZE;
			u64 move_offset = 0;

			Tools::ProgressBar progress;
			progress.reset(book_nodes.size() == 0 ? 0 : book_nodes.size() - 1);

			for(BookNodeIndex i = 0 ; i < BookNodeIndex(book_nodes.size()) ; ++i)
			{
				YbbIndexEntry entry;
				if (!read_ybb_index_entry(index_reader, entry))
				{
					sync_cout << "info string Error! : invalid ybb file : " << readbook_path << sync_endl;
					return Tools::ResultCode::FileReadError;
				}

				auto& book_node = book_nodes[i];
				const auto moves = make_output_moves(book_node);
				if (moves.size() > numeric_limits<u16>::max())
				{
					sync_cout << "info string Error! : too many moves in ybb record : " << writebook_path << sync_endl;
					return Tools::ResultCode::FileWriteError;
				}

				entry.moves_offset = move_offset;
				entry.move_count = u16(moves.size());

				writer.seekp(std::streamoff(index_offset), ios::beg);
				if (!writer || !write_ybb_index_entry(writer, entry))
					return Tools::ResultCode::FileWriteError;

				writer.seekp(std::streamoff(index_size + move_offset), ios::beg);
				if (!writer)
					return Tools::ResultCode::FileWriteError;

				for(const auto& move : moves)
				{
					Move16 m16 = (book_node.color == WHITE) ? flip_move(move.move) : move.move;
					if (!write_u16_le(writer, m16.raw())
						|| !write_u16_le(writer, u16(move.vd.value))
						|| !write_u16_le(writer, move.vd.depth))
						return Tools::ResultCode::FileWriteError;
				}

				index_offset += YBB_INDEX_RECORD_SIZE;
				move_offset += u64(moves.size()) * 6;
				progress.check(i);
			}

			cout << "write " + writebook_path << endl;
			return Tools::Result::Ok();
		}

		
		void output_result()
		{
			cout << "[ PetaShock Result ]" << endl;

			
			cout << "write book nodes   : " << book_nodes.size() << endl;

			
			cout << "const_nodes        : " << const_nodes << endl;
			cout << "non-const_nodes    : " << book_nodes.size() - const_nodes << endl;

			
			cout << "in-check nodes     : " << in_check_counter << endl;

			
			cout << "check-loops nodes1 : " << check_loop_counter << endl;

			
			cout << "check-loops nodes2 : " << check_loop_nodes.size() << endl;

			
			cout << "converged_moves    : " << converged_moves << endl;

			cout << endl << "Making a peta-shock book has been completed." << endl;
		}

	private:

		

		
		vector<BookNode> book_nodes;

		
		
		
		
		using HashKey2Index = unordered_map<Key, BookNodeIndex>;
		HashKey2Index hashkey_to_index;

		
		vector<string> original_sfens;

		
		vector<BookNodeIndex> check_loop_nodes;

		

		
		string readbook_path;

		
		string writebook_path;

		
		string sfen_temp_path;

		

		
		u64 converged_moves;

		
		u64 const_nodes;

		
		u64 in_check_counter;

		
		u64 check_loop_counter;


		

		
		bool shrink;

		
		bool fast;

	};

} 

namespace Book
{
	
	
	int makebook2025(std::istringstream& is, const std::string& token, const OptionsMap& options)
    {
		if (token == "peta_shock") {

			
			
			
			
			
			
			
			
			
			
			MakeBook2025::PetaShock ps;
			auto book_dir = options["BookDir"];
			ps.make_book(is, book_dir);
			return 1;
		}

		return 0;
	}

} 
} 

#endif 
