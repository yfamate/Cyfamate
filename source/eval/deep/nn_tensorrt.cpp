#include "nn_tensorrt.h"

#if defined(YANEURAOU_ENGINE_DEEP) && defined (TENSOR_RT)

#include <regex>
#include "unpack.cuh"

using namespace std;

namespace YaneuraOu {
using namespace Tools;

namespace {
	void FatalError(const std::string& s) {
		std::cerr << s << "\nAborting...\n";
		cudaDeviceReset();
		Tools::exit();
	}

	class Logger : public nvinfer1::ILogger
	{
		const char* error_type(Severity severity)
		{
			switch (severity)
			{
			case Severity::kINTERNAL_ERROR: return "[F] ";
			case Severity::kERROR         : return "[E] ";
			case Severity::kWARNING       : return "[W] ";
			case Severity::kINFO          : return "[I] ";
			case Severity::kVERBOSE       : return "[V] ";
			default: ASSERT(false);         return "";
			}
		}
		void log(Severity severity, const char* msg) noexcept
		{
			if (severity == Severity::kINTERNAL_ERROR) {
				std::cerr << error_type(severity) << msg << std::endl;
			}
		}
	} gLogger;

	constexpr long long int operator"" _MiB(long long unsigned int val)
	{
		return val * (1 << 20);
	}
}


void checkCudaErrors(cudaError_t status) {
	if (status != 0) {
		sync_cout << "Error! : Cuda failure , Error = " << cudaGetErrorString(status) << sync_endl;
		FatalError(cudaGetErrorString(status));
	}
}


namespace Eval::dlshogi
{
	struct NNTensorRT::InferenceSlot {
		PType* p1_dev = nullptr;
		PType* p2_dev = nullptr;
		DType* x1_dev = nullptr;
		DType* x2_dev = nullptr;
		NN_Output_Policy* y1_dev = nullptr;
		NN_Output_Value* y2_dev = nullptr;
		cudaStream_t stream = nullptr;
		InferUniquePtr<nvinfer1::IExecutionContext> context;
	};

	NNTensorRT::NNTensorRT() = default;

	NNTensorRT::~NNTensorRT()
	{
		release();
	}

	Tools::Result NNTensorRT::load(const std::string& model_path, int gpu_id, int max_batch_size)
	{
		this->gpu_id = gpu_id;
		this->max_batch_size = max_batch_size;

		set_device(gpu_id);

		return load_model(model_path);
	}

	void NNTensorRT::release()
	{

		set_device(gpu_id);

		for (auto& slot : slots)
		{
			if (slot->stream)
			{
				checkCudaErrors(cudaStreamSynchronize(slot->stream));
				checkCudaErrors(cudaStreamDestroy(slot->stream));
			}
			slot->context.reset();
			checkCudaErrors(cudaFree(slot->p1_dev));
			checkCudaErrors(cudaFree(slot->p2_dev));
			checkCudaErrors(cudaFree(slot->x1_dev));
			checkCudaErrors(cudaFree(slot->x2_dev));
			checkCudaErrors(cudaFree(slot->y1_dev));
			checkCudaErrors(cudaFree(slot->y2_dev));
		}
		slots.clear();
	}

	int NNTensorRT::get_device_count() {
		int device_count = 0;
		checkCudaErrors(cudaGetDeviceCount(&device_count));
		return device_count;
	}

	void NNTensorRT::set_device(int gpu_id)
	{
		checkCudaErrors(cudaSetDevice(gpu_id));
	}

	void NNTensorRT::build(const std::string& onnx_filename)
	{
		auto builder = InferUniquePtr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(gLogger));
		if (!builder)
		{
			FatalError("createInferBuilder");
		}

		const auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
		auto network = InferUniquePtr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicitBatch));
		if (!network)
		{
			FatalError("createNetworkV2");
		}

		auto config = InferUniquePtr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
		if (!config)
		{
			FatalError("createBuilderConfig");
		}

		auto parser = InferUniquePtr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, gLogger));
		if (!parser)
		{
			FatalError("createParser");
		}

		auto parsed = parser->parseFromFile(onnx_filename.c_str(), (int)nvinfer1::ILogger::Severity::kWARNING);
		if (!parsed)
		{
			FatalError("parseFromFile");
		}

