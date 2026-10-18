
#ifndef CLASSIC_NNUE_HALFKPE9_256X2_32_32_H_INCLUDED
#define CLASSIC_NNUE_HALFKPE9_256X2_32_32_H_INCLUDED

#include "../features/feature_set.h"
#include "../features/half_kpe9.h"

#include "../layers/input_slice.h"
#include "../layers/affine_transform.h"
#include "../layers/affine_transform_sparse_input.h"
#include "../layers/clipped_relu.h"

namespace YaneuraOu {
namespace Eval {

namespace NNUE {

	using RawFeatures = Features::FeatureSet<
	    Features::HalfKPE9<Features::Side::kFriend>>;
	
	constexpr IndexType kTransformedFeatureDimensions = 256;
	
	namespace Layers {
	
	using InputLayer = InputSlice<kTransformedFeatureDimensions * 2>;
	using HiddenLayer1 = ClippedReLU<AffineTransformSparseInput<InputLayer, 32>>;
	using HiddenLayer2 = ClippedReLU<AffineTransform<HiddenLayer1, 32>>;
	using OutputLayer = AffineTransform<HiddenLayer2, 1>;

}  

using Network = Layers::OutputLayer;

} 

} 
} 

#endif 
