
#include "../../config.h"

#if defined(EVAL_NNUE)

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

#define INCBIN_SILENCE_BITCODE_WARNING
#include "../../incbin/incbin.h"

#include "../../types.h"
#include "../../evaluate.h"
#include "../../position.h"
#include "../../memory.h"
#include "../../usi.h"

#if defined(USE_EVAL_HASH)
#include "../evalhash.h"
#endif

#include "evaluate_nnue.h"
#include "nnue_common.h"

namespace YaneuraOu::Eval::NNUE {
extern int FV_SCALE;
}
 

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

#if defined(NNUE_EMBEDDING_OFF)
    const char* default_eval_dir = "eval";
#else
    const char* default_eval_dir = "<internal>";
#endif
    Options.add("EvalDir", Option(default_eval_dir, [](const Option& o) {
                    std::string eval_dir = std::string(o);
                    if (last_eval_dir != eval_dir)
                    {
                        last_eval_dir = eval_dir;
                        eval_loaded   = false;
                    }
                    return std::nullopt;
                }));

#if !defined(__EMSCRIPTEN__)
    Options.add("EvalFile", Option(EvalFileDefaultName, [](const Option& o) {
                    static std::string last = EvalFileDefaultName;
                    if (last != std::string(o)) { last = std::string(o); eval_loaded = false; }
                    return std::nullopt;
                }));
#endif

    Options.add("FV_SCALE", Option(26, 1, 128, [&](const Option& o) {
                    YaneuraOu::Eval::NNUE::FV_SCALE = int(o);
                    return std::nullopt;
                }));

}
#endif



#if !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
INCBIN(EmbeddedNNUE, EvalFileDefaultName);
#else
const unsigned char        gEmbeddedNNUEData[1] = { 0x0 };
const unsigned char* const gEmbeddedNNUEEnd = &gEmbeddedNNUEData[1];
const unsigned int         gEmbeddedNNUESize = 1;
#endif


namespace {

	struct EmbeddedNNUE {
		EmbeddedNNUE(const unsigned char* embeddedData,
			const unsigned char* embeddedEnd,
			const unsigned int   embeddedSize) :
			data(embeddedData),
			end(embeddedEnd),
			size(embeddedSize) {
		}
		const unsigned char* data;
		const unsigned char* end;
		const unsigned int   size;
	};



	EmbeddedNNUE get_embedded() {
		return EmbeddedNNUE(gEmbeddedNNUEData, gEmbeddedNNUEEnd, gEmbeddedNNUESize);
	}
}


namespace YaneuraOu {
#if defined(CYFAMATE_NNUE_5GP)
#if defined(CYF_GP8)
namespace Cyfamate { int gp_stack_bucket(const Position& pos); inline void gp8_rebuild_tables(); }   
#else
namespace Cyfamate { int gp_stack_bucket(const Position& pos); }   
#endif 
#if defined(CYF_GP_INTERP)
namespace Cyfamate { inline double game_phase(const Position& pos); }
#endif
#endif
namespace Eval {
namespace NNUE {

	int FV_SCALE = 26;   

#if defined(SFNNwoPSQT) && NNUE_SFNN_PROGRESS_BUCKETS != 1
#endif

    SystemWideSharedConstant<NnueNetworks> shared_networks;

