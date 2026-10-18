#include "../../config.h"




#if defined (EVAL_KPP_KKPT)

#include <fstream>
#include <iostream>
#include <unordered_set>

#include "../../evaluate.h"
#include "../../position.h"
#include "../../misc.h"
#include "../../memory.h"
#include "../../shm.h"
#include "../../usi.h"
#include "../../extra/bitop.h"
#include "../evaluate_io.h"
#include "evaluate_kpp_kkpt.h"

#if defined (USE_EVAL_HASH)
#include "../evalhash.h"
#endif

using namespace std;





#if defined(USE_CLASSIC_EVAL)
using namespace YaneuraOu;
void add_options_(OptionsMap& options, ThreadPool& threads);

namespace {
YaneuraOu::OptionsMap* options_ptr;
YaneuraOu::ThreadPool* threads_ptr;
}


#define Options (*options_ptr)
#define Threads (*threads_ptr)

namespace YaneuraOu::Eval {
void add_options(OptionsMap& options, ThreadPool& threads) {
    options_ptr = &options;
    threads_ptr = &threads;
    add_options_(options, threads);
}
}



bool        eval_loaded   = false;
std::string last_eval_dir = "None";


void add_options_(OptionsMap& options, ThreadPool& threads) {

    const char* default_eval_dir = "eval";
    Options.add("EvalDir", Option(default_eval_dir, [](const Option& o) {
                    std::string eval_dir = std::string(o);
                    if (last_eval_dir != eval_dir)
                    {
                        
                        last_eval_dir = eval_dir;
                        eval_loaded   = false;
                    }
                    return std::nullopt;
                }));

}
#endif


namespace YaneuraOu {
namespace Eval {

	struct KppKkptEvalParameters {
		
		KppKkptEvalParameters() noexcept {}

		ValueKk  kk_body[SQ_NB][SQ_NB];
		ValueKkp kkp_body[SQ_NB][SQ_NB][fe_end];
		ValueKpp kpp_body[SQ_NB][fe_end][fe_end];
	};

	static_assert(std::is_trivially_copyable_v<KppKkptEvalParameters>,
		"KppKkptEvalParameters must be trivially copyable for shared memory support");
	static_assert(sizeof(KppKkptEvalParameters) == size_of_eval,
		"KppKkptEvalParameters layout must match the existing contiguous eval layout");

	
	

	ValueKk(*kk_)[SQ_NB][SQ_NB];
	ValueKkp(*kkp_)[SQ_NB][SQ_NB][fe_end];
	ValueKpp(*kpp_)[SQ_NB][fe_end][fe_end];

	SystemWideSharedConstant<KppKkptEvalParameters> shared_eval_parameters;
	constexpr std::size_t KppKkptSharedMemoryDiscriminator = 0x4b504b54u; 

} 
} 

template<>
struct std::hash<YaneuraOu::Eval::KppKkptEvalParameters> {
	std::size_t operator()(const YaneuraOu::Eval::KppKkptEvalParameters& p) const noexcept {
		return static_cast<std::size_t>(
			YaneuraOu::hash_bytes(reinterpret_cast<const char*>(&p), sizeof(p)));
	}
};

namespace YaneuraOu {
namespace Eval {

	
	void load_eval_impl()
	{
		
		
		
        auto make_name = [&](std::string filename) {
            auto eval_dir      = Options["EvalDir"];
            auto abs_eval_path = Path::Combine(Directory::GetBinaryFolder(), eval_dir);
            return Path::Combine(abs_eval_path, filename);
        };

		auto input = EvalIO::EvalInfo::build_kpp_kkpt32(make_name(KK_BIN), make_name(KKP_BIN), make_name(KPP_BIN));
		auto output = EvalIO::EvalInfo::build_kpp_kkpt32((void*)kk, (void*)kkp, (void*)kpp);

		
		input.fe_end = output.fe_end = Eval::fe_end;

		if (!EvalIO::eval_convert(input, output, nullptr))
			goto Error;

		

		
		return;

	Error:;
		
		sync_cout << "\ninfo string Error! open evaluation file failed.\n" << sync_endl;
		Tools::exit();
	}