#if 0
		std::unique_ptr<nvinfer1::IInt8Calibrator> calibrator;
		if (builder->platformHasFastInt8())
		{
			std::string calibration_cache_filename = std::string(onnx_filename) + ".calibcache";
			std::ifstream calibcache(calibration_cache_filename);
			if (calibcache.is_open())
			{
				calibcache.close();

				config->setFlag(nvinfer1::BuilderFlag::kINT8);
				calibrator.reset(new Int8EntropyCalibrator2(onnx_filename.c_str(), 1));
				config->setInt8Calibrator(calibrator.get());
			}
			else if (builder->platformHasFastFp16())
			{
				config->setFlag(nvinfer1::BuilderFlag::kFP16);
			}
		}
		else
#endif
		if (builder->platformHasFastFp16())
		{
			config->setFlag(nvinfer1::BuilderFlag::kFP16);
		}

#if defined(TRT_NN_FP16)
		network->getInput(0)->setType(nvinfer1::DataType::kHALF);
		network->getInput(1)->setType(nvinfer1::DataType::kHALF);
		network->getOutput(0)->setType(nvinfer1::DataType::kHALF);
		network->getOutput(1)->setType(nvinfer1::DataType::kHALF);
#endif

		ASSERT_LV3(network->getNbInputs() == 2);
		nvinfer1::Dims inputDims[] = { network->getInput(0)->getDimensions(), network->getInput(1)->getDimensions() };
		ASSERT_LV3(inputDims[0].nbDims == 4);
		ASSERT_LV3(inputDims[1].nbDims == 4);

		ASSERT_LV3(network->getNbOutputs() == 2);

#if defined(ENABLE_RYFAMATE_PATCH)
	if (true)
	{
		std::cout << "info string Ryfamate FP32 softmax patch." << std::endl;
		config->setFlag(nvinfer1::BuilderFlag::kFP16);
		config->setFlag(nvinfer1::BuilderFlag::kPREFER_PRECISION_CONSTRAINTS);
		for (int i = 0; i < network->getNbLayers(); ++i) {
			auto layer = network->getLayer(i);
			auto layer_before = network->getLayer(i > 0 ? i - 1 : 0);
			auto precition = layer->getPrecision();
			std::string layername = layer->getName();


			if (!layer->precisionIsSet()
				)
			{
				if (layer->getType() == nvinfer1::LayerType::kSOFTMAX
					|| (layer->getType() == nvinfer1::LayerType::kMATRIX_MULTIPLY && layername.find("blocks") != std::string::npos &&
						layer_before->getType() == nvinfer1::LayerType::kELEMENTWISE)
				){
					layer->setPrecision(nvinfer1::DataType::kFLOAT);
					std::cout << "info string FP32 layer " << i << " type " << (int)layer->getType() << " name " << layername << std::endl;
				}
			}
		}
	}
#endif


		const auto dims1 = inputDims[0].d;
		const auto dims2 = inputDims[1].d;
		for (int i = 0; i < profile_count; ++i)
		{
			auto profile = builder->createOptimizationProfile();
			profile->setDimensions("input1", nvinfer1::OptProfileSelector::kMIN, nvinfer1::Dims4(1, dims1[1], dims1[2], dims1[3]));
			profile->setDimensions("input1", nvinfer1::OptProfileSelector::kOPT, nvinfer1::Dims4(max_batch_size, dims1[1], dims1[2], dims1[3]));
			profile->setDimensions("input1", nvinfer1::OptProfileSelector::kMAX, nvinfer1::Dims4(max_batch_size, dims1[1], dims1[2], dims1[3]));
			profile->setDimensions("input2", nvinfer1::OptProfileSelector::kMIN, nvinfer1::Dims4(1, dims2[1], dims2[2], dims2[3]));
			profile->setDimensions("input2", nvinfer1::OptProfileSelector::kOPT, nvinfer1::Dims4(max_batch_size, dims2[1], dims2[2], dims2[3]));
			profile->setDimensions("input2", nvinfer1::OptProfileSelector::kMAX, nvinfer1::Dims4(max_batch_size, dims2[1], dims2[2], dims2[3]));
			config->addOptimizationProfile(profile);
		}
		config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 64_MiB);

#if NV_TENSORRT_MAJOR >= 8
		auto serializedEngine = InferUniquePtr<nvinfer1::IHostMemory>(builder->buildSerializedNetwork(*network, *config));
		if (!serializedEngine)
		{
			FatalError("buildSerializedNetwork");
		}
		auto runtime = InferUniquePtr<nvinfer1::IRuntime>(nvinfer1::createInferRuntime(gLogger));
		infer_engine.reset(runtime->deserializeCudaEngine(serializedEngine->data(), serializedEngine->size()));
		if (!infer_engine)
		{
			FatalError("deserializeCudaEngine");
		}