    const char* const kFileName = EvalFileDefaultName;

#if defined(CYF_NNUE_L1_MAIN_SCALE_VARIANT)
    std::string GetArchitectureString() { return GetArchitectureString(kWeightScaleBits, false); }
    std::string GetArchitectureString(int l1_main_shift, bool ft_internal_scale) {
#else
    std::string GetArchitectureString() {
#endif
        const std::string base = "Features=" + FeatureTransformer::GetStructureString() +
			",Network=" + Network::GetStructureString();
#if defined(SFNNwoPSQT)
#if NNUE_SFNN_GP_BUCKETS != 1
#if defined(CYF_GP8)
		if (!Cyfamate::Gp8::arch_string.empty() && l1_main_shift == kWeightScaleBits && !ft_internal_scale)
			return Cyfamate::Gp8::arch_string;
#endif 
		std::string bounds;
#if defined(CYF_GP8)
		for (int i = 0; i < Cyfamate::Gp8::stacks - 1; ++i) {
#else
		for (int i = 0; i < kLayerStacks - 1; ++i) {
#endif 
			char buf[32];
#if defined(CYF_GP8)
			std::snprintf(buf, sizeof(buf), "%s%g", i ? "," : "", Cyfamate::Gp8::bounds[i]);
#else
			std::snprintf(buf, sizeof(buf), "%s%g", i ? "," : "", kGpStackBounds[i]);
#endif 
			bounds += buf;
		}
		const std::string l1_scale = (l1_main_shift == kWeightScaleBits
			? std::string() : ";L1MainScale=" + std::to_string(1 << l1_main_shift))
			+ (ft_internal_scale ? ";FTScale=254" : "");
#if defined(CYF_GP8)
		return "ModelType=SFNNWithoutPsqt;" + base + "_" + std::to_string(Cyfamate::Gp8::stacks)
		     + "GP{LayerStack=" + std::to_string(Cyfamate::Gp8::stacks)
#else
		return "ModelType=SFNNWithoutPsqt;" + base + "{LayerStack=" + std::to_string(kLayerStacks)
#endif 
		     + ";Bucket=GP(" + bounds + ")" + l1_scale + "}";
#else
		return "ModelType=SFNNWithoutPsqt;" + base + "{LayerStack=" + std::to_string(kLayerStacks) + "}";
#endif
#else
		return base;
#endif
    }

namespace {
	namespace Detail {

		template <typename T>
		Tools::Result ReadParameters(std::istream& stream, T& obj) {
			std::uint32_t header;
			stream.read(reinterpret_cast<char*>(&header), sizeof(header));
			if (!stream) return Tools::ResultCode::FileReadError;
			if (header != T::GetHashValue())
				sync_cout << "info string Warning : nn.bin hash mismatch." << sync_endl;
			return obj.ReadParameters(stream);
		}

		template <typename T>
		bool WriteParameters(std::ostream& stream, const T& obj) {
			constexpr std::uint32_t header = T::GetHashValue();
			stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
			return obj.WriteParameters(stream);
		}

	}  

	Tools::Result LoadAndShare(std::istream& stream) {
		auto tmp = make_unique_large_page<NnueNetworks>();

		std::uint32_t hash_value;
		std::string architecture;
		Tools::Result result = ReadHeader(stream, &hash_value, &architecture, nullptr);
		if (result.is_not_ok()) return result;
#if defined(CYF_GP8)
		struct RestoreOnFailure {
			const Cyfamate::Gp8::LoadedState saved = Cyfamate::Gp8::LoadedState::save();
			bool committed = false;
			~RestoreOnFailure() {
				if (committed) return;
				saved.restore();
				Cyfamate::gp8_rebuild_tables();
			}
		} gp8_restore;
		const auto parsed = Cyfamate::Gp8::parse_arch(architecture);
		if (!parsed.error.empty()) {
			sync_cout << "info string NNUE architecture not readable by this build: " << parsed.error
				<< " (" << architecture << ")" << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
		if (parsed.ft_out != int(kTransformedFeatureDimensions) || parsed.h1 != int(kHidden1Dims)
		    || parsed.h2 != int(kHidden2Dims)) {
			sync_cout << "info string NNUE architecture mismatch: this build is "
				<< kTransformedFeatureDimensions << "/" << kHidden1Dims << "+1/" << kHidden2Dims
				<< " but the file is " << architecture << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
#if !defined(CYF_THREAT)
		if (parsed.threat) {
			sync_cout << "info string NNUE file has a threat block but this build has no threat features" << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
#endif
		{
			const bool fixed_gp5 = Cyfamate::Gp8::is_fixed_gp5(parsed);
			const std::uint32_t expected = Cyfamate::Gp8::expected_top_hash(parsed, kSfnnBaseHashValue);
			if (!fixed_gp5 && hash_value != expected) {
				sync_cout << "info string NNUE hash " << hash_value << " disagrees with architecture "
					<< architecture << " (expected " << expected << ")" << sync_endl;
				return Tools::ResultCode::FileMismatch;
			}
			Cyfamate::Gp8::stacks = parsed.stacks;
			for (int i = 0; i < Cyfamate::Gp8::kMaxStacks - 1; ++i)
				Cyfamate::Gp8::bounds[i] = i < int(parsed.bounds.size()) ? parsed.bounds[i] : 0.0;
			Cyfamate::Gp8::arch_string = architecture;
			Cyfamate::Gp8::top_hash = hash_value;
			Cyfamate::Gp8::threat_enabled = parsed.threat;
			Cyfamate::Gp8::hm = parsed.hm;
			Cyfamate::Gp8::fixed_gp5 = fixed_gp5;
			Cyfamate::gp8_rebuild_tables();
			if (!fixed_gp5) hash_value = kHashValue;
		}
#endif 
		int l1_main_shift = kWeightScaleBits;
		bool ft_internal_scale = false;
#if defined(CYF_NNUE_L1_MAIN_SCALE_VARIANT)
		const std::uint32_t variant = hash_value ^ kHashValue;
		constexpr std::uint32_t kBoth = kHashTagL1Main256 ^ kHashTagFtInternalScale;
		if (variant == kHashTagL1Main256 || variant == kHashTagFtInternalScale || variant == kBoth) {
			l1_main_shift = (variant == kHashTagFtInternalScale) ? kWeightScaleBits : kL1Main256Shift;
			ft_internal_scale = variant != kHashTagL1Main256;
			if (architecture != GetArchitectureString(l1_main_shift, ft_internal_scale)) {
				sync_cout << "info string NNUE architecture mismatch: expected "
					<< GetArchitectureString(l1_main_shift, ft_internal_scale)
					<< " but the file is " << architecture << sync_endl;
				return Tools::ResultCode::FileMismatch;
			}
		} else
#endif
		if (hash_value != kHashValue) {
#if NNUE_SFNN_GP_BUCKETS != 1
			sync_cout << "info string NNUE architecture mismatch: this build is " << GetArchitectureString()
				<< " but the file is " << architecture << sync_endl;
			return Tools::ResultCode::FileMismatch;
#else
			sync_cout << "info string Warning: NNUE hash mismatch: expected " << kHashValue
				<< " got " << hash_value
				<< " arch_in_file=" << architecture
				<< " arch_expected=" << GetArchitectureString()
				<< sync_endl;
#endif
		}

		FeatureTransformer::read_at_internal_scale_ = ft_internal_scale;
#if defined(CYF_GP8)
		FeatureTransformer::read_threat_block_ = parsed.threat;
#endif 
		result = Detail::ReadParameters<FeatureTransformer>(stream, tmp->feature_transformer);
		FeatureTransformer::read_at_internal_scale_ = false;
#if defined(CYF_GP8)
		FeatureTransformer::read_threat_block_ = false;
#endif 
		if (result.is_not_ok()) {
			sync_cout << "info string NNUE feature params read failed: " << result.to_string() << sync_endl;
			return result;
		}
#if defined(SFNNwoPSQT) && NNUE_SFNN_PROGRESS_BUCKETS != 1
#endif
#if defined(CYF_GP8)
		for (int i = 0; i < Cyfamate::Gp8::stacks; ++i) {
#else
		for (int i = 0; i < kLayerStacks; ++i) {
#endif 
			result = Detail::ReadParameters<Network>(stream, tmp->network[i]);
			if (result.is_not_ok()) {
				sync_cout << "info string NNUE network params read failed at stack " << i << ": " << result.to_string() << sync_endl;
				return result;
			}
		}

		if (!stream || stream.peek() != std::ios::traits_type::eof())
			return Tools::ResultCode::FileCloseError;
#if defined(CYF_NNUE_L1_MAIN_SCALE_VARIANT)
		for (int i = 0; i < kLayerStacks; ++i)
			tmp->network[i].l1_main_shift_ = l1_main_shift;
		if (l1_main_shift != kWeightScaleBits)
			sync_cout << "info string NNUE fc_0 main outputs at weight scale " << (1 << l1_main_shift) << sync_endl;
		if (ft_internal_scale)
			sync_cout << "info string NNUE feature transformer stored at internal scale 254" << sync_endl;
#else
		(void)l1_main_shift;
		(void)ft_internal_scale;
#endif

#if defined(CYF_FT_I8) && defined(CYF_FT_CLAMP8_TEST)
		tmp->feature_transformer.cyf_clamp8_test();
#endif
		shared_networks = SystemWideSharedConstant<NnueNetworks>(*tmp);
#if defined(CYF_FT_I8)
		networks().feature_transformer.cyf_build_ft_i8();
#endif
#if defined(CYF_GP8)
		gp8_restore.committed = true;
#endif

		return Tools::ResultCode::Ok;
	}

	}  
    Tools::Result ReadHeader(std::istream& stream,
        std::uint32_t* hash_value, std::string* architecture, std::uint32_t* version_out) {
        std::uint32_t version = 0, size = 0;
        stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        stream.read(reinterpret_cast<char*>(hash_value), sizeof(*hash_value));
        stream.read(reinterpret_cast<char*>(&size), sizeof(size));
		if (!stream) return Tools::ResultCode::FileReadError;
		if (version_out)
			*version_out = version;
        if (version != kVersion) {
			sync_cout << "info string NNUE header version mismatch: expected " << kVersion
				<< " got " << version << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
        architecture->resize(size);
        stream.read(&(*architecture)[0], size);
		return !stream.fail() ? Tools::ResultCode::Ok : Tools::ResultCode::FileReadError;
    }

    bool WriteHeader(std::ostream& stream,
        std::uint32_t hash_value, const std::string& architecture) {
        stream.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
        stream.write(reinterpret_cast<const char*>(&hash_value), sizeof(hash_value));
        const std::uint32_t size = static_cast<std::uint32_t>(architecture.size());
        stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        stream.write(architecture.data(), size);
        return !stream.fail();
    }

    	Tools::Result ReadParameters(std::istream& stream) {
    		return LoadAndShare(stream);
    	}
    bool WriteParameters(std::ostream& stream) {
#if defined(CYF_GP8)
        if (Cyfamate::Gp8::arch_string.empty()) return false;
        if (!WriteHeader(stream, Cyfamate::Gp8::top_hash, Cyfamate::Gp8::arch_string)) return false;
#else
#if defined(CYF_NNUE_L1_MAIN_SCALE_VARIANT)
        const int l1_main_shift = networks().network[0].l1_main_shift_;
        if (!WriteHeader(stream, l1_main_shift == kWeightScaleBits ? kHashValue : kHashValueL1Main256,
                         GetArchitectureString(l1_main_shift))) return false;
#else
        if (!WriteHeader(stream, kHashValue, GetArchitectureString())) return false;
#endif
#endif 
        if (!Detail::WriteParameters<FeatureTransformer>(stream, networks().feature_transformer)) return false;
#if defined(SFNNwoPSQT) && NNUE_SFNN_PROGRESS_BUCKETS != 1
#endif
#if defined(CYF_GP8)
        for (int i = 0; i < Cyfamate::Gp8::stacks; ++i) {
#else
        for (int i = 0; i < kLayerStacks; ++i) {
#endif 
            if (!Detail::WriteParameters<Network>(stream, networks().network[i])) return false;
        }
        return !stream.fail();
    }

    static void UpdateAccumulatorIfPossible(const Position& pos) {
        networks().feature_transformer.UpdateAccumulatorIfPossible(pos);
    }

#if defined(SFNNwoPSQT)
    static_assert(NNUE_SFNN_HAND_BUCKETS == 1 || NNUE_SFNN_HAND_BUCKETS == 4
        || NNUE_SFNN_HAND_BUCKETS == 16 || NNUE_SFNN_HAND_BUCKETS == 64
        || NNUE_SFNN_HAND_BUCKETS == 256 || NNUE_SFNN_HAND_BUCKETS == 1024,
        "unsupported NNUE_SFNN_HAND_BUCKETS");
    static_assert(
        (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_NONE && NNUE_SFNN_HAND_BUCKETS == 1)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND4 && NNUE_SFNN_HAND_BUCKETS == 4)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND16 && NNUE_SFNN_HAND_BUCKETS == 16)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND64 && NNUE_SFNN_HAND_BUCKETS == 64)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND64Z && NNUE_SFNN_HAND_BUCKETS == 64)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND256 && NNUE_SFNN_HAND_BUCKETS == 256)
        || (NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND1024 && NNUE_SFNN_HAND_BUCKETS == 1024),
        "NNUE_SFNN_HAND_BUCKET_TYPE does not match NNUE_SFNN_HAND_BUCKETS");
    static_assert(NNUE_SFNN_KING_BUCKETS == 1 || NNUE_SFNN_KING_BUCKETS == 9
        || NNUE_SFNN_KING_BUCKETS == 81 || NNUE_SFNN_KING_BUCKETS == 169
        || NNUE_SFNN_KING_BUCKETS == 441 || NNUE_SFNN_KING_BUCKETS == 841,
        "unsupported NNUE_SFNN_KING_BUCKETS");
    static_assert(
        (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_NONE && NNUE_SFNN_KING_BUCKETS == 1)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K3K3 && NNUE_SFNN_KING_BUCKETS == 9)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K9K9 && NNUE_SFNN_KING_BUCKETS == 81)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K21K21 && NNUE_SFNN_KING_BUCKETS == 441)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K29K29 && NNUE_SFNN_KING_BUCKETS == 841)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K9K9Z && NNUE_SFNN_KING_BUCKETS == 81)
        || (NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K13K13Z && NNUE_SFNN_KING_BUCKETS == 169),
        "NNUE_SFNN_KING_BUCKET_TYPE does not match NNUE_SFNN_KING_BUCKETS");
    static_assert(NNUE_SFNN_PROGRESS_BUCKETS == 1 || NNUE_SFNN_PROGRESS_BUCKETS == 2
        || NNUE_SFNN_PROGRESS_BUCKETS == 3 || NNUE_SFNN_PROGRESS_BUCKETS == 4
        || NNUE_SFNN_PROGRESS_BUCKETS == 8 || NNUE_SFNN_PROGRESS_BUCKETS == 16
        || NNUE_SFNN_PROGRESS_BUCKETS == 32,
        "unsupported NNUE_SFNN_PROGRESS_BUCKETS");
    static_assert(kLayerStacks == NNUE_SFNN_HAND_BUCKETS * NNUE_SFNN_KING_BUCKETS * NNUE_SFNN_PROGRESS_BUCKETS
                                  * NNUE_SFNN_GP_BUCKETS,
        "LayerStacks must match the SFNN bucket product");
    static_assert(NNUE_SFNN_GP_BUCKETS == 1
        || (NNUE_SFNN_HAND_BUCKETS == 1 && NNUE_SFNN_KING_BUCKETS == 1 && NNUE_SFNN_PROGRESS_BUCKETS == 1),
        "[5GP] GP buckets cannot be combined with hand/king/progress buckets");

#if NNUE_SFNN_GP_BUCKETS == 1   
    static int king3_by_king3_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        int f_rank = int(pos.square<KING>(stm)) % 9;
        int e_rank = int(pos.square<KING>(~stm)) % 9;