	u64 calc_check_sum()
	{
		u64 sum = 0;

		auto add_sum = [&](u16*ptr, size_t t)
		{
			for (size_t i = 0; i < t; ++i)
				sum += ptr[i];
		};

		
		

		
		
		
		

		add_sum(reinterpret_cast<u16*>(kk), size_of_kk / sizeof(u16));
		add_sum(reinterpret_cast<u16*>(kkp), size_of_kkp / sizeof(u16));
		add_sum(reinterpret_cast<u16*>(kpp), size_of_kpp / sizeof(u16));

		return sum;
	}

	void init(){}

	
	void eval_assign(void* ptr)
	{
		s8* p = (s8*)ptr;
		kk_  = (ValueKk (*)[SQ_NB][SQ_NB]) (p);
		kkp_ = (ValueKkp(*)[SQ_NB][SQ_NB][fe_end]) (p + size_of_kk);
		kpp_ = (ValueKpp(*)[SQ_NB][fe_end][fe_end]) (p + size_of_kk + size_of_kkp);
	}

	void load_eval()
	{
        if (eval_loaded)
            return;
        eval_loaded = true;  

		auto tmp = make_unique_large_page<KppKkptEvalParameters>();
		eval_assign(tmp.get());
		load_eval_impl();

		shared_eval_parameters =
			SystemWideSharedConstant<KppKkptEvalParameters>(*tmp, KppKkptSharedMemoryDiscriminator);

		if (shared_eval_parameters == nullptr)
		{
			sync_cout << "info string KPP_KKPT shared memory: no eval memory allocated";
			if (auto message = shared_eval_parameters.get_error_message())
				sync_cout << " (" << *message << ")";
			sync_cout << sync_endl;
			Tools::exit();
		}

		eval_assign(const_cast<KppKkptEvalParameters*>(&*shared_eval_parameters));
	}

	
	const int FV_SCALE = 32;

	
	
	void compute_eval_impl(const Position& pos)
	{
		
		
		ASSERT_LV1(&(kk) != nullptr);
		
		

		Square sq_bk = pos.square<KING>(BLACK);
		Square sq_wk = pos.square<KING>(WHITE);
		const auto* ppkppb = kpp[sq_bk];
		const auto* ppkppw = kpp[Inv(sq_wk)];

		auto& pos_ = *const_cast<Position*>(&pos);
		const int length = pos_.eval_list()->length();

#if !defined (USE_EVAL_MAKE_LIST_FUNCTION)

		auto list_fb = pos_.eval_list()->piece_list_fb();
		auto list_fw = pos_.eval_list()->piece_list_fw();

#else
		
		
		
		

		
		BonaPiece list_fb[40];
		BonaPiece list_fw[40];
		memcpy(list_fb, pos_.eval_list()->piece_list_fb(), sizeof(BonaPiece) * 40);
		memcpy(list_fw, pos_.eval_list()->piece_list_fw(), sizeof(BonaPiece) * 40);

		
		make_list_function(pos, list_fb, list_fw);

#endif

		int i, j;
		BonaPiece k0, k1, l0, l1;

		
		EvalSum sum;

#if defined(USE_SSE2)
		
		sum.m[0] = _mm_setzero_si128();
#else
		sum.p[0][0] =  sum.p[1][0] =  0;
#endif

		
		sum.p[2] = kk[sq_bk][sq_wk];

		for (i = 0; i < length; ++i)
		{
			k0 = list_fb[i];
			k1 = list_fw[i];
			const auto* pkppb = ppkppb[k0];
			const auto* pkppw = ppkppw[k1];
			for (j = 0; j < i; ++j)
			{
				l0 = list_fb[j];
				l1 = list_fw[j];

				
				sum.p[0][0] += pkppb[l0];
				sum.p[1][0] += pkppw[l1];
			}
			
			sum.p[2] += kkp[sq_bk][sq_wk][k0];
		}

		auto st = pos.state();
		sum.p[2][0] += st->materialValue * FV_SCALE;

		st->sum = sum;
	}

	
	
	
	
