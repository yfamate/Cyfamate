// A class that converts the input features of the NNUE evaluation function
// NNUE評価関数の入力特徴量の変換を行うクラス

#ifndef CLASSIC_NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#define CLASSIC_NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#if !defined(CYF_ARCH_K3K3)

#include "../../config.h"

#if defined(EVAL_NNUE)

#if defined(SFNNwoPSQT)
#define USE_ELEMENT_WISE_MULTIPLY
#endif

#include "nnue_common.h"
#include "nnue_architecture.h"
#include "features/index_list.h"

#include <algorithm>  // std::clamp
#include <cstring>  // std::memset()

#if defined(USE_FINNY_TABLES)
#include <array>
#include <cstdint>
#include <memory>
#endif

// [GP8 research] header-driven GP routing and step-attacker threat features.
#include "cyf_gp8_threat.h"
#if defined(CYF_THREAT) && !(defined(USE_AVX2) || defined(USE_AVX512))
#error "CYF_THREAT needs the AVX2 / AVX-512 accumulator path"
#endif
#if defined(CYF_THREAT) && defined(CYF_ACC_PARENT_FILL)
#error "CYF_THREAT does not implement CYF_ACC_PARENT_FILL"
#endif

namespace YaneuraOu {
namespace Eval::NNUE {

// If vector instructions are enabled, we update and refresh the
// accumulator tile by tile such that each tile fits in the CPU's
// vector registers.
// ベクトル命令が有効な場合、変数のタイルを、
// 各タイルがCPUのベクトルレジスタに収まるように、更新してリフレッシュする。
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
static constexpr IndexType kNumRegs = 8;  // only 8 are needed

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
static constexpr IndexType kNumRegs = 16;

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

/*
 例) SFNN1536のときのkNumChunksの計算

┌─────────┬───────────────┬─────────────────┬────────────┐
│  SIMD            │ sizeof(vec_t)                │ / sizeof(int16)                  │ kNumChunks             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ AVX-512          │ 64                           │ 32                               │ 1536/32=48             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ AVX2             │ 32                           │ 16                               │ 1536/16=96             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ SSE2             │ 16                           │ 8                                │ 1536/8=192             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ NEON             │ 16                           │ 8                                │ 1536/8=192             │
└─────────┴───────────────┴─────────────────┴────────────┘
*/

constexpr IndexType MaxChunkSize = 16;

// Input feature converter
// 入力特徴量変換器
class FeatureTransformer {
   private:
	// Number of output dimensions for one side
	// 片側分の出力の次元数
	static constexpr IndexType kHalfDimensions = kTransformedFeatureDimensions;

#if defined(VECTOR)
	//static constexpr IndexType kTileHeight = kNumRegs * sizeof(vec_t) / 2;
	//static_assert(kHalfDimensions % kTileHeight == 0, "kTileHeight must divide kHalfDimensions");
	// ⇨  AVX-512でこの制約守れないっぽ。
#endif

   public:
	// Output type
	// 出力の型
	using OutputType = TransformedFeatureType;
	using BiasType   = std::int16_t;
	using WeightType = std::int16_t;

	// Number of input/output dimensions
	// 入出力の次元数
	static constexpr IndexType kInputDimensions  = RawFeatures::kDimensions;
#if defined(USE_ELEMENT_WISE_MULTIPLY)
	static constexpr IndexType kOutputDimensions = kHalfDimensions;
#else
	static constexpr IndexType kOutputDimensions = kHalfDimensions * 2;
#endif

	// Size of forward propagation buffer
	// 順伝播用バッファのサイズ
	static constexpr std::size_t kBufferSize = kOutputDimensions * sizeof(OutputType);

	// Set by the loader for the duration of one ReadParameters call when the file header
	// declares that FT weights/biases are stored at the internal (doubled) scale.
	static inline bool read_at_internal_scale_ = false;
	// [GP8 research] set by the loader when the file carries a threat block after the FT.
	static inline bool read_threat_block_ = false;
	using ThreatList = Cyfamate::Gp8::Threat::List;

	// Hash value embedded in the evaluation file
	// 評価関数ファイルに埋め込むハッシュ値
	static constexpr std::uint32_t GetHashValue() {
#if defined(SFNNwoPSQT)
		// 学習部と整合性とるの面倒なのでSFNNwoPSQTのときはこれに固定しておく。
		return 0x5f134ab8u;
#else
		return RawFeatures::kHashValue ^ kOutputDimensions;
#endif
	}

	// A string that represents the structure
	// 構造を表す文字列
	static std::string GetStructureString() {
		return RawFeatures::GetName() + "[" + std::to_string(kInputDimensions) + "->"
		       + std::to_string(kHalfDimensions) + "x2]";
	}

	// Read network parameters
	// パラメータを読み込む
	Tools::Result ReadParameters(std::istream& stream) {
#if defined(USE_ELEMENT_WISE_MULTIPLY)
		read_leb_128<BiasType>(stream, biases_, kHalfDimensions);
		read_leb_128<WeightType>(stream, weights_, kHalfDimensions * kInputDimensions);

#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
		permute_weights(inverse_order_packs);
#endif
		// Files normally store FT at half the internal scale and are doubled here. A file
		// that declares the internal scale (see LoadAndShare) is used as stored.
		if (!read_at_internal_scale_)
			scale_weights(true);
		// [GP8 research] threat block: u32 profile id (3 = step-attacker), then raw int8 rows
		// (33408 x width) at the file scale (127). Widened to int16 and doubled here (the same
		// internal scale as the FT rows, which are read at 127 and doubled); the lanes are
		// permuted like the FT rows.
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
			std::memcpy(threat_weights_, raw.data(), kThreatCount);   // [threat-speed I8] file scale, widened x2 per use
#else
			for (std::size_t i = 0; i < kThreatCount; ++i)
				threat_weights_[i] = WeightType(int(raw[i]) * 2);
#endif
#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
			permute_threat_rows();
#endif
		}
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

