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
			NTSH_MODULE_ERROR("DirectX 12 Error.\nError code: " + std::to_string(check) + "\nFile: " + std::string(__FILE__) + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__), NTSH_RESULT_UNKNOWN_ERROR); \
		} \
	} while(0)

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Direct3D 12 Triangle") {}

	void init();
	void update(double dt);
	void destroy();

private:
	void getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** hardwareAdapter);

private:
	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12CommandQueue> m_commandQueue;

	ComPtr<IDXGISwapChain3> m_swapchain;
	uint32_t m_swapchainSize;
	uint32_t m_frameIndex;

	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	uint32_t m_descriptorHeapSize;
	std::vector<ComPtr<ID3D12Resource>> m_renderTargets;

	std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
};