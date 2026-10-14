// SFNN without PSQT architecture

#ifndef CLASSIC_NNUE_SFNN_SFNN_HALFKA2_1024_7_64_K3K3_H_INCLUDED
#define CLASSIC_NNUE_SFNN_SFNN_HALFKA2_1024_7_64_K3K3_H_INCLUDED

#include "../features/feature_set.h"

#include "../features/half_ka2.h"

#include "sfnn_network.h"

namespace YaneuraOu {
namespace Eval::NNUE {

// Input features used in evaluation function
// 評価関数で用いる入力特徴量

    using RawFeatures = Features::FeatureSet<
        Features::HalfKA2<Features::Side::kFriend>>;

    // Number of input feature dimensions after conversion
    // 変換後の入力特徴量の次元数
    // [GP8 research] FT width per build (512 / 768 / 1024 / 1536); the file header must agree.
#if !defined(CYF_FT_WIDTH)
#define CYF_FT_WIDTH 1024
#endif
    constexpr IndexType kTransformedFeatureDimensions = CYF_FT_WIDTH;

    // 小幅SFNN専用。従来幅のSFNNでは定義せず、既存の高速経路をそのまま使う。
    

    // Number of networks stored in the evaluation file
    // [5GP] GP で選ぶ 5 組。組 i = GP が [kGpStackBounds[i-1], kGpStackBounds[i]) に入る局面 (両端は -∞ / +∞)。
    //   玉バケット・手駒バケット・進行度バケットは使わない (いずれも 1)。
    // [GP8 research] capacity of the stack array; the live count and the GP boundaries are
    //   read from the evaluation-file header (Cyfamate::Gp8). GP5 files use 5 of them.
    constexpr int LayerStacks = 9;

    #define NNUE_SFNN_HAND_BUCKETS 1
    #define NNUE_SFNN_HAND_BUCKET_TYPE 0
    #define NNUE_SFNN_KING_BUCKETS 1
    #define NNUE_SFNN_KING_BUCKET_TYPE 0
    #define NNUE_SFNN_PROGRESS_BUCKETS 1
    #define NNUE_SFNN_GP_BUCKETS 9   // [GP8 research] = LayerStacks capacity

    // Number of groups for the first affine layer of SFNN.
    // common+shard fc_0でのみ2以上になる。
    constexpr IndexType kHidden1GroupCount = 1;

    // common+shard fc_0 settings. kHidden1ShardDimensions is per shard.
    constexpr bool kHidden1UsesCommonShard = false;
    constexpr IndexType kHidden1CommonDimensions = 0;
    constexpr IndexType kHidden1ShardDimensions = 0;

    // 各層の次元数
    constexpr IndexType kInputDims   = kTransformedFeatureDimensions;
    // [GP8 research] fc_0 main outputs: 7 (current form) or 15 (R systems); + shortcut.
#if !defined(CYF_H1)
#define CYF_H1 7
#endif
    constexpr IndexType kHidden1Dims = CYF_H1;
    constexpr bool kUseShortcut = true;
    constexpr IndexType kHidden1OutputDims = kHidden1Dims + (kUseShortcut ? 1 : 0);
    constexpr IndexType kHidden2Dims = 64;                              

    

    using Fc0Layer = Layers::AffineTransformSparseInputExplicit<kInputDims, kHidden1OutputDims>;
    using NetworkBase = SfnnNetwork<Fc0Layer, kInputDims, kHidden1Dims, kHidden2Dims, kUseShortcut>;

    struct Network : NetworkBase {
        static std::string GetStructureString() {
            // [GP8 research] stack count is appended by GetArchitectureString().
            return "SFNN_HALFKA2_" + std::to_string(kTransformedFeatureDimensions) + "_"
                 + std::to_string(kHidden1Dims) + "_" + std::to_string(kHidden2Dims);
        }
    };

}  // namespace Eval::NNUE
}  // namespace YaneuraOu

#endif // CLASSIC_NNUE_SFNN_HALFKA2_1024_7_64_K3K3_H_INCLUDED
