


#ifndef CLASSIC_NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#define CLASSIC_NNUE_FEATURE_TRANSFORMER_H_INCLUDED

#include "../../config.h"

#if defined(EVAL_NNUE)

#if defined(SFNNwoPSQT)
#define USE_ELEMENT_WISE_MULTIPLY
#endif

#include "nnue_common.h"
#include "nnue_architecture.h"
#include "features/index_list.h"

#include <algorithm>  
#include <cstring>  

#if defined(USE_FINNY_TABLES)
#include <array>
#include <cstdint>
#include <memory>
#endif

#if defined(CYF_GP8)

#include "cyf_gp8_threat.h"
#if defined(CYF_THREAT) && !(defined(USE_AVX2) || defined(USE_AVX512))
#error "CYF_THREAT needs the AVX2 / AVX-512 accumulator path"
#endif
#if defined(CYF_THREAT) && defined(CYF_ACC_PARENT_FILL)
#error "CYF_THREAT does not implement CYF_ACC_PARENT_FILL"
#endif

#endif 
namespace YaneuraOu {
namespace Eval::NNUE {






#define VECTOR

#if defined(USE_AVX512)
using vec_t = __m512i;
#define vec_load(a) _mm512_load_si512(a)
#define vec_store(a, b) _mm512_store_si512(a, b)
#define vec_add_16(a, b) _mm512_add_epi16(a, b)
#define vec_sub_16(a, b) _mm512_sub_epi16(a, b)
#define vec_mulhi_16(a, b) _mm512_mulhi_epi16(a, b)
#define vec_set_16(a) _mm512_set1_epi16(a)
#define vec_max_16(a, b) _mm512_max_epi16(a, b)
#define vec_min_16(a, b) _mm512_min_epi16(a, b)
#define vec_slli_16(a, b) _mm512_slli_epi16(a, b)
#define vec_packus_16(a, b) _mm512_packus_epi16(a, b)
#define vec_zero() _mm512_setzero_si512()
static constexpr IndexType kNumRegs = 24;  

#elif defined(USE_AVX2)
using vec_t = __m256i;
#define vec_load(a) _mm256_load_si256(a)
#define vec_store(a, b) _mm256_store_si256(a, b)
#define vec_add_16(a, b) _mm256_add_epi16(a, b)
#define vec_sub_16(a, b) _mm256_sub_epi16(a, b)
#define vec_mulhi_16(a, b) _mm256_mulhi_epi16(a, b)
#define vec_set_16(a) _mm256_set1_epi16(a)
#define vec_max_16(a, b) _mm256_max_epi16(a, b)
#define vec_min_16(a, b) _mm256_min_epi16(a, b)
#define vec_slli_16(a, b) _mm256_slli_epi16(a, b)
#define vec_packus_16(a, b) _mm256_packus_epi16(a, b)
#define vec_zero() _mm256_setzero_si256()
static constexpr IndexType kNumRegs = 12;  

#elif defined(USE_SSE2)
using vec_t = __m128i;
#define vec_load(a) (*(a))
#define vec_store(a, b) *(a) = (b)
#define vec_add_16(a, b) _mm_add_epi16(a, b)
#define vec_sub_16(a, b) _mm_sub_epi16(a, b)
#define vec_mulhi_16(a, b) _mm_mulhi_epi16(a, b)
#define vec_set_16(a) _mm_set1_epi16(a)
#define vec_max_16(a, b) _mm_max_epi16(a, b)
#define vec_min_16(a, b) _mm_min_epi16(a, b)
#define vec_slli_16(a, b) _mm_slli_epi16(a, b)
#define vec_packus_16(a, b) _mm_packus_epi16(a, b)
#define vec_zero() _mm_setzero_si128()
static constexpr IndexType kNumRegs = Is64Bit ? 16 : 8;

#elif defined(USE_MMX)
using vec_t = __m64;
#define vec_load(a) (*(a))
#define vec_store(a, b) *(a) = (b)
#define vec_add_16(a, b) _mm_add_pi16(a, b)
#define vec_sub_16(a, b) _mm_sub_pi16(a, b)
#define vec_zero() _mm_setzero_si64()
static constexpr IndexType kNumRegs = 8;

#elif defined(USE_NEON)
using vec_t = int16x8_t;
#define vec_load(a) (*(a))
#define vec_store(a, b) *(a) = (b)
#define vec_add_16(a, b) vaddq_s16(a, b)
#define vec_sub_16(a, b) vsubq_s16(a, b)
#define vec_mulhi_16(a, b) vqdmulhq_s16(a, b)
#define vec_set_16(a) vdupq_n_s16(a)
#define vec_max_16(a, b) vmaxq_s16(a, b)
#define vec_min_16(a, b) vminq_s16(a, b)
#define vec_slli_16(a, b) vshlq_s16(a, vec_set_16(b))
#define vec_packus_16(a, b) reinterpret_cast<vec_t>(vcombine_u8(vqmovun_s16(a), vqmovun_s16(b)))
#define vec_zero() \
	vec_t { 0 }
static constexpr IndexType kNumRegs = 16;

#else
#undef VECTOR

#endif



constexpr IndexType MaxChunkSize = 16;



#if defined(CYF_GP8) && defined(CYF_THREAT_I8) && !defined(CYF_NO_FT_I8)


#define CYF_FT_I8 1
inline const std::int8_t*& cyf_ft_i8() { static const std::int8_t* p = nullptr; return p; }
#endif
class FeatureTransformer {
   private:
	
	
	static constexpr IndexType kHalfDimensions = kTransformedFeatureDimensions;

#if defined(VECTOR)
	
	
	
#endif

   public:
	
	
	using OutputType = TransformedFeatureType;
	using BiasType   = std::int16_t;
	using WeightType = std::int16_t;

	
	
	static constexpr IndexType kInputDimensions  = RawFeatures::kDimensions;
	
#if defined(CYF_GP8)
	static IndexType stored_rows() {
		return Cyfamate::Gp8::hm ? static_cast<IndexType>(Cyfamate::Gp8::kHmRows) : kInputDimensions;
	}
#else
	static constexpr IndexType stored_rows() { return kInputDimensions; }
#endif
#if defined(USE_ELEMENT_WISE_MULTIPLY)
	static constexpr IndexType kOutputDimensions = kHalfDimensions;
#else
	static constexpr IndexType kOutputDimensions = kHalfDimensions * 2;
#endif

	
	
	static constexpr std::size_t kBufferSize = kOutputDimensions * sizeof(OutputType);

	
	
	static inline bool read_at_internal_scale_ = false;
#if defined(CYF_GP8)
	
	static inline bool read_threat_block_ = false;
#if defined(CYF_THREAT)
	using ThreatList = Cyfamate::Gp8::Threat::List;
#endif
#endif 

	
	
	static constexpr std::uint32_t GetHashValue() {
#if defined(SFNNwoPSQT)
		
		return 0x5f134ab8u;
#else
		return RawFeatures::kHashValue ^ kOutputDimensions;
#endif
	}

	
	
	static std::string GetStructureString() {
		return RawFeatures::GetName() + "[" + std::to_string(kInputDimensions) + "->"
		       + std::to_string(kHalfDimensions) + "x2]";
	}

	
	
	Tools::Result ReadParameters(std::istream& stream) {
#if defined(USE_ELEMENT_WISE_MULTIPLY)
		read_leb_128<BiasType>(stream, biases_, kHalfDimensions);
		read_leb_128<WeightType>(stream, weights_, std::size_t(kHalfDimensions) * stored_rows());
		
		
		std::fill(weights_ + std::size_t(kHalfDimensions) * stored_rows(),
		          weights_ + std::size_t(kHalfDimensions) * kInputDimensions, WeightType(0));

#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
		permute_weights(inverse_order_packs);
#endif
		
		
#if defined(CYF_HALF_ACC)
		if (read_at_internal_scale_) {   
			for (std::size_t i = 0; i < std::size_t(stored_rows()) * kHalfDimensions; ++i)
				if (weights_[i] & 1) { sync_cout << "info string CYF_HALF_ACC: internal-scale FT with odd weights" << sync_endl; return Tools::ResultCode::FileMismatch; }
			for (IndexType i = 0; i < kHalfDimensions; ++i)
				if (biases_[i] & 1) { sync_cout << "info string CYF_HALF_ACC: internal-scale FT with odd biases" << sync_endl; return Tools::ResultCode::FileMismatch; }
			scale_weights(false);
		}
#else
		if (!read_at_internal_scale_)
			scale_weights(true);
#endif
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
		
		
		
		
		if (read_threat_block_) {
			const auto profile = read_little_endian<std::uint32_t>(stream);
			if (profile != 3u) {
				sync_cout << "info string NNUE threat block profile " << profile << " is not step-attacker (3)" << sync_endl;
				return Tools::ResultCode::FileMismatch;
			}
			constexpr std::size_t kThreatCount = std::size_t(Cyfamate::Gp8::Threat::kDims) * kHalfDimensions;
			std::vector<std::int8_t> raw(kThreatCount);
			stream.read(reinterpret_cast<char*>(raw.data()), std::streamsize(kThreatCount));
#if defined(CYF_THREAT_I8)
			std::memcpy(threat_weights_, raw.data(), kThreatCount);   
#else
			for (std::size_t i = 0; i < kThreatCount; ++i)
#if defined(CYF_HALF_ACC)
				threat_weights_[i] = WeightType(int(raw[i]));   
#else
				threat_weights_[i] = WeightType(int(raw[i]) * 2);
#endif
#endif
#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
			permute_threat_rows();
#endif
		}
#endif
#endif 
#if defined(USE_FINNY_TABLES)
		if (!stream.fail())
			finny_generation_ = next_finny_generation();
#endif
#else
		for (std::size_t i = 0; i < kHalfDimensions; ++i) biases_[i] = read_little_endian<BiasType>(stream);
		for (std::size_t i = 0; i < kHalfDimensions * kInputDimensions; ++i)
			weights_[i] = read_little_endian<WeightType>(stream);
#if defined(USE_FINNY_TABLES)
		if (!stream.fail())
			finny_generation_ = next_finny_generation();
#endif
#endif
		return !stream.fail() ? Tools::ResultCode::Ok : Tools::ResultCode::FileReadError;
	}

	
	
