#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED


#include <exception>  

#include <functional>
#include <optional>
#include <cstring>
#include <memory>

#include <string_view>
#include <type_traits>


#include <fstream>
#include <mutex>
#include <atomic>
#include <sstream>
#include <queue>
#include <unordered_set>
#include <condition_variable>

#include "types.h"

namespace YaneuraOu {

class OptionsMap;






std::string engine_version_info();













std::string engine_info(const std::string& engine_name,
						const std::string& engine_author,
                        const std::string& engine_version,
                        const std::string& eval_name);


std::string compiler_info();


std::string config_info();









void prefetch(const void* addr);






void start_logger(const std::string& fname);







void dbg_hit_on(bool b);


void dbg_hit_on(bool c , bool b);


void dbg_mean_of(int v);



void dbg_print();





#if STOCKFISH




#endif



static TimePoint now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
    
}

#if STOCKFISH

#else

struct ElapsedTimer {
    ElapsedTimer();

    
    ElapsedTimer(TimePoint s);

    
    void reset();
    
    void reset(TimePoint s);

    
    TimePoint elapsed() const;

   private:
    
    TimePoint startTime;
};

#endif













enum SyncCout {
	IO_LOCK,
	IO_UNLOCK
};
std::ostream& operator<<(std::ostream&, SyncCout);

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK


void sync_cout_start();
void sync_cout_end();






template<typename T, std::size_t MaxSize>
class ValueList {

public:
   	
    std::size_t size() const { return size_; }

	
    int         ssize() const { return int(size_); }

	void        push_back(const T& value) {
        assert(size_ < MaxSize);
        values_[size_++] = value;
    }
	const T* begin() const { return values_; }
	const T* end() const { return values_ + size_; }

	const T& operator[](int index) const { return values_[index]; }
	

	
	
    T* make_space(size_t count) {
        T* result = &values_[size_];
        size_ += count;
        assert(size_ <= MaxSize);
        return result;
    }

	
	T* begin() { return values_; }
	T* end()   { return values_ + size_; }

private:
	T           values_[MaxSize];
	std::size_t size_ = 0;
};





template<typename T, std::size_t Size, std::size_t... Sizes>
class MultiArray;

namespace Detail {

	template<typename T, std::size_t Size, std::size_t... Sizes>
	struct MultiArrayHelper {
		using ChildType = MultiArray<T, Sizes...>;
	};

	template<typename T, std::size_t Size>
	struct MultiArrayHelper<T, Size> {
		using ChildType = T;
	};

	template<typename To, typename From>
	constexpr bool is_strictly_assignable_v =
		std::is_assignable_v<To&, From> && (std::is_same_v<To, From> || !std::is_convertible_v<From, To>);

}







template<typename T, std::size_t Size, std::size_t... Sizes>
class MultiArray {
	using ChildType = typename Detail::MultiArrayHelper<T, Size, Sizes...>::ChildType;
	using ArrayType = std::array<ChildType, Size>;
	ArrayType data_;

public:
	using value_type = typename ArrayType::value_type;
	using size_type = typename ArrayType::size_type;
	using difference_type = typename ArrayType::difference_type;
	using reference = typename ArrayType::reference;
	using const_reference = typename ArrayType::const_reference;
	using pointer = typename ArrayType::pointer;
	using const_pointer = typename ArrayType::const_pointer;
	using iterator = typename ArrayType::iterator;
	using const_iterator = typename ArrayType::const_iterator;
	using reverse_iterator = typename ArrayType::reverse_iterator;
	using const_reverse_iterator = typename ArrayType::const_reverse_iterator;

	constexpr auto& at(size_type index) noexcept { return data_.at(index); }
	constexpr const auto& at(size_type index) const noexcept { return data_.at(index); }

	constexpr auto& operator[](size_type index) noexcept { return data_[index]; }
	constexpr const auto& operator[](size_type index) const noexcept { return data_[index]; }

	constexpr auto& front() noexcept { return data_.front(); }
	constexpr const auto& front() const noexcept { return data_.front(); }
	constexpr auto& back() noexcept { return data_.back(); }
	constexpr const auto& back() const noexcept { return data_.back(); }