        if (stm == BLACK)
            e_rank = 8 - e_rank;
        else
            f_rank = 8 - f_rank;

        return (f_rank / 3) * 3 + e_rank / 3;
    }

    static int king9_by_king9_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        const auto f_king = pos.square<KING>(stm);
        const auto e_king = pos.square<KING>(~stm);
        int f_rank = int(stm == BLACK ? rank_of(f_king) : rank_of(Inv(f_king)));
        int e_rank = int(stm == BLACK ? rank_of(Inv(e_king)) : rank_of(e_king));
        if (f_rank < 0) f_rank = 0;
        if (f_rank > 8) f_rank = 8;
        if (e_rank < 0) e_rank = 0;
        if (e_rank > 8) e_rank = 8;
        return f_rank * 9 + e_rank;
    }

    static int file3_bucket(int file) {
        if (file < 0) file = 0;
        if (file > 8) file = 8;
        return file / 3;
    }

    static int king9_zone_single_bucket(Square sq) {
        int rank = int(rank_of(sq));
        int file = int(file_of(sq));
        if (rank < 0) rank = 0;
        if (rank > 8) rank = 8;

        if (rank < 3) return 0;
        if (rank < 6) return 1;
        if (rank == 6) return 2;
        return 3 + (rank - 7) * 3 + file3_bucket(file);
    }

    static int king9_zone_by_king9_zone_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        const auto f_king = pos.square<KING>(stm);
        const auto e_king = pos.square<KING>(~stm);
        const auto f_sq = stm == BLACK ? f_king : Inv(f_king);
        const auto e_sq = stm == BLACK ? Inv(e_king) : e_king;
        return king9_zone_single_bucket(f_sq) * 9 + king9_zone_single_bucket(e_sq);
    }

    static int king13_zone_single_bucket(Square sq) {
        int rank = int(rank_of(sq));
        int file = int(file_of(sq));
        if (rank < 0) rank = 0;
        if (rank > 8) rank = 8;

        if (rank < 7) return rank;
        return 7 + (rank - 7) * 3 + file3_bucket(file);
    }

    static int king13_zone_by_king13_zone_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        const auto f_king = pos.square<KING>(stm);
        const auto e_king = pos.square<KING>(~stm);
        const auto f_sq = stm == BLACK ? f_king : Inv(f_king);
        const auto e_sq = stm == BLACK ? Inv(e_king) : e_king;
        return king13_zone_single_bucket(f_sq) * 13 + king13_zone_single_bucket(e_sq);
    }

    static int king21_single_bucket(Square sq) {
        int rank = int(rank_of(sq));
        int file = int(file_of(sq));
        if (rank < 0) rank = 0;
        if (rank > 8) rank = 8;
        if (file < 0) file = 0;
        if (file > 8) file = 8;

        if (rank < 3) return 0;
        if (rank < 6) return 1;
        if (rank == 6) return 2;
        return 3 + (rank - 7) * 9 + file;
    }

    static int king21_by_king21_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        const auto f_king = pos.square<KING>(stm);
        const auto e_king = pos.square<KING>(~stm);
        const auto f_sq = stm == BLACK ? f_king : Inv(f_king);
        const auto e_sq = stm == BLACK ? Inv(e_king) : e_king;
        return king21_single_bucket(f_sq) * 21 + king21_single_bucket(e_sq);
    }

    static int king29_single_bucket(Square sq) {
        int rank = int(rank_of(sq));
        int file = int(file_of(sq));
        if (rank < 0) rank = 0;
        if (rank > 8) rank = 8;
        if (file < 0) file = 0;
        if (file > 8) file = 8;

        if (rank < 3) return 0;
        if (rank < 6) return 1;
        return 2 + (rank - 6) * 9 + file;
    }

    static int king29_by_king29_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        const auto f_king = pos.square<KING>(stm);
        const auto e_king = pos.square<KING>(~stm);
        const auto f_sq = stm == BLACK ? f_king : Inv(f_king);
        const auto e_sq = stm == BLACK ? Inv(e_king) : e_king;
        return king29_single_bucket(f_sq) * 29 + king29_single_bucket(e_sq);
    }

    static int hand4_single_bucket(Hand hand) {
        return hand_count(hand, BISHOP) > 0 ? 1 : 0;
    }

    static int hand16_single_bucket(Hand hand) {
        int bucket = 0;
        if (hand_count(hand, PAWN) > 0)
            bucket |= 1;
        if (hand_count(hand, BISHOP) > 0)
            bucket |= 2;
        return bucket;
    }

    static int hand64_single_bucket(Hand hand) {
        int bucket = 0;
        if (hand_count(hand, PAWN) + hand_count(hand, LANCE) + hand_count(hand, KNIGHT) > 0)
            bucket |= 1;
        if (hand_count(hand, GOLD) + hand_count(hand, SILVER) + hand_count(hand, ROOK) > 0)
            bucket |= 2;
        if (hand_count(hand, BISHOP) > 0)
            bucket |= 4;
        return bucket;
    }

    static int hand64z_single_bucket(Hand hand) {
        const int score =
              hand_count(hand, PAWN)
            + (hand_count(hand, LANCE) + hand_count(hand, KNIGHT)) * 2
            + (hand_count(hand, SILVER) + hand_count(hand, GOLD)) * 3
            + (hand_count(hand, BISHOP) + hand_count(hand, ROOK)) * 5;

        int bucket = (score + 3) / 4;
        if (bucket < 0) bucket = 0;
        if (bucket > 7) bucket = 7;
        return bucket;
    }

    static int hand256_single_bucket(Hand hand) {
        int bucket = 0;
        if (hand_count(hand, PAWN) + hand_count(hand, LANCE) + hand_count(hand, KNIGHT) > 0)
            bucket |= 1;
        if (hand_count(hand, SILVER) + hand_count(hand, GOLD) > 0)
            bucket |= 2;
        if (hand_count(hand, BISHOP) > 0)
            bucket |= 4;
        if (hand_count(hand, ROOK) > 0)
            bucket |= 8;
        return bucket;
    }

    static int hand1024_single_bucket(Hand hand) {
        int bucket = 0;
        if (hand_count(hand, PAWN) > 0)
            bucket |= 1;
        if (hand_count(hand, LANCE) + hand_count(hand, KNIGHT) > 0)
            bucket |= 2;
        if (hand_count(hand, SILVER) + hand_count(hand, GOLD) > 0)
            bucket |= 4;
        if (hand_count(hand, BISHOP) > 0)
            bucket |= 8;
        if (hand_count(hand, ROOK) > 0)
            bucket |= 16;
        return bucket;
    }

    static int hand4_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand4_single_bucket(pos.hand_of(stm)) * 2
            + hand4_single_bucket(pos.hand_of(~stm));
    }

    static int hand16_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand16_single_bucket(pos.hand_of(stm)) * 4
            + hand16_single_bucket(pos.hand_of(~stm));
    }

    static int hand64_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand64_single_bucket(pos.hand_of(stm)) * 8
            + hand64_single_bucket(pos.hand_of(~stm));
    }

    static int hand64z_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand64z_single_bucket(pos.hand_of(stm)) * 8
            + hand64z_single_bucket(pos.hand_of(~stm));
    }

    static int hand256_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand256_single_bucket(pos.hand_of(stm)) * 16
            + hand256_single_bucket(pos.hand_of(~stm));
    }

    static int hand1024_bucket(const Position& pos) {
        const auto stm = pos.side_to_move();
        return hand1024_single_bucket(pos.hand_of(stm)) * 32
            + hand1024_single_bucket(pos.hand_of(~stm));
    }

    static int progress_bucket(const Position& pos) {
#if NNUE_SFNN_PROGRESS_BUCKETS == 1
        return 0;
#else
#endif
    }
