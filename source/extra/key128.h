#ifndef KEY128_H_INCLUDED
#define KEY128_H_INCLUDED

#include "../types.h"

namespace YaneuraOu {


#if HASH_KEY_BITS <= 64
#define SET_HASH(x,p0,p1,p2,p3) { x = (p0); auto dummy_func = [](u64,u64,u64){}; dummy_func(p1,p2,p3); }
#elif HASH_KEY_BITS <= 128
#define SET_HASH(x,p0,p1,p2,p3) { Key _K0=(p0); Key _K1=(p1); x.set(_K0, _K1); auto dummy_func = [](u64,u64){}; dummy_func(p2,p3); }
#else
#define SET_HASH(x,p0,p1,p2,p3) { Key _K0=(p0); Key _K1=(p1); Key _K2=(p2); Key _K3=(p3); x.set(_K0, _K1, _K2, _K3); }
#endif


using Key64 = uint64_t;


struct alignas(16) Key128
{
#if defined (USE_SSE2)
	union {
		__m128i m;
		u64 p[2];
	};
#else
		u64 p[2];
#endif

	Key128() {}

	Key128(const Key64& k) { set(k, 0); }

#if defined (USE_SSE2)
	Key128(const Key128& bb) { _mm_store_si128(&this->m, bb.m); }
	Key128& operator = (const Key128& rhs) { _mm_store_si128(&this->m, rhs.m); return *this; }
#else
#endif

	void set(Key64 k0, Key64 k1) {
#if defined(USE_SSE2)
	m = _mm_set_epi64x(k1,k0);
#else
	p[0] = k0; p[1] = k1;
#endif
	}


	template <int n>
	u64 extract64() const
	{
		static_assert(n == 0 || n == 1, "");
	#if defined(USE_SSE41) && defined(IS_64BIT)
		return (u64)(_mm_extract_epi64(m, n));
	#else
		return p[n];
	#endif
	}

#if defined (USE_SSE2)
	Key128& operator += (const Key128& b1) { this->m = _mm_add_epi64(m, b1.m); return *this; }
	Key128& operator -= (const Key128& b1) { this->m = _mm_sub_epi64(m, b1.m); return *this; }
	Key128& operator ^= (const Key128& b1) { this->m = _mm_xor_si128(m, b1.m); return *this; }
#else
	Key128& operator += (const Key128& b1) { this->p[0] += b1.p[0]; this->p[1] += b1.p[1]; return *this; }
	Key128& operator -= (const Key128& b1) { this->p[0] -= b1.p[0]; this->p[1] -= b1.p[1]; return *this; }
	Key128& operator ^= (const Key128& b1) { this->p[0] ^= b1.p[0]; this->p[1] ^= b1.p[1]; return *this; }
#endif

	Key128& operator *= (int64_t i) { p[0] *= i; p[1] *= i; return *this; }
	bool operator == (const Key128& rhs) const {
#if defined (USE_SSE41)
		__m128i neq = _mm_xor_si128(this->m, rhs.m);
		return _mm_test_all_zeros(neq, neq) ? true : false;
#else
		return p[0]==rhs.p[0] && p[1]==rhs.p[1];
#endif
	}
	bool operator != (const Key128& rhs) const { return !(*this == rhs); }

	Key128 operator * (const int64_t i) const { return Key128(*this) *= i; }

	Key128 operator + (const Key128& rhs) const { return Key128(*this) += rhs; }
	Key128 operator ^ (const Key128& rhs) const { return Key128(*this) ^= rhs; }

    bool operator < (const Key128& rhs) const {
        if (this->p[0] != rhs.p[0]) {
            return this->p[0] < rhs.p[0];
        }
        return this->p[1] < rhs.p[1];
    }

