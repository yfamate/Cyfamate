

#ifdef _WIN32
#if _WIN32_WINNT < 0x0601
#undef  _WIN32_WINNT
#define _WIN32_WINNT 0x0601 
#endif






#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>




extern "C" {
using fun1_t = bool(*)(LOGICAL_PROCESSOR_RELATIONSHIP,
                       PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
using fun2_t = bool(*)(USHORT, PGROUP_AFFINITY);
using fun3_t = bool(*)(HANDLE, CONST GROUP_AFFINITY*, PGROUP_AFFINITY);
using fun4_t = bool(*)(USHORT, PGROUP_AFFINITY, USHORT, PUSHORT);
using fun5_t = WORD(*)();
using fun6_t = bool(*)(HANDLE, DWORD, PHANDLE);
using fun7_t = bool(*)(LPCSTR, LPCSTR, PLUID);
using fun8_t = bool(*)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
}
#endif

#include <fstream>
#include <iomanip>

#include <sstream>



#if defined(__linux__) && !defined(__ANDROID__)
#include <stdlib.h>
#include <sys/mman.h> 
#endif

#if defined(__APPLE__) || defined(__ANDROID__) || defined(__OpenBSD__) || (defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC) && !defined(_WIN32)) || defined(__e2k__)
#define POSIXALIGNEDALLOC
#include <stdlib.h>
#endif

#include "misc.h"
#include "thread.h"



#include <ctime>				
#include <cstring>				
#include <cstdio>				
#include <cmath>				
#include <filesystem>           
#include "usi.h"				
#include "thread.h"             
#include "testcmd/unit_test.h"	

using namespace std;

namespace YaneuraOu {
namespace {















struct Tie : public streambuf {  

	Tie(streambuf* b, streambuf* l) :
		buf(b),
		logBuf(l) {
	}

	int sync() override { return logBuf->pubsync(), buf->pubsync(); }
	int overflow(int c) override { return log(buf->sputc(char(c)), "<< "); }
	int underflow() override { return buf->sgetc(); }
	int uflow() override { return log(buf->sbumpc(), ">> "); }

	streambuf *buf, *logBuf;

	int log(int c, const char* prefix) {

		static int last = '\n';  

		if (last == '\n')
			logBuf->sputn(prefix, 3);

		return last = logBuf->sputc(char(c));
	}
};

class Logger {

	Logger() :
		in(cin.rdbuf(), file.rdbuf()),
		out(cout.rdbuf(), file.rdbuf()) {
	}
	~Logger() { start(""); }

	ofstream file;    
	Tie           in, out; 

public:
	
	
	static void start(const string& fname) {

		string fname2 = fname;
		string upper_fname = StringExtension::ToUpper(fname2);
		
		
		if (upper_fname == "FALSE")
			fname2 = ""; 
		else if (upper_fname == "TRUE")
			fname2 = "io_log.txt";

		static Logger l;

		if (l.file.is_open())
		{
			cout.rdbuf(l.out.buf);
			cin.rdbuf(l.in.buf);
			l.file.close();
		}

		if (!fname2.empty())
		{
			l.file.open(fname2, ifstream::out);

			if (!l.file.is_open())
			{
				cerr << "Unable to open debug log file " << fname2 << endl;
				exit(EXIT_FAILURE);
			}

			cin.rdbuf(&l.in);
			cout.rdbuf(&l.out);
		}
	}
};

} 






























std::string engine_version_info() {
	std::stringstream ss;
#if STOCKFISH
	ss << "Stockfish " << version << std::setfill('0');
#else
    ss << "YaneuraOu" << ENGINE_VERSION;
#endif

	
#if STOCKFISH
	if constexpr (version == "dev")
#else
    if (StringExtension::Contains(ENGINE_VERSION, "dev"))
#endif
	{
		ss << "-";
#ifdef GIT_DATE
		ss << stringify(GIT_DATE);
#else
		constexpr std::string_view months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");

		std::string       month, day, year;
		std::stringstream date(__DATE__);  

		date >> month >> day >> year;
		ss << year << std::setw(2) << std::setfill('0') << (1 + months.find(month) / 4)
			<< std::setw(2) << std::setfill('0') << day;
#endif

		ss << "-";

#ifdef GIT_SHA
		ss << stringify(GIT_SHA);
#else
		ss << "nogit";
#endif
	}

	return ss.str();
}

std::string engine_info(const std::string& engine_name,
                        const std::string& engine_author,
						const std::string& engine_version,
                        const std::string& eval_name)
{
#if STOCKFISH
    return engine_version_info() + (to_uci ? "\nid author " : " by ")
         + "the Stockfish developers (see AUTHORS file)";
#endif
    

	stringstream ss;
	string engine_name_, engine_author_;

	
	ifstream ifs("engine_name.txt");
	if (!ifs.fail())
	{
		
        engine_name_ = "default engine";
        Tools::getline(ifs, engine_name_);

		
        engine_author_ = "default author";
        Tools::getline(ifs, engine_author_);
	}
	else
	{
		engine_name_ =
			
#if defined(ENGINE_NAME_FROM_MAKEFILE)
			
			
#define STRINGIFY(n) #n
#define TOSTRING(n) STRINGIFY(n)
			TOSTRING(ENGINE_NAME_FROM_MAKEFILE)
#undef STRINGIFY
#undef TOSTRING
#else
			engine_name
#endif
            + (eval_name.empty() ? "" : std::string(" ") + eval_name)
			+ ' ' + engine_version
			+ ' ' + (Is64Bit ? "64" : "32")
			+ TARGET_CPU
#if defined(FOR_TOURNAMENT)
			+" TOURNAMENT"
#endif
			;
			engine_author_ = engine_author;
                
                
	}

	return engine_name_ + "\n" + "id author " + engine_author_; 
}


string compiler_info() {

#define stringify2(x) #x
#define stringify(x) stringify2(x)
#define make_version_string(major, minor, patch) stringify(major) "." stringify(minor) "." stringify(patch)

	
	
	
	
	
	
	
	

	string compiler = "\nCompiled by ";

#if defined(__INTEL_LLVM_COMPILER)
	compiler += "ICX ";
	compiler += stringify(__INTEL_COMPILER) " update " stringify(__INTEL_COMPILER_UPDATE);
#elif defined(__clang__)
	compiler += "clang++ ";
	compiler += make_version_string(__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif _MSC_VER
	compiler += "MSVC ";
	compiler += "(version ";
	compiler += stringify(_MSC_FULL_VER) "." stringify(_MSC_BUILD);
	compiler += ")";
#elif defined(__e2k__) && defined(__LCC__)
	#define dot_ver2(n) \
        compiler += char('.'); \
        compiler += char('0' + (n) / 10); \
        compiler += char('0' + (n) % 10);

	compiler += "MCST LCC ";
	compiler += "(version ";
	compiler += to_string(__LCC__ / 100);
	dot_ver2(__LCC__ % 100) dot_ver2(__LCC_MINOR__) compiler += ")";

#elif __GNUC__
	compiler += "g++ (GNUC) ";
	compiler += make_version_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	compiler += "Unknown compiler ";
	compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
	compiler += " on Apple";
#elif defined(__CYGWIN__)
	compiler += " on Cygwin";
#elif defined(__MINGW64__)
	compiler += " on MinGW64";
#elif defined(__MINGW32__)
	compiler += " on MinGW32";
#elif defined(__ANDROID__)
	compiler += " on Android";
#elif defined(__linux__)
	compiler += " on Linux";
#elif defined(_WIN64)
	compiler += " on Microsoft Windows 64-bit";
#elif defined(_WIN32)
	compiler += " on Microsoft Windows 32-bit";
#else
	compiler += " on unknown system";
#endif

	compiler += "\nCompilation architecture   : ";
#if defined(ARCH)
	compiler += stringify(ARCH);
#else
	compiler += "(undefined architecture)";
#endif

	compiler += "\nCompilation settings       : ";
	compiler += (Is64Bit ? "64bit" : "32bit");
#if defined(USE_VNNI)
	compiler += " VNNI";
#endif
#if defined(USE_AVX512)
	compiler += " AVX512";
#endif




#if defined(USE_AVX2)
	compiler += " AVX2";
#endif
#if defined(USE_SSE41)
	compiler += " SSE41";
#endif
#if defined(USE_SSSE3)
	compiler += " SSSE3";
#endif
#if defined(USE_SSE2)
	compiler += " SSE2";
#endif
#if defined(USE_NEON_DOTPROD)
	compiler += " NEON_DOTPROD";
#elif defined(USE_NEON)
	compiler += " NEON";
#endif

#if STOCKFISH
	compiler += (HasPopCnt ? " POPCNT" : "");
	
#endif

#if !defined(NDEBUG)
	compiler += " DEBUG";
#endif

	compiler += "\nCompiler __VERSION__ macro : ";
#ifdef __VERSION__
	
	compiler += __VERSION__;
#else
	compiler += "(undefined macro)";
#endif

	compiler += "\n";

	return compiler;
}


string config_info()
{
	string config = "\nconfigured by config.h";

	auto o  = [](string(p) , string(q)) { return "\n" + (p + string(20,' ')).substr(0,20) + " : " + q; };
	auto o1 = [&o](const char* p , u64  u ) { return o(string(p) , std::to_string(u) ); };
	auto o2 = [&o](const char* p , bool b ) { return o(string(p) , b ? "true":"false"); };

	
	string eval_type =
#if defined(YANEURAOU_ENGINE_DEEP)
	"DEEP";
#elif defined(YANEURAOU_ENGINE_NNUE)

	
	#if defined(NNUE_ARCHITECTURE_HEADER)
		NNUE_ARCHITECTURE_HEADER;
	#elif defined(EVAL_NNUE_HALFKP256)
		"halfkp_256x2_32_32";
	#elif defined(EVAL_NNUE_KP256)
		"kp_256x2_32_32";
	#elif defined(EVAL_NNUE_HALFKPE9)
		"halfkpe9_256x2_32_32";
	#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_512X2_16_32)
		"halfkp_512x2_16_32";
	#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_1024X2_8_32)
		"halfkp_1024x2_8_32";
	#elif defined(YANEURAOU_ENGINE_NNUE_HALFKP_1024X2_8_64)
		"halfkp_1024x2_8_64";
	#elif defined(YANEURAOU_ENGINE_SFNN1536)
		"sfnn-1536";
	#elif defined(EVAL_NNUE_HALFKP_VM_256X2_32_32)
		"halfkpvm_256x2_32_32";
	#else
		"halfkp_256x2_32_32";
	#endif

#elif defined(YANEURAOU_ENGINE_KPPT)
	"KPPT";
#elif defined(YANEURAOU_ENGINE_KPP_KKPT)
	"KPP_KKPT";
#elif defined(YANEURAOU_ENGINE_MATERIAL)
	"MATERIAL_LV" + to_string(MATERIAL_LEVEL);
#else
	"Unknown";
#endif

	config += o ("EVAL"                , eval_type);

	config += o1("ASSERT_LV"           , ASSERT_LV      );
	config += o1("HASH_KEY_BITS"       , HASH_KEY_BITS  );
	config += o1("TT_CLUSTER_SIZE"     , TT_CLUSTER_SIZE);

	bool for_tournament = 
#if defined(FOR_TOURNAMENT)
		true;
#else
		false;
#endif

	bool test_cmd =
#if defined(ENABLE_TEST_CMD)
		true;
#else
		false;
#endif

	bool make_book_cmd = 
#if defined(ENABLE_MAKEBOOK_CMD)
		true;
#else
		false;
#endif

	bool use_mate_dfpn =
#if defined(USE_MATE_DFPN)
		true;
#else
		false;
#endif

	config += o2("PRETTY_JP"                , pretty_jp          );
	config += o2("FOR_TOURNAMENT"           , for_tournament     );
	config += o2("ENABLE_TEST_CMD"          , test_cmd           );
	config += o2("ENABLE_MAKEBOOK_CMD"      , make_book_cmd      );
	config += o2("USE_MATE_DFPN"            , use_mate_dfpn      );

	
	

	

	return config;
}





static atomic<int64_t> hits[2], means[2];

void dbg_hit_on(bool b) { ++hits[0]; if (b) ++hits[1]; }
void dbg_hit_on(bool c, bool b) { if (c) dbg_hit_on(b); }
void dbg_mean_of(int v) { ++means[0]; means[1] += v; }

void dbg_print() {

	if (hits[0])
		cerr << "Total " << hits[0] << " Hits " << hits[1]
		<< " hit rate (%) " << fixed << setprecision(3) << (100.0f * hits[1] / hits[0]) << endl;

	if (means[0])
		cerr << "Total " << means[0] << " Mean "
		<< (double)means[1] / means[0] << endl;
}







ostream& operator<<(ostream& os, SyncCout sc) {

	static mutex m;

	if (sc == IO_LOCK)
		m.lock();

	if (sc == IO_UNLOCK)
		m.unlock();

	return os;
}

void sync_cout_start() { std::cout << IO_LOCK; }
void sync_cout_end() { std::cout << IO_UNLOCK; }


uint64_t hash_bytes(const char* data, size_t size) {
    const uint64_t m = 0xc6a4a7935bd1e995ull;
    const int      r = 47;

    uint64_t h = size * m;

    const char* end = data + (size & ~(size_t) 7);

    for (const char* p = data; p != end; p += 8)
    {
        uint64_t k;
        std::memcpy(&k, p, sizeof(k));

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    if (size & 7)
    {
        uint64_t k = 0;
        for (int i = (size & 7) - 1; i >= 0; i--)
            k = (k << 8) | (uint64_t) end[i];

        h ^= k;
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}


void start_logger(const string& fname) { Logger::start(fname); }






#if defined (NO_PREFETCH)

void prefetch(const void*) {}

#else

void prefetch([[maybe_unused]] const void* addr) {

	
#if defined (USE_SSE2)

	
	
	ASSERT_LV3(((u64)addr & 0x1f) == 0);

	
	
	

	#if defined(_MSC_VER)
	_mm_prefetch((char const*)addr, _MM_HINT_T0);
	
	#else
	__builtin_prefetch(addr);
	#endif

#endif
}

#endif







ElapsedTimer::ElapsedTimer() :
    startTime(now()) {}
ElapsedTimer::ElapsedTimer(TimePoint s) :
    startTime(s) {}

void ElapsedTimer::reset() { reset(now()); }
void ElapsedTimer::reset(TimePoint s) { startTime = s; }

TimePoint ElapsedTimer::elapsed() const { return TimePoint(now() - startTime); }




namespace Tools {

	

	
	
	void memclear(ThreadPool& threads, const char* name_, void* table, size_t size)
	{
#if !defined(__EMSCRIPTEN__)

		
		
		
		

		
		const size_t threadCount = threads.num_threads();

		if (name_ != nullptr)
			sync_cout << "info string " + string(name_) + " : Start clearing with " << threadCount << " threads , size =  " << size / (1024 * 1024) << "[MB]" << sync_endl;

		

		for (size_t i = 0; i < threadCount; ++i)
		{
			threads.run_on_thread(i, [table, size, threadCount, i]() {
				
				
				
				
				
				
				const size_t stride = size / threadCount,
				start = stride * i,
				len = (i != threadCount - 1) ? stride : size - start;

				memset((uint8_t*)table + start, 0, len);
			});
		}

		for (size_t i = 0; i < threadCount; ++i)
			threads.wait_on_thread(i);

		if (name_ != nullptr)
			sync_cout << "info string " + string(name_) + " : Finish clearing." << sync_endl;

#else
		
		

		memset(table, 0, size);
#endif

	}

	
	
	void exit()
	{
		sleep(3000); 
		::exit(EXIT_FAILURE);
	}

	
	void sleep(u64 ms)
	{
		this_thread::sleep_for(chrono::milliseconds(ms));
	}

	
	string now_string()
	{
		
		

#if defined(_MSC_VER)
		
#pragma warning(disable : 4996)
#endif

		auto now = chrono::system_clock::now();
		auto tp = chrono::system_clock::to_time_t(now);
		auto result = string(ctime(&tp));

		
		while (*result.rbegin() == '\n' || (*result.rbegin() == '\r'))
			result.pop_back();
		return result;
	}

	
	
	
	
	bool getline(ifstream& fs, string& s)
	{
		bool b = (bool)std::getline(fs, s);
		StringExtension::trim_inplace(s);
		return b;
	}

	
	
	wstring MultiByteToWideChar(const string& s)
	{
#if !defined(_WIN32)
		return wstring(s.begin(), s.end()); 
		
		
#else

		
		

		

		int length = ::MultiByteToWideChar(
			CP_THREAD_ACP,			
			MB_PRECOMPOSED,			
			s.c_str(),				
			(int)s.length() + 1,	
			nullptr,				
			0						
		);

		
		

		
		

		
		

		wchar_t* buffer = new wchar_t[length];
		SCOPE_EXIT( delete[] buffer; );

		int result = ::MultiByteToWideChar(
			CP_THREAD_ACP,			
			MB_PRECOMPOSED,			
			s.c_str(),				
			(int)s.length() + 1,	
			buffer,					
			length					
		);

		if (result == 0)
			return wstring(); 

		return wstring(buffer);
#endif
	}

	
	ProgressBar::ProgressBar(u64 size_)
	{
		reset(size_);
	}

	void ProgressBar::reset(u64 size_)
	{
		size = size_;
		if (enable_)
			cout << "0% [";
		dots = 0;
	}

	
	
	void ProgressBar::check(u64 current)
	{
		if (!enable_)
			return;

		
		const size_t all_dots = 70; 

		
		size_t d = (size == 0) ? all_dots : min((size_t)(all_dots * current / size), all_dots);

		for (; dots < d ; ++dots)
			cout << ".";

		if (dots == all_dots)
		{
			cout << "] 100%" << endl;
			dots++; 
		}
	}
	bool ProgressBar::enable_ = false;


	
	std::string to_string(ResultCode code)
	{
		

		switch (code)
		{
		case ResultCode::Ok                   : return "Ok";
		case ResultCode::MemoryAllocationError: return "MemoryAllocationError";
		case ResultCode::SomeError            : return "SomeError";
		case ResultCode::FileNotFound         : return "FileNotFound";
		case ResultCode::FileOpenError        : return "FileOpenError";
		case ResultCode::FileReadError        : return "FileReadError";
		case ResultCode::FileWriteError       : return "FileWriteError";
		case ResultCode::FileCloseError       : return "FileCloseError";
		case ResultCode::FileMismatch         : return "FileMissMatch";
		case ResultCode::CreateFolderError    : return "CreateFolderError";
		case ResultCode::NotImplementedError  : return "NotImplementedError";
		default                               : return "OtherError";
		}
	}
}





namespace SystemIO
{

	
	
	Tools::Result ReadAllLines(const string& filename, vector<string>& lines, bool trim)
	{
#if 0
		ifstream fs(filename);
		if (fs.fail())
			return 1; 

		while (!fs.fail() && !fs.eof())
		{
			string line;
			Dependency::getline(fs, line);
			if (trim)
				line = StringExtension::trim(line);
			if (line.length())
				lines.push_back(line);
		}
		fs.close();

		return 0;
#endif

		
		

		TextReader reader;

		
		reader.SetTrim(trim);
		
		reader.SkipEmptyLine(true);

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		auto result = reader.Open(path);
		if (!result.is_ok())
			return result;

		string line;
		while (reader.ReadLine(line).is_ok())
			lines.emplace_back(line);

		return Tools::Result::Ok();
	}

	
	Tools::Result WriteAllLines(const string& filename, vector<string>& lines)
	{
		TextWriter writer;

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		if (writer.Open(path).is_not_ok())
			return Tools::ResultCode::FileOpenError;

		for(auto& line : lines)
		{
			if (writer.WriteLine(line).is_not_ok())
			return Tools::ResultCode::FileWriteError;
		}

		return Tools::ResultCode::Ok;
	}

	Tools::Result ReadFileToMemory(const string& filename, function<void* (size_t)> callback_func)
	{
		

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		FILE* fp = fopen(path.c_str(), "rb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		fseek(fp, 0, SEEK_END);
		size_t endPos = (size_t)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		size_t beginPos = (size_t)ftell(fp);
		size_t file_size = endPos - beginPos;

		
		
		void* ptr = callback_func(file_size);

		
		
		
		if (ptr == nullptr)
			return Tools::Result(Tools::ResultCode::MemoryAllocationError);

		

		const size_t block_size = 1024 * 1024 * 1024; 
		for (size_t pos = 0; pos < file_size; pos += block_size)
		{
			
			size_t read_size = (pos + block_size < file_size) ? block_size : (file_size - pos);

			if (fread((u8*)ptr + pos, 1, read_size, fp) != read_size)
				
				return Tools::Result(Tools::ResultCode::FileReadError);

			
		}
		fclose(fp);

		return Tools::Result::Ok();
	}


	Tools::Result WriteMemoryToFile(const string& filename, void* ptr, size_t size)
	{
		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		fstream fs(path, ios::out | ios::binary);
		if (fs.fail())
			return Tools::Result(Tools::ResultCode::FileOpenError);

		const u64 block_size = 1024 * 1024 * 1024; 
		for (u64 pos = 0; pos < size; pos += block_size)
		{
			
			u64 write_size = (pos + block_size < size) ? block_size : (size - pos);
			fs.write((char*)ptr + pos, write_size);
			

			if (fs.fail())
				return Tools::Result(Tools::ResultCode::FileWriteError); 
		}

		
		fs.close();

		return Tools::Result::Ok();
	}

	
	

	size_t ftell64(FILE* f)
	{
#if defined(_MSC_VER)
		return _ftelli64(f);
#elif defined(__GNUC__) && defined(IS_64BIT) && !(defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ < 24) && !defined(__MACH__)
		return ftello64(f);
#else
		return ftell(f);
#endif
	}

	int fseek64(FILE* f, size_t offset, int origin)
	{
#if defined(_MSC_VER)
		return _fseeki64(f, offset, origin);
#elif defined(__GNUC__) && defined(IS_64BIT) && !(defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ < 24) && !defined(__MACH__)
		return fseeko64(f, offset, origin);
#else
		return fseek(f, offset, origin);
#endif
	}

	

	
	
	TextReader::TextReader()
	{
		buffer.resize(1024 * 1024);
		line_buffer.reserve(2048);
		clear();

		
		
		trim = false;
		skipEmptyLine = false;
	}

	TextReader::~TextReader()
	{
		Close();
	}

	
	void TextReader::clear()
	{
		fp = nullptr;
		is_eof = false;
		cursor = 0;
		read_size = 0;
		is_prev_cr = false;
		line_number = 0;
	}

	
	Tools::Result TextReader::Open(const string& filename)
	{
		Close();

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		
		fp = fopen(path.c_str(), "rb");
		return (fp == nullptr) ? Tools::Result(Tools::ResultCode::FileOpenError) : Tools::Result::Ok();
	}

	
	void TextReader::Close()
	{
		if (fp != nullptr)
			fclose(fp);

		clear();
	}

	
	int TextReader::read_char()
	{
		
		while (!(is_eof && cursor >= read_size))
		{
			if (cursor < read_size)
				return (int)buffer[cursor++];

			
			read_next_block();
		}
		return -1;
	}

	
	
	
	Tools::Result TextReader::read_line_simple()
	{
		
		
		line_buffer.clear();

		

		while (true)
		{
			int c = read_char();
			if (c == -1 )
			{
				
				
				if (line_buffer.size() == 0)
					return Tools::ResultCode::Eof;

				break;
			}

			if (c == '\r')
			{
				
				is_prev_cr = true;
				break;
			}

			
			
			auto prev_cr = is_prev_cr;
			is_prev_cr = false;

			if (c == '\n')
			{
				if (!prev_cr)
					break;
				
				
			}
			else
			{
				
				line_buffer.push_back(c);
			}
		}

		
		

		return Tools::ResultCode::Ok;
	}


	
	
	Tools::Result TextReader::ReadLine(string& line)
	{
		while (true)
		{
			if (read_line_simple().is_eof())
				return Tools::ResultCode::Eof;

			
			if (trim)
				while (line_buffer.size() > 0)
				{
					char c = *line_buffer.rbegin();
					if (!(c == ' ' || c == '\t'))
						break;

					line_buffer.resize(line_buffer.size() - 1);
				}

			
			size_t skip_byte = 0;
			if (line_number == 0)
				
				if (line_buffer.size() >= 3 && line_buffer[0] == 0xef && line_buffer[1] == 0xbb && line_buffer[2] == 0xbf)
					skip_byte = 3;
			

			
			size_t line_size = line_buffer.size() - skip_byte;

			
			line_number++;

			
			if (skipEmptyLine && line_size == 0)
				continue;

			line = string((const char*)line_buffer.data() + skip_byte, line_size );
			return Tools::ResultCode::Ok;
		}
	}

	
	void TextReader::read_next_block()
	{
		if (::feof(fp))
			read_size = 0;
		else
			read_size = ::fread(&buffer[0], sizeof(u8), buffer.size(), fp);

		
		cursor = 0;

		
		is_eof = read_size == 0;
	}

	
	
	size_t TextReader::GetSize()
	{
		ASSERT_LV3(fp != nullptr);

		fseek64(fp, 0, SEEK_END);
		
		size_t endPos = ftell64(fp);
		fseek64(fp, 0, SEEK_SET);
		size_t beginPos = ftell64(fp);
		size_t file_size = endPos - beginPos;

		return file_size;
	}

	

	Tools::Result TextWriter::Open(const string& filename)
	{
		Close();

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		fp = fopen(path.c_str(), "wb");
		return fp == nullptr ? Tools::ResultCode::FileOpenError
                             : Tools::ResultCode::Ok;
	}

	
	Tools::Result TextWriter::Write(const string& str)
	{
		return Write(str.c_str(), str.size());
	}

	
	Tools::Result TextWriter::WriteLine(const string& line)
	{
		auto result = Write(line.c_str(), line.size());
		if (result.is_not_ok())
			return result;

		
		return Write("\r\n", (size_t)2);
	}

	
	Tools::Result TextWriter::Write(const char* ptr, size_t size)
	{
		
		if (fp == nullptr)
			return Tools::ResultCode::FileWriteError;

		
		size_t write_cursor_end = write_cursor + size;
		char* ptr2 = const_cast<char*>(ptr);

		size_t write_size;
		while (write_cursor_end >= buf_size)
		{
			

			
			write_size = buf_size - write_cursor;
			memcpy(&buf[write_cursor], ptr2, write_size);
			if (fwrite(buf.data(), buf_size, 1, fp) == 0)
				return Tools::ResultCode::FileWriteError;

			
			

			ptr2             += write_size;
			size             -= write_size;
			write_cursor_end -= buf_size;
			write_cursor      = 0;
		}
		memcpy(&buf[write_cursor], ptr2, size);
		write_cursor += size;

		return Tools::ResultCode::Ok;
	}

	
	
	Tools::Result TextWriter::Flush()
	{
		
		if (fp == nullptr)
			return Tools::ResultCode::FileWriteError;

		
		if (write_cursor > 0 && fwrite(&buf[0], write_cursor, 1, fp) == 0)
			return Tools::ResultCode::FileWriteError;

		write_cursor = 0;
		return Tools::ResultCode::Ok;
	}

	Tools::Result TextWriter::Close()
	{
		if (fp)
		{
			
			if (Flush().is_not_ok())
				return Tools::ResultCode::FileWriteError;

			fclose(fp); 
			fp = nullptr;
		}
		return Tools::ResultCode::Ok;
	}

	

	
	Tools::Result BinaryBase::Close()
	{
		Tools::ResultCode result = Tools::ResultCode::Ok;
		if (fp != nullptr)
		{
			if (fclose(fp) != 0)
				result = Tools::ResultCode::FileCloseError;
			fp = nullptr;
		}
		return Tools::Result(result);
	}

	

	
	Tools::Result BinaryReader::Open(const string& filename)
	{
		auto close_result = Close();
		if (!close_result.is_ok()) {
			return close_result;
		}

		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		fp = fopen(path.c_str(), "rb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		return Tools::Result::Ok();
	}

	
	
	size_t BinaryReader::GetSize()
	{
		ASSERT_LV3(fp != nullptr);

		fseek64(fp, 0, SEEK_END);
		size_t endPos = ftell64(fp);
		fseek64(fp, 0, SEEK_SET);
		size_t beginPos = ftell64(fp);
		size_t file_size = endPos - beginPos;

		return file_size;
	}

	
	Tools::Result BinaryReader::Read(void* ptr, size_t size, size_t* size_of_read_bytes)
	{
		size_t actual_size_of_read_bytes = fread(ptr, 1, size, fp);

		if (size_of_read_bytes) {
			*size_of_read_bytes = actual_size_of_read_bytes;
		}

		if (feof(fp)) {
			
			return Tools::Result(Tools::ResultCode::Eof);
		}

		if (actual_size_of_read_bytes != size)
			return Tools::Result(Tools::ResultCode::FileReadError);

		
		
		return Tools::Result::Ok();
	}

	

	
	Tools::Result BinaryWriter::Open(const string& filename, bool append)
	{
		
		std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

		fp = fopen(path.c_str(), append ? "ab" : "wb");
		if (fp == nullptr)
			return Tools::Result(Tools::ResultCode::FileOpenError);

		return Tools::Result::Ok();
	}

	
	Tools::Result BinaryWriter::Write(void* ptr, size_t size)
	{
		if (fwrite((u8*)ptr, 1, size, fp) != size)
			return Tools::Result(Tools::ResultCode::FileWriteError);

		return Tools::Result::Ok();
	}
}







optional<string> read_file_to_string(const string& filename) {

	
	std::string path = Path::Combine(Directory::GetBinaryFolder(), filename);

	ifstream f(path, ios_base::binary);
	if (!f)
		return nullopt;
	return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}







namespace Path
{
	
	
	
	string Combine(const string& folder, const string& filename)
	{
		
		if (IsAbsolute(filename))
			return filename;

		if (folder.length() >= 1 && *folder.rbegin() != '/' && *folder.rbegin() != '\\')
			return folder + "/" + filename;

		return folder + filename;
	}

	
	string GetFileName(const string& path)
	{
		
		auto path_index1 = path.find_last_of("\\");
		auto path_index2 = path.find_last_of("/");

		
		
		if (path_index1 == string::npos && path_index2 == string::npos)
			return path;

		
		
		path_index1 = path_index1 == string::npos ? 0 : path_index1;
		path_index2 = path_index2 == string::npos ? 0 : path_index2;
		auto path_index = max(path_index1, path_index2);

		
		return path.substr(path_index + 1);
	}

	
	string GetDirectoryName(const string& path)
	{
		

		auto length = path.length() - GetFileName(path).length() - 1;
		return (length == 0) ? "" : path.substr(0,length);
	}

	
	
	bool IsAbsolute(const string& path)
	{
		
		const auto path_char1 = '\\';
		const auto path_char2 = '/';

		
		const auto home_char  = '~';

		
		const auto drive_char = ':';

		if (path.length() >= 1)
		{
			const char c = path[0];
			if (c == path_char1 || c == path_char2 || c == home_char)
				return true;

			
			if (path.length() >= 2 && path[1] == drive_char)
				return true;
		}

		return false;
	}

	
	bool Exists(const std::string& path)
	{
		std::ifstream ifs(path);

		
        return ifs.is_open();
	}

};







namespace Directory
{
	namespace fs = std::filesystem;

	
	
	
	vector<string> EnumerateFiles(const string& sourceDirectory, const string& extension)
	{
		vector<string> filenames;

		
		
		auto src = sourceDirectory.empty() ? fs::current_path() : fs::path(sourceDirectory);

		for (auto ent : fs::directory_iterator(src))
			if (fs::is_regular_file(ent)
				&& StringExtension::EndsWith(ent.path().filename().string(), extension))

				filenames.push_back(Path::Combine(ent.path().parent_path().string(), ent.path().filename().string()));

		return filenames;
	}

	
	
	Tools::Result CreateFolder(const std::string& dir_name) {
		std::error_code ec;
		bool created = fs::create_directory(dir_name, ec);
		return created
			? Tools::Result::Ok()
			: Tools::Result(Tools::ResultCode::CreateFolderError);
	}

	
	string GetBinaryFolder() { return CommandLine::get_binary_directory(); }
}






namespace Parser
{

	

	
	string LineScanner::peek_text()
	{
		
		if (!token.empty())
			return token;

		

		
		while (!raw_eol())
		{
			char c = line[pos];
			if (c != ' ')
				break;
			pos++;
		}

		while (!raw_eol())
		{
			
			char c = line[pos++];
			if (c == ' ')
				break;
			token += c;
		}

		
		while (!raw_eol())
		{
			char c = line[pos];
			if (c != ' ')
				break;
			pos++;
		}

		return token;
	}

	
	string LineScanner::get_text()
	{
		auto result = (!token.empty() ? token : peek_text());
		token.clear();
		return result;
	}

	
	
	string LineScanner::get_rest()
	{
		return token.empty()
			? line.substr(pos)
			: token + " " + line.substr(pos);
	}

	
	s64 LineScanner::get_number(s64 defaultValue)
	{
		string token = get_text();
		return token.empty() ? defaultValue : atoll(token.c_str());
	}

	
	double LineScanner::get_double(double defaultValue)
	{
		string token = get_text();
		return token.empty() ? defaultValue : atof(token.c_str());
	}

}





double Math::sigmoid(double x) {
	return 1.0 / (1.0 + exp(-x));
}

double Math::dsigmoid(double x) {
	return sigmoid(x) * (1.0 - sigmoid(x));
}





namespace {
	
	string to_upper(const string source)
	{
		string destination;
		destination.resize(source.size());
		transform(source.cbegin(), source.cend(), destination.begin(), [](char c) { return (char)toupper(c); });
		return destination;
	}
}

namespace StringExtension
{
	
	
	
	bool stricmp(const string& s1, const string& s2)
	{
		
		

		return to_upper(s1) != to_upper(s2);
	}

	
	bool is_space(char c) { return c == '\r' || c == '\n' || c == ' ' || c == '\t'; }

	
	bool is_number(char c) { return '0' <= c && c <= '9'; }

	
	string trim(const string& input)
	{
		
		string s = input;

		
		
		
		

		auto cur = s.length();

		
		
		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
		return s;
	}

	
	void trim_inplace(string& s)
	{
		auto cur = s.length();

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
	}

	
	
	string trim_number(const string& input)
	{
		string s = input;
		auto cur = s.length();

		
		

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		while (cur > 0 && is_number(s[cur-1]))
			cur--;

		while (cur > 0 && is_space(s[cur-1]))
			cur--;

		s.resize(cur);
		return s;
	}

	
	void trim_number_inplace(string& s)
	{
		auto cur = s.length();

		while (cur > 0 && is_space(s[cur - 1]))
			cur--;

		while (cur > 0 && is_number(s[cur - 1]))
			cur--;

		while (cur > 0 && is_space(s[cur - 1]))
			cur--;

		s.resize(cur);
	}

	
	int to_int(const string input, int default_)
	{
		
		
		

		istringstream ss(input);
		int result = default_; 
		ss >> result;
		return result;
	}

	
	float to_float(const string input, float default_)
	{
		istringstream ss(input);
		float result = default_; 
		ss >> result;
		return result;
	}

	
	vector<string> split(const string& input)
	{
		auto result = vector<string>();
		Parser::LineScanner scanner(input);
		while (!scanner.eol())
			result.push_back(scanner.get_text());

		return result;
	}

	
	
	string to_string_with_zero(u64 n, int digit)
	{
		
		ios::fmtflags curret_flag = cout.flags();

		ostringstream ss;
		ss << setw(digit) << setfill('0') << n;
		string s(ss.str());

		
		cout.flags(curret_flag);
		return s;
	}

	
	bool StartsWith(string const& value, string const& starting)
	{
		if (starting.size() > value.size()) return false;
		return equal(starting.begin(), starting.end(), value.begin());
	};

	
	bool EndsWith(string const& value, string const& ending)
	{
		if (ending.size() > value.size()) return false;
		return equal(ending.rbegin(), ending.rend(), value.rbegin());
	};

	
	bool Contains(const string& s, const string& t) {
	   return s.find(t) != string::npos;
	   
	}

	
	string Replace(string const& value, char x, char y)
	{
		string r(value);
		for (size_t i = 0; i < r.size(); ++i)
			if (r[i] == x)
				r[i] = y;
		return r;
	}

	
	string ToUpper(string const& value)
	{
		string s(value);
		transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return toupper(c); });
		return s;
	}

	
	
	
	vector<string_view> Split(string_view s, string_view delimiter) {
		vector<string_view> res;

		if (s.empty())
			return res;

		size_t begin = 0;
		for (;;)
		{
			const size_t end = s.find(delimiter, begin);
			if (end == string::npos)
				break;

			res.emplace_back(s.substr(begin, end - begin));
			begin = end + delimiter.size();
		}

		res.emplace_back(s.substr(begin));

		return res;
	}

	
	
	string Join(const vector<string>& v , const string& delimiter)
	{
		string result;
		for (size_t i = 0; i < v.size(); ++i) {
			result += v[i];
			if (i < v.size() - 1) {
				result += delimiter;
			}
		}
		return result;
	}

};



vector<string_view> split(string_view s, string_view delimiter)
{
	return StringExtension::Split(s, delimiter);
}


void remove_whitespace(string& s) {
	s.erase(remove_if(s.begin(), s.end(), [](char c) { return isspace(c); }), s.end());
}


bool is_whitespace(string_view s) {
	return all_of(s.begin(), s.end(), [](char c) { return isspace(c); });
}


size_t str_to_size_t(const string& s) {
	unsigned long long value = stoull(s);
	if (value > numeric_limits<size_t>::max())
		exit(EXIT_FAILURE);
	return static_cast<size_t>(value);
}





#ifdef _WIN32
#include <direct.h>
#define GETCWD _getcwd
#else
#include <unistd.h>
#define GETCWD getcwd
#endif

string CommandLine::get_binary_directory(std::string argv0) {
	std::string pathSeparator;

#ifdef _WIN32
	pathSeparator = "\\";
#ifdef _MSC_VER
	
	
	char* pgmptr = nullptr;
	if (!_get_pgmptr(&pgmptr) && pgmptr != nullptr && *pgmptr)
		argv0 = pgmptr;
#endif
#else
	pathSeparator = "/";
#endif

	
	auto workingDirectory = CommandLine::get_working_directory();

	
	auto   binaryDirectory = argv0;
	size_t pos = binaryDirectory.find_last_of("\\/");
	if (pos == string::npos)
		binaryDirectory = "." + pathSeparator;
	else
		binaryDirectory.resize(pos + 1);

	
	if (binaryDirectory.find("." + pathSeparator) == 0)
		binaryDirectory.replace(0, 1, workingDirectory);

	return binaryDirectory;
}


string CommandLine::get_working_directory() {
	string workingDirectory = "";
	char        buff[40000];
	char* cwd = GETCWD(buff, 40000);
	if (cwd)
		workingDirectory = cwd;

	return workingDirectory;
}


CommandLine CommandLine::g;







string StandardInput::input()
{
	string cmd;
	if (cmds.size() == 0)
	{
		if (!std::getline(cin, cmd)) 
			cmd = "quit";
	} else {
		
		
		
		
		
		
		cmd = cmds.front();
		cmds.pop();
	}
	return cmd;
}


void StandardInput::push(const string& s)
{
	cmds.push(s);
}

void StandardInput::parse_args(const CommandLine& cli)
{
	
	if (cli.argc >= 3 && string(cli.argv[1]) == "file")
	{
		vector<string> cmds0;
		SystemIO::ReadAllLines(cli.argv[2], cmds0);

		
		for (auto c : cmds0)
			push(c);

	} else {

		string cmd;

		
		

		for (int i = 1; i < cli.argc; ++i)
		{
			string s = cli.argv[i];

			
			while (*s.rbegin() == ' ') s.pop_back();
			while (*s.begin() == ' ') s = s.substr(1, s.size() - 1);

			if (s != ",")
				cmd += s + " ";
			else
			{
				push(cmd);
				cmd = "";
			}
		}
		if (cmd.size() != 0)
			cmds.push(cmd);
	}
}





namespace Misc {
	
	void UnitTest(Test::UnitTester& tester, IEngine& engine)
	{
		auto section1 = tester.section("Misc");

		{
			auto section2 = tester.section("Path");

			{
				auto section3 = tester.section("Combine");

				tester.test("Absolute Path Root1",        Path::Combine("xxxx"  , "/dir"   ) == "/dir"     );
				tester.test("Absolute Path Root2",        Path::Combine("xxxx"  , "\\dir"  ) == "\\dir"    );
				tester.test("Absolute Path Home",         Path::Combine("xxxx"  , "~dir"   ) == "~dir"     );
				tester.test("Absolute Path Drive Letter", Path::Combine("xxxx"  , "c:\\dir") == "c:\\dir"  );
				tester.test("Absolute Path UNC",          Path::Combine("xxxx"  , "\\\\dir") == "\\\\dir"  );
				tester.test("Relative Path1",             Path::Combine("xxxx"  , "yyy"    ) == "xxxx/yyy" );
				tester.test("Relative Path2",             Path::Combine("xxxx/" , "yyy"    ) == "xxxx/yyy" );
				tester.test("Relative Path3",             Path::Combine("xxxx\\", "yyy"    ) == "xxxx\\yyy");
			}

		}
		{
			auto section2 = tester.section("StringExtension");
			{
				tester.test("to_string_with_zero", StringExtension::to_string_with_zero(123 , 6) == "000123");
				tester.test("to_string_with_zero", StringExtension::to_string_with_zero(1234, 6) == "001234");
				tester.test("ToUpper"            , StringExtension::ToUpper("False&True") == "FALSE&TRUE");

				auto v = StringExtension::Split("ABC ; DEF ; GHI", " ; ");
				tester.test("Split"              , v[0]=="ABC" && v[1]=="DEF" && v[2] =="GHI");
			}
		}
	}
} 

} 