#endif 

    static int stack_index_for_nnue(const Position& pos) {
#if NNUE_SFNN_GP_BUCKETS != 1
        return Cyfamate::gp_stack_bucket(pos);   
#elif NNUE_SFNN_HAND_BUCKETS == 1 && NNUE_SFNN_KING_BUCKETS == 9 && NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K3K3 && NNUE_SFNN_PROGRESS_BUCKETS == 1
        return king3_by_king3_bucket(pos);
#else
        int idx = 0;

#if NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND4
        idx = hand4_bucket(pos);
#elif NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND16
        idx = hand16_bucket(pos);
#elif NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND64
        idx = hand64_bucket(pos);
#elif NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND64Z
        idx = hand64z_bucket(pos);
#elif NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND256
        idx = hand256_bucket(pos);
#elif NNUE_SFNN_HAND_BUCKET_TYPE == NNUE_SFNN_HAND_BUCKET_TYPE_HAND1024
        idx = hand1024_bucket(pos);
#endif

#if NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K3K3
        idx = idx * 9 + king3_by_king3_bucket(pos);
#elif NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K9K9
        idx = idx * 81 + king9_by_king9_bucket(pos);
#elif NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K9K9Z
        idx = idx * 81 + king9_zone_by_king9_zone_bucket(pos);
#elif NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K13K13Z
        idx = idx * 169 + king13_zone_by_king13_zone_bucket(pos);
#elif NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K21K21
        idx = idx * 441 + king21_by_king21_bucket(pos);
#elif NNUE_SFNN_KING_BUCKET_TYPE == NNUE_SFNN_KING_BUCKET_TYPE_K29K29
        idx = idx * 841 + king29_by_king29_bucket(pos);
#endif

#if NNUE_SFNN_PROGRESS_BUCKETS != 1
#endif

        if (idx < 0) idx = 0;
        if (idx >= kLayerStacks) idx = kLayerStacks - 1;
        return idx;
#endif
    }