	Value compute_eval(const Position& pos)
	{
		compute_eval_impl(pos);
		return Value(pos.state()->sum.sum(pos.side_to_move()) / FV_SCALE);
	}

	
	s32 do_a_black(const Position& pos, const ExtBonaPiece ebp) {
		const Square sq_bk = pos.square<KING>(BLACK);
		const auto* list0 = pos.eval_list()->piece_list_fb();
		const int length = pos.eval_list()->length();

		const auto* pkppb = kpp[sq_bk][ebp.fb];

		s32 sum = pkppb[list0[0]];
		for (int i = 1; i < length; ++i)
			sum += pkppb[list0[i]];

		return sum;
	}

	
	s32 do_a_white(const Position& pos, const ExtBonaPiece ebp) {
		const Square sq_wk = pos.square<KING>(WHITE);
		const auto* list1 = pos.eval_list()->piece_list_fw();
		const int length = pos.eval_list()->length();

		const auto* pkppw = kpp[Inv(sq_wk)][ebp.fw];

		s32 sum = pkppw[list1[0]];
		for (int i = 1; i < length ; ++i)
			sum += pkppw[list1[i]];

		return sum;
	}

	
	EvalSum do_a_pc(const Position& pos, const ExtBonaPiece ebp) {
		
		const Square sq_bk = pos.square<KING>(BLACK);
		const Square sq_wk = pos.square<KING>(WHITE);
		const auto list0 = pos.eval_list()->piece_list_fb();
		const auto list1 = pos.eval_list()->piece_list_fw();
		const int length = pos.eval_list()->length();

		EvalSum sum;

		
#if defined(USE_SSE2)
		sum.m[0] = _mm_setzero_si128();
#else
		sum.p[0][0] = 0;
		sum.p[1][0] = 0;
#endif

		
		sum.p[2] = kkp[sq_bk][sq_wk][ebp.fb];

		const auto* pkppb = kpp[sq_bk][ebp.fb];
		const auto* pkppw = kpp[Inv(sq_wk)][ebp.fw];

        
        
        
        
        
        
        

        
        
        
        
        
        
        

        

#if defined(USE_AVX2)
        __m256i sum0 = _mm256_setzero_si256();
        __m256i sum1 = _mm256_setzero_si256();
        int i = 0;
        for (; i + 8 < length; i += 8) {
            __m256i index0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(list0 + i));
            __m256i w0 = _mm256_i32gather_epi32(reinterpret_cast<const int*>(pkppb), index0, 2);
            w0 = _mm256_slli_epi32(w0, 16);
            w0 = _mm256_srai_epi32(w0, 16);
            sum0 = _mm256_add_epi32(sum0, w0);

            __m256i index1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(list1 + i));
            __m256i w1 = _mm256_i32gather_epi32(reinterpret_cast<const int*>(pkppw), index1, 2);
            w1 = _mm256_slli_epi32(w1, 16);
            w1 = _mm256_srai_epi32(w1, 16);
            sum1 = _mm256_add_epi32(sum1, w1);
        }

        
		
		
		
		{
            __m256i w0 = _mm256_set_epi32(
                0,
                0,
                pkppb[list0[i + 5]],
                pkppb[list0[i + 4]],
                pkppb[list0[i + 3]],
                pkppb[list0[i + 2]],
                pkppb[list0[i + 1]],
                pkppb[list0[i + 0]]);
            __m256i w1 = _mm256_set_epi32(
                0,
                0,
                pkppw[list1[i + 5]],
                pkppw[list1[i + 4]],
                pkppw[list1[i + 3]],
                pkppw[list1[i + 2]],
                pkppw[list1[i + 1]],
                pkppw[list1[i + 0]]);
            sum0 = _mm256_add_epi32(sum0, w0);
            sum1 = _mm256_add_epi32(sum1, w1);
        }

        
        sum0 = _mm256_add_epi32(sum0, _mm256_srli_si256(sum0, 8));
        sum0 = _mm256_add_epi32(sum0, _mm256_srli_si256(sum0, 4));
        sum.p[0][0] = _mm_extract_epi32(_mm256_extracti128_si256(sum0, 0), 0) +
            _mm_extract_epi32(_mm256_extracti128_si256(sum0, 1), 0);
        