	auto* data() { return data_.data(); }
	const auto* data() const { return data_.data(); }

	constexpr auto begin() noexcept { return data_.begin(); }
	constexpr auto end() noexcept { return data_.end(); }
	constexpr auto begin() const noexcept { return data_.begin(); }
	constexpr auto end() const noexcept { return data_.end(); }
	constexpr auto cbegin() const noexcept { return data_.cbegin(); }
	constexpr auto cend() const noexcept { return data_.cend(); }

	constexpr auto rbegin() noexcept { return data_.rbegin(); }
	constexpr auto rend() noexcept { return data_.rend(); }
	constexpr auto rbegin() const noexcept { return data_.rbegin(); }
	constexpr auto rend() const noexcept { return data_.rend(); }
	constexpr auto crbegin() const noexcept { return data_.crbegin(); }
	constexpr auto crend() const noexcept { return data_.crend(); }

	constexpr bool      empty() const noexcept { return data_.empty(); }
	constexpr size_type size() const noexcept { return data_.size(); }
	constexpr size_type max_size() const noexcept { return data_.max_size(); }

	template<typename U>
	void fill(const U& v) {
		static_assert(Detail::is_strictly_assignable_v<T, U>,
			"Cannot assign fill value to entry type");
		for (auto& ele : data_)
		{
			if constexpr (sizeof...(Sizes) == 0)
				ele = v;
			else
				ele.fill(v);
		}
	}

	constexpr void swap(MultiArray<T, Size, Sizes...>& other) noexcept { data_.swap(other.data_); }
};







template<typename T, std::size_t Rows, std::size_t Cols>
class FlatMultiArray2 {
	std::array<T, Rows * Cols> data_;

	template<typename P>
	struct Row {
		P p;
		constexpr auto& operator[](std::size_t j) const noexcept { return p[j]; }
	};

public:
	static constexpr std::size_t index(std::size_t i, std::size_t j) noexcept { return i * Cols + j; }

	constexpr auto operator[](std::size_t i) noexcept { return Row<T*>{data_.data() + i * Cols}; }
	constexpr auto operator[](std::size_t i) const noexcept { return Row<const T*>{data_.data() + i * Cols}; }

	constexpr T&       flat(std::size_t k) noexcept { return data_[k]; }
	constexpr const T& flat(std::size_t k) const noexcept { return data_[k]; }

	template<typename U>
	void fill(const U& v) {
		static_assert(Detail::is_strictly_assignable_v<T, U>, "Cannot assign fill value to entry type");
		for (auto& ele : data_)
			ele = v;
	}
};









struct PRNG
{
	PRNG(u64 seed) : s(seed) { ASSERT_LV1(seed); }

	
	PRNG() {
		
		
		

		
		s = (u64)(time(NULL)) + ((u64)(this) << 32)
		
		
		
			+ (u64)std::chrono::steady_clock::now().time_since_epoch().count();
	}

	
	template<typename T> T rand() { return T(rand64()); }

	
	u64 rand(u64 n) { return rand<u64>() % n; }

	
	u64 get_seed() const { return s;  }

private:
	u64 s;
	u64 rand64() {
		s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
		return s * 2685821657736338717LL;
	}
};


static std::ostream& operator<<(std::ostream& os, PRNG& prng)
{
	os << "PRNG::seed = " << std::hex << prng.get_seed() << std::dec;
	return os;
}





inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(IS_64BIT)
    __extension__ using uint128 = unsigned __int128;
    return (uint128(a) * uint128(b)) >> 64;
#else
	
    uint64_t aL = uint32_t(a), aH = a >> 32;
    uint64_t bL = uint32_t(b), bH = b >> 32;
    uint64_t c1 = (aL * bL) >> 32;
    uint64_t c2 = aH * bL + c1;
    uint64_t c3 = aL * bH + uint32_t(c2);
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}









uint64_t hash_bytes(const char*, size_t);





