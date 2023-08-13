#pragma once
#include "../Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <vector>

using Microsoft::WRL::ComPtr;

#define NTSHENGN_DX12_CHECK(f) \
	do { \
		HRESULT check = f; \
		if (FAILED(check)) { \
			char str[64] = {}; \
			sprintf_s(str, "0x%08X", static_cast<uint32_t>(check)); \
			NTSHENGN_MODULE_ERROR("DirectX 12 Error.\nError code: " + std::string(str) + "\nFile: " + std::string(__FILE__) + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__), NtshEngn::Result::UnknownError); \
		} \
	} while(0)

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Direct3D 12 Graphics Triangle") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

		// Draws a text on the UI with the font in the fontID parameter using the position on screen and color
		void drawUIText(FontID fontID, const std::string& text, const Math::vec2& position, const Math::vec4& color);
		// Draws a line on the UI according to its start and end points and its color
		void drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color);
		// Draws a rectangle on the UI according to its position, its size (width and height) and its color
		void drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color);

	private:
		void getHardwareAdapter(IDXGIFactory1* factory, IDXGIAdapter1** hardwareAdapter);

		void waitForGPUIDle();

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

}