	bool WriteParameters(std::ostream& stream) const {
#if defined(CYF_GP8)
        
        const auto arch = Cyfamate::Gp8::parse_arch(Cyfamate::Gp8::arch_string);
        if (!arch.error.empty()) return false;
        std::vector<uint8_t> encoded;
        auto flush_leb = [&]() {
            if (encoded.size() > UINT32_MAX) return false;
            const uint32_t size = static_cast<uint32_t>(encoded.size());
            stream.write(Leb128MagicString, Leb128MagicStringSize);
            stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
            stream.write(reinterpret_cast<const char*>(encoded.data()), size);
            encoded.clear();
            return bool(stream);
        };
        auto write_row = [&](const auto* source, bool internal, bool narrow) {
            using T = std::remove_cv_t<std::remove_pointer_t<decltype(source)>>;
            alignas(64) T row[kHalfDimensions];
            std::memcpy(row, source, sizeof(row));
#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT) && defined(USE_AVX2)
#if defined(USE_AVX512)
            constexpr IndexType di = 16;
#else
            constexpr IndexType di = 8;
#endif
            auto* packed = reinterpret_cast<uint64_t*>(row);
            
            
            uint64_t permutation[di];
            for (IndexType j = 0; j < di; ++j) permutation[j] = j;
            inverse_order_packs(permutation);
            for (IndexType i = 0; i < kHalfDimensions * sizeof(T) / sizeof(uint64_t); i += di) {
                uint64_t scratch[di];
                for (IndexType j = 0; j < di; ++j) scratch[permutation[j]] = packed[i + j];
                std::memcpy(packed + i, scratch, sizeof(scratch));
            }
#endif
            for (auto& v : row) {
                if (!internal) {
                    if (v % 2) return false;
                    v /= 2;
                }
                if (narrow) {
                    if (v < -128 || v > 127) return false;
                    const int8_t b = static_cast<int8_t>(v);
                    stream.write(reinterpret_cast<const char*>(&b), sizeof(b));
                } else {
                    int32_t value = v;
                    bool more;
                    do {
                        uint8_t byte = uint8_t(value & 0x7f);
                        value >>= 7;
                        more = !((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40)));
                        encoded.push_back(byte | (more ? 0x80 : 0));
                    } while (more);
                }
            }
            return bool(stream);
        };
        if (!write_row(biases_, arch.ft_scale_254, false)) return false;
        if (!flush_leb()) return false;
        for (IndexType j = 0; j < stored_rows(); ++j)
            if (!write_row(weights_ + std::size_t(j) * kHalfDimensions, arch.ft_scale_254, false)) return false;
        if (!flush_leb()) return false;
#if defined(CYF_THREAT)
        if (Cyfamate::Gp8::threat_enabled) {
            const uint32_t profile = 3;
            stream.write(reinterpret_cast<const char*>(&profile), sizeof(profile));
            for (IndexType j = 0; j < Cyfamate::Gp8::Threat::kDims; ++j)
#if defined(CYF_THREAT_I8)
            {   
                alignas(64) WeightType wide[kHalfDimensions];
                for (IndexType i = 0; i < kHalfDimensions; ++i)
                    wide[i] = WeightType(2 * int(threat_weights_[std::size_t(j) * kHalfDimensions + i]));
                if (!write_row(wide, false, true)) return false;
            }
#else
                if (!write_row(threat_weights_ + std::size_t(j) * kHalfDimensions, false, true)) return false;
#endif
        }
#endif
#else
		stream.write(reinterpret_cast<const char*>(biases_), kHalfDimensions * sizeof(BiasType));
		stream.write(reinterpret_cast<const char*>(weights_), kHalfDimensions * kInputDimensions * sizeof(WeightType));
#endif 
		return !stream.fail();
	}

	
	
	bool UpdateAccumulatorIfPossible(const Position& pos) const {
		
		
		if (pos.square<KING>(BLACK) == SQ_NB || pos.square<KING>(WHITE) == SQ_NB)
			return false;
		const auto now = pos.state();
		if (now->accumulator.computed_accumulation) {
			return true;
		}
		const auto prev = now->previous;
		if (prev && prev->accumulator.computed_accumulation) {
			update_accumulator(pos);
			return true;
		}
#if defined(VECTOR) && !defined(CYF_NO_ACC_GRANDPARENT)
		
		
		
		if (prev && prev->previous && prev->previous->accumulator.computed_accumulation
		    && update_accumulator_from_grandparent(pos))
			return true;
#endif
		return false;
	}

#if defined(VECTOR) && !defined(CYF_NO_ACC_GRANDPARENT)
	
	
	
	
	bool update_accumulator_from_grandparent(const Position& pos) const {
		static_assert(std::is_same_v<RawFeatures, Features::FeatureSet<Features::HalfKA2<Features::Side::kFriend>>>);
		static_assert(kRefreshTriggers.size() == 1);
		const StateInfo* s2 = pos.state();
		const StateInfo* s1 = s2->previous;
		const StateInfo* s0 = s1->previous;
		const Eval::DirtyPiece* plies[2] = { &s1->dirtyPiece, &s2->dirtyPiece };
		
		
		bool own_king_moved[COLOR_NB] = { false, false };
		for (const auto* dp : plies) {
			if (dp->dirty_num <= 0) {
#if defined(CYF_ACC_STATS)
				++acc_stats().bail_null;
#endif
				return false;
			}
			if (dp->pieceNo[0] >= PIECE_NUMBER_KING)
				own_king_moved[dp->pieceNo[0] - PIECE_NUMBER_KING] = true;
		}
		if (own_king_moved[BLACK] && own_king_moved[WHITE]) {
#if defined(CYF_ACC_STATS)
			++acc_stats().bail_king;
#endif
			return false;
		}
		auto& accumulator = pos.state()->accumulator;
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
		
		ThreatList t_removed[2], t_added[2];
		threat_changes(pos, plies, 2, t_removed, t_added);
		prefetch_threat_rows(t_removed, t_added);
#endif
#endif 
#if defined(CYF_ACC_PARENT_FILL)   
		
		
		
		
		if (!own_king_moved[BLACK] && !own_king_moved[WHITE]) {
			auto& parent = const_cast<StateInfo*>(s1)->accumulator;
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
				const BonaPiece* pieces = perspective == BLACK ? pos.eval_list()->piece_list_fb()
				                                               : pos.eval_list()->piece_list_fw();
				Square sq_k = static_cast<Square>(
					(pieces[PIECE_NUMBER_KING + perspective] - Eval::f_king) % SQ_NB);
				const bool mirrored = Features::HalfKA2<Features::Side::kFriend>::MirrorKing(sq_k);
				Features::IndexList removed[2], added[2];
				for (int k = 0; k < 2; ++k)
					for (int i = 0; i < plies[k]->dirty_num; ++i) {
						removed[k].push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
							sq_k, static_cast<BonaPiece>(plies[k]->changed_piece[i].old_piece.from[perspective]), mirrored));
						added[k].push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
							sq_k, static_cast<BonaPiece>(plies[k]->changed_piece[i].new_piece.from[perspective]), mirrored));
					}
				update_accumulator_tiled(
					s0->accumulator.accumulation[perspective][0], parent.accumulation[perspective][0],
					[&](vec_t* acc, IndexType tile_offset) {
						for (const auto index : removed[0]) sub_weight_from_tile(acc, index, tile_offset);
						for (const auto index : added[0]) add_weight_to_tile(acc, index, tile_offset);
					});
				update_accumulator_tiled(
					parent.accumulation[perspective][0], accumulator.accumulation[perspective][0],
					[&](vec_t* acc, IndexType tile_offset) {
						for (const auto index : removed[1]) sub_weight_from_tile(acc, index, tile_offset);
						for (const auto index : added[1]) add_weight_to_tile(acc, index, tile_offset);
					});
			}
			parent.computed_accumulation = true;
			parent.computed_score = false;
			accumulator.computed_accumulation = true;
			accumulator.computed_score = false;
			return true;
		}
#endif
		for (int c = 0; c < COLOR_NB; ++c) {
			const Color perspective = static_cast<Color>(c);
			if (own_king_moved[perspective]) {
				Features::IndexList active;
				Features::HalfKA2<Features::Side::kFriend>::AppendActiveIndices(pos, perspective, &active);
#if defined(CYF_GP8)
#if defined(CYF_THREAT_DERIVE)
				derive_reset(pos, perspective, 2, s0->accumulator.accumulation[perspective][0], active,
				             accumulator.accumulation[perspective][0], t_removed[perspective], t_added[perspective]);
				continue;   
#endif
#if defined(CYF_THREAT)
				ThreatList threat_active;
				collect_threat_active(pos, perspective, threat_active);
#endif
#endif 
#if defined(USE_FINNY_TABLES)
				if constexpr (kUseFinnyTables) {
					auto& cache = finny_cache();
					const Square bucket = finny_bucket_square(pos, kRefreshTriggers[0], perspective);
					refresh_accumulator_using_finny_entry(
#if defined(CYF_GP8)
						accumulator.accumulation[perspective][0], cache.entries[0][perspective][bucket], active, 0
#if defined(CYF_THREAT)
						, threat_active
#endif
					);
#else
						accumulator.accumulation[perspective][0], cache.entries[0][perspective][bucket], active, 0);
#endif 
				} else
#endif
#if defined(CYF_GP8)
				refresh_accumulator_from_scratch(accumulator.accumulation[perspective][0], active, 0
#if defined(CYF_THREAT)
					, &threat_active
#endif
				);
#else
				refresh_accumulator_from_scratch(accumulator.accumulation[perspective][0], active, 0);
#endif 
				continue;
			}
			const BonaPiece* pieces = perspective == BLACK ? pos.eval_list()->piece_list_fb()
			                                               : pos.eval_list()->piece_list_fw();
			Square sq_k = static_cast<Square>(
				(pieces[PIECE_NUMBER_KING + perspective] - Eval::f_king) % SQ_NB);
			const bool mirrored = Features::HalfKA2<Features::Side::kFriend>::MirrorKing(sq_k);
			Features::IndexList removed, added;
			for (const auto* dp : plies)
				for (int i = 0; i < dp->dirty_num; ++i) {
					removed.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
						sq_k, static_cast<BonaPiece>(dp->changed_piece[i].old_piece.from[perspective]), mirrored));
					added.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
						sq_k, static_cast<BonaPiece>(dp->changed_piece[i].new_piece.from[perspective]), mirrored));
				}
			update_accumulator_tiled(
				s0->accumulator.accumulation[perspective][0], accumulator.accumulation[perspective][0],
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : removed) sub_weight_from_tile(acc, index, tile_offset);
					for (const auto index : added) add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
					for (int k = 0; k < t_removed[perspective].n; ++k) sub_threat_from_tile(acc, t_removed[perspective].v[k], tile_offset);
					for (int k = 0; k < t_added[perspective].n; ++k) add_threat_to_tile(acc, t_added[perspective].v[k], tile_offset);