template<typename T>
inline std::size_t get_raw_data_hash(const T& value) {
    
    static_assert(std::has_unique_object_representations<T>());

    return static_cast<std::size_t>(
      hash_bytes(reinterpret_cast<const char*>(&value), sizeof(value)));
}

template<typename T>
inline void hash_combine(std::size_t& seed, const T& v) {
    std::size_t x;
    
    
    if constexpr (std::is_integral<T>())
        x = v;
    else
        x = std::hash<T>{}(v);
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}



inline std::uint64_t hash_string(const std::string& sv) { return hash_bytes(sv.data(), sv.size()); }





template<std::size_t Capacity>
class FixedString {
   public:

	
    FixedString() :
        length_(0) {
        data_[0] = '\0';
    }

	
	
    FixedString(const char* str) {
        size_t len = std::strlen(str);
        if (len > Capacity)
            std::terminate();
        std::memcpy(data_, str, len);
        length_        = len;
        data_[length_] = '\0';
    }

	
	
    FixedString(const std::string& str) {
        if (str.size() > Capacity)
            std::terminate();
        std::memcpy(data_, str.data(), str.size());
        length_        = str.size();
        data_[length_] = '\0';
    }

	
    std::size_t size() const { return length_; }

	
    std::size_t capacity() const { return Capacity; }

	
    const char* c_str() const { return data_; }
	const char* data() const { return data_; }

	
    char& operator[](std::size_t i) { return data_[i]; }
    const char& operator[](std::size_t i) const { return data_[i]; }

	
	
    FixedString& operator+=(const char* str) {
        size_t len = std::strlen(str);
        if (length_ + len > Capacity)
            std::terminate();
        std::memcpy(data_ + length_, str, len);
        length_ += len;
        data_[length_] = '\0';
        return *this;
    }

	
    FixedString& operator+=(const FixedString& other) { return (*this += other.c_str()); }

	
    operator std::string() const { return std::string(data_, length_); }

	
    operator std::string_view() const { return std::string_view(data_, length_); }

	
    template<typename T>
    bool operator==(const T& other) const noexcept {
        return (std::string_view) (*this) == other;
    }

	
    template<typename T>
    bool operator!=(const T& other) const noexcept {
        return (std::string_view) (*this) != other;
    }

	
    void clear() {
        length_  = 0;
        data_[0] = '\0';
    }

   private:
	
    char        data_[Capacity + 1];  

	
    std::size_t length_;
};





struct CommandLine {
public:
	CommandLine() {}
	CommandLine(int _argc, char** _argv) :
		argc(_argc),
		argv(_argv) {}

	
	void set_arg(int _argc, char** _argv) { argc = _argc, argv = _argv; }

	
	
	
	
    static std::string get_binary_directory() { return g.get_binary_directory(g.argv[0]); }

	
	
	static std::string get_binary_directory(std::string argv0);

	
	static std::string get_working_directory();

	int    argc;
	char** argv;

	
	static CommandLine g;
};





namespace Utility {



template<typename T, typename Predicate>
void move_to_front(std::vector<T>& vec, Predicate pred) {
    auto it = std::find_if(vec.begin(), vec.end(), pred);

    if (it != vec.end())
    {
        std::rotate(vec.begin(), it, it + 1);
    }
}
}





#if defined(__GNUC__)
    #define sf_always_inline __attribute__((always_inline))
#elif defined(_MSC_VER)
	#define sf_always_inline __forceinline
#else
    
    #define sf_always_inline
#endif

#if defined(__GNUC__) && !defined(__clang__)
    #if __GNUC__ >= 13
        #define sf_assume(cond) __attribute__((assume(cond)))
    #else
        #define sf_assume(cond) \
            do \
            { \
                if (!(cond)) \
                    __builtin_unreachable(); \
            } while (0)
    #endif
#else
    
    #define sf_assume(cond)
#endif





class ThreadPool;

namespace Tools
{
	
	
	
	
	
	
	void memclear(YaneuraOu::ThreadPool& threads, const char* name, void* table, size_t size);

	
	
