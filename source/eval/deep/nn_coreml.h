#ifndef __NN_COREML_H_INCLUDED__
#define __NN_COREML_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)
#if defined(COREML)


#include "nn.h"
#include "nn_types.h"

namespace YaneuraOu {
namespace Eval::dlshogi {

	class NNCoreML : public NN
	{
	public:
		virtual ~NNCoreML();

		virtual Tools::Result load(const std::string& model_path , int gpu_id , int batch_size);

		virtual void forward(const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2);

		static int get_device_count();

	private:
		int fixed_batch_size; 
		void* model; 
		DType* input_buf;

	};

} 
} 

#endif 
#endif 
#endif 

