#ifndef __DLSHOGI_EVALUATE_H_INCLUDED__
#define __DLSHOGI_EVALUATE_H_INCLUDED__

#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)

#include "../../position.h"

#include <cstddef>
#include <string>
#include <vector>

#if defined(TENSOR_RT)
#define UNPACK_CUDA
#endif

#if defined(TRT_NN_FP16)
#include <cuda_fp16.h>
#endif

namespace YaneuraOu {
namespace Eval::dlshogi {

	void init();




	constexpr int MAX_HPAWN_NUM   = 8; 
	constexpr int MAX_HLANCE_NUM  = 4;
	constexpr int MAX_HKNIGHT_NUM = 4;
	constexpr int MAX_HSILVER_NUM = 4;
	constexpr int MAX_HGOLD_NUM   = 4;
	constexpr int MAX_HBISHOP_NUM = 2;
	constexpr int MAX_HROOK_NUM   = 2;

	const int MAX_PIECES_IN_HAND[] =
	{
		MAX_HPAWN_NUM   , 
		MAX_HLANCE_NUM  , 
		MAX_HKNIGHT_NUM , 
		MAX_HSILVER_NUM , 
		MAX_HGOLD_NUM   , 
		MAX_HBISHOP_NUM , 
		MAX_HROOK_NUM   , 
	};

	constexpr u32 MAX_PIECES_IN_HAND_SUM = MAX_HPAWN_NUM + MAX_HLANCE_NUM + MAX_HKNIGHT_NUM + MAX_HSILVER_NUM + MAX_HGOLD_NUM + MAX_HBISHOP_NUM + MAX_HROOK_NUM;

	constexpr u32 MAX_FEATURES2_HAND_NUM = (int)COLOR_NB * MAX_PIECES_IN_HAND_SUM;

	const int PIECETYPE_NUM = 14;

	const int PieceTypeNum = PieceType::DRAGON + 1;

	const int HandPieceNum = 7;

	constexpr int MAX_ATTACK_NUM = 3; 

	constexpr u32 MAX_FEATURES1_NUM = PIECETYPE_NUM + PIECETYPE_NUM + MAX_ATTACK_NUM;

	constexpr u32 FEATURES2_NUM_WCSC28 = MAX_FEATURES2_HAND_NUM + 1;

	constexpr u32 MAX_NYUGYOKU_OPP_FIELD = 10; 
	constexpr u32 MAX_NYUGYOKU_SCORE     = 20; 
	constexpr u32 MAX_FEATURES2_NYUGYOKU_NUM = 1 + MAX_NYUGYOKU_OPP_FIELD + MAX_NYUGYOKU_SCORE;
	constexpr u32 FEATURES2_NUM_WCSC35 = FEATURES2_NUM_WCSC28 + (int)COLOR_NB * MAX_FEATURES2_NYUGYOKU_NUM;

	constexpr const char* MODEL_ARCHITECTURE_WCSC28 = "dlshogi-WCSC28";
	constexpr const char* MODEL_ARCHITECTURE_WCSC35 = "dlshogi-WCSC35";
	constexpr const char* DEFAULT_MODEL_ARCHITECTURE = MODEL_ARCHITECTURE_WCSC28;

	struct InputFeatureSpec
	{
		std::string architecture;
		u32 features1_channels;
		u32 features2_channels;
		bool use_nyugyoku_features;
	};

	const std::vector<std::string>& model_architecture_names();
	bool set_model_architecture(const std::string& architecture);
	const InputFeatureSpec& input_feature_spec();

	inline size_t input1_element_count(const int batch_size) {
		return (size_t)batch_size * input_feature_spec().features1_channels * (size_t)SQ_NB;
	}

	inline size_t input2_element_count(const int batch_size) {
		return (size_t)batch_size * input_feature_spec().features2_channels * (size_t)SQ_NB;
	}

	inline size_t packed_input1_byte_count(const int batch_size) {
		return (input1_element_count(batch_size) + 7) >> 3;
	}

	inline size_t packed_input2_byte_count(const int batch_size) {
		return ((size_t)batch_size * input_feature_spec().features2_channels + 7) >> 3;
	}

	enum MOVE_DIRECTION {
		UP, UP_LEFT, UP_RIGHT, LEFT, RIGHT, DOWN, DOWN_LEFT, DOWN_RIGHT, UP2_LEFT, UP2_RIGHT,
		UP_PROMOTE, UP_LEFT_PROMOTE, UP_RIGHT_PROMOTE, LEFT_PROMOTE, RIGHT_PROMOTE, DOWN_PROMOTE, DOWN_LEFT_PROMOTE, DOWN_RIGHT_PROMOTE, UP2_LEFT_PROMOTE, UP2_RIGHT_PROMOTE,
		MOVE_DIRECTION_NUM,
		MOVE_DIRECTION_NONE = -1 
	};

	const MOVE_DIRECTION MOVE_DIRECTION_PROMOTED[] = {
		UP_PROMOTE, UP_LEFT_PROMOTE, UP_RIGHT_PROMOTE, LEFT_PROMOTE, RIGHT_PROMOTE, DOWN_PROMOTE, DOWN_LEFT_PROMOTE, DOWN_RIGHT_PROMOTE, UP2_LEFT_PROMOTE, UP2_RIGHT_PROMOTE
	};

	constexpr int MAX_MOVE_LABEL_NUM = MOVE_DIRECTION_NUM + HandPieceNum;

#if defined(TRT_NN_FP16)
	typedef uint8_t PType;
	typedef __half DType;
	extern const DType dtype_zero;
	extern const DType dtype_one;
	inline float to_float(const DType x) {
		return __half2float(x);
	}
	inline DType to_dtype(const float x) {
		return __float2half(x);
	}
#else
	typedef uint8_t PType;
	typedef float DType;
	constexpr const DType dtype_zero = 0.0f; 
	constexpr const DType dtype_one  = 1.0f; 
	inline float to_float(const DType x) {
		return x;
	}
	inline float to_dtype(const float x) {
		return x;
	}
#endif

	typedef DType NN_Input1;
	typedef DType NN_Input2;

	typedef DType NN_Output_Value;

	typedef DType NN_Output_Policy[MAX_MOVE_LABEL_NUM * int(SQ_NB)];

	void make_input_features(const Position& position, int batch_index, PType* packed_features1, PType* packed_features2);

	void extract_input_features(int batch_size, PType* packed_features1, PType* packed_features2, NN_Input1* features1, NN_Input2* features2);

	int make_move_label(Move move, Color color);

	void softmax_temperature_with_normalize(std::vector<float>& log_probabilities);

	void set_softmax_temperature(const float temperature);


	float cp_to_value(const Value score , const float eval_coef);

	Value value_to_cp(const float score , const float eval_coef);


} 
} 

#endif 
#endif 
