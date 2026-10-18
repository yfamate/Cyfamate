#ifndef _EVALUATE_IO_H_
#define _EVALUATE_IO_H_


#include "../types.h"
#include <vector>

namespace YaneuraOu {
namespace EvalIO {

	enum EvalFeature { KK , KKP, KPP, PP , KPPP, KKPP , VAR };

	struct FileOrMemory
	{
		FileOrMemory(std::string filename_) : filename(filename_) , ptr(nullptr) {}
		FileOrMemory(void* ptr_) : ptr(ptr_){}

		bool memory() const { return ptr != nullptr; }

		bool file() const { return ptr == nullptr; }

		void* ptr;
		
		std::string filename;
	};


	struct EvalArrayInfo
	{
		EvalArrayInfo(EvalFeature feature_, u64 element_size_ , u64 element_num_ , FileOrMemory file_or_memory_) :
			feature(feature_) , element_size(element_size_) , element_num(element_num_) , file_or_memory(file_or_memory_)
		{}
			
		EvalFeature feature;
		
		u64 element_size;

		u64 element_num;

		FileOrMemory file_or_memory;
	};

	struct EvalInfo
	{
		EvalInfo(u64 SQ_NB_, u64 fe_end_ ) : sq_nb(SQ_NB_), fe_end(fe_end_) {}

		u64 sq_nb;

		u64 fe_end;

		std::vector<EvalArrayInfo> eval_info_array;


		template <typename T1,typename T2, typename T3>
		static EvalInfo build_kpp(T1 kk_, T2 kkp_, T3 kpp_)
		{
			EvalInfo ei(81  ,1535 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 1 , FileOrMemory(kk_ ))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 1 , FileOrMemory(kkp_))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 1 , FileOrMemory(kpp_))); 
			return ei;
		}

		template <typename T1, typename T2, typename T3>
		static EvalInfo build_kppt32(T1 kk_, T2 kkp_, T3 kpp_)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 2 , FileOrMemory(kk_ ))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2 , FileOrMemory(kkp_))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 2 , FileOrMemory(kpp_))); 
			return ei;
		}

		template <typename T1, typename T2, typename T3>
		static EvalInfo build_kppt16(T1 kk_, T2 kkp_, T3 kpp_)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 2, 2 , FileOrMemory(kk_  ))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 2, 2 , FileOrMemory(kkp_ ))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 2 , FileOrMemory(kpp_ ))); 
			return ei;
		}

		template <typename T1, typename T2, typename T3>
		static EvalInfo build_kpp_kkpt32(T1 kk_, T2 kkp_, T3 kpp_)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 2, FileOrMemory(kk_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2, FileOrMemory(kkp_))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 1, FileOrMemory(kpp_))); 
			return ei;
		}

		template <typename T1, typename T2, typename T3, typename T4>
		static EvalInfo build_kppp_kkpt32(T1 kk_, T2 kkp_, T3 kpp_ , T4 kppp_ , u64 size_of_kppp)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK  , 4, 2, FileOrMemory(kk_)));   
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP , 4, 2, FileOrMemory(kkp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP , 2, 1, FileOrMemory(kpp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(VAR , 2, size_of_kppp / 2 , FileOrMemory(kppp_))); 
			return ei;
		}
		template <typename T1, typename T2, typename T3, typename T4>
		static EvalInfo build_kpppt32(T1 kk_, T2 kkp_, T3 kpp_, T4 kppp_, u64 size_of_kppp)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 2, FileOrMemory(kk_)));   
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2, FileOrMemory(kkp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 2, FileOrMemory(kpp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(VAR, 2, size_of_kppp /2 , FileOrMemory(kppp_))); 
			return ei;
		}
		template <typename T1, typename T2, typename T3, typename T4>
		static EvalInfo build_kkpp_kkpt32(T1 kk_, T2 kkp_, T3 kpp_, T4 kkpp_, u64 size_of_kkpp)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 2, FileOrMemory(kk_)));   
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2, FileOrMemory(kkp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 1, FileOrMemory(kpp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(VAR, 2, size_of_kkpp / 2, FileOrMemory(kkpp_))); 
			return ei;
		}
		template <typename T1, typename T2, typename T3, typename T4>
		static EvalInfo build_kkppt32(T1 kk_, T2 kkp_, T3 kpp_, T4 kkpp_, u64 size_of_kkpp)
		{
			EvalInfo ei(81 , 1548 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK , 4, 2, FileOrMemory(kk_)));   
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2, FileOrMemory(kkp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 2, FileOrMemory(kpp_)));  
			ei.eval_info_array.emplace_back(EvalArrayInfo(VAR, 2, size_of_kkpp / 2, FileOrMemory(kkpp_))); 
			return ei;
		}
		template <typename T1, typename T2, typename T3>
		static EvalInfo build_nabla(T1 kk_, T2 kkp_, T3 kpp_)
		{
			EvalInfo ei(81 , 1548 + 1024*4 );
			ei.eval_info_array.emplace_back(EvalArrayInfo(KK, 4, 2, FileOrMemory(kk_)));   
			ei.eval_info_array.emplace_back(EvalArrayInfo(KKP, 4, 2, FileOrMemory(kkp_))); 
			ei.eval_info_array.emplace_back(EvalArrayInfo(KPP, 2, 1, FileOrMemory(kpp_))); 
			return ei;
		}

	};

	bool eval_convert(const EvalInfo& input, const EvalInfo& output, const std::vector<u16 >* map);

} 
} 

#endif 