	// Write network parameters
	// パラメータを書き込む
	bool WriteParameters(std::ostream& stream) const {
        // Serialize copies of individual rows; never mutate a live evaluator.
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
            // Invert the loader's actual permutation (the legacy order_packs helper
            // does not invert its AVX-512 mapping).
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
        for (IndexType j = 0; j < kInputDimensions; ++j)
            if (!write_row(weights_ + j * kHalfDimensions, arch.ft_scale_254, false)) return false;
        if (!flush_leb()) return false;
        if (Cyfamate::Gp8::threat_enabled) {
            const uint32_t profile = 3;
            stream.write(reinterpret_cast<const char*>(&profile), sizeof(profile));
            for (IndexType j = 0; j < Cyfamate::Gp8::Threat::kDims; ++j)
#if defined(CYF_THREAT_I8)
            {   // widen the (lane-permuted) int8 row to the int16 internal scale; write_row undoes the permutation
                alignas(64) WeightType wide[kHalfDimensions];
                for (IndexType i = 0; i < kHalfDimensions; ++i)
                    wide[i] = WeightType(2 * int(threat_weights_[std::size_t(j) * kHalfDimensions + i]));
                if (!write_row(wide, false, true)) return false;
            }
#else
                if (!write_row(threat_weights_ + std::size_t(j) * kHalfDimensions, false, true)) return false;
#endif
        }
		return !stream.fail();
	}

	// Proceed with the difference calculation if possible
	// 可能なら差分計算を進める
	bool UpdateAccumulatorIfPossible(const Position& pos) const {
		// Kingless tsume has an empty feature set for that perspective. Avoid the
		// ordinary king-square arithmetic in incremental/grandparent updates.
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
		// The parent was never evaluated (e.g. its eval came from the TT) but the
		// grandparent was: apply both plies' feature changes to the grandparent's
		// accumulator instead of refreshing (measured: ~15% of requests in search).
		if (prev && prev->previous && prev->previous->accumulator.computed_accumulation
		    && update_accumulator_from_grandparent(pos))
			return true;
#endif
		return false;
	}

#if defined(VECTOR) && !defined(CYF_NO_ACC_GRANDPARENT)
	// Exact for HalfKA2(Friend): an index is (own king square, BonaPiece). If neither ply
	// moved a king, the king square is the same in all three positions, so the two plies'
	// removed/added indices can be applied to the grandparent in one pass. Integer
	// adds/subs commute modulo 2^16, so the result equals a refresh bit for bit.
	bool update_accumulator_from_grandparent(const Position& pos) const {
		static_assert(std::is_same_v<RawFeatures, Features::FeatureSet<Features::HalfKA2<Features::Side::kFriend>>>);
		static_assert(kRefreshTriggers.size() == 1);
		const StateInfo* s2 = pos.state();
		const StateInfo* s1 = s2->previous;
		const StateInfo* s0 = s1->previous;
		const Eval::DirtyPiece* plies[2] = { &s1->dirtyPiece, &s2->dirtyPiece };
		// A perspective whose own king moved in either ply changes every index (new
		// king square) and is refreshed alone; the other perspective still takes the diff.
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
		// Threat edges changed over the two plies (board before ply 1 -> current), per perspective.
		ThreatList t_removed[2], t_added[2];
		threat_changes(pos, plies, 2, t_removed, t_added);
		prefetch_threat_rows(t_removed, t_added);
#if defined(CYF_ACC_PARENT_FILL)   // opt-in: exact but measured no faster
		// No king moved in either ply: materialize the parent's accumulator first (one ply's diff),
		// then the child from it. Siblings evaluated later under the same parent take the cheaper
		// parent path instead of redoing both plies (measured: parent 91 ns vs grandparent 232 ns).
		// Exact: the same integer adds/subs, only split across two stored results.
		if (!own_king_moved[BLACK] && !own_king_moved[WHITE]) {
			auto& parent = const_cast<StateInfo*>(s1)->accumulator;
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
				const BonaPiece* pieces = perspective == BLACK ? pos.eval_list()->piece_list_fb()
				                                               : pos.eval_list()->piece_list_fw();
				const Square sq_k = static_cast<Square>(
					(pieces[PIECE_NUMBER_KING + perspective] - Eval::f_king) % SQ_NB);
				Features::IndexList removed[2], added[2];
				for (int k = 0; k < 2; ++k)
					for (int i = 0; i < plies[k]->dirty_num; ++i) {
						removed[k].push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
							sq_k, static_cast<BonaPiece>(plies[k]->changed_piece[i].old_piece.from[perspective])));
						added[k].push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
							sq_k, static_cast<BonaPiece>(plies[k]->changed_piece[i].new_piece.from[perspective])));
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
#if defined(CYF_THREAT_DERIVE)
				derive_reset(pos, perspective, 2, s0->accumulator.accumulation[perspective][0], active,
				             accumulator.accumulation[perspective][0], t_removed[perspective], t_added[perspective]);
				continue;   // the combined-only refresh below is not used in a derive build
#endif
				ThreatList threat_active;
				collect_threat_active(pos, perspective, threat_active);
#if defined(USE_FINNY_TABLES)
				if constexpr (kUseFinnyTables) {
					auto& cache = finny_cache();
					const Square bucket = finny_bucket_square(pos, kRefreshTriggers[0], perspective);
					refresh_accumulator_using_finny_entry(
						accumulator.accumulation[perspective][0], cache.entries[0][perspective][bucket], active, 0
						, threat_active
					);
				} else
#endif
				refresh_accumulator_from_scratch(accumulator.accumulation[perspective][0], active, 0
					, &threat_active
				);
				continue;
			}
			const BonaPiece* pieces = perspective == BLACK ? pos.eval_list()->piece_list_fb()
			                                               : pos.eval_list()->piece_list_fw();
			const Square sq_k = static_cast<Square>(
				(pieces[PIECE_NUMBER_KING + perspective] - Eval::f_king) % SQ_NB);
			Features::IndexList removed, added;
			for (const auto* dp : plies)
				for (int i = 0; i < dp->dirty_num; ++i) {
					removed.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
						sq_k, static_cast<BonaPiece>(dp->changed_piece[i].old_piece.from[perspective])));
					added.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(
						sq_k, static_cast<BonaPiece>(dp->changed_piece[i].new_piece.from[perspective])));
				}
			update_accumulator_tiled(
				s0->accumulator.accumulation[perspective][0], accumulator.accumulation[perspective][0],
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : removed) sub_weight_from_tile(acc, index, tile_offset);
					for (const auto index : added) add_weight_to_tile(acc, index, tile_offset);
					for (int k = 0; k < t_removed[perspective].n; ++k) sub_threat_from_tile(acc, t_removed[perspective].v[k], tile_offset);
					for (int k = 0; k < t_added[perspective].n; ++k) add_threat_to_tile(acc, t_added[perspective].v[k], tile_offset);
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
	// Diagnostic build only: how each accumulator request is served, per process.
	// refresh_* counts requests that fell back to a (Finny) refresh because the parent
	// had no accumulator; *_ancestor_k says the nearest computed ancestor was k plies up.
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
	// Diagnostic build only: cycles and counts per accumulator case, plus the output transform.
	// case 0 already computed, 1 from parent, 2 from grandparent/ancestor (incremental), 3 refresh, 4 forced
	struct EvalProf { uint64_t n[5]{}, cyc[5]{}, transform_n = 0, transform_cyc = 0;
	                  // [GP8 research] threat edge enumeration (threat_changes) and the number of
	                  // threat rows applied (removed + added, both perspectives) per incremental update.
	                  uint64_t threat_enum_n = 0, threat_enum_cyc = 0, threat_rows = 0;
	                  // own-king-move resets inside the incremental update: full threat enumeration
	                  // (collect_threat_active) time and the threat rows the Finny path then applies.
	                  uint64_t threat_reset_n = 0, threat_collect_cyc = 0, threat_finny_rows = 0; };
	static inline EvalProf& eval_prof() { static EvalProf p; return p; }
