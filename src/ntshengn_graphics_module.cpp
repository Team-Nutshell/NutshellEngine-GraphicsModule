#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Common/module_interfaces/ntshengn_window_module_interface.h"
#include "../external/d3dx12/d3dx12.h"
#include <algorithm>

void NtshEngn::GraphicsModule::init() {
	uint32_t factoryFlags = 0;
#if defined(NTSHENGN_DEBUG)
	// Enable the debug layer
	ComPtr<ID3D12Debug> debugLayer;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();

		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else {
		NTSHENGN_MODULE_WARNING("Could not enable debug layer.");
	}
#endif

	// Create factory
	NTSHENGN_DX12_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)));

	// Pick a hardware adapter and create a device
	ComPtr<IDXGIAdapter1> hardwareAdapter;
	getHardwareAdapter(m_factory.Get(), &hardwareAdapter);
	NTSHENGN_DX12_CHECK(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	// Create command queue
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;
	NTSHENGN_DX12_CHECK(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_imageCount = 2;

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		m_viewport.Height = static_cast<float>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
		m_viewport.MinDepth = D3D12_MIN_DEPTH;
		m_viewport.MaxDepth = D3D12_MAX_DEPTH;

		m_scissor.left = 0;
		m_scissor.top = 0;
		m_scissor.right = windowModule->getWindowWidth(windowModule->getMainWindowID());
		m_scissor.bottom = windowModule->getWindowHeight(windowModule->getMainWindowID());

		m_savedWidth = windowModule->getWindowWidth(windowModule->getMainWindowID());
		m_savedHeight = windowModule->getWindowHeight(windowModule->getMainWindowID());

		// Create swapchain
		ComPtr<IDXGISwapChain1> swapchain;
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = 0;
		swapchainDesc.Height = 0;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.Stereo = FALSE;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = m_imageCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		NTSHENGN_DX12_CHECK(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), reinterpret_cast<HWND>(windowModule->getWindowNativeHandle(windowModule->getMainWindowID())), &swapchainDesc, nullptr, nullptr, &swapchain));

		NTSHENGN_DX12_CHECK(swapchain.As(&m_swapchain));
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}
	else {
		m_imageCount = 1;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = 1280;
		resourceDesc.Height = 720;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 0.0f;
		clearValue.DepthStencil.Depth = 0.0f;
		clearValue.DepthStencil.Stencil = 0;

		NTSHENGN_DX12_CHECK(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&m_drawImage)));

		m_frameIndex = 0;
	}

	// Create descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NumDescriptors = m_imageCount;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDesc.NodeMask = 0;
	NTSHENGN_DX12_CHECK(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	m_descriptorHeapSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resource
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	m_renderTargets.resize(m_imageCount);
	m_commandAllocators.resize(m_imageCount);
	for (uint32_t i = 0; i < m_imageCount; i++) {
		if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
			NTSHENGN_DX12_CHECK(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		}
		else {
			m_drawImage.As(&m_renderTargets[i]);
		}
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_descriptorHeapSize);

		// Create command allocator
		NTSHENGN_DX12_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
	}

	// Create empty root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	NTSHENGN_DX12_CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	NTSHENGN_DX12_CHECK(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

	// Create pipeline state
	uint32_t shaderCompileFlags = 0;
