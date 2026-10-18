#pragma once

#include "config.h"
#include "misc.h"
#include <vector>

namespace YaneuraOu {

#if 0
class Tracer 
{
public:

	using T = uint32_t;


	Tracer(){
		size = 8ULL*1024*1024*1024/sizeof(T);
		trace.reserve(size);
	}

	void restart() { restarted = true; idx = 0; }

	void checkpoint(T data)
	{
		if (!restarted)
		{
			if (trace.size() == size)
				return;

			trace.emplace_back(data);
		} else {

			if (trace.size() <= idx)
				return;

			if (trace[idx] != data)
			{
				std::cout << "checkpoint error , first time = " << trace[idx]
					      << " , now = " << data << std::endl;
				ASSERT(false);
			}
		}
		++idx;
	}

private:
	std::vector<T> trace;
	size_t size;
	size_t idx = 0;
	bool restarted = false;
};
#endif


class Tracer
{
public:
	const char* FILE_NAME = "C:\\Users\\yaneen\\Desktop\\yaneuraou-log.txt";

	Tracer(){
		fopen_s(&fp, FILE_NAME,"w");
	}

	void restart() {
		fclose(fp);
		fopen_s(&fp, FILE_NAME,"r");

		restarted = true;
	}

	void checkpoint(std::string data1,int data2)
	{
		checkpoint(data1 + " , " + std::to_string(data2));
	}

	void checkpoint(std::string data)
	{
		if (!restarted)
		{
			fprintf(fp, "%s\n", data.c_str());
		} else {

			char line[256];
			
			if (fgets(line,sizeof(line),fp) == nullptr)
			{
				std::cout<< "checkpoint error! overrun!!" << std::endl;
				ASSERT(false);
			}

			if (std::string(line)!=data+"\n")
			{
				std::cout << "checkpoint error , first time = " << line
					      << " , now = " << data << std::endl;

				ASSERT(false);
			}
		}
	}

	void checkpoint(int data)
	{
		if (!restarted)
		{
			fprintf(fp, "%d\n", data);
		} else {

			char line[256];
			
			if (fgets(line,sizeof(line),fp) == nullptr)
			{
				std::cout<< "checkpoint error! overrun!!" << std::endl;
				ASSERT(false);
			}

			if (atoi(line)!=data)
			{
				std::cout << "checkpoint error , first time = " << line
					      << " , now = " << data << std::endl;

				ASSERT(false);
			}
		}
	}

private:
	FILE* fp;
	bool restarted = false;
};

} 