#endif
#if defined(CYF_BLEND_INCREMENTAL_ACC)
    // On a GPU value hit, retain cheap parent/grandparent updates only.
    // A failed update leaves normal EnsureAccumulator to refresh on demand.
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
	void EnsureAccumulator(const Position& pos, bool refresh) const {
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
			if (kase != 4) kase = 3;   // no incremental path was possible
#endif

			refresh_accumulator(pos);
		}
	}

	// Convert input features
	// 入力特徴量を変換する
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
			// Packed output is sizeof(vec_t) bytes for each SIMD register
#if defined(USE_AVX512)
			constexpr IndexType OutputChunkSize = 64;
#else
			constexpr IndexType OutputChunkSize = kSimdWidth;
#endif
		static_assert((kHalfDimensions / 2) % OutputChunkSize == 0);
		constexpr IndexType NumOutputChunks = kHalfDimensions / 2 / OutputChunkSize;

		vec_t Zero = vec_zero();
		vec_t One = vec_set_16(127 * 2);

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
				const vec_t sum0a =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 0], One), Zero), shift);
				const vec_t sum0b =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 1], One), Zero), shift);
				const vec_t sum1a = vec_min_16(in1[j * 2 + 0], One);
				const vec_t sum1b = vec_min_16(in1[j * 2 + 1], One);

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
				sum0 = std::clamp<BiasType>(sum0, 0, 127 * 2);
				sum1 = std::clamp<BiasType>(sum1, 0, 127 * 2);
				const int product = (int(sum0) << shift) * int(sum1);
				const int value = product >> 16;
				output[offset + j] = static_cast<OutputType>(std::clamp(value, 0, 255));
			}

		}
#endif

#else

		// 以下は旧NNUEのコード。
		// ループ本体がx86とNEONで異なる（2入力→1出力 vs 1入力→1出力）ため、
		// kNumChunksの意味自体がアーキテクチャごとに違うため、共通化しにくい。触らないことにする。

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
#else  // SSE41非対応だがSSE2は使える環境
		const __m128i k0x80s = _mm_set1_epi8(-128);
#endif

#elif defined(USE_MMX)
		// USE_MMX を config.h では現状、有効化することがないので dead code
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
#else  // SSE41非対応だがSSE2は使える環境
				                _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s)
#endif
				);
			}

#elif defined(USE_MMX)
			// USE_MMX を config.h では現状、有効化することがないので dead code
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
		// USE_MMX を config.h では現状、有効化することがないので dead code
		_mm_empty();
#endif
#endif
	}

   private:
	static void order_packs([[maybe_unused]] uint64_t* v) {
#if defined(USE_AVX512)  // _mm512_set_epi32 packs in the order [15 11 7 3 14 10 6 2 13 9 5 1 12 8 4 0]
		uint64_t tmp0 = v[4], tmp1 = v[5];
		v[4] = v[6], v[5] = v[7];
		v[6] = tmp0, v[7] = tmp1;
		tmp0 = v[8], tmp1 = v[9];
		v[8] = v[12], v[9] = v[13];
		v[12] = v[10], v[13] = v[11];
		v[10] = tmp0, v[11] = tmp1;
#elif defined(USE_AVX2)  // _mm256_set_epi32 packs in the order [7 3 6 2 5 1 4 0]
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
#elif defined(USE_AVX2)  // Inverse _mm256_packs_epi16 ordering
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

		for (IndexType j = 0; j < kInputDimensions; ++j)
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
		for (IndexType j = 0; j < kInputDimensions; ++j)
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
	// 変更された各特徴量ごとにaccumulator全体を読み書きするのを避けるため、
	// SIMDレジスタに収まるタイル単位で差分をまとめて適用する。
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
#if defined(CYF_THREAT_I8)
	__attribute__((flatten))   // [threat-speed] keep the row helpers inlined so the tile stays in registers
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

	void add_weight_to_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_add_16(acc[k], vec_load(column + k));
	}

	void sub_weight_from_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_sub_16(acc[k], vec_load(column + k));
	}

	// [GP8 research] threat rows are int16 at the internal (doubled) scale, exactly like the
	// FT rows, so a threat row costs the same as a HalfKA2 row in the tiled update.