#endif
#endif 
				});
#if defined(CYF_ACC_VERIFY)
			{
				Features::IndexList active[2];
				RawFeatures::AppendActiveIndices(pos, kRefreshTriggers[0], active);
				alignas(kCacheLineSize) BiasType check[kHalfDimensions];
				refresh_accumulator_from_scratch(check, active[perspective], 0);
				if (std::memcmp(check, accumulator.accumulation[perspective][0], sizeof(check)) != 0) {
					std::cerr << "CYF_ACC_VERIFY mismatch at " << pos.sfen() << std::endl;
					std::abort();
				}
				++acc_verify_count();
			}
#endif
		}
		accumulator.computed_accumulation = true;
		accumulator.computed_score = false;
		return true;
	}
#if defined(CYF_ACC_VERIFY)
public:
	static std::uint64_t& acc_verify_count() { static std::uint64_t n = 0; return n; }
	struct AccVerifyReport { ~AccVerifyReport() {
		if (const char* p = std::getenv("CYF_ACC_STATS_FILE")) { std::ofstream f(p, std::ios::app);
			f << "{\"grandparent_updates_verified\":" << acc_verify_count() << "}\n"; } } };
	static inline AccVerifyReport acc_verify_report_{};
#endif
#endif

#if defined(CYF_ACC_STATS)
	
	
	
	struct AccStats {
		std::atomic<std::uint64_t> forced{0}, computed{0}, from_parent{0}, refresh{0},
		    ancestor_2{0}, ancestor_3{0}, ancestor_4plus{0}, no_ancestor{0}, bail_king{0}, bail_null{0};
		~AccStats() {
			if (const char* path = std::getenv("CYF_ACC_STATS_FILE")) {
				std::ofstream f(path, std::ios::app);
				f << "{\"forced\":" << forced << ",\"computed\":" << computed << ",\"from_parent\":" << from_parent
				  << ",\"refresh\":" << refresh << ",\"ancestor_2\":" << ancestor_2 << ",\"ancestor_3\":" << ancestor_3
				  << ",\"ancestor_4plus\":" << ancestor_4plus << ",\"no_ancestor\":" << no_ancestor
				  << ",\"bail_king\":" << bail_king << ",\"bail_null\":" << bail_null << "}\n";
			}
		}
	};
	static AccStats& acc_stats() { static AccStats s; return s; }
#endif

#if defined(CYF_EVAL_PROFILE)
	
	
#if defined(CYF_GP8)
	struct EvalProf { uint64_t n[5]{}, cyc[5]{}, transform_n = 0, transform_cyc = 0;
	                  
	                  
	                  uint64_t threat_enum_n = 0, threat_enum_cyc = 0, threat_rows = 0;
	                  
	                  
	                  uint64_t threat_reset_n = 0, threat_collect_cyc = 0, threat_finny_rows = 0; };
#else
	struct EvalProf { uint64_t n[5]{}, cyc[5]{}, transform_n = 0, transform_cyc = 0; };
#endif 
	static inline EvalProf& eval_prof() { static EvalProf p; return p; }
#endif
#if defined(CYF_GPU_ACC)
    void PrepareGpuAccumulator() const { CyfGpuAcc::service().init(biases_,weights_,sizeof(weights_),finny_generation_); }
#endif
#if defined(CYF_DEMAND_TRACE)
    std::uint64_t DemandModelGeneration() const { return finny_generation_; }
#endif
#if defined(CYF_GPU_POOL)
    
    const BiasType* PoolBiases() const { return biases_; }
    const WeightType* PoolWeights() const { return weights_; }
    static constexpr IndexType PoolFeatures() { return kInputDimensions; }
    std::uint64_t PoolGeneration() const { return finny_generation_; }
#endif
#if defined(CYF_BLEND_INCREMENTAL_ACC)
    
    
    void MaintainIncrementalAccumulator(const Position& pos) const {
        const auto now = pos.state();
        if (now->accumulator.computed_accumulation) return;
        const auto prev = now->previous;
        if (!prev || now->dirtyPiece.dirty_num <= 0
            || now->dirtyPiece.pieceNo[0] >= PIECE_NUMBER_KING) return;
        if (!prev->accumulator.computed_accumulation
            && (prev->dirtyPiece.dirty_num <= 0
                || prev->dirtyPiece.pieceNo[0] >= PIECE_NUMBER_KING)) return;
        UpdateAccumulatorIfPossible(pos);
    }
#endif
#if defined(CYF_DEMAND_TRACE)
	
	void demand_trace_refresh(const Position& pos, bool forced) const {
		static_assert(kHalfDimensions == 1024 && kRefreshTriggers.size() == 1);
		static_assert(sizeof(pos.state()->accumulator.accumulation) == 4096);
		auto& s = CyfDemand::state();
		CyfDemand::Rec r{};
		const auto now = pos.state();
		r.search = s.search; r.seq = s.seq++; r.acc_req = s.acc_req;
		r.t_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - s.t0).count());
		r.key = uint64_t(now->key()); r.parent_key = now->previous ? uint64_t(now->previous->key()) : 0;
		r.game_ply = uint16_t(pos.game_ply()); r.forced = forced; r.stm = pos.side_to_move() == BLACK ? 0 : 1;
		int dist = 255, kings = 0, nulls = 0;
		{
			const StateInfo* st = now;
			for (int k = 0; st && k < 254; st = st->previous, ++k) {
				if (k > 0 && st->accumulator.computed_accumulation) { dist = k; break; }
				
				if (st->dirtyPiece.dirty_num <= 0) ++nulls;
				else if (st->dirtyPiece.pieceNo[0] >= PIECE_NUMBER_KING) ++kings;
			}
		}
		r.dist = uint8_t(dist); r.king_plies = uint8_t(std::min(kings, 255)); r.null_plies = uint8_t(std::min(nulls, 255));
		Features::IndexList lists[2];
		RawFeatures::AppendActiveIndices(pos, kRefreshTriggers[0], lists);
		std::fill(r.ix, r.ix + 80, -1);
		for (int c = 0; c < 2; ++c) {
			if (lists[c].size() > 40) std::abort();
			int j = 0; for (auto id : lists[c]) r.ix[c * 40 + j++] = int32_t(id);
			std::sort(r.ix + c * 40, r.ix + c * 40 + j);
		}
		const uint64_t t0 = __rdtsc();
		refresh_accumulator(pos);
		const uint64_t t1 = __rdtsc();
		r.refresh_tsc = uint32_t(std::min<uint64_t>(t1 - t0, 0xffffffffull));
		r.state_hash = CyfDemand::fnv64(now->accumulator.accumulation, sizeof(now->accumulator.accumulation));
		if (forced) ++s.n_forced; else ++s.n_refresh;
		s.refresh_tsc += t1 - t0;
		CyfDemand::push(r);
	}
#endif
	void EnsureAccumulator(const Position& pos, bool refresh) const {
#if defined(CYF_DEMAND_TRACE)
		const bool dt_on = CyfDemand::on();
		int dt_case = 0;   
		if (dt_on) {
			++CyfDemand::state().acc_req;
			const auto now_ = pos.state();
			dt_case = now_->accumulator.computed_accumulation ? 0
			        : (now_->previous && now_->previous->accumulator.computed_accumulation) ? 1 : 2;
		}
#endif
#if defined(CYF_EVAL_PROFILE)
		const auto st_ = pos.state();
		int kase = refresh ? 4 : st_->accumulator.computed_accumulation ? 0
		         : (st_->previous && st_->previous->accumulator.computed_accumulation) ? 1 : 2;
		const uint64_t t0_ = __rdtsc();
		struct Done { int& k; uint64_t t0; ~Done() { auto& p = eval_prof(); ++p.n[k]; p.cyc[k] += __rdtsc() - t0; } } done_{kase, t0_};
#endif
#if defined(CYF_ACC_STATS)
		{
			auto& s = acc_stats();
			const auto now = pos.state();
			if (refresh) ++s.forced;
			else if (now->accumulator.computed_accumulation) ++s.computed;
			else if (now->previous && now->previous->accumulator.computed_accumulation) ++s.from_parent;
			else {
				++s.refresh;
				int k = 1;
				const StateInfo* st = now->previous;
				while (st && !st->accumulator.computed_accumulation) { st = st->previous; ++k; }
				if (!st) ++s.no_ancestor;
				else if (k == 2) ++s.ancestor_2;
				else if (k == 3) ++s.ancestor_3;
				else ++s.ancestor_4plus;
			}
		}
#endif
		if (refresh || !UpdateAccumulatorIfPossible(pos)) {
#if defined(CYF_EVAL_PROFILE)
			if (kase != 4) kase = 3;   
#endif

#if defined(CYF_GPU_ACC)
            auto& gpu=CyfGpuAcc::service();
            if(gpu.mode && !refresh){
                static_assert(kHalfDimensions==1024 && kRefreshTriggers.size()==1);
                static_assert(sizeof(pos.state()->accumulator.accumulation)==4096);
                gpu.init(biases_,weights_,sizeof(weights_),finny_generation_);
                Features::IndexList lists[2];RawFeatures::AppendActiveIndices(pos,kRefreshTriggers[0],lists);
                int ix[80];std::fill(ix,ix+80,-1);
                for(int c=0;c<2;++c){if(lists[c].size()>40)std::abort();int j=0;for(auto id:lists[c])ix[c*40+j++]=id;std::sort(ix+c*40,ix+c*40+j);}
                auto& acc=pos.state()->accumulator;
                if(gpu.get(ix,reinterpret_cast<short*>(acc.accumulation))){
#if defined(CYF_GPU_ACC_VERIFY)
                    auto saved=acc;
                    refresh_accumulator(pos);
                    if(std::memcmp(saved.accumulation,acc.accumulation,4096)){std::fprintf(stderr,"GPU accumulator mismatch\n");std::abort();}
                    ++gpu.verified;
#endif
                    acc.computed_accumulation=true;acc.computed_score=false;return;
                }
                refresh_accumulator(pos);
                if(gpu.mode==2)gpu.put(ix,reinterpret_cast<const short*>(acc.accumulation));
                return;
            }
#endif
#if defined(CYF_DEMAND_TRACE)
			if (dt_on) { demand_trace_refresh(pos, refresh); return; }
#endif
			refresh_accumulator(pos);
		}
#if defined(CYF_DEMAND_TRACE)
		else if (dt_on) {
			auto& s = CyfDemand::state();
			if (dt_case == 0) ++s.n_computed; else if (dt_case == 1) ++s.n_parent; else ++s.n_grandparent;
		}
#endif
	}

	
	
	void Transform(const Position& pos, OutputType* output, bool refresh) const {
		EnsureAccumulator(pos, refresh);
#if defined(CYF_VERIFY_ACC)
        auto saved = pos.state()->accumulator;
        refresh_accumulator(pos);
        if (std::memcmp(saved.accumulation, pos.state()->accumulator.accumulation,
                        sizeof(saved.accumulation)) != 0) {
            sync_cout << "info string accumulator mismatch sfen " << pos.sfen() << sync_endl;
            std::abort();
        }
        pos.state()->accumulator = saved;
#endif
#if defined(CYF_EVAL_PROFILE)
		struct TDone { uint64_t t0 = __rdtsc(); ~TDone() { auto& p = eval_prof(); ++p.transform_n; p.transform_cyc += __rdtsc() - t0; } } tdone_;
#endif
		const auto& accumulation = pos.state()->accumulator.accumulation;

#if defined(USE_ELEMENT_WISE_MULTIPLY)

#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
			
#if defined(USE_AVX512)
			constexpr IndexType OutputChunkSize = 64;
#else
			constexpr IndexType OutputChunkSize = kSimdWidth;
#endif
		static_assert((kHalfDimensions / 2) % OutputChunkSize == 0);
		constexpr IndexType NumOutputChunks = kHalfDimensions / 2 / OutputChunkSize;

		vec_t Zero = vec_zero();
#if defined(CYF_HALF_ACC)
		vec_t One = vec_set_16(127);   
#else
		vec_t One = vec_set_16(127 * 2);
#endif

		const Color perspectives[2] = { pos.side_to_move(), ~pos.side_to_move() };
		for (IndexType p = 0; p < 2; ++p) {
			const IndexType offset = (kHalfDimensions / 2) * p;

			const vec_t* in0 = reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][0][0]));
			const vec_t* in1 = reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][0][kHalfDimensions / 2]));
			vec_t* out = reinterpret_cast<vec_t*>(output + offset);

			constexpr int shift =