#if defined(NTSHENGN_DEBUG)
	shaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	const std::string vertexShaderCode = R"HLSL(
		static const float2 positions[3] = {
			float2(0.0, 0.5),
			float2(-0.5, -0.5),
			float2(0.5, -0.5)
		};
		
		static const float3 colors[3] = {
			float3(1.0, 0.0, 0.0),
			float3(0.0, 0.0, 1.0),
			float3(0.0, 1.0, 0.0)
		};
		
		struct VSOut {
			float4 position : SV_POSITION;
			float3 color : COLOR;
		};
		
		VSOut main(uint vertexID : SV_VertexID) {
			VSOut vsOut;
			vsOut.position = float4(positions[vertexID], 0.0, 1.0);
			vsOut.color = colors[vertexID];
			
			return vsOut;
		}
		)HLSL";
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> vertexShaderError;
	if (FAILED(D3DCompile(vertexShaderCode.c_str(), vertexShaderCode.size(), nullptr, nullptr, nullptr, "main", "vs_5_0", shaderCompileFlags, 0, &vertexShader, &vertexShaderError))) {
		std::string shaderCompilationError(reinterpret_cast<const char*>(vertexShaderError->GetBufferPointer()), vertexShaderError->GetBufferSize());
		NTSHENGN_MODULE_ERROR("Vertex shader compilation failed:\n" + shaderCompilationError, Result::ModuleError);
	}

	const std::string pixelShaderCode = R"HLSL(
		struct PSIn {
			float4 position : SV_POSITION;
			float3 color : COLOR;
		};
		
		float4 main(PSIn psIn) : SV_TARGET {
			return float4(pow(psIn.color, 1.0/2.2), 1.0);
		}
		)HLSL";
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> pixelShaderError;
	if (FAILED(D3DCompile(pixelShaderCode.c_str(), pixelShaderCode.size(), nullptr, nullptr, nullptr, "main", "ps_5_0", shaderCompileFlags, 0, &pixelShader, &pixelShaderError))) {
		std::string shaderCompilationError(reinterpret_cast<const char*>(pixelShaderError->GetBufferPointer()), pixelShaderError->GetBufferSize());
		NTSHENGN_MODULE_ERROR("Pixel shader compilation failed:\n" + shaderCompilationError, Result::ModuleError);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {};
	graphicsPipelineStateDesc.pRootSignature = m_rootSignature.Get();
	graphicsPipelineStateDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	graphicsPipelineStateDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	graphicsPipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPipelineStateDesc.SampleMask = UINT_MAX;
	graphicsPipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, TRUE, D3D12_DEFAULT_DEPTH_BIAS, D3D12_DEFAULT_DEPTH_BIAS_CLAMP, D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS, TRUE, FALSE, FALSE, 0, D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = FALSE;
	graphicsPipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
	graphicsPipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
	graphicsPipelineStateDesc.InputLayout.NumElements = 0;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleDesc.Quality = 0;
	NTSHENGN_DX12_CHECK(m_device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&m_graphicsPipeline)));

	// Create command list
	NTSHENGN_DX12_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_graphicsPipeline.Get(), IID_PPV_ARGS(&m_commandList)));
	NTSHENGN_DX12_CHECK(m_commandList->Close());

	// Create sync objects
	m_fenceValues.resize(m_imageCount);
	std::fill(m_fenceValues.begin(), m_fenceValues.end(), 0);
	NTSHENGN_DX12_CHECK(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValues[m_frameIndex]++;

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	NTSHENGN_ASSERT(m_fenceEvent != nullptr);

	waitForGPUIDle();
}

