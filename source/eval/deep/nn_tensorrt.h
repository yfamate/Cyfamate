#ifndef __NN_TENSORRT_H_INCLUDED__
#define __NN_TENSORRT_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP) && defined(TENSOR_RT)


#include <cuda_runtime.h>

#include "NvInferRuntimeCommon.h"
#include "NvInfer.h"
#include "NvOnnxParser.h"

#include "nn.h"

#include <memory>
#include <mutex>
#include <vector>

namespace YaneuraOu {

void checkCudaErrors(cudaError_t status);

namespace Eval::dlshogi
{
	struct InferDeleter
	{
		template <typename T>
		void operator()(T* obj) const
		{
			if (obj)
			{
#if NV_TENSORRT_MAJOR >= 8
				delete obj;
#else
				obj->destroy();
#endif
			}
		}
	};

	template <typename T>
	using InferUniquePtr = std::unique_ptr<T, InferDeleter>;

	class NNTensorRT : public NN {
	public:
		NNTensorRT();
		virtual ~NNTensorRT();

		virtual Tools::Result load(const std::string& model_path , int gpu_id , int max_batch_size);

		virtual void forward(const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2);
		virtual void forward(const int slot_id, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2);

		virtual void set_profile_count(int profile_count);
		virtual int slot_capacity() const;
		virtual void prepare_slots(int slot_count);

		static int get_device_count();

		virtual void set_device(int gpu_id);

	private:
		struct InferenceSlot;

		std::unique_ptr<InferenceSlot> create_slot(int profile_index);
		InferenceSlot* get_slot(int slot_id);
		void forward_impl(InferenceSlot* slot, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2);

		void release();

		Tools::Result load_model(const std::string& model_path);

		int gpu_id;
		int max_batch_size;
		int profile_count = 1;

		u32 unpack_size1;
		u32 unpack_size2;
		InferUniquePtr<nvinfer1::ICudaEngine> infer_engine;
		std::vector<std::unique_ptr<InferenceSlot>> slots;
		std::mutex slots_mutex;
		nvinfer1::Dims inputDims1;
		nvinfer1::Dims inputDims2;

		void build(const std::string& onnx_filename);
	};

} 
} 

#endif 
#endif 