#if defined(USE_SSE2)
				7;
#else
				6;
#endif

			for (IndexType j = 0; j < NumOutputChunks; ++j)
			{
#if defined(CYF_HALF_ACC)
				
				const vec_t sum0a =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 0], One), Zero), shift + 1);
				const vec_t sum0b =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 1], One), Zero), shift + 1);
				const vec_t h1a = vec_min_16(in1[j * 2 + 0], One), h1b = vec_min_16(in1[j * 2 + 1], One);
				const vec_t sum1a = vec_add_16(h1a, h1a);
				const vec_t sum1b = vec_add_16(h1b, h1b);
#else
				const vec_t sum0a =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 0], One), Zero), shift);
				const vec_t sum0b =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 1], One), Zero), shift);
				const vec_t sum1a = vec_min_16(in1[j * 2 + 0], One);
				const vec_t sum1b = vec_min_16(in1[j * 2 + 1], One);
#endif

				const vec_t pa = vec_mulhi_16(sum0a, sum1a);
				const vec_t pb = vec_mulhi_16(sum0b, sum1b);

				out[j] = vec_packus_16(pa, pb);
			}

		}

#else
		constexpr int shift =
#if defined(VECTOR) && !defined(USE_SSE2)
			6;
#else
			7;
#endif

		const Color perspectives[2] = { pos.side_to_move(), ~pos.side_to_move() };
		for (IndexType p = 0; p < 2; ++p) {
			const IndexType offset = (kHalfDimensions / 2) * p;

			for (IndexType j = 0; j < kHalfDimensions / 2; ++j)
			{
				BiasType sum0 = accumulation[perspectives[p]][0][j];
				BiasType sum1 = accumulation[perspectives[p]][0][j + kHalfDimensions / 2];
#if defined(CYF_HALF_ACC)
				sum0 = BiasType(2 * std::clamp<int>(sum0, 0, 127));   
				sum1 = BiasType(2 * std::clamp<int>(sum1, 0, 127));
#else
				sum0 = std::clamp<BiasType>(sum0, 0, 127 * 2);
				sum1 = std::clamp<BiasType>(sum1, 0, 127 * 2);
#endif
				const int product = (int(sum0) << shift) * int(sum1);
				const int value = product >> 16;
				output[offset + j] = static_cast<OutputType>(std::clamp(value, 0, 255));
			}

		}
#endif

#else

		
		
		

#if defined(USE_AVX512)
		constexpr IndexType kNumChunks = kHalfDimensions / (kSimdWidth * 2);
		static_assert(kHalfDimensions % (kSimdWidth * 2) == 0);
		const __m512i kControl = _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7);
		const __m512i kZero    = _mm512_setzero_si512();

#elif defined(USE_AVX2)
		constexpr IndexType kNumChunks = kHalfDimensions / kSimdWidth;
		constexpr int       kControl   = 0b11011000;
		const __m256i       kZero      = _mm256_setzero_si256();

#elif defined(USE_SSE2)
		constexpr IndexType kNumChunks = kHalfDimensions / kSimdWidth;
#if defined(USE_SSE41)
		const __m128i kZero = _mm_setzero_si128();
#else  
		const __m128i k0x80s = _mm_set1_epi8(-128);
#endif

#elif defined(USE_MMX)
		
		constexpr IndexType kNumChunks = kHalfDimensions / kSimdWidth;
		const __m64         k0x80s     = _mm_set1_pi8(-128);

#elif defined(USE_NEON)
		constexpr IndexType kNumChunks = kHalfDimensions / (kSimdWidth / 2);
		const int8x8_t      kZero      = {0};
#endif
		const Color perspectives[2] = {pos.side_to_move(), ~pos.side_to_move()};
		for (IndexType p = 0; p < 2; ++p) {
			const IndexType offset = kHalfDimensions * p;
#if defined(USE_AVX512)
			auto out = reinterpret_cast<__m512i*>(&output[offset]);
			for (IndexType j = 0; j < kNumChunks; ++j) {
				__m512i sum0 =
				    _mm512_load_si512(&reinterpret_cast<const __m512i*>(accumulation[perspectives[p]][0])[j * 2 + 0]);
				__m512i sum1 =
				    _mm512_load_si512(&reinterpret_cast<const __m512i*>(accumulation[perspectives[p]][0])[j * 2 + 1]);
				for (IndexType i = 1; i < kRefreshTriggers.size(); ++i) {
					sum0 = _mm512_add_epi16(
					    sum0,
					    reinterpret_cast<const __m512i*>(accumulation[perspectives[p]][i])[j * 2 + 0]);
					sum1 = _mm512_add_epi16(
					    sum1,
					    reinterpret_cast<const __m512i*>(accumulation[perspectives[p]][i])[j * 2 + 1]);
				}
				_mm512_store_si512(&out[j], _mm512_permutexvar_epi64(
								 kControl, _mm512_max_epi8(_mm512_packs_epi16(sum0, sum1), kZero)));
			}

#elif defined(USE_AVX2)
			auto out = reinterpret_cast<__m256i*>(&output[offset]);
			for (IndexType j = 0; j < kNumChunks; ++j) {
					__m256i sum0 =
					    _mm256_loadu_si256(&reinterpret_cast<const __m256i*>(accumulation[perspectives[p]][0])[j * 2 + 0]);
					__m256i sum1 =
					    _mm256_loadu_si256(&reinterpret_cast<const __m256i*>(accumulation[perspectives[p]][0])[j * 2 + 1]);
					for (IndexType i = 1; i < kRefreshTriggers.size(); ++i) {
						sum0 = _mm256_add_epi16(
							sum0,
							_mm256_loadu_si256(&reinterpret_cast<const __m256i*>(accumulation[perspectives[p]][i])[j * 2 + 0]));
						sum1 = _mm256_add_epi16(
							sum1,
							_mm256_loadu_si256(&reinterpret_cast<const __m256i*>(accumulation[perspectives[p]][i])[j * 2 + 1]));
					}
					_mm256_store_si256(&out[j], _mm256_permute4x64_epi64(
									 _mm256_max_epi8(_mm256_packs_epi16(sum0, sum1), kZero), kControl));
			}

#elif defined(USE_SSE2)
			auto out = reinterpret_cast<__m128i*>(&output[offset]);
			for (IndexType j = 0; j < kNumChunks; ++j) {
				__m128i sum0 =
				    _mm_load_si128(&reinterpret_cast<const __m128i*>(accumulation[perspectives[p]][0])[j * 2 + 0]);
				__m128i sum1 =
				    _mm_load_si128(&reinterpret_cast<const __m128i*>(accumulation[perspectives[p]][0])[j * 2 + 1]);
				for (IndexType i = 1; i < kRefreshTriggers.size(); ++i) {
					sum0 = _mm_add_epi16(sum0,
					                     reinterpret_cast<const __m128i*>(accumulation[perspectives[p]][i])[j * 2 + 0]);
					sum1 = _mm_add_epi16(sum1,
					                     reinterpret_cast<const __m128i*>(accumulation[perspectives[p]][i])[j * 2 + 1]);
				}

				const __m128i packedbytes = _mm_packs_epi16(sum0, sum1);
				_mm_store_si128(&out[j],
#if defined(USE_SSE41)
				                _mm_max_epi8(packedbytes, kZero)
#else  
				                _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s)
#endif
				);
			}

#elif defined(USE_MMX)
			
			auto out = reinterpret_cast<__m64*>(&output[offset]);
			for (IndexType j = 0; j < kNumChunks; ++j) {
				__m64       sum0 = *(&reinterpret_cast<const __m64*>(accumulation[perspectives[p]][0])[j * 2 + 0]);
				__m64       sum1 = *(&reinterpret_cast<const __m64*>(accumulation[perspectives[p]][0])[j * 2 + 1]);
				const __m64 packedbytes = _mm_packs_pi16(sum0, sum1);
				out[j]                  = _mm_subs_pi8(_mm_adds_pi8(packedbytes, k0x80s), k0x80s);
			}

#elif defined(USE_NEON)
			const auto out = reinterpret_cast<int8x8_t*>(&output[offset]);
			for (IndexType j = 0; j < kNumChunks; ++j) {
				int16x8_t sum = reinterpret_cast<const int16x8_t*>(accumulation[perspectives[p]][0])[j];
				for (IndexType i = 1; i < kRefreshTriggers.size(); ++i) {
					sum = vaddq_s16(sum, reinterpret_cast<const int16x8_t*>(accumulation[perspectives[p]][i])[j]);
				}
				out[j] = vmax_s8(vqmovn_s16(sum), kZero);
			}