#endif

    static Value ComputeScore(const Position& pos, bool refresh = false) {
        auto& accumulator = pos.state()->accumulator;
        if (!refresh && accumulator.computed_score) {
            return accumulator.score;
        }

        alignas(kCacheLineSize) char buffer[Network::kBufferSize];
#if defined(SFNNwoPSQT)
        const auto bucket = stack_index_for_nnue(pos);
#if defined(CYF_HALF_ACC) && defined(NNUE_HAS_SFNN_ACCUMULATOR_PROPAGATE)
#error "CYF_HALF_ACC: PropagateSfnnFromAccumulator not converted"
#endif
#if defined(USE_AVX512) && defined(NNUE_HAS_SFNN_ACCUMULATOR_PROPAGATE)
        networks().feature_transformer.EnsureAccumulator(pos, refresh);
        const auto output = networks().network[bucket].PropagateFromAccumulator(
            accumulator.accumulation, pos.side_to_move(), buffer);
#else
        alignas(kCacheLineSize) TransformedFeatureType
            transformed_features[FeatureTransformer::kBufferSize];
        networks().feature_transformer.Transform(pos, transformed_features, refresh);
#if defined(CYF_COST_ACC_EXTRA)
        for (int cyf_i = 0; cyf_i < CYF_COST_ACC_EXTRA; ++cyf_i) {
            accumulator.computed_accumulation = false;
            networks().feature_transformer.EnsureAccumulator(pos, refresh);
        }
#endif
#if defined(CYF_COST_XFORM_EXTRA)
        for (int cyf_i = 0; cyf_i < CYF_COST_XFORM_EXTRA; ++cyf_i) {
            alignas(kCacheLineSize) TransformedFeatureType cyf_features[FeatureTransformer::kBufferSize];
            networks().feature_transformer.Transform(pos, cyf_features, false);
            static thread_local volatile TransformedFeatureType cyf_sink_x;
            cyf_sink_x = cyf_features[cyf_i & 1023];
        }
#endif
#if defined(CYF_GP_INTERP)
        static constexpr double centres[5] = {0.2257811210218592, 0.4806486472259377,
          0.727059260000921, 0.9664557508027126, 1.175100027362141};
        const double gp = Cyfamate::game_phase(pos);
        int lo = 0;
        while (lo < 4 && gp >= centres[lo + 1]) ++lo;
        const int hi = gp <= centres[0] ? 0 : std::min(lo + 1, 4);
        const double a = networks().network[lo].Propagate(transformed_features, buffer)[0];
        double output[1] = {a};
        if (hi != lo) {
            const double b = networks().network[hi].Propagate(transformed_features, buffer)[0];
            const double t = std::clamp((gp - centres[lo]) / (centres[hi] - centres[lo]), 0.0, 1.0);
            output[0] = a * (1.0 - t) + b * t;
        }
#else
        const auto output = networks().network[bucket].Propagate(transformed_features, buffer);
#endif
#if defined(CYF_COST_FORWARD_EXTRA)
        for (int cyf_i = 0; cyf_i < CYF_COST_FORWARD_EXTRA; ++cyf_i) {
            alignas(kCacheLineSize) char cyf_buffer[Network::kBufferSize];
            static thread_local volatile std::int32_t cyf_sink;
            cyf_sink = networks().network[bucket].Propagate(transformed_features, cyf_buffer)[0];
        }
#endif
#endif
#else
        alignas(kCacheLineSize) TransformedFeatureType
            transformed_features[FeatureTransformer::kBufferSize];
        networks().feature_transformer.Transform(pos, transformed_features, refresh);
        const auto output = networks().network[0].Propagate(transformed_features, buffer);
#endif




        auto score = static_cast<Value>(output[0] / FV_SCALE);

        score = Math::clamp(score, -VALUE_MAX_EVAL, VALUE_MAX_EVAL);

        accumulator.score = score;
        accumulator.computed_score = true;
        return accumulator.score;
    }

}  