void NtshEngn::GraphicsModule::update(double dt) {
	NTSHENGN_UNUSED(dt);

	if (windowModule && !windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		// Do not update if the main window got closed
		return;
	}

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		// Check for window resize
		if ((windowModule->getWindowWidth(windowModule->getMainWindowID()) != m_savedWidth) || (windowModule->getWindowHeight(windowModule->getMainWindowID()) != m_savedHeight)) {
			resize();
		}
	}
	
	// Record commands
	NTSHENGN_DX12_CHECK(m_commandAllocators[m_frameIndex]->Reset());

	NTSHENGN_DX12_CHECK(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_graphicsPipeline.Get()));

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissor);

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_presentToRenderTargetBarrier = {};
		m_presentToRenderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		m_presentToRenderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		m_presentToRenderTargetBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
		m_presentToRenderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_presentToRenderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		m_presentToRenderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_commandList->ResourceBarrier(1, &m_presentToRenderTargetBarrier);
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_descriptorHeapSize);
	m_commandList->OMSetRenderTargets(1, &descriptorHandle, FALSE, nullptr);

	m_commandList->ClearRenderTargetView(descriptorHandle, m_clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_renderTargetToPresentBarrier = {};
		m_renderTargetToPresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		m_renderTargetToPresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		m_renderTargetToPresentBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
		m_renderTargetToPresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_renderTargetToPresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_renderTargetToPresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_commandList->ResourceBarrier(1, &m_renderTargetToPresentBarrier);
	}

	NTSHENGN_DX12_CHECK(m_commandList->Close());

	// Execute command list
	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, commandLists);

	// Present
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		NTSHENGN_DX12_CHECK(m_swapchain->Present(0, 0));
	}

	// Sync
	uint64_t currentFenceValue = m_fenceValues[m_frameIndex];
	NTSHENGN_DX12_CHECK(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
		NTSHENGN_DX12_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void NtshEngn::GraphicsModule::destroy() {
	waitForGPUIDle();

	CloseHandle(m_fenceEvent);
}

NtshEngn::MeshID NtshEngn::GraphicsModule::load(const Mesh& mesh) {
	NTSHENGN_UNUSED(mesh);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_MESH_UNKNOWN;
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	NTSHENGN_UNUSED(image);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_IMAGE_UNKNOWN;
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	NTSHENGN_UNUSED(font);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_FONT_UNKNOWN;
}

void NtshEngn::GraphicsModule::playAnimation(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);
}

void NtshEngn::GraphicsModule::pauseAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
}

void NtshEngn::GraphicsModule::stopAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
}

bool NtshEngn::GraphicsModule::isAnimationPlaying(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);

	return false;
}

void NtshEngn::GraphicsModule::drawUIText(FontID fontID, const std::string& text, const Math::vec2& position, const Math::vec4& color) {
	NTSHENGN_UNUSED(fontID);
	NTSHENGN_UNUSED(text);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color) {
	NTSHENGN_UNUSED(start);
	NTSHENGN_UNUSED(end);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color) {
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(size);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color) {
	NTSHENGN_UNUSED(imageID);
	NTSHENGN_UNUSED(imageSamplerFilter);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(rotation);
	NTSHENGN_UNUSED(scale);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** hardwareAdapter) {
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
	NTSHENGN_ASSERT(adapter.Get() != nullptr);

	*hardwareAdapter = adapter.Detach();
}

void NtshEngn::GraphicsModule::waitForGPUIDle() {
	NTSHENGN_DX12_CHECK(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	NTSHENGN_DX12_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	m_fenceValues[m_frameIndex]++;
}

void NtshEngn::GraphicsModule::resize() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		while ((windowModule->getWindowWidth(windowModule->getMainWindowID()) == 0) || (windowModule->getWindowHeight(windowModule->getMainWindowID()) == 0)) {
			windowModule->pollEvents();
		}

		waitForGPUIDle();
		
		for (uint32_t i = 0; i < m_imageCount; i++) {
			m_renderTargets[i].Reset();
			m_fenceValues[i] = m_frameIndex;
		}

		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		m_viewport.Height = static_cast<float>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
		m_viewport.MinDepth = D3D12_MIN_DEPTH;
		m_viewport.MaxDepth = D3D12_MAX_DEPTH;

		m_scissor.left = 0;
		m_scissor.top = 0;
		m_scissor.right = windowModule->getWindowWidth(windowModule->getMainWindowID());
		m_scissor.bottom = windowModule->getWindowHeight(windowModule->getMainWindowID());

		m_savedWidth = windowModule->getWindowWidth(windowModule->getMainWindowID());
		m_savedHeight = windowModule->getWindowHeight(windowModule->getMainWindowID());

		NTSHENGN_DX12_CHECK(m_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

		CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

		for (uint32_t i = 0; i < m_imageCount; i++) {
			NTSHENGN_DX12_CHECK(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, descriptorHandle);
			descriptorHandle.Offset(1, m_descriptorHeapSize);
		}

		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}