#else
			for (IndexType j = 0; j < kHalfDimensions; ++j) {
				BiasType sum = accumulation[perspectives[p]][0][j];
				for (IndexType i = 1; i < kRefreshTriggers.size(); ++i) {
					sum += accumulation[perspectives[p]][i][j];
				}
				output[offset + j] = static_cast<OutputType>(std::clamp<int>(sum, 0, 127));
			}
#endif
		}
#if defined(USE_MMX)
		
		_mm_empty();
#endif
#endif
	}

   private:
	static void order_packs([[maybe_unused]] uint64_t* v) {
#if defined(USE_AVX512)  
		uint64_t tmp0 = v[4], tmp1 = v[5];
		v[4] = v[6], v[5] = v[7];
		v[6] = tmp0, v[7] = tmp1;
		tmp0 = v[8], tmp1 = v[9];
		v[8] = v[12], v[9] = v[13];
		v[12] = v[10], v[13] = v[11];
		v[10] = tmp0, v[11] = tmp1;
#elif defined(USE_AVX2)  
		uint64_t tmp0 = v[2], tmp1 = v[3];
		v[2] = v[4], v[3] = v[5];
		v[4] = tmp0, v[5] = tmp1;
#endif
	}

	static void inverse_order_packs([[maybe_unused]] uint64_t* v) {
#if defined(USE_AVX512)
		uint64_t tmp0 = v[2], tmp1 = v[3];
		v[2] = v[4], v[3] = v[5];
		v[4] = v[8], v[5] = v[9];
		v[8] = tmp0, v[9] = tmp1;
		tmp0 = v[6], tmp1 = v[7];
		v[6] = v[12], v[7] = v[13];
		v[12] = v[10], v[13] = v[11];
		v[10] = tmp0, v[11] = tmp1;
#elif defined(USE_AVX2)  
		uint64_t tmp0 = v[2], tmp1 = v[3];
		v[2] = v[4], v[3] = v[5];
		v[4] = tmp0, v[5] = tmp1;
#endif
	}

	void permute_weights([[maybe_unused]] void (*order_fn)(uint64_t*)) const {
#if defined(USE_AVX2)
#if defined(USE_AVX512)
		constexpr IndexType di = 16;
#else
		constexpr IndexType di = 8;
#endif
		uint64_t* b = reinterpret_cast<uint64_t*>(const_cast<BiasType*>(&biases_[0]));
		for (IndexType i = 0; i < kHalfDimensions * sizeof(BiasType) / sizeof(uint64_t); i += di)
			order_fn(&b[i]);

		for (IndexType j = 0; j < stored_rows(); ++j)
		{
			uint64_t* w =
				reinterpret_cast<uint64_t*>(const_cast<WeightType*>(&weights_[j * kHalfDimensions]));
			for (IndexType i = 0; i < kHalfDimensions * sizeof(WeightType) / sizeof(uint64_t);
					i += di)
				order_fn(&w[i]);
		}
#endif
	}

	inline void scale_weights(bool read) const {
		for (IndexType j = 0; j < stored_rows(); ++j)
		{
			WeightType* w = const_cast<WeightType*>(&weights_[j * kHalfDimensions]);
			for (IndexType i = 0; i < kHalfDimensions; ++i)
				w[i] = read ? w[i] * 2 : w[i] / 2;
		}

		BiasType* b = const_cast<BiasType*>(biases_);
		for (IndexType i = 0; i < kHalfDimensions; ++i)
			b[i] = read ? b[i] * 2 : b[i] / 2;
	}

#if defined(VECTOR)
	
	
	static constexpr IndexType kVectorHeight = sizeof(vec_t) / sizeof(BiasType);
	static_assert(kHalfDimensions % kVectorHeight == 0, "kVectorHeight must divide kHalfDimensions");
	static constexpr IndexType kNumVectorChunks = kHalfDimensions / kVectorHeight;
	static constexpr IndexType kTileRegs = [] {
		IndexType regs = std::min(kNumRegs, kNumVectorChunks);
		while (kNumVectorChunks % regs != 0)
			--regs;
		return regs;
	}();
	static constexpr IndexType kTileHeight = kTileRegs * kVectorHeight;

	template <typename ApplyChanges>
#if defined(CYF_GP8)
#if defined(CYF_THREAT_I8)
	__attribute__((flatten))   
#endif
#endif 
	void update_accumulator_tiled(
		const BiasType* source, BiasType* destination,
		ApplyChanges apply_changes) const {
		for (IndexType tile_offset = 0; tile_offset < kHalfDimensions; tile_offset += kTileHeight) {
			vec_t acc[kTileRegs];

			if (source) {
				const auto* source_tile = reinterpret_cast<const vec_t*>(source + tile_offset);
				for (IndexType k = 0; k < kTileRegs; ++k)
					acc[k] = vec_load(source_tile + k);
			} else {
				for (IndexType k = 0; k < kTileRegs; ++k)
					acc[k] = vec_zero();
			}

			apply_changes(acc, tile_offset);

			auto* destination_tile = reinterpret_cast<vec_t*>(destination + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k)
				vec_store(destination_tile + k, acc[k]);
		}
	}

#if defined(CYF_FT_I8)
public:
#if defined(CYF_FT_CLAMP8_TEST)
	void cyf_clamp8_test() {   
		for (std::size_t i = 0; i < std::size_t(kHalfDimensions) * kInputDimensions; ++i)
			weights_[i] = WeightType(2 * std::clamp(weights_[i] / 2, -128, 127));
		sync_cout << "info string FT-I8 test: FT clamped to int8" << sync_endl;
	}
#endif
	
	void cyf_build_ft_i8() const {
		const std::size_t N = std::size_t(kHalfDimensions) * kInputDimensions;
		static std::int8_t* own = nullptr;
		cyf_ft_i8() = nullptr;
		const char* e = std::getenv("CYF_FT_I8");
		if (e && e[0] == '0') { sync_cout << "info string FT-I8 off (env)" << sync_endl; return; }
		std::size_t bad = 0;
		for (std::size_t i = 0; i < N; ++i) {
			const int w = weights_[i];
#if defined(CYF_HALF_ACC)
			bad += (w < -128) | (w > 127);   
#else
			bad += (w & 1) | (w < -256) | (w > 254);
#endif
		}
		if (bad) { sync_cout << "info string FT-I8 off: " << bad << " weights need int16" << sync_endl; return; }
		if (!own) own = static_cast<std::int8_t*>(aligned_large_pages_alloc(N));
		if (!own) { sync_cout << "info string FT-I8 off: alloc failed" << sync_endl; return; }
#if defined(CYF_HALF_ACC)
		for (std::size_t i = 0; i < N; ++i) own[i] = std::int8_t(weights_[i]);
#else
		for (std::size_t i = 0; i < N; ++i) own[i] = std::int8_t(weights_[i] / 2);
#endif
		cyf_ft_i8() = own;
		sync_cout << "info string FT-I8 on (int8 FT rows)" << sync_endl;
	}
private:
#endif
	void add_weight_to_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
#if defined(CYF_FT_I8)
		if (const std::int8_t* w8 = cyf_ft_i8()) {
			const std::int8_t* c8 = w8 + std::size_t(kHalfDimensions) * index + tile_offset;
			for (IndexType k = 0; k < kTileRegs; ++k)
				acc[k] = vec_add_16(acc[k], threat_widen2(c8, k));
			return;
		}
#endif
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_add_16(acc[k], vec_load(column + k));
	}

	void sub_weight_from_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
#if defined(CYF_FT_I8)
		if (const std::int8_t* w8 = cyf_ft_i8()) {
			const std::int8_t* c8 = w8 + std::size_t(kHalfDimensions) * index + tile_offset;
			for (IndexType k = 0; k < kTileRegs; ++k)
				acc[k] = vec_sub_16(acc[k], threat_widen2(c8, k));
			return;
		}
#endif
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_sub_16(acc[k], vec_load(column + k));
	}
#if defined(CYF_GP8)

#if defined(CYF_THREAT)
	
	
#if defined(CYF_THREAT_I8)
	
#if defined(USE_AVX512)
	static vec_t threat_widen2(const std::int8_t* p, IndexType k) {
		const __m512i w = _mm512_cvtepi8_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(p) + k));
#if defined(CYF_HALF_ACC)
		return w;   
#else
		return _mm512_add_epi16(w, w);
#endif
	}
#else
	static vec_t threat_widen2(const std::int8_t* p, IndexType k) {
		const __m256i w = _mm256_cvtepi8_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(p) + k));
#if defined(CYF_HALF_ACC)
		return w;   
#else
		return _mm256_add_epi16(w, w);
#endif
	}
#endif
	void add_threat_to_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const std::int8_t* column = &threat_weights_[std::size_t(kHalfDimensions) * index + tile_offset];
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_add_16(acc[k], threat_widen2(column, k));
	}
	void sub_threat_from_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const std::int8_t* column = &threat_weights_[std::size_t(kHalfDimensions) * index + tile_offset];
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_sub_16(acc[k], threat_widen2(column, k));
	}
#else
	void add_threat_to_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&threat_weights_[std::size_t(kHalfDimensions) * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_add_16(acc[k], vec_load(column + k));
	}
	void sub_threat_from_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&threat_weights_[std::size_t(kHalfDimensions) * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_sub_16(acc[k], vec_load(column + k));
	}
