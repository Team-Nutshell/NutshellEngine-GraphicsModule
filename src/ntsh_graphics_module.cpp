#include "ntsh_graphics_module.h"
#include "../external/Module/ntsh_module_defines.h"
#include "../external/Module/ntsh_dynamic_library.h"
#include "../external/Common/ntsh_engine_defines.h"
#include "../external/Common/ntsh_engine_enums.h"
#include "../external/Common/module_interfaces/ntsh_window_module_interface.h"
#include "../external/d3dx12/d3dx12.h"

void NutshellGraphicsModule::init() {
	uint32_t factoryFlags = 0;
#ifdef NTSH_DEBUG
	// Enable the debug layer
	ComPtr<ID3D12Debug> debugLayer;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();

		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else {
		NTSH_MODULE_WARNING("Could not enable debug layer.");
	}
#endif

	// Create factory
	ComPtr<IDXGIFactory4> factory;
	NTSH_DX12_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

	// Pick a hardware adapter and create a device
	ComPtr<IDXGIAdapter1> hardwareAdapter;
	getHardwareAdapter(factory.Get(), &hardwareAdapter);
	NTSH_DX12_CHECK(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	// Create command queue
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;
	NTSH_DX12_CHECK(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));

	if (m_windowModule) {
		m_swapchainSize = 2;

		// Create swapchain
		ComPtr<IDXGISwapChain1> swapchain;
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = 0;
		swapchainDesc.Height = 0;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.Stereo = false;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = m_swapchainSize;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		NTSH_DX12_CHECK(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_windowModule->getNativeHandle(), &swapchainDesc, nullptr, nullptr, &swapchain));

		NTSH_DX12_CHECK(factory->MakeWindowAssociation(m_windowModule->getNativeHandle(), DXGI_MWA_NO_ALT_ENTER));
		NTSH_DX12_CHECK(swapchain.As(&m_swapchain));
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}

	// Create descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NumDescriptors = m_swapchainSize;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDesc.NodeMask = 0;
	NTSH_DX12_CHECK(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	m_descriptorHeapSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resource
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	m_renderTargets.resize(m_swapchainSize);
	m_commandAllocators.resize(m_swapchainSize);
	for (uint32_t i = 0; i < m_swapchainSize; i++) {
		NTSH_DX12_CHECK(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_descriptorHeapSize);

		// Create command allocator
		NTSH_DX12_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
	}

	// Create empty root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	NTSH_DX12_CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

	// Create pipeline state
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> fragmentShader;
}

void NutshellGraphicsModule::update(double dt) {
	NTSH_UNUSED(dt);
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NutshellGraphicsModule::destroy() {
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NutshellGraphicsModule::getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** hardwareAdapter) {
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIFactory6> factory6;

	// Find an adapter according to GPU preference
	if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
		for (uint32_t i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); i++) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Do not select the software renderer adapter
			if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
				break;
			}
		}
	}
	
	// If no adapter has been found, enumerate all adapters and pick one
	if (adapter.Get() == nullptr) {
		for (uint32_t i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); i++) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Do not select the software renderer adapter
			if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
				break;
			}
		}
	}
	NTSH_ASSERT(adapter.Get() != nullptr);

	*hardwareAdapter = adapter.Detach();
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface* createModule() {
	return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface* m) {
	delete m;
}