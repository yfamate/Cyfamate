#include "evaluate_io.h"
#include "../misc.h"
#include "../usi.h"

#include <fstream>
#include <cstring>	

namespace YaneuraOu {
namespace EvalIO {

	bool eval_convert(const EvalInfo& input, const EvalInfo& output, const std::vector<u16>* map)
	{
		if (input.eval_info_array.size() != output.eval_info_array.size())
		{
			std::cout << "Error! input.eval_info_array.size() != output.eval_info_array.size() in eval_io()" << std::endl;
			return false;
		}

		int size = (int)input.eval_info_array.size();
		for (int i = 0; i < size; ++i)
		{
			auto in_ = input.eval_info_array[i];
			auto out_ = output.eval_info_array[i];

			if (in_.feature != out_.feature)
			{
				std::cout << "Error! input.feature != output.feature in eval_io()" << std::endl;
				return false;
			}

			auto calc_block_size = [](u64 element_num , u64 element_size , EvalFeature feature , u64 sq_nb , u64 fe_end)
			{
				u64 block_size = element_size * element_num;
				switch (feature)
				{
				case KK  : block_size *= (sq_nb  ) * (sq_nb  )                      ; break;
				case KKP : block_size *= (sq_nb  ) * (sq_nb  ) * (fe_end)           ; break;
				case KPP : block_size *= (sq_nb  ) * (fe_end ) * (fe_end)           ; break;
				case PP  : block_size *= (fe_end ) * (fe_end )                      ; break;
				case KKPP: block_size *= (sq_nb  ) * (sq_nb  ) * (fe_end) * (fe_end); break;
				case KPPP: block_size *= (sq_nb  ) * (fe_end ) * (fe_end) * (fe_end); break;
				case VAR : block_size *= 1; break;
				default:
					ASSERT_LV1(false);
				}
				return block_size;
			};

			auto input_block_size = calc_block_size(in_.element_num, in_.element_size, in_.feature, input.sq_nb, input.fe_end);
			auto output_block_size = calc_block_size(out_.element_num, out_.element_size, out_.feature, output.sq_nb, output.fe_end);

			bool convertless = 
					   in_.element_size == out_.element_size
					&& in_.element_num == out_.element_num
					&& input.sq_nb == output.sq_nb
					&& input.fe_end == output.fe_end
					&& map == nullptr;

			if (convertless)
			{
				if (in_.file_or_memory.memory() && out_.file_or_memory.memory())
				{
					memcpy(out_.file_or_memory.ptr, in_.file_or_memory.ptr, input_block_size);
				}
				else if (in_.file_or_memory.file() && out_.file_or_memory.memory())
				{
					if (SystemIO::ReadFileToMemory(in_.file_or_memory.filename, [&](size_t size) {
						if ((u64)size != input_block_size)
						{
							std::cout << "info string Error! : file size incorrect , file = " << in_.file_or_memory.filename
								<< " , actual size = "<< size << " , needed_size = " << input_block_size << std::endl;
							return (void*)nullptr;
						}
						return out_.file_or_memory.ptr;
					}).is_not_ok() )
					{
						std::cout << "info string Error! : read file error , file = " << in_.file_or_memory.filename << std::endl;
						return false;
					}
				}
				else if (in_.file_or_memory.memory() && out_.file_or_memory.file())
				{
					if (SystemIO::WriteMemoryToFile(out_.file_or_memory.filename, in_.file_or_memory.ptr, output_block_size).is_not_ok())
					{
						std::cout << "info string write file error , file = " << out_.file_or_memory.filename << std::endl;
						return false;
					};
				}
				else if (in_.file_or_memory.file() && out_.file_or_memory.file())
				{
					std::vector<u8> buffer(input_block_size);
					std::ifstream ifs(in_.file_or_memory.filename, std::ios::binary);
					if (ifs) ifs.read(reinterpret_cast<char*>(buffer.data()), input_block_size);
					else
					{
						std::cout << "info string read file error , file = " << in_.file_or_memory.filename << std::endl;
						return false;
					};
					std::ofstream ofs(out_.file_or_memory.filename, std::ios::binary);
					if (ofs) ofs.write(reinterpret_cast<char*>(buffer.data()), output_block_size);
					else
					{
						std::cout << "info string write file error , file = " << out_.file_or_memory.filename << std::endl;
						return false;
					}
				}
			}
			else {
				

				std::vector<u8> output_buffer;
				void* out_ptr = out_.file_or_memory.ptr;
				if (out_ptr == nullptr)
				{
					output_buffer.resize(output_block_size);
					out_ptr = (void*)&output_buffer[0];
				}


				std::vector<u8> input_buffer;
				void* in_ptr = in_.file_or_memory.ptr;
				if (in_ptr == nullptr)
				{
					input_buffer.resize(input_block_size);
					in_ptr = (void*)&input_buffer[0];

					if (SystemIO::ReadFileToMemory(in_.file_or_memory.filename, [&](size_t file_size) {
						if ((u64)file_size != input_block_size)
						{
							std::cout << "info string Error! file_size = " << file_size << " , input_block_size = " << input_block_size << std::endl;
							return (void*)nullptr;
						}
						return in_ptr;
					}).is_not_ok())
						return false;
				}


				u64 input_feature_size = in_.element_size * in_.element_num;
				u64 output_feature_size = out_.element_size * out_.element_num;
				auto conv = [&](u8* src, u8* dst)
				{
					for (u64 i = 0; i < out_.element_num; ++i)
					{
						s64 n;
						if (i < in_.element_num)
						{
							switch (in_.element_size)
							{
							case 1: n = *(s8*)src; break;
							case 2: n = *(s16*)src; break;
							case 4: n = *(s32*)src; break;
							case 8: n = *(s64*)src; break;
							default:
								UNREACHABLE;
							}
						}
						else {
							n = 0;
						}

						switch (out_.element_size)
						{
						case 1: *(s8* )dst = (s8 )n; break;
						case 2: *(s16*)dst = (s16)n; break;
						case 4: *(s32*)dst = (s32)n; break;
						case 8: *(s64*)dst = (s64)n; break;
						default:
							UNREACHABLE;
						}

						src += in_.element_size;
						dst += out_.element_size;
					}
				};
				switch (in_.feature)
				{
				case KK:
					for(u64 k1=0;k1 < output.sq_nb ; ++k1)
						for (u64 k2 = 0; k2 < output.sq_nb; ++k2)
						{
							u64 input_index = (k1)* input.sq_nb + (k2);
							u64 output_index = (k1)* output.sq_nb + (k2);
							conv((u8*)in_ptr + input_index * input_feature_size , (u8*)out_ptr + output_index * output_feature_size);
						}
					break;

				case KKP:
					for (u64 k1 = 0; k1 < output.sq_nb; ++k1)
						for (u64 k2 = 0; k2 < output.sq_nb; ++k2)
							for (u64 p1 = 0; p1< output.fe_end;++p1)
							{
								u64 input_p1 = map == nullptr ? p1 : map->at(p1);
								if (input_p1 >= input.fe_end)
									input_p1 = 0;
								u64 input_index  = ((k1)* input.sq_nb  + (k2)) * input.fe_end  + input_p1;
								u64 output_index = ((k1)* output.sq_nb + (k2)) * output.fe_end +       p1;
								conv((u8*)in_ptr + input_index * input_feature_size, (u8*)out_ptr + output_index * output_feature_size);
							}
					break;

				case KPP:
					for (u64 k1 = 0; k1 < output.sq_nb; ++k1)
						for (u64 p1 = 0; p1 < output.fe_end; ++p1)
						{
							u64 input_p1 = map == nullptr ? p1 : map->at(p1);
							if (input_p1 >= input.fe_end)
								input_p1 = 0;
							for (u64 p2 = 0; p2 < output.fe_end; ++p2)
							{
								u64 input_p2 = map == nullptr ? p2 : map->at(p2);
								if (input_p2 >= input.fe_end)
									input_p2 = 0;
								u64 input_index  = ((k1)* input.fe_end  + (input_p1)) * input.fe_end  + input_p2;
								u64 output_index = ((k1)* output.fe_end + (      p1)) * output.fe_end +       p2;
								conv((u8*)in_ptr + input_index * input_feature_size, (u8*)out_ptr + output_index * output_feature_size);
							}
						}
					break;


				case PP:
					for (u64 p1 = 0; p1 < output.fe_end; ++p1)
					{
						u64 input_p1 = map == nullptr ? p1 : map->at(p1);
						if (input_p1 >= input.fe_end)
							input_p1 = 0;
						for (u64 p2 = 0; p2 < output.fe_end; ++p2)
						{
							u64 input_p2 = map == nullptr ? p2 : map->at(p2);
							if (input_p2 >= input.fe_end)
								input_p2 = 0;
							u64 input_index =  (input_p1) * input.fe_end  + input_p2;
							u64 output_index = (      p1) * output.fe_end + p2;
							conv((u8*)in_ptr + input_index * input_feature_size, (u8*)out_ptr + output_index * output_feature_size);
						}
					}
					break;

				case KKPP:
					for (u64 k1 = 0; k1 < output.sq_nb; ++k1)
						for (u64 k2 = 0; k2 < output.sq_nb; ++k2)
							for (u64 p1 = 0; p1 < output.fe_end; ++p1)
							{
								u64 input_p1 = map == nullptr ? p1 : map->at(p1);
								if (input_p1 >= input.fe_end)
									input_p1 = 0;
								for (u64 p2 = 0; p2 < output.fe_end; ++p2)
								{
									u64 input_p2 = map == nullptr ? p2 : map->at(p2);
									if (input_p2 >= input.fe_end)
										input_p2 = 0;
									u64 input_index  = ((k1*input.sq_nb  + k2) * input.fe_end  + (input_p1)) * input.fe_end  + input_p2;
									u64 output_index = ((k1*output.sq_nb + k2) * output.fe_end + (      p1)) * output.fe_end +       p2;
									conv((u8*)in_ptr + input_index * input_feature_size, (u8*)out_ptr + output_index * output_feature_size);
								}
							}
					break;

				case KPPP:
					for (u64 k1 = 0; k1 < output.sq_nb; ++k1)
						for (u64 p1 = 0; p1 < output.fe_end; ++p1)
						{
							u64 input_p1 = map == nullptr ? p1 : map->at(p1);
							if (input_p1 >= input.fe_end)
								input_p1 = 0;
							for (u64 p2 = 0; p2 < output.fe_end; ++p2)
							{
								u64 input_p2 = map == nullptr ? p2 : map->at(p2);
								if (input_p2 >= input.fe_end)
									input_p2 = 0;
								for (u64 p3 = 0; p3 < output.fe_end; ++p3)
								{
									u64 input_p3 = map == nullptr ? p3 : map->at(p3);
									if (input_p3 >= input.fe_end)
										input_p3 = 0;
									u64 input_index  = (((k1)* input.fe_end  + (input_p1)) * input.fe_end  + input_p2)*input.fe_end  + input_p3;
									u64 output_index = (((k1)* output.fe_end + (      p1)) * output.fe_end +       p2)*output.fe_end +       p3;
									conv((u8*)in_ptr + input_index * input_feature_size, (u8*)out_ptr + output_index * output_feature_size);
								}
							}
						}
					break;

				case VAR:
					conv((u8*)in_ptr , (u8*)out_ptr);
					break;
				}


				if (out_.file_or_memory.ptr == nullptr)
				{
					if (SystemIO::WriteMemoryToFile(out_.file_or_memory.filename , out_ptr , output_block_size).is_not_ok())
					{
						std::cout << "info string write file error , file = " << out_.file_or_memory.filename << std::endl;
						return false;
					}
				}
			}
		}

		return true;
	}

} 
} 