#if defined(CYF_THREAT_I8)
	// [threat-speed I8] k-th vector of a threat row at tile_offset, widened to int16 and doubled.
#if defined(USE_AVX512)
	static vec_t threat_widen2(const std::int8_t* p, IndexType k) {
		const __m512i w = _mm512_cvtepi8_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(p) + k));
		return _mm512_add_epi16(w, w);
	}
#else
	static vec_t threat_widen2(const std::int8_t* p, IndexType k) {
		const __m256i w = _mm256_cvtepi8_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(p) + k));
		return _mm256_add_epi16(w, w);
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

	// [GP8 research v6] prefetch the threat rows an update will read, as soon as the edge diff is
	// known (the HalfKA2 index computation and the first tiles run while the lines arrive).
	// No values change; only memory timing. Opt out with -DCYF_NO_THREAT_PREFETCH.
	void prefetch_threat_rows(const ThreatList* removed, const ThreatList* added) const {
#if !defined(CYF_NO_THREAT_PREFETCH)
#if defined(CYF_THREAT_SUBPROF)
		const uint64_t tp0 = __rdtsc();
#endif
		constexpr std::size_t row_bytes = kHalfDimensions * sizeof(threat_weights_[0]);
		for (int c = 0; c < 2; ++c)
			for (const ThreatList* l : {&removed[c], &added[c]})
				for (int k = 0; k < l->n; ++k) {
					const char* row = reinterpret_cast<const char*>(&threat_weights_[std::size_t(kHalfDimensions) * l->v[k]]);
#if !defined(CYF_THREAT_PREFETCH_STRIDE)
#define CYF_THREAT_PREFETCH_STRIDE 64   // [CHAT-G] 128 = every other line (adjacent-line prefetcher fetches the pair)
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

	// Same lane permutation as permute_weights(inverse_order_packs) for the threat rows.
	void permute_threat_rows() const {
#if defined(USE_AVX2)
#if defined(USE_AVX512)
		constexpr IndexType di = 16;
#else
		constexpr IndexType di = 8;
#endif
		for (IndexType j = 0; j < IndexType(Cyfamate::Gp8::Threat::kDims); ++j) {
#if defined(CYF_THREAT_I8)
			// int16 lane chunk q (4 lanes = 8 bytes) corresponds to int8 chunk q (4 lanes = 4 bytes): the same
			// chunk permutation on the 32-bit chunks of the int8 row widens to the permuted int16 row.
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

	// Active threat indices of `pos` from `perspective`, ascending (threat rows only).
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
		// [v7] table-driven full enumeration (piece-indexed effect tables, direct index), sorted.
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
	// Threat indices removed / added by the given plies (oldest first), per perspective.
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
		// [v7] table-driven enumeration writing both perspectives' indices directly.
#if defined(CYF_THREAT_ENUM_V8)
		Cyfamate::Gp8::Threat::changed_indices_v8(pos, plies, nplies, removed, added);   // [threat-speed] VBMI v8
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

	// StockfishのAccumulatorCaches(Finny Tables)と同じ発想。
	// 王位置ごとにrefresh済みaccumulatorを持ち、次回はactive featureの差分だけを適用する。
#if defined(USE_FINNY_TABLES)
	static constexpr bool kUseFinnyTables = kHalfDimensions <= 4096;

	struct alignas(kCacheLineSize) FinnyEntry {
		BiasType accumulation[kHalfDimensions];
		Features::IndexList active_indices;
		ThreatList threat_indices;
		bool initialized = false;
	};

	struct FinnyCache {
		// SQ_NB is the missing-king sentinel used by kingless tsume positions.
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
		BiasType* current, const Features::IndexList& active_indices, IndexType trigger_index
		, const ThreatList* threat_active = nullptr
		) const {
#if defined(VECTOR)
		const auto* source = trigger_index == 0 ? biases_ : nullptr;
		update_accumulator_tiled(
			source, current,
			[&](vec_t* acc, IndexType tile_offset) {
				for (const auto index : active_indices)
					add_weight_to_tile(acc, index, tile_offset);
				if (threat_active)
					for (int k = 0; k < threat_active->n; ++k)
						add_threat_to_tile(acc, threat_active->v[k], tile_offset);
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
#if defined(CYF_THREAT_I8)
	__attribute__((flatten))
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
		IndexType trigger_index
		, const ThreatList& threat_active
		) const {
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
					for (int k = 0; k < threat_active.n; ++k)
						add_threat_to_tile(acc, threat_active.v[k], tile_offset);
				});
#else
			refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index);
			std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
#endif
			copy_index_list(entry.active_indices, active_indices);
			entry.threat_indices = threat_active;
			entry.initialized = true;
		} else {
			Features::IndexList removed_indices, added_indices;
            make_index_diff(entry.active_indices, active_indices, removed_indices, added_indices);
			ThreatList t_removed, t_added;
			threat_list_diff(entry.threat_indices, threat_active, t_removed, t_added);
#if defined(CYF_EVAL_PROFILE)
			eval_prof().threat_finny_rows += uint64_t(
				removed_indices.size() + added_indices.size() >= active_indices.size() ? threat_active.n : t_removed.n + t_added.n);
#endif
#if defined(CYFAMATE_TESTING)
            ++TestFinnyCounters().hits;
#endif
            // A distant position may need more edits than a full rebuild. Keep the
            // cheaper path, then seed the cache with the new position either way.
            if (removed_indices.size() + added_indices.size() >= active_indices.size()) {
                refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index
                    , &threat_active
                );
                std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
                copy_index_list(entry.active_indices, active_indices);
                entry.threat_indices = threat_active;
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
					for (int k = 0; k < t_removed.n; ++k)
						sub_threat_from_tile(acc, t_removed.v[k], tile_offset);
					for (int k = 0; k < t_added.n; ++k)
						add_threat_to_tile(acc, t_added.v[k], tile_offset);
				});
			entry.threat_indices = threat_active;
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
        static thread_local std::unique_ptr<FinnyCache> cache;
        if (!cache) cache = std::make_unique<FinnyCache>();
        if (cache->owner != this || cache->generation != finny_generation_)
            cache->reset(this, finny_generation_);
        return *cache;
    }

    void refresh_accumulator_with_finny_cache(const Position& pos) const {
        auto& cache = finny_cache();
        auto& accumulator = pos.state()->accumulator;
		ThreatList threat_active[2];
		collect_threat_active(pos, BLACK, threat_active[BLACK]);
		collect_threat_active(pos, WHITE, threat_active[WHITE]);
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
					active_indices[perspective], i
					, threat_active[perspective]
				);
			}
		}

		accumulator.computed_accumulation = true;
		accumulator.computed_score = false;
	}