#else
		engine.reset(builder->buildEngineWithConfig(*network, *config));
		if (!engine)
		{
			FatalError("buildEngineWithConfig");
		}
#endif
	}

	Tools::Result NNTensorRT::load_model(const string& filename)
	{
		infer_engine.reset();


		cudaDeviceProp device_prop;
		checkCudaErrors(cudaGetDeviceProperties(&device_prop, gpu_id));



		std::string serialized_filename =
			filename + "." +
				std::regex_replace(std::string(device_prop.name), std::regex("[^A-Za-z0-9._-]"), std::string("_")) + "." +
				std::to_string(max_batch_size) + "." +
				"profiles" + std::to_string(profile_count) + "." +
				"TRT" + std::to_string(getInferLibVersion()) + "." +
#if defined(TRT_NN_FP16)
				"FP16." +
#endif
			"serialized";

		sync_cout << "info string serialized filename = " << serialized_filename << sync_endl;


		std::unique_ptr<u8[]> modelPtr;
		size_t modelSize = 0;
		auto result = SystemIO::ReadFileToMemory(serialized_filename, [&](size_t size) {
			modelPtr = make_unique<u8[]>(size); modelSize = size; return modelPtr.get();
		});

		if (result.is_ok())
		{
			auto runtime = InferUniquePtr<nvinfer1::IRuntime>(nvinfer1::createInferRuntime(gLogger));
			infer_engine = InferUniquePtr<nvinfer1::ICudaEngine>(runtime->deserializeCudaEngine(modelPtr.get(), modelSize));

			if (!infer_engine)
				sync_cout << "info string Warning! TensorRT : Failed to deserialize the model file. filename = " << serialized_filename << sync_endl;
		}

		if (!infer_engine)
		{
			sync_cout << "info string TensorRT : build the model file." << sync_endl;

			build(filename);

			auto serializedEngine = InferUniquePtr<nvinfer1::IHostMemory>(infer_engine->serialize());
			if (!serializedEngine)
			{
				return Tools::ResultCode::SomeError;
			}
			std::ofstream engineFile(serialized_filename, std::ios::binary);
			if (!engineFile)
			{
				return Tools::ResultCode::FileOpenError;
			}
			engineFile.write(static_cast<char*>(serializedEngine->data()), serializedEngine->size());
			if (engineFile.fail())
			{
				return Tools::ResultCode::FileOpenError;
			}
		}

		inputDims1 = infer_engine->getTensorShape("input1");
		inputDims2 = infer_engine->getTensorShape("input2");
		const auto& spec = input_feature_spec();
		if (inputDims1.nbDims != 4 || inputDims2.nbDims != 4
			|| inputDims1.d[1] != (int64_t)spec.features1_channels
			|| inputDims2.d[1] != (int64_t)spec.features2_channels)
		{
			sync_cout << "info string TensorRT input shape mismatch. ModelArchitecture = "
			          << spec.architecture << ", expected input1 = " << spec.features1_channels
			          << ", expected input2 = " << spec.features2_channels
			          << ", model input1 = " << (inputDims1.nbDims == 4 ? inputDims1.d[1] : -1)
			          << ", model input2 = " << (inputDims2.nbDims == 4 ? inputDims2.d[1] : -1)
			          << sync_endl;
			return Tools::ResultCode::SomeError;
		}

		return Tools::ResultCode::Ok;
	}

	void NNTensorRT::set_profile_count(int profile_count)
	{
		this->profile_count = profile_count > 0 ? profile_count : 1;
	}

	int NNTensorRT::slot_capacity() const
	{
		return infer_engine ? infer_engine->getNbOptimizationProfiles() : profile_count;
	}

	std::unique_ptr<NNTensorRT::InferenceSlot> NNTensorRT::create_slot(int profile_index)
	{
		if (!infer_engine || profile_index < 0 || profile_index >= infer_engine->getNbOptimizationProfiles())
			FatalError("NNTensorRT::create_slot() : invalid profile_index");

		auto slot = std::unique_ptr<InferenceSlot>(new InferenceSlot());
		checkCudaErrors(cudaMalloc((void**)&slot->p1_dev, sizeof(PType)            * packed_input1_byte_count(max_batch_size)));
		checkCudaErrors(cudaMalloc((void**)&slot->p2_dev, sizeof(PType)            * packed_input2_byte_count(max_batch_size)));
		checkCudaErrors(cudaMalloc((void**)&slot->x1_dev, sizeof(DType)            * input1_element_count(max_batch_size)));
		checkCudaErrors(cudaMalloc((void**)&slot->x2_dev, sizeof(DType)            * input2_element_count(max_batch_size)));
		checkCudaErrors(cudaMalloc((void**)&slot->y1_dev, sizeof(NN_Output_Policy) * max_batch_size));
		checkCudaErrors(cudaMalloc((void**)&slot->y2_dev, sizeof(NN_Output_Value)  * max_batch_size));
		checkCudaErrors(cudaStreamCreateWithFlags(&slot->stream, cudaStreamNonBlocking));

		slot->context = InferUniquePtr<nvinfer1::IExecutionContext>(infer_engine->createExecutionContext());
		if (!slot->context)
			FatalError("NNTensorRT::create_slot() : createExecutionContext");

		if (!slot->context->setOptimizationProfileAsync(profile_index, slot->stream))
			FatalError("NNTensorRT::create_slot() : setOptimizationProfileAsync");

		if (!slot->context->setTensorAddress("input1", slot->x1_dev)
			|| !slot->context->setTensorAddress("input2", slot->x2_dev)
			|| !slot->context->setTensorAddress("output_policy", slot->y1_dev)
			|| !slot->context->setTensorAddress("output_value", slot->y2_dev))
			FatalError("NNTensorRT::create_slot() : setTensorAddress");

		return slot;
	}

	void NNTensorRT::prepare_slots(int slot_count)
	{
		std::lock_guard<std::mutex> lock(slots_mutex);
		set_device(gpu_id);
		if (slot_count > slot_capacity())
			FatalError("NNTensorRT::prepare_slots() : slot_count exceeds optimization profile count");

		while ((int)slots.size() < slot_count)
			slots.emplace_back(create_slot((int)slots.size()));
	}

	NNTensorRT::InferenceSlot* NNTensorRT::get_slot(int slot_id)
	{
		if (slot_id < 0 || slot_id >= (int)slots.size())
			FatalError("NNTensorRT::get_slot() : invalid slot_id");

		return slots[slot_id].get();
	}

	void NNTensorRT::forward(const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2)
	{
		if (slots.empty())
			prepare_slots(1);

		forward_impl(get_slot(0), batch_size, p1, p2, x1, x2, y1, y2);
	}

	void NNTensorRT::forward(const int slot_id, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2)
	{
		forward_impl(get_slot(slot_id), batch_size, p1, p2, x1, x2, y1, y2);
	}

	void NNTensorRT::forward_impl(InferenceSlot* slot, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2)
	{
#if defined(UNPACK_CUDA)
		checkCudaErrors(cudaMemcpyAsync(slot->p1_dev, p1, sizeof(PType) * packed_input1_byte_count(batch_size), cudaMemcpyHostToDevice, slot->stream));
		checkCudaErrors(cudaMemcpyAsync(slot->p2_dev, p2, sizeof(PType) * packed_input2_byte_count(batch_size), cudaMemcpyHostToDevice, slot->stream));
		const auto& spec = input_feature_spec();
		unpack_features1(batch_size, spec.features1_channels, slot->p1_dev, slot->x1_dev, slot->stream);
		unpack_features2(batch_size, spec.features2_channels, slot->p2_dev, slot->x2_dev, slot->stream);
		checkCudaErrors(cudaGetLastError());
#else
		checkCudaErrors(cudaMemcpyAsync(slot->x1_dev, x1, sizeof(DType) * input1_element_count(batch_size), cudaMemcpyHostToDevice, slot->stream));
		checkCudaErrors(cudaMemcpyAsync(slot->x2_dev, x2, sizeof(DType) * input2_element_count(batch_size), cudaMemcpyHostToDevice, slot->stream));
#endif

		auto dims1 = inputDims1;
		auto dims2 = inputDims2;
		dims1.d[0] = batch_size;
		dims2.d[0] = batch_size;
		if (!slot->context->setInputShape("input1", dims1)
			|| !slot->context->setInputShape("input2", dims2))
			FatalError("NNTensorRT::forward_impl() : setInputShape");

		const bool status = slot->context->enqueueV3(slot->stream);
		ASSERT_LV3(status);
		checkCudaErrors(cudaMemcpyAsync(y1, slot->y1_dev, sizeof(NN_Output_Policy) * batch_size, cudaMemcpyDeviceToHost, slot->stream));
		checkCudaErrors(cudaMemcpyAsync(y2, slot->y2_dev, sizeof(NN_Output_Value ) * batch_size, cudaMemcpyDeviceToHost, slot->stream));
		checkCudaErrors(cudaStreamSynchronize(slot->stream));
	}

} 
} 

#endif 

