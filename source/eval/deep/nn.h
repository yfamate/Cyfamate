#ifndef __NN_H_INCLUDED__
#define __NN_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "nn_types.h"

namespace YaneuraOu {
namespace Eval::dlshogi {

	class NN
	{
	public:
		void* alloc(size_t size);

		void free(void* ptr);

		virtual void forward(const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2) = 0;

		virtual void forward(const int slot_id, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2)
		{
			(void)slot_id;
			forward(batch_size, p1, p2, x1, x2, y1, y2);
		}

		virtual Tools::Result load(const std::string& model_path, int gpu_id , int batch_size) = 0;

		virtual void set_profile_count(int profile_count) { (void)profile_count; }

		virtual int slot_capacity() const { return 1; }

		virtual void prepare_slots(int slot_count) { (void)slot_count; }

		static int get_device_count();

		virtual void set_device(int gpu_id) {};

		static std::shared_ptr<NN> build_nn(const std::string& model_path, int gpu_id, int batch_size, int profile_count = 1);

		virtual ~NN() {}
	};

} 
} 

#endif 
#endif 