#endif

	// Calculate cumulative value without using difference calculation
	// 差分計算を用いずに累積値を計算する
	void refresh_accumulator(const Position& pos) const {
#if defined(CYF_THREAT_DERIVE)
		refresh_derive_all(pos);  // [threat-speed] HalfKA2-only Finny entries + every active threat row
		return;
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
		// Stockfishでは fc27d15(2020-09-07) にcomputed_scoreが排除されているので確認
		accumulator.computed_score = false;
	}

#if defined(CYF_FT_PREFETCH)
	// Experimental: right after do_move, prefetch the FT weight rows the child's incremental update
	// will read (removed + added indices, both perspectives). Rows of perspectives that will be
	// refreshed (own king moved) are skipped. No values change; only memory timing.
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
#if defined(CYF_THREAT_DERIVE)
	// [threat-speed CYF_THREAT_DERIVE] The threat rows are king-independent, so on an own-king move the threat part
	// of the older accumulator is recovered exactly as (combined - HalfKA2 part) modulo 2^16, with the HalfKA2 part of
	// the older position taken from the HalfKA2-only Finny entry of the old king square. No per-state threat copy and
	// no extra work on ordinary updates; the reset needs neither a full threat enumeration nor a stale-list diff.
	// Finny entries hold bias + HalfKA2 only in this mode.

	// HalfKA2-only Finny entry (perspective, raw king square ksq) brought to `active`; returns its accumulation
	// (`scratch` when the build has no Finny tables).
	__attribute__((flatten)) const BiasType* finny_h(Color perspective, Square ksq, const Features::IndexList& active,
	                                               BiasType* scratch) const {
		static_assert(kRefreshTriggers.size() == 1);
		Features::IndexList h_removed, h_added, h_none;
		const Features::IndexList* h_rem = &h_none;     // default: bias + every active row
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
				// A distant position may need more edits than a full rebuild: keep the cheaper path.
				if (h_removed.size() + h_added.size() < active.size()) { h_src = entry->accumulation; h_rem = &h_removed; h_add = &h_added; }
#if defined(CYFAMATE_TESTING)
				else ++TestFinnyCounters().rebuilt;
#endif
			}
		}