#endif

	
	
	
	void prefetch_threat_rows(const ThreatList* removed, const ThreatList* added) const {


#if defined(CYF_THREAT_PREFETCH) && !defined(CYF_NO_THREAT_PREFETCH)
#if defined(CYF_THREAT_SUBPROF)
		const uint64_t tp0 = __rdtsc();
#endif
		constexpr std::size_t row_bytes = kHalfDimensions * sizeof(threat_weights_[0]);
		for (int c = 0; c < 2; ++c)
			for (const ThreatList* l : {&removed[c], &added[c]})
				for (int k = 0; k < l->n; ++k) {
					const char* row = reinterpret_cast<const char*>(&threat_weights_[std::size_t(kHalfDimensions) * l->v[k]]);
#if !defined(CYF_THREAT_PREFETCH_STRIDE)
#define CYF_THREAT_PREFETCH_STRIDE 64   
#endif
					for (std::size_t o = 0; o < row_bytes; o += CYF_THREAT_PREFETCH_STRIDE) _mm_prefetch(row + o, _MM_HINT_T0);
				}
#if defined(CYF_THREAT_SUBPROF)
		Cyfamate::Gp8::Threat::enum_prof().prefetch_cyc += __rdtsc() - tp0;
#endif
#else
		(void)removed; (void)added;
#endif
	}

	
	void permute_threat_rows() const {
#if defined(USE_AVX2)
#if defined(USE_AVX512)
		constexpr IndexType di = 16;
#else
		constexpr IndexType di = 8;
#endif
		for (IndexType j = 0; j < IndexType(Cyfamate::Gp8::Threat::kDims); ++j) {
#if defined(CYF_THREAT_I8)
			
			
			uint32_t* w = reinterpret_cast<uint32_t*>(const_cast<std::int8_t*>(&threat_weights_[std::size_t(j) * kHalfDimensions]));
			for (IndexType i = 0; i < kHalfDimensions / 4; i += di) {
				uint64_t tmp[di];
				for (IndexType q = 0; q < di; ++q) tmp[q] = w[i + q];
				inverse_order_packs(tmp);
				for (IndexType q = 0; q < di; ++q) w[i + q] = uint32_t(tmp[q]);
			}
#else
			uint64_t* w = reinterpret_cast<uint64_t*>(
				const_cast<WeightType*>(&threat_weights_[std::size_t(j) * kHalfDimensions]));
			for (IndexType i = 0; i < kHalfDimensions * sizeof(WeightType) / sizeof(uint64_t); i += di)
				inverse_order_packs(&w[i]);
#endif
		}
#endif
	}

	
	static void collect_threat_active(const Position& pos, Color perspective, ThreatList& out) {
		out.n = 0;
		if (!Cyfamate::Gp8::threat_enabled) return;
#if defined(CYF_THREAT_ENUM_V5)
		Cyfamate::Gp8::Threat::for_each_edge(pos, [&](const Cyfamate::Gp8::Threat::Edge& e) {
			const int idx = Cyfamate::Gp8::Threat::index_of(e, perspective);
			if (idx >= 0 && out.n < Cyfamate::Gp8::Threat::kMaxActive) out.v[out.n++] = std::uint32_t(idx);
		});
		std::sort(out.v, out.v + out.n);
#else
		
		Cyfamate::Gp8::Threat::all_indices(pos, perspective, out);
#endif
	}
	static void threat_list_diff(const ThreatList& old_list, const ThreatList& new_list, ThreatList& removed, ThreatList& added) {
		removed.n = 0; added.n = 0;
		int i = 0, j = 0;
		while (i < old_list.n && j < new_list.n) {
			if (old_list.v[i] == new_list.v[j]) { ++i; ++j; }
			else if (old_list.v[i] < new_list.v[j]) removed.v[removed.n++] = old_list.v[i++];
			else added.v[added.n++] = new_list.v[j++];
		}
		while (i < old_list.n) removed.v[removed.n++] = old_list.v[i++];
		while (j < new_list.n) added.v[added.n++] = new_list.v[j++];
	}
	
	static void threat_changes(const Position& pos, const Eval::DirtyPiece* const* plies, int nplies,
	                           ThreatList removed[2], ThreatList added[2]) {
		removed[0].n = removed[1].n = added[0].n = added[1].n = 0;
		if (!Cyfamate::Gp8::threat_enabled) return;
#if defined(CYF_EVAL_PROFILE)
		const uint64_t t0 = __rdtsc();
#endif
#if defined(CYF_THREAT_ENUM_V5)
		static thread_local std::vector<Cyfamate::Gp8::Threat::Edge> rem, add;
		Cyfamate::Gp8::Threat::changed_edges(pos, plies, nplies, rem, add);
#if defined(CYF_THREAT_SUBPROF)
		const uint64_t ti0 = __rdtsc();
#endif
		for (int c = 0; c < 2; ++c) {
			const Color p = Color(c);
			for (const auto& e : rem) {
				const int idx = Cyfamate::Gp8::Threat::index_of(e, p);
				if (idx >= 0 && removed[c].n < Cyfamate::Gp8::Threat::kMaxActive) removed[c].v[removed[c].n++] = std::uint32_t(idx);
			}
			for (const auto& e : add) {
				const int idx = Cyfamate::Gp8::Threat::index_of(e, p);
				if (idx >= 0 && added[c].n < Cyfamate::Gp8::Threat::kMaxActive) added[c].v[added[c].n++] = std::uint32_t(idx);
			}
		}
#if defined(CYF_THREAT_SUBPROF)
		Cyfamate::Gp8::Threat::enum_prof().index_cyc += __rdtsc() - ti0;
#endif
#else
		
#if defined(CYF_THREAT_ENUM_V8)
		Cyfamate::Gp8::Threat::changed_indices_v8(pos, plies, nplies, removed, added);   
#else
		Cyfamate::Gp8::Threat::changed_indices(pos, plies, nplies, removed, added);
#endif
#endif
#if defined(CYF_EVAL_PROFILE)
		auto& prof = eval_prof();
		++prof.threat_enum_n;
		prof.threat_enum_cyc += __rdtsc() - t0;
		prof.threat_rows += uint64_t(removed[0].n + removed[1].n + added[0].n + added[1].n);
		++Cyfamate::Gp8::Threat::enum_prof().calls;
#endif
	}
#endif 
#endif 
#endif

	
	
#if defined(USE_FINNY_TABLES)
	static constexpr bool kUseFinnyTables = kHalfDimensions <= 4096;

	struct alignas(kCacheLineSize) FinnyEntry {
		BiasType accumulation[kHalfDimensions];
		Features::IndexList active_indices;
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
		ThreatList threat_indices;
#endif
#endif 
		bool initialized = false;
	};

	struct FinnyCache {
		
		using TriggerEntries = std::array<std::array<FinnyEntry, SQ_NB + 1>, COLOR_NB>;

		const FeatureTransformer* owner = nullptr;
		std::uint64_t generation = 0;
		std::array<TriggerEntries, kRefreshTriggers.size()> entries;

		void reset(const FeatureTransformer* new_owner, std::uint64_t new_generation) {
			owner = new_owner;
			generation = new_generation;
			for (auto& trigger_entries : entries)
				for (auto& perspective_entries : trigger_entries)
					for (auto& entry : perspective_entries)
						entry.initialized = false;
		}
	};

	static Square finny_bucket_square(
		const Position& pos, Features::TriggerEvent trigger, Color perspective) {
		switch (trigger) {
		case Features::TriggerEvent::kFriendKingMoved:
			return pos.square<KING>(perspective);
		case Features::TriggerEvent::kEnemyKingMoved:
			return pos.square<KING>(~perspective);
		case Features::TriggerEvent::kAnyKingMoved:
			return pos.square<KING>(perspective);
		default:
			return SQ_ZERO;
		}
	}

	static void make_index_diff(
		const Features::IndexList& old_active,
		const Features::IndexList& new_active,
		Features::IndexList& removed,
		Features::IndexList& added) {
		if (old_active.size() == new_active.size()) {
			for (std::size_t i = 0; i < old_active.size(); ++i) {
				if (old_active[i] == new_active[i])
					continue;
				removed.push_back(old_active[i]);
				added.push_back(new_active[i]);
			}
			return;
		}

		bool old_matched[RawFeatures::kMaxActiveDimensions] = {};
		bool new_matched[RawFeatures::kMaxActiveDimensions] = {};

		for (std::size_t oi = 0; oi < old_active.size(); ++oi) {
			for (std::size_t ni = 0; ni < new_active.size(); ++ni) {
				if (!new_matched[ni] && old_active[oi] == new_active[ni]) {
					old_matched[oi] = true;
					new_matched[ni] = true;
					break;
				}
			}
		}

		for (std::size_t oi = 0; oi < old_active.size(); ++oi)
			if (!old_matched[oi])
				removed.push_back(old_active[oi]);
		for (std::size_t ni = 0; ni < new_active.size(); ++ni)
			if (!new_matched[ni])
				added.push_back(new_active[ni]);
	}

	static void copy_index_list(
		Features::IndexList& destination,
		const Features::IndexList& source) {
		destination.resize(source.size());
		for (std::size_t i = 0; i < source.size(); ++i)
			destination[i] = source[i];
	}
#endif

	void refresh_accumulator_from_scratch(
#if defined(CYF_GP8)
		BiasType* current, const Features::IndexList& active_indices, IndexType trigger_index
#if defined(CYF_THREAT)
		, const ThreatList* threat_active = nullptr
#endif
		) const {
#else
		BiasType* current, const Features::IndexList& active_indices, IndexType trigger_index) const {
#endif 
#if defined(VECTOR)
		const auto* source = trigger_index == 0 ? biases_ : nullptr;
		update_accumulator_tiled(
			source, current,
			[&](vec_t* acc, IndexType tile_offset) {
				for (const auto index : active_indices)
					add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
				if (threat_active)
					for (int k = 0; k < threat_active->n; ++k)
						add_threat_to_tile(acc, threat_active->v[k], tile_offset);
#endif
#endif 
			});
#else
		if (trigger_index == 0)
			std::memcpy(current, biases_, kHalfDimensions * sizeof(BiasType));
		else
			std::memset(current, 0, kHalfDimensions * sizeof(BiasType));

		for (const auto index : active_indices) {
			const IndexType offset = kHalfDimensions * index;
			for (IndexType j = 0; j < kHalfDimensions; ++j)
				current[j] += weights_[offset + j];
		}
#endif
	}

#if defined(USE_FINNY_TABLES)
#if defined(VECTOR)
	template <typename ApplyChanges>
#if defined(CYF_GP8)
#if defined(CYF_THREAT_I8)
	__attribute__((flatten))
#endif
#endif 
	void update_accumulator_tiled_to_two(
		const BiasType* source, BiasType* destination0, BiasType* destination1,
		ApplyChanges apply_changes) const {
		for (IndexType tile_offset = 0; tile_offset < kHalfDimensions; tile_offset += kTileHeight) {
			vec_t acc[kTileRegs];

			if (source) {
				const auto* source_tile = reinterpret_cast<const vec_t*>(source + tile_offset);
				for (IndexType k = 0; k < kTileRegs; ++k)
					acc[k] = vec_load(source_tile + k);
			} else {
				for (IndexType k = 0; k < kTileRegs; ++k)
					acc[k] = vec_zero();
			}

			apply_changes(acc, tile_offset);

			auto* destination0_tile = reinterpret_cast<vec_t*>(destination0 + tile_offset);
			auto* destination1_tile = reinterpret_cast<vec_t*>(destination1 + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k) {
				vec_store(destination0_tile + k, acc[k]);
				vec_store(destination1_tile + k, acc[k]);
			}
		}
	}