	template <typename T >
	void insertion_sort(T* arr, int left, int right)
	{
		for (int i = left + 1; i < right; i++)
		{
			auto key = arr[i];
			int j = i - 1;

			
			while (j >= left && (arr[j] > key))
			{
				arr[j + 1] = arr[j];
				j = j - 1;
			}
			arr[j + 1] = key;
		}
	}

	
	
	void exit();

	
	void sleep(u64 ms);

	
	std::string now_string();

	
	
	
	
	bool getline(std::ifstream& fs, std::string& s);

	
	
	std::wstring MultiByteToWideChar(const std::string& s);

	
	
	struct __FINALLY__ {
		__FINALLY__(std::function<void()> fn_) : fn(fn_) {}
		~__FINALLY__() { fn(); }
	private:
		std::function<void()> fn;
	};

	
	
	

	
	class ProgressBar
	{
	public:
		ProgressBar(){}

		
		ProgressBar(u64 size_);

		
		void reset(u64 size_);

		
		
		void check(u64 current);

		
		
		
		static void enable(bool b) { enable_ = b; }

	private:
		
		u64 size;

		
		size_t dots;

		static bool enable_;
	};

	
	
	

	
	enum struct ResultCode
	{
		
		Ok,

		
		Eof,

		
		SomeError,

		
		MemoryAllocationError,

		
		FileNotFound,

		
		FileOpenError,

		
		FileReadError,

		
		FileWriteError,

		
		FileCloseError,

		
		FileMismatch,

		
		CreateFolderError,

		
		NotImplementedError,
	};

	
	std::string to_string(ResultCode);

	
	
	struct Result
	{
		constexpr Result(ResultCode code_) : code(code_) {}

		
		ResultCode code;

		
		bool is_ok() const { return code == ResultCode::Ok; }

		
		bool is_not_ok() const { return code != ResultCode::Ok; }

		
		bool is_eof() const { return code == ResultCode::Eof; }

		
		std::string to_string() const { return Tools::to_string(code); }

		
		static constexpr Result Ok() { return Result(ResultCode::Ok); }
	};
}



#define SCOPE_EXIT(STATEMENT) Tools::__FINALLY__ __clean_up_object__([&]{ STATEMENT });






namespace SystemIO
{
	
	
	
	
	
	Tools::Result ReadAllLines(const std::string& filename, std::vector<std::string>& lines, bool trim = false);

	
	
	Tools::Result WriteAllLines(const std::string& filename, std::vector<std::string>& lines);


	
	
	
	
	
	
	
	
	
	

	Tools::Result ReadFileToMemory(const std::string& filename, std::function<void* (size_t)> callback_func);
	Tools::Result WriteMemoryToFile(const std::string& filename, void* ptr, size_t size);

	

	size_t ftell64(FILE* f);
	int fseek64(FILE* f, size_t offset, int origin);

	
	
	
	struct TextReader
	{
		TextReader();
		virtual ~TextReader();

		
		
		Tools::Result Open(const std::string& filename);

		
		void Close();

		
		

		
		
		
		
		
		
		Tools::Result ReadLine(std::string& line);

		
		
		
		void SkipEmptyLine(bool skip = true) { skipEmptyLine = skip; }

		
		
		
		
		void SetTrim(bool trim = true) { this->trim = trim; }

		
		
		size_t GetSize();

		
		
		size_t GetFilePos() { return ftell64(fp); }

		
		
		size_t GetLineNumber() const { return line_number; }

	private:
		
		void clear();

		
		void read_next_block();

		
		
		FILE* fp;

		
		int read_char();

		
		
		Tools::Result read_line_simple();

		
		std::vector<u8> buffer;

		
		std::vector<u8> line_buffer;

		
		size_t read_size;

		
		
		
		size_t cursor;

		
		
		bool is_eof;

		
		bool is_prev_cr;

		
		
		
		size_t line_number;

		
		bool trim;

		
		bool skipEmptyLine;
	};

	
	class TextWriter
	{
	public:
		
		static constexpr size_t buf_size = 4096;

		
		Tools::Result Open(const std::string& filename);

		
		Tools::Result Write(const std::string& str);

		
		Tools::Result WriteLine(const std::string& line);

		
		Tools::Result Write(const char* ptr, size_t size);

		
		