#else
		(void)perspective; (void)ksq;
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

	// Own king of `perspective` moved in one of the last `nplies` plies (1: parent update, 2: grandparent update).
	// src = combined accumulation `nplies` plies ago, active = current complete HalfKA2 list, t_* = threat delta
	// of those plies. dst = src - H(old) + H(now) - t_removed + t_added.
	__attribute__((flatten)) void derive_reset(const Position& pos, Color perspective, int nplies, const BiasType* src,
	                                           const Features::IndexList& active, BiasType* dst,
	                                           const ThreatList& t_removed, const ThreatList& t_added) const {
		// The BonaPiece list `nplies` plies ago: revert the dirty pieces, newest ply first.
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
		const Square raw_old = perspective == BLACK ? rel_old : Inv(rel_old);   // Finny entries are keyed by the raw square
		Features::IndexList old_active;
		for (PieceNumber i = PIECE_NUMBER_ZERO; i < PIECE_NUMBER_NB; ++i)
			old_active.push_back(Features::HalfKA2<Features::Side::kFriend>::MakeIndex(rel_old, old_list[i]));
		alignas(kCacheLineSize) BiasType buf_old[kHalfDimensions];
		alignas(kCacheLineSize) BiasType buf_new[kHalfDimensions];
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

	// Full refresh of both perspectives: HalfKA2-only Finny + every active threat row.
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
#endif // CYF_THREAT_DERIVE
	// Calculate cumulative value using difference calculation
	// 差分計算を用いて累積値を計算する
	void update_accumulator(const Position& pos) const {
		const auto& prev_accumulator = pos.state()->previous->accumulator;
		auto&      accumulator      = pos.state()->accumulator;
		// Threat edges changed by the last ply (parent -> current), per perspective; the
		// full list is only needed for a perspective whose own king moved (refresh).
		const Eval::DirtyPiece* plies1[1] = { &pos.state()->dirtyPiece };
		ThreatList t_removed[2], t_added[2];
		threat_changes(pos, plies1, 1, t_removed, t_added);
		prefetch_threat_rows(t_removed, t_added);
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList removed_indices[2], added_indices[2];
			bool                reset[2];
			RawFeatures::AppendChangedIndices(pos, kRefreshTriggers[i], removed_indices, added_indices, reset);
			for (int c = 0; c < COLOR_NB; ++c) {
                const Color perspective = static_cast<Color>(c);
#if defined(CYF_THREAT_DERIVE)
				static_assert(kRefreshTriggers.size() == 1);
				if (reset[perspective]) {   // own king moved: AppendChangedIndices gave the complete active list
#if defined(CYF_EVAL_PROFILE)
					++eval_prof().threat_reset_n;
#endif
					derive_reset(pos, perspective, 1, prev_accumulator.accumulation[perspective][i], added_indices[perspective],
					             accumulator.accumulation[perspective][i], t_removed[perspective], t_added[perspective]);
					continue;
				}
#endif // CYF_THREAT_DERIVE
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
#if defined(USE_FINNY_TABLES)
                if constexpr (kUseFinnyTables) {
                    if (reset[perspective]) {
                        auto& cache = finny_cache();
                        const Square bucket = finny_bucket_square(pos, kRefreshTriggers[i], perspective);
#if defined(CYFAMATE_TESTING)
                        ++TestFinnyCounters().king_refreshes;
                        TestFinnyCounters().king_cache_hits += cache.entries[i][perspective][bucket].initialized;
#endif
                        // AppendChangedIndices supplies the complete active list on reset.
                        refresh_accumulator_using_finny_entry(
                            accumulator.accumulation[perspective][i],
                            cache.entries[i][perspective][bucket], added_indices[perspective], i
                            , threat_active
                        );
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
							for (int k = 0; k < threat_active.n; ++k)
								add_threat_to_tile(acc, threat_active.v[k], tile_offset);
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
							for (int k = 0; k < t_removed[perspective].n; ++k)
								sub_threat_from_tile(acc, t_removed[perspective].v[k], tile_offset);
							for (int k = 0; k < t_added[perspective].n; ++k)
								add_threat_to_tile(acc, t_added[perspective].v[k], tile_offset);
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
					// Difference calculation for the feature amount changed from 1 to 0
					// 1から0に変化した特徴量に関する差分計算
					std::memcpy(accumulator.accumulation[perspective][i], prev_accumulator.accumulation[perspective][i],
					            kHalfDimensions * sizeof(BiasType));
					for (const auto index : removed_indices[perspective]) {
						const IndexType offset = kHalfDimensions * index;
						for (IndexType j = 0; j < kHalfDimensions; ++j) {
							accumulator.accumulation[perspective][i][j] -= weights_[offset + j];
						}
					}
				}
				// Difference calculation for features that changed from 0 to 1
				// 0から1に変化した特徴量に関する差分計算
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
		// Stockfishでは fc27d15(2020-09-07) にcomputed_scoreが排除されているので確認
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
	// [GP8 research] step-attacker threat rows. The file holds int8 at the file scale (127);
	// they are widened to int16 and doubled at load (v3; v1/v2 kept int8 and widened per
	// update, ~29 ns per 768-wide row against ~13 ns for an int16 row), lanes permuted like weights_.
#if defined(CYF_THREAT_I8)
	alignas(kCacheLineSize) std::int8_t threat_weights_[std::size_t(Cyfamate::Gp8::Threat::kDims) * kHalfDimensions];
#else
	alignas(kCacheLineSize) WeightType threat_weights_[std::size_t(Cyfamate::Gp8::Threat::kDims) * kHalfDimensions];
#endif
#if defined(USE_FINNY_TABLES)
    // A fresh object may reuse the same address after a model reload. A per-object
    // increment would restart at 1 and could accidentally retain stale caches.
    static std::uint64_t next_finny_generation() {
        static std::atomic<std::uint64_t> sequence{0};
        return sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    std::uint64_t finny_generation_ = 0;
#endif
};

} // namespace Eval::NNUE
} // namespace YaneuraOu

#endif  // defined(EVAL_NNUE)

#else

#include "../../config.h"

#if defined(EVAL_NNUE)

#if defined(SFNNwoPSQT)
#define USE_ELEMENT_WISE_MULTIPLY
#endif

#include "nnue_common.h"
#include "nnue_architecture.h"
#include "features/index_list.h"

#include <algorithm>  // std::clamp
#include <cstring>  // std::memset()

#if defined(USE_FINNY_TABLES)
#include <array>
#include <cstdint>
#include <memory>
#endif

namespace YaneuraOu {
namespace Eval::NNUE {

// If vector instructions are enabled, we update and refresh the
// accumulator tile by tile such that each tile fits in the CPU's
// vector registers.
// ベクトル命令が有効な場合、変数のタイルを、
// 各タイルがCPUのベクトルレジスタに収まるように、更新してリフレッシュする。
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
static constexpr IndexType kNumRegs = 8;  // only 8 are needed

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
static constexpr IndexType kNumRegs = 16;

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

/*
 例) SFNN1536のときのkNumChunksの計算

┌─────────┬───────────────┬─────────────────┬────────────┐
│  SIMD            │ sizeof(vec_t)                │ / sizeof(int16)                  │ kNumChunks             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ AVX-512          │ 64                           │ 32                               │ 1536/32=48             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ AVX2             │ 32                           │ 16                               │ 1536/16=96             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ SSE2             │ 16                           │ 8                                │ 1536/8=192             │
├─────────┼───────────────┼─────────────────┼────────────┤
│ NEON             │ 16                           │ 8                                │ 1536/8=192             │
└─────────┴───────────────┴─────────────────┴────────────┘
*/

constexpr IndexType MaxChunkSize = 16;

// Input feature converter
// 入力特徴量変換器
class FeatureTransformer {
   private:
	// Number of output dimensions for one side
	// 片側分の出力の次元数
	static constexpr IndexType kHalfDimensions = kTransformedFeatureDimensions;

#if defined(VECTOR)
	//static constexpr IndexType kTileHeight = kNumRegs * sizeof(vec_t) / 2;
	//static_assert(kHalfDimensions % kTileHeight == 0, "kTileHeight must divide kHalfDimensions");
	// ⇨  AVX-512でこの制約守れないっぽ。
#endif

   public:
	// Output type
	// 出力の型
	using OutputType = TransformedFeatureType;
	using BiasType   = std::int16_t;
	using WeightType = std::int16_t;

	// Number of input/output dimensions
	// 入出力の次元数
	static constexpr IndexType kInputDimensions  = RawFeatures::kDimensions;
#if defined(USE_ELEMENT_WISE_MULTIPLY)
	static constexpr IndexType kOutputDimensions = kHalfDimensions;
#else
	static constexpr IndexType kOutputDimensions = kHalfDimensions * 2;
#endif

	// Size of forward propagation buffer
	// 順伝播用バッファのサイズ
	static constexpr std::size_t kBufferSize = kOutputDimensions * sizeof(OutputType);

	// Hash value embedded in the evaluation file
	// 評価関数ファイルに埋め込むハッシュ値
	static constexpr std::uint32_t GetHashValue() {
#if defined(SFNNwoPSQT)
		// 学習部と整合性とるの面倒なのでSFNNwoPSQTのときはこれに固定しておく。
		return 0x5f134ab8u;
#else
		return RawFeatures::kHashValue ^ kOutputDimensions;
#endif
	}

	// A string that represents the structure
	// 構造を表す文字列
	static std::string GetStructureString() {
		return RawFeatures::GetName() + "[" + std::to_string(kInputDimensions) + "->"
		       + std::to_string(kHalfDimensions) + "x2]";
	}

	// Read network parameters
	// パラメータを読み込む
	Tools::Result ReadParameters(std::istream& stream) {
#if defined(USE_ELEMENT_WISE_MULTIPLY)
		read_leb_128<BiasType>(stream, biases_, kHalfDimensions);
		read_leb_128<WeightType>(stream, weights_, kHalfDimensions * kInputDimensions);

#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
		permute_weights(inverse_order_packs);
#endif
		scale_weights(true);
#if defined(USE_FINNY_TABLES)
		if (!stream.fail())
			++finny_generation_;
#endif
#else
		for (std::size_t i = 0; i < kHalfDimensions; ++i) biases_[i] = read_little_endian<BiasType>(stream);
		for (std::size_t i = 0; i < kHalfDimensions * kInputDimensions; ++i)
			weights_[i] = read_little_endian<WeightType>(stream);
#if defined(USE_FINNY_TABLES)
		if (!stream.fail())
			++finny_generation_;
#endif
#endif
		return !stream.fail() ? Tools::ResultCode::Ok : Tools::ResultCode::FileReadError;
	}

	// Write network parameters
	// パラメータを書き込む
	bool WriteParameters(std::ostream& stream) const {
		stream.write(reinterpret_cast<const char*>(biases_), kHalfDimensions * sizeof(BiasType));
		stream.write(reinterpret_cast<const char*>(weights_), kHalfDimensions * kInputDimensions * sizeof(WeightType));
		return !stream.fail();
	}

	// Proceed with the difference calculation if possible
	// 可能なら差分計算を進める
	bool UpdateAccumulatorIfPossible(const Position& pos) const {
		const auto now = pos.state();
		if (now->accumulator.computed_accumulation) {
			return true;
		}
		const auto prev = now->previous;
		if (prev && prev->accumulator.computed_accumulation) {
			update_accumulator(pos);
			return true;
		}
		return false;
	}

	void EnsureAccumulator(const Position& pos, bool refresh) const {
		if (refresh || !UpdateAccumulatorIfPossible(pos)) {
			refresh_accumulator(pos);
		}
	}

	// Convert input features
	// 入力特徴量を変換する
	void Transform(const Position& pos, OutputType* output, bool refresh) const {
		EnsureAccumulator(pos, refresh);
		const auto& accumulation = pos.state()->accumulator.accumulation;

#if defined(USE_ELEMENT_WISE_MULTIPLY)

#if defined(VECTOR) && !defined(NNUE_SMALL_SFNN_FT)
			// Packed output is sizeof(vec_t) bytes for each SIMD register
#if defined(USE_AVX512)
			constexpr IndexType OutputChunkSize = 64;
#else
			constexpr IndexType OutputChunkSize = kSimdWidth;
#endif
		static_assert((kHalfDimensions / 2) % OutputChunkSize == 0);
		constexpr IndexType NumOutputChunks = kHalfDimensions / 2 / OutputChunkSize;

		vec_t Zero = vec_zero();
		vec_t One = vec_set_16(127 * 2);

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
				const vec_t sum0a =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 0], One), Zero), shift);
				const vec_t sum0b =
					vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 1], One), Zero), shift);
				const vec_t sum1a = vec_min_16(in1[j * 2 + 0], One);
				const vec_t sum1b = vec_min_16(in1[j * 2 + 1], One);

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
				sum0 = std::clamp<BiasType>(sum0, 0, 127 * 2);
				sum1 = std::clamp<BiasType>(sum1, 0, 127 * 2);
				const int product = (int(sum0) << shift) * int(sum1);
				const int value = product >> 16;
				output[offset + j] = static_cast<OutputType>(std::clamp(value, 0, 255));
			}

		}