        sum1 = _mm256_add_epi32(sum1, _mm256_srli_si256(sum1, 8));
        sum1 = _mm256_add_epi32(sum1, _mm256_srli_si256(sum1, 4));
        sum.p[1][0] = _mm_extract_epi32(_mm256_extracti128_si256(sum1, 0), 0) +
            _mm_extract_epi32(_mm256_extracti128_si256(sum1, 1), 0);
#else
        sum.p[0][0] = pkppb[list0[0]];
        sum.p[1][0] = pkppw[list1[0]];
        for (int i = 1; i < length; ++i) {
            sum.p[0][0] += pkppb[list0[i]];
            sum.p[1][0] += pkppw[list1[i]];
        }
#endif

        return sum;
    }


#if defined (USE_EVAL_HASH)
	

	struct EvaluateHashTable : HashTable<EvalSum> {};
	EvaluateHashTable g_evalTable;

	void EvalHash_Resize(size_t mbSize) { g_evalTable.resize(Threads, mbSize); }
	void EvalHash_Clear() { g_evalTable.clear(Threads); };

	
	void prefetch_evalhash(const Key key)
	{
		prefetch(g_evalTable[key]);
	}

#endif

#if !defined(USE_EVAL_MAKE_LIST_FUNCTION)
	void evaluateBody(const Position& pos)
	{
		

		auto now = pos.state();
		auto prev = now->previous;

		
		
		
		
		
		if (!prev->sum.evaluated())
		{
			
			compute_eval_impl(pos);

			return;
			
		}

		
		
		

		auto& dp = now->dirtyPiece;

		
		int moved_piece_num = dp.dirty_num;

		auto list0 = pos.eval_list()->piece_list_fb();
		auto list1 = pos.eval_list()->piece_list_fw();
		auto length = pos.eval_list()->length();

		auto dirty = dp.pieceNo[0];

		
		if (dirty >= PIECE_NUMBER_KING)
		{
			
			
			
			EvalSum diff = prev->sum;

			auto sq_bk = pos.square<KING>(BLACK);
			auto sq_wk = pos.square<KING>(WHITE);

			
			diff.p[2] = kk[sq_bk][sq_wk];
			diff.p[2][0] += now->materialValue * FV_SCALE;

			
			if (dirty == PIECE_NUMBER_WKING)
			{
				const auto ppkppw = kpp[Inv(sq_wk)];

				
				diff.p[1][0] = 0;
				

#if defined(USE_AVX2)
                __m256i sum1_256 = _mm256_setzero_si256();
                __m128i sum1_128 = _mm_setzero_si128();

                for (int i = 0; i < length ; ++i)
                {
					
					
					diff.p[2] += kkp[sq_bk][sq_wk][list0[i]];
					
					const int k1 = list1[i];
                    const auto* pkppw = ppkppw[k1];
                    int j = 0;
                    for (; j + 8 < i; j += 8) {
                        __m256i index1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(list1 + j));
                        __m256i w1 = _mm256_i32gather_epi32(reinterpret_cast<const int*>(pkppw), index1, 2);
                        w1 = _mm256_slli_epi32(w1, 16);
                        w1 = _mm256_srai_epi32(w1, 16);
                        sum1_256 = _mm256_add_epi32(sum1_256, w1);
                    }

                    for (; j + 4 < i; j += 4) {
                        __m128i index1 = _mm_load_si128(reinterpret_cast<const __m128i*>(list1 + j));
                        __m128i w1 = _mm_i32gather_epi32(reinterpret_cast<const int*>(pkppw), index1, 2);
                        w1 = _mm_slli_epi32(w1, 16);
                        w1 = _mm_srai_epi32(w1, 16);
                        sum1_128 = _mm_add_epi32(sum1_128, w1);
                    }

                    for (; j < i; ++j) {
                        diff.p[1][0] += pkppw[list1[j]];
                    }
                }
                sum1_128 = _mm_add_epi32(sum1_128, _mm256_extracti128_si256(sum1_256, 0));
                sum1_128 = _mm_add_epi32(sum1_128, _mm256_extracti128_si256(sum1_256, 1));
                sum1_128 = _mm_add_epi32(sum1_128, _mm_srli_si128(sum1_128, 8));
                sum1_128 = _mm_add_epi32(sum1_128, _mm_srli_si128(sum1_128, 4));
                diff.p[1][0] += _mm_extract_epi32(sum1_128, 0);
#else
				for (int i = 0; i < length ; ++i)
				{
					diff.p[2] += kkp[sq_bk][sq_wk][list0[i]];

					const int k1 = list1[i];
					const auto* pkppw = ppkppw[k1];
					for (int j = 0; j < i; ++j)
					{
						const int l1 = list1[j];
						diff.p[1][0] += pkppw[l1];
					}
				}
#endif

				
				if (moved_piece_num == 2)
				{
					
					

					const int listIndex_cap = dp.pieceNo[1];
					diff.p[0][0] += do_a_black(pos, dp.changed_piece[1].new_piece);
					list0[listIndex_cap] = dp.changed_piece[1].old_piece.fb;
					diff.p[0][0] -= do_a_black(pos, dp.changed_piece[1].old_piece);
					list0[listIndex_cap] = dp.changed_piece[1].new_piece.fb;
				}

			}
			else {

				
				

				const auto* ppkppb = kpp[sq_bk];
				diff.p[0][0] = 0;
				

#if defined(USE_AVX2)
                __m256i sum0_256 = _mm256_setzero_si256();
                __m128i sum0_128 = _mm_setzero_si128();

                for (int i = 0; i < length ; ++i)
                {
                    const int k0 = list0[i];
                    const auto* pkppb = ppkppb[k0];
                    int j = 0;
                    for (; j + 8 < i; j += 8) {
                        __m256i index0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(list0 + j));
                        __m256i w0 = _mm256_i32gather_epi32(reinterpret_cast<const int*>(pkppb), index0, 2);
                        w0 = _mm256_slli_epi32(w0, 16);
                        w0 = _mm256_srai_epi32(w0, 16);
                        sum0_256 = _mm256_add_epi32(sum0_256, w0);
                    }

                    for (; j + 4 < i; j += 4) {
                        __m128i index0 = _mm_load_si128(reinterpret_cast<const __m128i*>(list0 + j));
                        __m128i w0 = _mm_i32gather_epi32(reinterpret_cast<const int*>(pkppb), index0, 2);
                        w0 = _mm_slli_epi32(w0, 16);
                        w0 = _mm_srai_epi32(w0, 16);
                        sum0_128 = _mm_add_epi32(sum0_128, w0);
                    }

                    for (; j < i; ++j) {
                        diff.p[0][0] += pkppb[list0[j]];
                    }

                    diff.p[2] += kkp[sq_bk][sq_wk][k0];
                }
                sum0_128 = _mm_add_epi32(sum0_128, _mm256_extracti128_si256(sum0_256, 0));
                sum0_128 = _mm_add_epi32(sum0_128, _mm256_extracti128_si256(sum0_256, 1));
                sum0_128 = _mm_add_epi32(sum0_128, _mm_srli_si128(sum0_128, 8));
                sum0_128 = _mm_add_epi32(sum0_128, _mm_srli_si128(sum0_128, 4));
                diff.p[0][0] += _mm_extract_epi32(sum0_128, 0);
#else
                for (int i = 0; i < length ; ++i)
                {
                    const int k0 = list0[i];
                    const auto* pkppb = ppkppb[k0];
                    for (int j = 0; j < i; ++j) {
                        const int l0 = list0[j];
                        diff.p[0][0] += pkppb[l0];
                    }
                    diff.p[2] += kkp[sq_bk][sq_wk][k0];
                }
#endif

				if (moved_piece_num == 2) {
					const int listIndex_cap = dp.pieceNo[1];
					diff.p[1][0] += do_a_white(pos, dp.changed_piece[1].new_piece);
					list1[listIndex_cap] = dp.changed_piece[1].old_piece.fw;
					diff.p[1][0] -= do_a_white(pos, dp.changed_piece[1].old_piece);
					list1[listIndex_cap] = dp.changed_piece[1].new_piece.fw;
				}
			}

			
			now->sum = diff;

		}
		else {

			
			

			const int listIndex = dp.pieceNo[0];

			auto diff = do_a_pc(pos, dp.changed_piece[0].new_piece);
			if (moved_piece_num == 1) {

				
				list0[listIndex] = dp.changed_piece[0].old_piece.fb;
				list1[listIndex] = dp.changed_piece[0].old_piece.fw;
				diff -= do_a_pc(pos, dp.changed_piece[0].old_piece);

			}
			else {

				

				auto sq_bk = pos.square<KING>(BLACK);
				auto sq_wk = pos.square<KING>(WHITE);

				diff += do_a_pc(pos, dp.changed_piece[1].new_piece);
				diff.p[0][0] -= kpp[    sq_bk ][dp.changed_piece[0].new_piece.fb][dp.changed_piece[1].new_piece.fb];
				diff.p[1][0] -= kpp[Inv(sq_wk)][dp.changed_piece[0].new_piece.fw][dp.changed_piece[1].new_piece.fw];

				const PieceNumber listIndex_cap = dp.pieceNo[1];
				list0[listIndex_cap] = dp.changed_piece[1].old_piece.fb;
				list1[listIndex_cap] = dp.changed_piece[1].old_piece.fw;

				list0[listIndex] = dp.changed_piece[0].old_piece.fb;
				list1[listIndex] = dp.changed_piece[0].old_piece.fw;
				diff -= do_a_pc(pos, dp.changed_piece[0].old_piece);
				diff -= do_a_pc(pos, dp.changed_piece[1].old_piece);

				diff.p[0][0] += kpp[    sq_bk ][dp.changed_piece[0].old_piece.fb][dp.changed_piece[1].old_piece.fb];
				diff.p[1][0] += kpp[Inv(sq_wk)][dp.changed_piece[0].old_piece.fw][dp.changed_piece[1].old_piece.fw];
				list0[listIndex_cap] = dp.changed_piece[1].new_piece.fb;
				list1[listIndex_cap] = dp.changed_piece[1].new_piece.fw;
			}

			list0[listIndex] = dp.changed_piece[0].new_piece.fb;
			list1[listIndex] = dp.changed_piece[0].new_piece.fw;

			
			diff.p[2][0] += (now->materialValue - prev->materialValue) * FV_SCALE;

			now->sum = diff + prev->sum;
		}
		
	}