#if defined(USE_EVAL_HASH)

struct alignas(16) ScoreKeyValue {
#if defined(USE_SSE2)
    ScoreKeyValue() = default;
    ScoreKeyValue(const ScoreKeyValue & other) {
        static_assert(sizeof(ScoreKeyValue) == sizeof(__m128i),
            "sizeof(ScoreKeyValue) should be equal to sizeof(__m128i)");
        _mm_store_si128(&as_m128i, other.as_m128i);
    }
    ScoreKeyValue& operator=(const ScoreKeyValue & other) {
        _mm_store_si128(&as_m128i, other.as_m128i);
        return *this;
    }
#endif

    void encode() {
#if defined(USE_SSE2)
#else
        key ^= score;
#endif
    }
    void decode() { encode(); }

    union {
        struct {
            std::uint64_t key;
            std::uint64_t score;
        };
#if defined(USE_SSE2)
        __m128i as_m128i;
#endif
    };
};


struct EvaluateHashTable : HashTable<ScoreKeyValue> {};

EvaluateHashTable g_evalTable;
void EvalHash_Resize(size_t mbSize) { g_evalTable.resize(mbSize); }
void EvalHash_Clear() { g_evalTable.clear(); };

void prefetch_evalhash(const Key key) {
    constexpr auto mask = ~((u64)0x1f);
    prefetch((void*)((u64)g_evalTable[key] & mask));
}
#endif