#endif

	void refresh_accumulator_using_finny_entry(
		BiasType* current,
		FinnyEntry& entry,
		const Features::IndexList& active_indices,
#if defined(CYF_GP8)
		IndexType trigger_index
#if defined(CYF_THREAT)
		, const ThreatList& threat_active
#endif
		) const {
#else
		IndexType trigger_index) const {
#endif 
        if (!entry.initialized) {
#if defined(CYFAMATE_TESTING)
            ++TestFinnyCounters().cold;
#endif
#if defined(VECTOR)
			const auto* source = trigger_index == 0 ? biases_ : nullptr;
			update_accumulator_tiled_to_two(
				source, entry.accumulation, current,
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : active_indices)
						add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
					for (int k = 0; k < threat_active.n; ++k)
						add_threat_to_tile(acc, threat_active.v[k], tile_offset);
#endif
#endif 
				});
#else
			refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index);
			std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
#endif
			copy_index_list(entry.active_indices, active_indices);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
			entry.threat_indices = threat_active;
#endif
#endif 
			entry.initialized = true;
		} else {
			Features::IndexList removed_indices, added_indices;
            make_index_diff(entry.active_indices, active_indices, removed_indices, added_indices);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
			ThreatList t_removed, t_added;
			threat_list_diff(entry.threat_indices, threat_active, t_removed, t_added);
#if defined(CYF_EVAL_PROFILE)
			eval_prof().threat_finny_rows += uint64_t(
				removed_indices.size() + added_indices.size() >= active_indices.size() ? threat_active.n : t_removed.n + t_added.n);
#endif
#endif
#endif 
#if defined(CYFAMATE_TESTING)
            ++TestFinnyCounters().hits;
#endif
            
            
            if (removed_indices.size() + added_indices.size() >= active_indices.size()) {
#if defined(CYF_GP8)
                refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index
#if defined(CYF_THREAT)
                    , &threat_active
#endif
                );
#else
                refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index);
#endif 
                std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
                copy_index_list(entry.active_indices, active_indices);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
                entry.threat_indices = threat_active;
#endif
#endif 
#if defined(CYFAMATE_TESTING)
                ++TestFinnyCounters().rebuilt;
#endif
                return;
            }

#if defined(VECTOR)
			update_accumulator_tiled_to_two(
				entry.accumulation, entry.accumulation, current,
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : removed_indices)
						sub_weight_from_tile(acc, index, tile_offset);
					for (const auto index : added_indices)
						add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
					for (int k = 0; k < t_removed.n; ++k)
						sub_threat_from_tile(acc, t_removed.v[k], tile_offset);
					for (int k = 0; k < t_added.n; ++k)
						add_threat_to_tile(acc, t_added.v[k], tile_offset);
#endif
#endif 
				});
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
			entry.threat_indices = threat_active;
#endif
#endif 
#else
			for (const auto index : removed_indices) {
				const IndexType offset = kHalfDimensions * index;
				for (IndexType j = 0; j < kHalfDimensions; ++j)
					entry.accumulation[j] -= weights_[offset + j];
			}
			for (const auto index : added_indices) {
				const IndexType offset = kHalfDimensions * index;
				for (IndexType j = 0; j < kHalfDimensions; ++j)
					entry.accumulation[j] += weights_[offset + j];
			}
#endif
			copy_index_list(entry.active_indices, active_indices);
#if !defined(VECTOR)
			std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
#endif
		}
	}

    FinnyCache& finny_cache() const {
        
        struct FinnyLpDeleter { void operator()(FinnyCache* c) const { c->~FinnyCache(); aligned_large_pages_free(c); } };
        static thread_local std::unique_ptr<FinnyCache, FinnyLpDeleter> cache;
        if (!cache)
        {
            void* m = aligned_large_pages_alloc(sizeof(FinnyCache));
            if (!m) cyf_alloc_failed(sizeof(FinnyCache));
            cache.reset(new (m) FinnyCache());
        }
        if (cache->owner != this || cache->generation != finny_generation_)
            cache->reset(this, finny_generation_);
        return *cache;
    }

    void refresh_accumulator_with_finny_cache(const Position& pos) const {
        auto& cache = finny_cache();
        auto& accumulator = pos.state()->accumulator;
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
		ThreatList threat_active[2];
		collect_threat_active(pos, BLACK, threat_active[BLACK]);
		collect_threat_active(pos, WHITE, threat_active[WHITE]);
#endif
#endif 
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList active_indices[2];
			const auto trigger = kRefreshTriggers[i];
			RawFeatures::AppendActiveIndices(pos, trigger, active_indices);
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
				const Square bucket = finny_bucket_square(pos, trigger, perspective);
				auto& entry = cache.entries[i][perspective][bucket];
				refresh_accumulator_using_finny_entry(
					accumulator.accumulation[perspective][i], entry,
#if defined(CYF_GP8)
					active_indices[perspective], i
#if defined(CYF_THREAT)
					, threat_active[perspective]
#endif
				);
#else
					active_indices[perspective], i);
#endif 
			}
		}

		accumulator.computed_accumulation = true;
		accumulator.computed_score = false;
	}
#endif

	
	
	void refresh_accumulator(const Position& pos) const {
#if defined(CYF_GP8)
#if defined(CYF_THREAT_DERIVE)
		refresh_derive_all(pos);  
		return;
#endif
#endif 
#if defined(USE_FINNY_TABLES)
		if constexpr (kUseFinnyTables) {
			refresh_accumulator_with_finny_cache(pos);
			return;
		}
#endif

		auto& accumulator = pos.state()->accumulator;
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList active_indices[2];
			RawFeatures::AppendActiveIndices(pos, kRefreshTriggers[i], active_indices);
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
#if defined(VECTOR)
				auto* current = accumulator.accumulation[perspective][i];
				refresh_accumulator_from_scratch(current, active_indices[perspective], i);
#else
				refresh_accumulator_from_scratch(
					accumulator.accumulation[perspective][i], active_indices[perspective], i);
#endif
			}
		}

		accumulator.computed_accumulation = true;
		
		accumulator.computed_score = false;
	}

#if defined(CYF_FT_PREFETCH)
	
	
	
public:
	void prefetch_changed(const Position& pos) const {
		Features::IndexList removed[2], added[2]; bool reset[2] = {false, false};
		RawFeatures::AppendChangedIndices(pos, kRefreshTriggers[0], removed, added, reset);
		constexpr size_t row_bytes = kHalfDimensions * sizeof(WeightType);
		for (int c = 0; c < COLOR_NB; ++c) {
			if (reset[c]) continue;
			for (const auto* list : {&removed[c], &added[c]})
				for (const auto index : *list) {
					const char* row = reinterpret_cast<const char*>(&weights_[size_t(kHalfDimensions) * index]);
					for (size_t o = 0; o < row_bytes; o += 64) _mm_prefetch(row + o, _MM_HINT_T0);
				}
		}
	}
private:
#endif
#if defined(CYF_GP8)
#if defined(CYF_THREAT_DERIVE)
	
	
	
	
	

	
	
	__attribute__((flatten)) const BiasType* finny_h(Color perspective, Square ksq, const Features::IndexList& active,
	                                               BiasType* scratch) const {
		static_assert(kRefreshTriggers.size() == 1);
		Features::IndexList h_removed, h_added, h_none;
		const Features::IndexList* h_rem = &h_none;     
		const Features::IndexList* h_add = &active;
		const BiasType* h_src = biases_;
		BiasType* dst = scratch;
#if defined(USE_FINNY_TABLES)
		FinnyEntry* entry = nullptr;
		if constexpr (kUseFinnyTables) {
			entry = &finny_cache().entries[0][perspective][ksq];
			dst = entry->accumulation;
			if (!entry->initialized) {
#if defined(CYFAMATE_TESTING)
				++TestFinnyCounters().cold;
#endif
			} else {
#if defined(CYFAMATE_TESTING)
				++TestFinnyCounters().hits;
#endif
				make_index_diff(entry->active_indices, active, h_removed, h_added);
				
				if (h_removed.size() + h_added.size() < active.size()) { h_src = entry->accumulation; h_rem = &h_removed; h_add = &h_added; }
#if defined(CYFAMATE_TESTING)
				else ++TestFinnyCounters().rebuilt;
#endif
			}
		}
#else
		(void)perspective; (void)ksq;
#endif
#if CYF_FINNY_PFR   
		{
			auto pf = [&](IndexType index) {
#if defined(CYF_FT_I8)
				if (const std::int8_t* w8 = cyf_ft_i8()) { __builtin_prefetch(w8 + std::size_t(kHalfDimensions) * index); return; }
#endif
				__builtin_prefetch(&weights_[std::size_t(kHalfDimensions) * index]);
			};
			for (const auto index : *h_rem) pf(index);
			for (const auto index : *h_add) pf(index);
		}
#endif
		for (IndexType tile_offset = 0; tile_offset < kHalfDimensions; tile_offset += kTileHeight) {
			vec_t h[kTileRegs];
			const auto* hs = reinterpret_cast<const vec_t*>(h_src + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k) h[k] = vec_load(hs + k);
			for (const auto index : *h_rem) sub_weight_from_tile(h, index, tile_offset);
			for (const auto index : *h_add) add_weight_to_tile(h, index, tile_offset);
			auto* d = reinterpret_cast<vec_t*>(dst + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k) vec_store(d + k, h[k]);
		}
#if defined(USE_FINNY_TABLES)
		if (entry) {
			copy_index_list(entry->active_indices, active);
			entry->initialized = true;
		}
#endif
		return dst;
	}

	
	
	
	__attribute__((flatten)) void derive_reset(const Position& pos, Color perspective, int nplies, const BiasType* src,
	                                           const Features::IndexList& active, BiasType* dst,
	                                           const ThreatList& t_removed, const ThreatList& t_added) const {
		
		const BonaPiece* cur = perspective == BLACK ? pos.eval_list()->piece_list_fb() : pos.eval_list()->piece_list_fw();
		BonaPiece old_list[PIECE_NUMBER_NB];
		std::memcpy(old_list, cur, sizeof(old_list));
		const StateInfo* st = pos.state();
		for (int k = 0; k < nplies; ++k, st = st->previous) {
			const auto& dp = st->dirtyPiece;
			for (int i = 0; i < dp.dirty_num; ++i)
				old_list[dp.pieceNo[i]] = static_cast<BonaPiece>(dp.changed_piece[i].old_piece.from[perspective]);
		}
		const Square rel_old = static_cast<Square>((old_list[PIECE_NUMBER_KING + perspective] - Eval::f_king) % SQ_NB);
		const Square raw_old = perspective == BLACK ? rel_old : Inv(rel_old);   
		Square bucket_old = rel_old;
		const bool mirrored_old = Features::HalfKA2<Features::Side::kFriend>::MirrorKing(bucket_old);
		Features::IndexList old_active;
		for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i)
			old_active.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(bucket_old, old_list[i], mirrored_old));
		alignas(kCacheLineSize) BiasType buf_old[kHalfDimensions];
		alignas(kCacheLineSize) BiasType buf_new[kHalfDimensions];