#else
	
	void evaluateBody(const Position& pos)
	{
		compute_eval_impl(pos);
	}
#endif 

	
	Value evaluate(const Position& pos)
	{
		auto st = pos.state();
		auto &sum = st->sum;

		
		if (sum.evaluated())
			return Value(sum.sum(pos.side_to_move()) / FV_SCALE);
		

#if defined(USE_GLOBAL_OPTIONS)
		
		
		if (!GlobalOptions.use_eval_hash)
		{
			evaluateBody(pos);
			ASSERT_LV5(pos.state()->materialValue == Eval::material(pos));
			return Value(sum.sum(pos.side_to_move()) / FV_SCALE);
		}
#endif

#if defined ( USE_EVAL_HASH )
		
		const Key key = pos.key();

		

		
		EvalSum entry = *g_evalTable[key];   
		entry.decode();
		if (entry.key == key)
		{
			

			
			sum = entry;
			return Value(entry.sum(pos.side_to_move()) / FV_SCALE);
		}
		

#endif

		
		evaluateBody(pos);

#if defined(USE_EVAL_HASH)
		
		sum.key = key;
		sum.encode();
		*g_evalTable[key] = sum;
#endif

		ASSERT_LV5(pos.state()->materialValue == Eval::material(pos));
		
		

#if 0
		if (!(Value(st->sum.sum(pos.side_to_move()) / FV_SCALE) == compute_eval(pos)))
		{
			st->sum.p[0][0] = VALUE_NOT_EVALUATED;
			evaluateBody(pos);
		}
#endif

		auto v = Value(sum.sum(pos.side_to_move()) / FV_SCALE);

		
		
		

		return v;
	}

	void evaluate_with_no_return(const Position& pos)
	{
		
		

#if !defined(USE_EVAL_MAKE_LIST_FUNCTION)
		
		if (!pos.state()->sum.evaluated())
			evaluate(pos);
#else
		
		
#endif
	}


	
	void print_eval_stat(Position& pos)
	{
		cout << "--- EVAL STAT\n";

		Square sq_bk = pos.square<KING>(BLACK);
		Square sq_wk = pos.square<KING>(WHITE);
		const auto* ppkppb = kpp[sq_bk];
		const auto* ppkppw = kpp[Inv(sq_wk)];

		auto& pos_ = *const_cast<Position*>(&pos);

		auto list_fb = pos_.eval_list()->piece_list_fb();
		auto list_fw = pos_.eval_list()->piece_list_fw();

		int i, j;
		BonaPiece k0, k1, l0, l1;

		
		for (i = 0; i < PIECE_NUMBER_KING; ++i)
			cout << int(list_fb[i]) << " = " << list_fb[i] << " , " << int(list_fw[i]) << " =  " << list_fw[i] << endl;

		
		EvalSum sum;

#if defined(USE_SSE2)
		
		sum.m[0] = _mm_setzero_si128();
#else
		sum.p[0][0] =  sum.p[1][0] =  0;
#endif

		
		sum.p[2] = kk[sq_bk][sq_wk];
		cout << "KKC : " << sq_bk << " " << sq_wk << " = " << kk[sq_bk][sq_wk][0] << " + " << kk[sq_bk][sq_wk][1] << endl;

		for (i = 0; i < PIECE_NUMBER_KING; ++i)
		{
			k0 = list_fb[i];
			k1 = list_fw[i];
			const auto* pkppb = ppkppb[k0];
			const auto* pkppw = ppkppw[k1];
			for (j = 0; j < i; ++j)
			{
				l0 = list_fb[j];
				l1 = list_fw[j];

				cout << "BKPP : " << sq_bk << " " << k0 << " " << l0 << " = " << pkppb[l0] << endl;
				cout << "WKPP : " << sq_wk << " " << k1 << " " << l1 << " = " << pkppw[l1] << endl;

				sum.p[0][0] += pkppb[l0];
				sum.p[1][0] += pkppw[l1];
			}
			sum.p[2] += kkp[sq_bk][sq_wk][k0];

			cout << "KKP : " << sq_bk << " " << sq_wk << " " << k0 << " = " << kkp[sq_bk][sq_wk][k0][0] << " + " << kkp[sq_bk][sq_wk][k0][1] << endl;

		}

		cout << "Material = " << pos.state()->materialValue << endl;
		cout << sum;
		cout << "---" << endl;

		
		

	}

	
	
	void foreach_eval_param(std::function<void(s32, s32)>f, int type)
	{
		
		if (type == -1 || type == 0)
		{
			for (u64 i = 0; i < (u64)SQ_NB * (u64)SQ_NB; ++i)
			{
				auto v = ((ValueKk*)kk)[i];
				f(v[0], v[1]);

				
				
			}
		}

		
		if (type == -1 || type == 1)
		{
			for (u64 i = 0; i < (u64)SQ_NB * (u64)SQ_NB * (u64)fe_end; ++i)
			{
				auto v = ((ValueKkp*)kkp)[i];
				f(v[0], v[1]);
			}
		}

		
		if (type == -1 || type == 2)
		{
			for (u64 i = 0; i < (u64)SQ_NB * (u64)fe_end * (u64)fe_end; ++i)
			{
				auto v = ((ValueKpp*)kpp)[i];
				f(v, 0); 
			}
		}
	}

} 
} 

#endif 