#if defined(CYF_NNUE_MEMO)
static std::atomic<uint64_t> nnue_memo_epoch{1};
#endif
void load_eval() {
#if defined(CYF_NNUE_MEMO)
    ++nnue_memo_epoch;
#endif
    if (eval_loaded)
        return;

    {
        const std::string dir_name = Options["EvalDir"];
    #if !defined(__EMSCRIPTEN__)
		const std::string file_name = Options.count("EvalFile") ? std::string(Options["EvalFile"]) : std::string(NNUE::kFileName);
#else
        const std::string file_name = Options["EvalFile"];
    #endif
        const Tools::Result result = [&] {
            if (dir_name != "<internal>") {
                auto abs_eval_path = Path::Combine(Directory::GetBinaryFolder(), dir_name);
                const std::string file_path = Path::Combine(abs_eval_path, file_name);
                std::ifstream stream(file_path, std::ios::binary);
                sync_cout << "info string loading eval file : " << file_path << sync_endl;
				if (!stream.is_open())
					return Tools::Result(Tools::ResultCode::FileNotFound);

                return NNUE::ReadParameters(stream);
            }
            else {
                class MemoryBuffer : public std::basic_streambuf<char> {
                    public: MemoryBuffer(char* p, size_t n) {
                        std::streambuf::setg(p, p, p + n);
                        std::streambuf::setp(p, p + n);
                    }
                };

			    const auto embedded = get_embedded();

                MemoryBuffer buffer(
                              const_cast<char*>(reinterpret_cast<const char*>(embedded.data)),
                              size_t(embedded.size));

                std::istream stream(&buffer);
                sync_cout << "info string loading eval file : <internal>" << sync_endl;

                return NNUE::ReadParameters(stream);
            }
        }();


        if (result.is_not_ok())
        {
            sync_cout << "Error! : failed to read " << file_name << " : " << result.to_string() << sync_endl;
            Tools::exit();
        }

		eval_loaded = true;
    }
}