#endif

#else

		// 以下は旧NNUEのコード。
		// ループ本体がx86とNEONで異なる（2入力→1出力 vs 1入力→1出力）ため、
		// kNumChunksの意味自体がアーキテクチャごとに違うため、共通化しにくい。触らないことにする。

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
#else  // SSE41非対応だがSSE2は使える環境
		const __m128i k0x80s = _mm_set1_epi8(-128);
#endif

#elif defined(USE_MMX)
		// USE_MMX を config.h では現状、有効化することがないので dead code
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
#else  // SSE41非対応だがSSE2は使える環境
				                _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s)
#endif
				);
			}

#elif defined(USE_MMX)
			// USE_MMX を config.h では現状、有効化することがないので dead code
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
		// USE_MMX を config.h では現状、有効化することがないので dead code
		_mm_empty();
#endif
#endif
	}

   private:
	static void order_packs([[maybe_unused]] uint64_t* v) {
#if defined(USE_AVX512)  // _mm512_set_epi32 packs in the order [15 11 7 3 14 10 6 2 13 9 5 1 12 8 4 0]
		uint64_t tmp0 = v[4], tmp1 = v[5];
		v[4] = v[6], v[5] = v[7];
		v[6] = tmp0, v[7] = tmp1;
		tmp0 = v[8], tmp1 = v[9];
		v[8] = v[12], v[9] = v[13];
		v[12] = v[10], v[13] = v[11];
		v[10] = tmp0, v[11] = tmp1;
#elif defined(USE_AVX2)  // _mm256_set_epi32 packs in the order [7 3 6 2 5 1 4 0]
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
#elif defined(USE_AVX2)  // Inverse _mm256_packs_epi16 ordering
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

		for (IndexType j = 0; j < kInputDimensions; ++j)
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
		for (IndexType j = 0; j < kInputDimensions; ++j)
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
	// 変更された各特徴量ごとにaccumulator全体を読み書きするのを避けるため、
	// SIMDレジスタに収まるタイル単位で差分をまとめて適用する。
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

	void add_weight_to_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_add_16(acc[k], vec_load(column + k));
	}

	void sub_weight_from_tile(vec_t* acc, IndexType index, IndexType tile_offset) const {
		const auto* column = reinterpret_cast<const vec_t*>(
			&weights_[kHalfDimensions * index + tile_offset]);
		for (IndexType k = 0; k < kTileRegs; ++k)
			acc[k] = vec_sub_16(acc[k], vec_load(column + k));
	}
