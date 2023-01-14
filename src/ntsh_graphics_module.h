#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <vector>

using Microsoft::WRL::ComPtr;

#define NTSH_DX12_CHECK(f) \
	do { \
		HRESULT check = f; \
		if (FAILED(check)) { \
			char str[64] = {}; \
			sprintf_s(str, "0x%08X", static_cast<uint32_t>(check)); \
			NTSH_MODULE_ERROR("DirectX 12 Error.\nError code: " + std::string(str) + "\nFile: " + std::string(__FILE__) + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__), NTSH_RESULT_UNKNOWN_ERROR); \
		} \
	} while(0)

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Direct3D 12 Triangle") {}

	void init();
	void update(double dt);
	void destroy();

	// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
	Ntsh::MeshId load(const Ntsh::Mesh mesh);
	// Loads the image described in the image parameter in the internal format and returns a unique identifier
	Ntsh::ImageId load(const Ntsh::Image image);

private:
	void getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** hardwareAdapter);
	
	void waitForGPUIdle();

	// On window resize
	void resize();

private:
	ComPtr<IDXGIFactory4> m_factory;

	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12CommandQueue> m_commandQueue;

	ComPtr<IDXGISwapChain3> m_swapchain;
	uint32_t m_frameIndex;

	ComPtr<ID3D12Resource> m_drawImage;

	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	uint32_t m_descriptorHeapSize;
	std::vector<ComPtr<ID3D12Resource>> m_renderTargets;

	std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	D3D12_RESOURCE_BARRIER m_presentToRenderTargetBarrier;
	D3D12_RESOURCE_BARRIER m_renderTargetToPresentBarrier;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_graphicsPipeline;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissor;

	ComPtr<ID3D12Fence> m_fence;
	std::vector<uint64_t> m_fenceValues;
	HANDLE m_fenceEvent;

	uint32_t m_imageCount;

	const float m_clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int m_savedWidth;
	int m_savedHeight;
};