	operator Key64() const { return static_cast<int64_t>(extract64<0>()); }
};

static std::ostream& operator << (std::ostream& os, const Key128& k)
{
	os <<  k.extract64<1>() << ":" << k.extract64<0>();
	return os;
}

struct alignas(32) Key256
{
#if defined(USE_AVX2)
	union {
		__m256i m;
		u64 p[4];
	};
#else
		u64 p[4];
#endif

	Key256() {}
	Key256(const Key64& k) { set(k, 0, 0, 0); }
#if defined(USE_AVX2)
	Key256(const Key256& bb) { _mm256_store_si256(&this->m, bb.m); }
	Key256& operator = (const Key256& rhs) { _mm256_store_si256(&this->m, rhs.m); return *this; }
#endif

	void set(Key64 k0, Key64 k1, Key64 k2, Key64 k3) {
#if defined(USE_AVX2)
	m = _mm256_set_epi64x(k3, k2, k1, k0);
#else
	p[0] = k0; p[1] = k1; p[2] = k2; p[3] = k3;
#endif
	}


	template <int n>
	u64 extract64() const
	{
		static_assert(n == 0 || n == 1 || n == 2 || n == 3 , "");
	#if defined(USE_AVX2) && defined(IS_64BIT)
		return (u64)(_mm256_extract_epi64(m, n));
	#else
		return p[n];
	#endif
	}

#if defined(USE_AVX2)
	Key256& operator += (const Key256& b1) { this->m = _mm256_add_epi64(m, b1.m); return *this; }
	Key256& operator -= (const Key256& b1) { this->m = _mm256_sub_epi64(m, b1.m); return *this; }
	Key256& operator ^= (const Key256& b1) { this->m = _mm256_xor_si256(m, b1.m); return *this; }
#else
	Key256& operator += (const Key256& b1) { this->p[0] += b1.p[0]; this->p[1] += b1.p[1]; this->p[2] += b1.p[2]; this->p[3] += b1.p[3]; return *this; }
	Key256& operator -= (const Key256& b1) { this->p[0] -= b1.p[0]; this->p[1] -= b1.p[1]; this->p[2] -= b1.p[2]; this->p[3] -= b1.p[3]; return *this; }
	Key256& operator ^= (const Key256& b1) { this->p[0] ^= b1.p[0]; this->p[1] ^= b1.p[1]; this->p[2] ^= b1.p[2]; this->p[3] ^= b1.p[3]; return *this; }
#endif
	Key256& operator *= (int64_t i) { p[0] *= i; p[1] *= i; p[2] *= i; p[3] *= i; return *this; }
	bool operator == (const Key256& rhs) const {
#if defined(USE_AVX2)
		return (_mm256_testc_si256(_mm256_cmpeq_epi8(this->m, rhs.m), _mm256_set1_epi8(static_cast<char>(0xffu))) ? true : false);
#else
		return p[0]==rhs.p[0] && p[1]==rhs.p[1] && p[2]==rhs.p[2] && p[3]==rhs.p[3];
#endif
	}

	bool operator != (const Key256& rhs) const { return !(*this == rhs); }

	Key256 operator * (const int64_t i) const { return Key256(*this) *= i; }

	Key256 operator + (const Key256& rhs) const { return Key256(*this) += rhs; }
	Key256 operator ^ (const Key256& rhs) const { return Key256(*this) ^= rhs; }

    operator Key64() const { return static_cast<int64_t>(extract64<0>()); }
};

static std::ostream& operator << (std::ostream& os, const Key256& k)
{
	os <<  k.extract64<3>() << ":" << k.extract64<2>() <<  k.extract64<1>() << ":" << k.extract64<0>();
	return os;
}

} 


template <>
struct std::hash<YaneuraOu::Key128> {
	size_t operator()(const YaneuraOu::Key128& k) const {
		return (size_t)(k.extract64<0>());
	}
};

template <>
struct std::hash<YaneuraOu::Key256> {
	size_t operator()(const YaneuraOu::Key256& k) const {
		return (size_t)(k.extract64<0>());
	}
};

#endif 
