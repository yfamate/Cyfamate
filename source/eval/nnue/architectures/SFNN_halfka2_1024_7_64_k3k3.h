
#ifndef CLASSIC_NNUE_SFNN_SFNN_HALFKA2_1024_7_64_K3K3_H_INCLUDED
#define CLASSIC_NNUE_SFNN_SFNN_HALFKA2_1024_7_64_K3K3_H_INCLUDED

#include "../features/feature_set.h"

#include "../features/half_ka2.h"

#include "sfnn_network.h"

namespace YaneuraOu {
namespace Eval::NNUE {


    using RawFeatures = Features::FeatureSet<
        Features::HalfKA2<Features::Side::kFriend>>;

#if defined(CYF_GP8)
#if !defined(CYF_FT_WIDTH)
#define CYF_FT_WIDTH 1024
#endif
    constexpr IndexType kTransformedFeatureDimensions = CYF_FT_WIDTH;
#else
    constexpr IndexType kTransformedFeatureDimensions = 1024;
#endif 

    

#if defined(CYFAMATE_NNUE_5GP)
#if defined(CYF_GP8)
    constexpr int LayerStacks = 9;
#else
    constexpr int LayerStacks = 5;
    constexpr double kGpStackBounds[LayerStacks - 1] = {0.4, 0.6, 0.8, 1.0};
#endif 

    #define NNUE_SFNN_HAND_BUCKETS 1
    #define NNUE_SFNN_HAND_BUCKET_TYPE 0
    #define NNUE_SFNN_KING_BUCKETS 1
    #define NNUE_SFNN_KING_BUCKET_TYPE 0
    #define NNUE_SFNN_PROGRESS_BUCKETS 1
#if defined(CYF_GP8)
    #define NNUE_SFNN_GP_BUCKETS 9   
#else
    #define NNUE_SFNN_GP_BUCKETS 5
#endif 
#else
    constexpr int LayerStacks = 9;

    #define NNUE_SFNN_HAND_BUCKETS 1
    #define NNUE_SFNN_HAND_BUCKET_TYPE 0
    #define NNUE_SFNN_KING_BUCKETS 9
    #define NNUE_SFNN_KING_BUCKET_TYPE 1
    #define NNUE_SFNN_PROGRESS_BUCKETS 1
#endif

    constexpr IndexType kHidden1GroupCount = 1;

    constexpr bool kHidden1UsesCommonShard = false;
    constexpr IndexType kHidden1CommonDimensions = 0;
    constexpr IndexType kHidden1ShardDimensions = 0;

    constexpr IndexType kInputDims   = kTransformedFeatureDimensions;
#if defined(CYF_GP8)
#if !defined(CYF_H1)
#define CYF_H1 7
#endif
    constexpr IndexType kHidden1Dims = CYF_H1;
#else
    constexpr IndexType kHidden1Dims = 7;
#endif 
    constexpr bool kUseShortcut = true;
    constexpr IndexType kHidden1OutputDims = kHidden1Dims + (kUseShortcut ? 1 : 0);
    constexpr IndexType kHidden2Dims = 64;                              

    

    using Fc0Layer = Layers::AffineTransformSparseInputExplicit<kInputDims, kHidden1OutputDims>;
    using NetworkBase = SfnnNetwork<Fc0Layer, kInputDims, kHidden1Dims, kHidden2Dims, kUseShortcut>;

    struct Network : NetworkBase {
        static std::string GetStructureString() {
#if defined(CYFAMATE_NNUE_5GP)
#if defined(CYF_GP8)
            return "SFNN_HALFKA2_" + std::to_string(kTransformedFeatureDimensions) + "_"
                 + std::to_string(kHidden1Dims) + "_" + std::to_string(kHidden2Dims);
#else
            return "SFNN_HALFKA2_1024_7_64_5GP";
#endif 
#else
            return "SFNN_HALFKA2_1024_7_64_K3K3";
#endif
        }
    };

}  
}  

#endif 