Value compute_eval(const Position& pos) {
    return NNUE::ComputeScore(pos, true);
}

Value evaluate(const Position& pos) {
    const auto& accumulator = pos.state()->accumulator;
    if (accumulator.computed_score) {
        return accumulator.score;
    }
#if defined(CYF_NNUE_MEMO)
    struct MemoEntry { uint64_t key = 0; Value value = VALUE_ZERO; bool valid = false; };
#ifndef CYF_NNUE_MEMO_SIZE
#define CYF_NNUE_MEMO_SIZE 16384
#endif
    static_assert((CYF_NNUE_MEMO_SIZE & (CYF_NNUE_MEMO_SIZE - 1)) == 0);
    static thread_local std::array<MemoEntry, CYF_NNUE_MEMO_SIZE> memo{};
    static thread_local uint64_t epoch = 0;
    const uint64_t current = nnue_memo_epoch.load(std::memory_order_relaxed);
    if (epoch != current) { memo.fill(MemoEntry{}); epoch = current; }
    const uint64_t memo_key = uint64_t(pos.key());
    auto& slot = memo[(memo_key ^ (memo_key >> 32)) & (memo.size() - 1)];
    if (slot.valid && slot.key == memo_key) {
#if defined(CYF_NNUE_MEMO_KEEP_ACC)
        NNUE::networks().feature_transformer.EnsureAccumulator(pos,false);
#endif
        return slot.value;
    }
    const Value memo_value = NNUE::ComputeScore(pos);
    slot = {memo_key, memo_value, true};
    return memo_value;
#endif

#if defined(USE_GLOBAL_OPTIONS)
    if (!GlobalOptions.use_eval_hash) {
        ASSERT_LV5(pos.state()->materialValue == Eval::material(pos));
        return NNUE::ComputeScore(pos);
    }
#endif

#if defined(USE_EVAL_HASH)
    const Key key = pos.state()->key();
    ScoreKeyValue entry = *g_evalTable[key];
    entry.decode();
    if (entry.key == key) {
        return Value(entry.score);
    }
#endif

    Value score = NNUE::ComputeScore(pos);
#if defined(USE_EVAL_HASH)
    entry.key = key;
    entry.score = score;
    entry.encode();
    *g_evalTable[key] = entry;
#endif

    return score;
}

void evaluate_with_no_return(const Position& pos) {
    NNUE::UpdateAccumulatorIfPossible(pos);
}

void print_eval_stat(Position& ) {
    std::cout << "--- EVAL STAT: not implemented" << std::endl;
}

} 
} 

#endif  