		Tools::Result Flush();

		Tools::Result Close();
		TextWriter() : buf(buf_size) { clear(); }
		virtual ~TextWriter() { Close(); }

	private:
		
		void clear() { fp = nullptr; write_cursor = 0; }

		FILE* fp = nullptr;

		
		std::vector<char> buf;

		
		size_t write_cursor;
	};

	
	class BinaryBase
	{
	public:
		
		Tools::Result Close();

		virtual ~BinaryBase() { Close(); }

	protected:
		FILE* fp = nullptr;
	};

	
	class BinaryReader : public BinaryBase
	{
	public:
		
		
		Tools::Result Open(const std::string& filename);

		
		
		size_t GetSize();

		
		
		
		
		
		
		Tools::Result Read(void* ptr , size_t size, size_t* size_of_read_bytes = nullptr);
	};

	
	class BinaryWriter : public BinaryBase
	{
	public:
		
		
		
		Tools::Result Open(const std::string& filename, bool append = false);

		
		
		Tools::Result Write(void* ptr, size_t size);
	};
};









std::optional<std::string> read_file_to_string(const std::string& filename);







namespace Path
{
	
	
	
	
	
	
	std::string Combine(const std::string& folder, const std::string& filename);

	
	std::string GetFileName(const std::string& path);

	
	std::string GetDirectoryName(const std::string& path);

	
	
	
	
	
	
	
	
	
	
	
	bool IsAbsolute(const std::string& path);

	
	bool Exists(const std::string& path);

};







namespace Directory
{
	
	
	
	std::vector<std::string> EnumerateFiles(const std::string& sourceDirectory, const std::string& extension);

	
	
	
	
	Tools::Result CreateFolder(const std::string& dir_name);

	
	std::string GetBinaryFolder();
}






struct AsyncPRNG
{
	
	template<typename T> T rand() {
		std::unique_lock<std::mutex> lk(mutex);
		return prng.rand<T>();
	}

	
	u64 rand(u64 n) {
		std::unique_lock<std::mutex> lk(mutex);
		return prng.rand(n);
	}

	
	u64 get_seed() const { return prng.get_seed(); }

protected:
	std::mutex mutex;
	PRNG prng;
};


static std::ostream& operator<<(std::ostream& os, AsyncPRNG& prng)
{
	os << "AsyncPRNG::seed = " << std::hex << prng.get_seed() << std::dec;
	return os;
}





namespace Parser
{
	
	struct LineScanner
	{
		
		LineScanner(std::string line_) : line(line_), pos(0) {}

		
		std::string peek_text();

		
		std::string get_text();

		
		
		std::string get_rest();

		
		
		
		s64 get_number(s64 defaultValue);
		double get_double(double defaultValue);

		
		
		
		
		
		bool eol() const { return token.empty() && raw_eol(); }

	private:
		
		bool raw_eol() const { return !(pos < line.length()); }

		
		std::string line;

		
		unsigned int pos;

		
		std::string token;
	};

	
	
	
	
	
	
	
	
	
	
	
	

	class ArgumentParser
	{
	public:
		typedef std::pair<std::string , std::function<void(std::istringstream&)>> ArgPair;

		
		template<typename T>
		void add_argument(const std::string& arg_name, T& v)
		{
			auto f = [&](std::istringstream& is) { is >> v; };
			a.emplace_back(ArgPair(arg_name,f));
		}

		
		void parse_args(std::istringstream& is)
		{
			std::string token;
			while (is >> token)
				for (auto p : a)
					
					if (p.first == token)
					{
						p.second(is);
						break;
					}
		}

		std::vector <ArgPair> a;
	};
}






namespace Math {
	
	
	double sigmoid(double x);

	
	
	double dsigmoid(double x);

	
	
	template<class T> constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
		return v < lo ? lo : v > hi ? hi : v;
	}

	
	template<class T> constexpr const T align(const T v, const int c)
	{
		
		ASSERT_LV3((c & (c - 1)) == 0);
		return (v + (T)(c - 1)) & ~(T)(c - 1);
	}

}