#if defined(CYFAMATE_TESTING)
		++TestFinnyCounters().king_refreshes;   
		if (finny_cache().entries[0][perspective][pos.square<KING>(perspective)].initialized) ++TestFinnyCounters().king_cache_hits;
#endif
		const BiasType* h_old = finny_h(perspective, raw_old, old_active, buf_old);
		const BiasType* h_new = finny_h(perspective, pos.square<KING>(perspective), active, buf_new);
		for (IndexType tile_offset = 0; tile_offset < kHalfDimensions; tile_offset += kTileHeight) {
			vec_t a[kTileRegs];
			const auto* s = reinterpret_cast<const vec_t*>(src + tile_offset);
			const auto* ho = reinterpret_cast<const vec_t*>(h_old + tile_offset);
			const auto* hn = reinterpret_cast<const vec_t*>(h_new + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k) a[k] = vec_add_16(vec_sub_16(vec_load(s + k), vec_load(ho + k)), vec_load(hn + k));
			for (int j = 0; j < t_removed.n; ++j) sub_threat_from_tile(a, t_removed.v[j], tile_offset);
			for (int j = 0; j < t_added.n; ++j) add_threat_to_tile(a, t_added.v[j], tile_offset);
			auto* d = reinterpret_cast<vec_t*>(dst + tile_offset);
			for (IndexType k = 0; k < kTileRegs; ++k) vec_store(d + k, a[k]);
		}
	}

	
	__attribute__((flatten)) void refresh_derive_all(const Position& pos) const {
		auto& accumulator = pos.state()->accumulator;
		Features::IndexList active_indices[2];
		RawFeatures::AppendActiveIndices(pos, kRefreshTriggers[0], active_indices);
		for (int c = 0; c < COLOR_NB; ++c) {
			const Color perspective = Color(c);
			ThreatList threat_active;
			collect_threat_active(pos, perspective, threat_active);
			alignas(kCacheLineSize) BiasType buf[kHalfDimensions];
			const BiasType* h = finny_h(perspective, pos.square<KING>(perspective), active_indices[c], buf);
			update_accumulator_tiled(h, accumulator.accumulation[c][0], [&](vec_t* acc, IndexType tile_offset) {
				for (int j = 0; j < threat_active.n; ++j) add_threat_to_tile(acc, threat_active.v[j], tile_offset);
			});
		}
		accumulator.computed_accumulation = true;
		accumulator.computed_score = false;
	}
#endif 
#endif 
	
	
	void update_accumulator(const Position& pos) const {
		const auto& prev_accumulator = pos.state()->previous->accumulator;
		auto&      accumulator      = pos.state()->accumulator;
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
		
		
		const Eval::DirtyPiece* plies1[1] = { &pos.state()->dirtyPiece };
		ThreatList t_removed[2], t_added[2];
		threat_changes(pos, plies1, 1, t_removed, t_added);
		prefetch_threat_rows(t_removed, t_added);
#endif
#endif 
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList removed_indices[2], added_indices[2];
			bool                reset[2];
			RawFeatures::AppendChangedIndices(pos, kRefreshTriggers[i], removed_indices, added_indices, reset);
			for (int c = 0; c < COLOR_NB; ++c) {
                const Color perspective = static_cast<Color>(c);
#if defined(CYF_GP8)
#if defined(CYF_THREAT_DERIVE)
				static_assert(kRefreshTriggers.size() == 1);
				if (reset[perspective]) {   
#if defined(CYF_EVAL_PROFILE)
					++eval_prof().threat_reset_n;
#endif
					derive_reset(pos, perspective, 1, prev_accumulator.accumulation[perspective][i], added_indices[perspective],
					             accumulator.accumulation[perspective][i], t_removed[perspective], t_added[perspective]);
					continue;
				}
#endif 
#if defined(CYF_THREAT)
				ThreatList threat_active;
				if (reset[perspective]) {
#if defined(CYF_EVAL_PROFILE)
					const uint64_t tc0 = __rdtsc();
#endif
					collect_threat_active(pos, perspective, threat_active);
#if defined(CYF_EVAL_PROFILE)
					auto& prof = eval_prof();
					++prof.threat_reset_n;
					prof.threat_collect_cyc += __rdtsc() - tc0;
#endif
				}
#endif
#endif 
#if defined(USE_FINNY_TABLES)
                if constexpr (kUseFinnyTables) {
                    if (reset[perspective]) {
                        auto& cache = finny_cache();
                        const Square bucket = finny_bucket_square(pos, kRefreshTriggers[i], perspective);
#if defined(CYFAMATE_TESTING)
                        ++TestFinnyCounters().king_refreshes;
                        TestFinnyCounters().king_cache_hits += cache.entries[i][perspective][bucket].initialized;
#endif
                        
                        refresh_accumulator_using_finny_entry(
                            accumulator.accumulation[perspective][i],
#if defined(CYF_GP8)
                            cache.entries[i][perspective][bucket], added_indices[perspective], i
#if defined(CYF_THREAT)
                            , threat_active
#endif
                        );
#else
                            cache.entries[i][perspective][bucket], added_indices[perspective], i);
#endif 
                        continue;
                    }
                }
#endif
#if defined(VECTOR)
                auto* current = accumulator.accumulation[perspective][i];
                if (reset[perspective]) {
					const auto* source = i == 0 ? biases_ : nullptr;
					update_accumulator_tiled(
						source, current,
						[&](vec_t* acc, IndexType tile_offset) {
							for (const auto index : added_indices[perspective])
								add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
							for (int k = 0; k < threat_active.n; ++k)
								add_threat_to_tile(acc, threat_active.v[k], tile_offset);
#endif
#endif 
						});
				} else {
					update_accumulator_tiled(
						prev_accumulator.accumulation[perspective][i],
						current,
						[&](vec_t* acc, IndexType tile_offset) {
							for (const auto index : removed_indices[perspective])
								sub_weight_from_tile(acc, index, tile_offset);
							for (const auto index : added_indices[perspective])
								add_weight_to_tile(acc, index, tile_offset);
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
							for (int k = 0; k < t_removed[perspective].n; ++k)
								sub_threat_from_tile(acc, t_removed[perspective].v[k], tile_offset);
							for (int k = 0; k < t_added[perspective].n; ++k)
								add_threat_to_tile(acc, t_added[perspective].v[k], tile_offset);
#endif
#endif 
						});
				}
#else
				if (reset[perspective]) {
					if (i == 0) {
						std::memcpy(accumulator.accumulation[perspective][i], biases_,
						            kHalfDimensions * sizeof(BiasType));
					} else {
						std::memset(accumulator.accumulation[perspective][i], 0, kHalfDimensions * sizeof(BiasType));
					}
				} else {
					
					
					std::memcpy(accumulator.accumulation[perspective][i], prev_accumulator.accumulation[perspective][i],
					            kHalfDimensions * sizeof(BiasType));
					for (const auto index : removed_indices[perspective]) {
						const IndexType offset = kHalfDimensions * index;
						for (IndexType j = 0; j < kHalfDimensions; ++j) {
							accumulator.accumulation[perspective][i][j] -= weights_[offset + j];
						}
					}
				}
				
				
				for (const auto index : added_indices[perspective]) {
					const IndexType offset = kHalfDimensions * index;
					for (IndexType j = 0; j < kHalfDimensions; ++j) {
							accumulator.accumulation[perspective][i][j] += weights_[offset + j];
					}
				}
#endif
			}
		}

		accumulator.computed_accumulation = true;
		
		accumulator.computed_score = false;
	}

#if defined(CYFAMATE_TESTING)
public:
    struct FinnyCounters { std::uint64_t cold=0, hits=0, rebuilt=0, king_refreshes=0, king_cache_hits=0; };
    static FinnyCounters& TestFinnyCounters() {
        static thread_local FinnyCounters counters;
        return counters;
    }
    void TestSeedParameters(std::uint64_t seed) {
        for (IndexType j=0; j<kHalfDimensions; ++j) biases_[j]=BiasType(32+(j+seed)%32);
        for (std::size_t i=0; i<std::size_t(kHalfDimensions)*kInputDimensions; ++i) {
            seed ^= seed<<13; seed ^= seed>>7; seed ^= seed<<17;
            weights_[i]=WeightType(int(seed%5)-2);
        }
#if defined(USE_FINNY_TABLES)
        finny_generation_=next_finny_generation();
#endif
    }
    void TestRefreshFromScratch(const Position& pos) const {
        for (IndexType i=0; i<kRefreshTriggers.size(); ++i) {
            Features::IndexList active[2];
            RawFeatures::AppendActiveIndices(pos,kRefreshTriggers[i],active);
            for (int c=0;c<COLOR_NB;++c)
                refresh_accumulator_from_scratch(pos.state()->accumulator.accumulation[c][i],active[c],i);
        }
        pos.state()->accumulator.computed_accumulation=true;
        pos.state()->accumulator.computed_score=false;
    }
#endif
private:
    alignas(kCacheLineSize) BiasType biases_[kHalfDimensions];
	alignas(kCacheLineSize) WeightType weights_[kHalfDimensions * kInputDimensions];
#if defined(CYF_GP8)
#if defined(CYF_THREAT)
	
	
	
#if defined(CYF_THREAT_I8)
	alignas(kCacheLineSize) std::int8_t threat_weights_[std::size_t(Cyfamate::Gp8::Threat::kDims) * kHalfDimensions];
#else
	alignas(kCacheLineSize) WeightType threat_weights_[std::size_t(Cyfamate::Gp8::Threat::kDims) * kHalfDimensions];
#endif
#endif
#endif 
#if defined(USE_FINNY_TABLES)
    
    
    static std::uint64_t next_finny_generation() {
        static std::atomic<std::uint64_t> sequence{0};
        return sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    std::uint64_t finny_generation_ = 0;
#endif
};

} 
} 

#endif  

#endif  
