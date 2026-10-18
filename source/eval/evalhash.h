#ifndef EVALHASH_H_INCLUDED
#define EVALHASH_H_INCLUDED

#include "../types.h"
#include "../misc.h"

namespace YaneuraOu {

template <typename T>
struct HashTable
{
	void resize(ThreadPool& threads, size_t mbSize)
	{
		size_t newClusterCount = mbSize * 1024 * 1024 / sizeof(T);
		newClusterCount = (size_t)1 << MSB64(newClusterCount); 

		if (newClusterCount != size)
		{
			release();
			size = newClusterCount;

			entries_ = (T*)aligned_large_pages_alloc(size * sizeof(T));
			clear(threads);
		}
	}

	void release()
	{
		if (entries_)
		{
			aligned_large_pages_free(entries_);
			entries_ = nullptr;
		}
	}

	~HashTable() { release(); }

	T* operator[] (const Key k) { return entries_ + (static_cast<size_t>(k) & (size - 1)); }
	void clear(ThreadPool& threads) { Tools::memclear(threads, "eHash", entries_, size * sizeof(T)); }

private:

	size_t size = 0;
	T* entries_ = nullptr;
};

} 

#endif 