namespace StringExtension
{
	
	
	
	
	
	bool stricmp(const std::string& s1, const std::string& s2);

	
	
	std::string trim(const std::string& input);

	
	void trim_inplace(std::string& input);

	
	
	
	
	std::string trim_number(const std::string& input);

	
	void trim_number_inplace(std::string& s);

	
	int to_int(const std::string input, int default_);

	
	float to_float(const std::string input, float default_);

	
	std::vector<std::string> split(const std::string& input);

	
	
	std::string to_string_with_zero(u64 n, int digit);

	

	
	bool StartsWith(std::string const& value, std::string const& starting);

	
	bool EndsWith(std::string const& value, std::string const& ending);

	
	bool Contains(const std::string& s, const std::string& t);

	
	std::string Replace(std::string const& value, char x, char y);

	
	std::string ToUpper(std::string const& value);

	
	std::vector<std::string_view> Split(std::string_view s, std::string_view delimiter);

	
	
	std::string Join(const std::vector<std::string>& v , const std::string& delimiter);
};



std::vector<std::string_view> split(std::string_view s, std::string_view delimiter);


void remove_whitespace(std::string& s);


bool is_whitespace(std::string_view s);


size_t str_to_size_t(const std::string& s);






namespace Concurrent
{
	
	template <typename T>
	class ConcurrentQueue
	{
	public:
		
		T pop()
		{
			std::unique_lock<std::mutex> lk(mutex_);

			
			while (queue_.empty())
				cond_.wait(lk);

			auto val = queue_.front();
			queue_.pop();

			lk.unlock();
			cond_.notify_one();
			return val;
		}

		
		T& front() {
			
			
			return queue_.front();
		}

		
		void push(const T& item)
		{
			std::unique_lock<std::mutex> lk(mutex_);
			queue_.push(item);
			lk.unlock();
			cond_.notify_one();
		}

		
		size_t size()
		{
			std::unique_lock<std::mutex> lk(mutex_);
			return queue_.size();
		}

		
		void clear()
		{
			std::unique_lock<std::mutex> lk(mutex_);
			
			queue_ = std::queue<T>();
		}

		
		ConcurrentQueue() = default;
		ConcurrentQueue(const ConcurrentQueue&) = delete;

		
		ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

	private:
		std::queue<T> queue_;
		std::mutex mutex_;
		std::condition_variable cond_;
	};

	
	template <typename T>
	class ConcurrentSet
	{
	public:
		
		void remove(const T& item)
		{
			std::unique_lock<std::mutex> lk(mutex_);
			set_.remove(item);
		}

		
		void emplace(const T& item)
		{
			std::unique_lock<std::mutex> lk(mutex_);
			set_.insert(item);
		}

		
		bool contains(const T& item)
		{
			std::unique_lock<std::mutex> lk(mutex_);
			return set_.find(item) != set_.end();
		}

		
		size_t size()
		{
			std::unique_lock<std::mutex> lk(mutex_);
			return set_.size();
		}

		
		void clear()
		{
			std::unique_lock<std::mutex> lk(mutex_);
			
			set_ = std::unordered_set<T>();
		}

		
		ConcurrentSet() = default;
		ConcurrentSet(const ConcurrentSet&) = delete;

		
		ConcurrentSet& operator=(const ConcurrentSet&) = delete;

	private:
		std::unordered_set<T> set_;
		std::mutex mutex_;
	};
}







class StandardInput
{
public:
	
	
	std::string input();

	
	void push(const std::string& s);

	
	void parse_args(const CommandLine& cli);

private:
	
	
	std::queue<std::string> cmds;
};





namespace Misc {
	
	void UnitTest(Test::UnitTester& tester, IEngine& engine);
}

} 




template<std::size_t N>
struct std::hash<YaneuraOu::FixedString<N>> {
    std::size_t operator()(const YaneuraOu::FixedString<N>& fstr) const noexcept {
        return YaneuraOu::hash_bytes(fstr.data(), fstr.size());
    }
};

#endif 