#endif

	// StockfishのAccumulatorCaches(Finny Tables)と同じ発想。
	// 王位置ごとにrefresh済みaccumulatorを持ち、次回はactive featureの差分だけを適用する。
#if defined(USE_FINNY_TABLES)
	static constexpr bool kUseFinnyTables = kHalfDimensions <= 4096;

	struct alignas(kCacheLineSize) FinnyEntry {
		BiasType accumulation[kHalfDimensions];
		Features::IndexList active_indices;
		bool initialized = false;
	};

	struct FinnyCache {
		using TriggerEntries = std::array<std::array<FinnyEntry, SQ_NB>, COLOR_NB>;

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
		BiasType* current, const Features::IndexList& active_indices, IndexType trigger_index) const {
#if defined(VECTOR)
		const auto* source = trigger_index == 0 ? biases_ : nullptr;
		update_accumulator_tiled(
			source, current,
			[&](vec_t* acc, IndexType tile_offset) {
				for (const auto index : active_indices)
					add_weight_to_tile(acc, index, tile_offset);
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
		IndexType trigger_index) const {
		if (!entry.initialized) {
#if defined(VECTOR)
			const auto* source = trigger_index == 0 ? biases_ : nullptr;
			update_accumulator_tiled_to_two(
				source, entry.accumulation, current,
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : active_indices)
						add_weight_to_tile(acc, index, tile_offset);
				});
#else
			refresh_accumulator_from_scratch(entry.accumulation, active_indices, trigger_index);
			std::memcpy(current, entry.accumulation, kHalfDimensions * sizeof(BiasType));
#endif
			copy_index_list(entry.active_indices, active_indices);
			entry.initialized = true;
		} else {
			Features::IndexList removed_indices, added_indices;
			make_index_diff(entry.active_indices, active_indices, removed_indices, added_indices);

#if defined(VECTOR)
			update_accumulator_tiled_to_two(
				entry.accumulation, entry.accumulation, current,
				[&](vec_t* acc, IndexType tile_offset) {
					for (const auto index : removed_indices)
						sub_weight_from_tile(acc, index, tile_offset);
					for (const auto index : added_indices)
						add_weight_to_tile(acc, index, tile_offset);
				});
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

	void refresh_accumulator_with_finny_cache(const Position& pos) const {
		static thread_local std::unique_ptr<FinnyCache> cache;
		if (!cache)
			cache = std::make_unique<FinnyCache>();
		if (cache->owner != this || cache->generation != finny_generation_)
			cache->reset(this, finny_generation_);

		auto& accumulator = pos.state()->accumulator;
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList active_indices[2];
			const auto trigger = kRefreshTriggers[i];
			RawFeatures::AppendActiveIndices(pos, trigger, active_indices);
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
				const Square bucket = finny_bucket_square(pos, trigger, perspective);
				auto& entry = cache->entries[i][perspective][bucket];
				refresh_accumulator_using_finny_entry(
					accumulator.accumulation[perspective][i], entry,
					active_indices[perspective], i);
			}
		}

		accumulator.computed_accumulation = true;
		accumulator.computed_score = false;
	}
#endif

	// Calculate cumulative value without using difference calculation
	// 差分計算を用いずに累積値を計算する
	void refresh_accumulator(const Position& pos) const {
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
		// Stockfishでは fc27d15(2020-09-07) にcomputed_scoreが排除されているので確認
		accumulator.computed_score = false;
	}

	// Calculate cumulative value using difference calculation
	// 差分計算を用いて累積値を計算する
	void update_accumulator(const Position& pos) const {
		const auto& prev_accumulator = pos.state()->previous->accumulator;
		auto&      accumulator      = pos.state()->accumulator;
		for (IndexType i = 0; i < kRefreshTriggers.size(); ++i) {
			Features::IndexList removed_indices[2], added_indices[2];
			bool                reset[2];
			RawFeatures::AppendChangedIndices(pos, kRefreshTriggers[i], removed_indices, added_indices, reset);
			for (int c = 0; c < COLOR_NB; ++c) {
				const Color perspective = static_cast<Color>(c);
#if defined(VECTOR)
				auto* current = accumulator.accumulation[perspective][i];
				if (reset[perspective]) {
					const auto* source = i == 0 ? biases_ : nullptr;
					update_accumulator_tiled(
						source, current,
						[&](vec_t* acc, IndexType tile_offset) {
							for (const auto index : added_indices[perspective])
								add_weight_to_tile(acc, index, tile_offset);
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
					// Difference calculation for the feature amount changed from 1 to 0
					// 1から0に変化した特徴量に関する差分計算
					std::memcpy(accumulator.accumulation[perspective][i], prev_accumulator.accumulation[perspective][i],
					            kHalfDimensions * sizeof(BiasType));
					for (const auto index : removed_indices[perspective]) {
						const IndexType offset = kHalfDimensions * index;
						for (IndexType j = 0; j < kHalfDimensions; ++j) {
							accumulator.accumulation[perspective][i][j] -= weights_[offset + j];
						}
					}
				}
				// Difference calculation for features that changed from 0 to 1
				// 0から1に変化した特徴量に関する差分計算
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
		// Stockfishでは fc27d15(2020-09-07) にcomputed_scoreが排除されているので確認
		accumulator.computed_score = false;
	}

	// parameter type
	// パラメータの型

	// parameter
	// パラメータ
	alignas(kCacheLineSize) BiasType biases_[kHalfDimensions];
	alignas(kCacheLineSize) WeightType weights_[kHalfDimensions * kInputDimensions];
#if defined(USE_FINNY_TABLES)
	std::uint64_t finny_generation_ = 0;
#endif
};

} // namespace Eval::NNUE
} // namespace YaneuraOu

#endif  // defined(EVAL_NNUE)

#endif
#endif  // #ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED
