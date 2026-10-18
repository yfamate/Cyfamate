
#ifndef CLASSIC_NNUE_SFNN_1536_H_INCLUDED
#define CLASSIC_NNUE_SFNN_1536_H_INCLUDED

#include "../features/feature_set.h"
#include "../features/half_ka_hm2.h"

#include "sfnn_network.h"

namespace YaneuraOu {
namespace Eval::NNUE {

using RawFeatures = Features::FeatureSet<
    Features::HalfKA_hm2<Features::Side::kFriend>>;

constexpr IndexType kTransformedFeatureDimensions = 1536;

constexpr int LayerStacks = 9;

constexpr IndexType kInputDims   = kTransformedFeatureDimensions;
constexpr IndexType kHidden1Dims = 15;
constexpr IndexType kHidden2Dims = 32;

using Fc0Layer = Layers::AffineTransformSparseInputExplicit<kInputDims, kHidden1Dims + 1>;
using NetworkBase = SfnnNetwork<Fc0Layer, kInputDims, kHidden1Dims, kHidden2Dims>;

struct Network : NetworkBase {
	static std::string GetStructureString() {
		return "SFNN-1536";
	}
};

}  
}  

#endif 
