#ifndef CLASSIC_NNUE_HALFKP_1024X2_8_64_H_INCLUDED
#define CLASSIC_NNUE_HALFKP_1024X2_8_64_H_INCLUDED

#include "../features/feature_set.h"
#include "../features/half_kp.h"

#include "../layers/input_slice.h"
#include "../layers/affine_transform.h"
#include "../layers/affine_transform_sparse_input.h"
#include "../layers/clipped_relu.h"

namespace YaneuraOu {
namespace Eval::NNUE {

	using RawFeatures = Features::FeatureSet<
		Features::HalfKP<Features::Side::kFriend>>;

	constexpr IndexType kTransformedFeatureDimensions = 1024;

	namespace Layers {

		using InputLayer = InputSlice<kTransformedFeatureDimensions * 2>;
		using HiddenLayer1 = ClippedReLU<AffineTransformSparseInput<InputLayer, 8>>;
		using HiddenLayer2 = ClippedReLU<AffineTransform<HiddenLayer1, 64>>;
		using OutputLayer = AffineTransform<HiddenLayer2, 1>;

	}  

	using Network = Layers::OutputLayer;

} 
} 

#endif 
