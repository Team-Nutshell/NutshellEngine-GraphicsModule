#pragma once
#include "../Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include <webgpu/webgpu.h>
#if defined(WEBGPU_BACKEND_WGPU)
#include <webgpu/wgpu.h>
#endif

#define NTSHENGN_WEBGPU_ASSIGN_CHECK(o, f) \
	do { \
		o = f; \
		if (!(o)) { \
			NTSHENGN_MODULE_ERROR("WebGPU Error.\nFile: " + std::string(__FILE__) + "\nLine: " + std::to_string(__LINE__) + "\nError with object \"" + #o + "\" assignation.", NtshEngn::Result::ModuleError); \
		} \
	} while(0)

struct AdapterRequestUserData {
	WGPUAdapter adapter = nullptr;
};

void onAdapterRequestEndCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) {
	AdapterRequestUserData& userData = *reinterpret_cast<AdapterRequestUserData*>(pUserData);
	if (status != WGPURequestAdapterStatus_Success) {
		NTSHENGN_MODULE_ERROR(message, NtshEngn::Result::ModuleError);
	}
	userData.adapter = adapter;
}

struct DeviceRequestUserData {
	WGPUDevice device = nullptr;
};

void onDeviceRequestEndCallback(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
	DeviceRequestUserData& userData = *reinterpret_cast<DeviceRequestUserData*>(pUserData);
	if (status != WGPURequestDeviceStatus_Success) {
		NTSHENGN_MODULE_ERROR(message, NtshEngn::Result::ModuleError);
	}
	userData.device = device;
}

void debugCallback(WGPUErrorType type, char const* message, void* pUserData) {
	NTSHENGN_UNUSED(pUserData);
	if (type != WGPUErrorType_NoError) {
		NTSHENGN_MODULE_WARNING("WebGPU uncaptured error: " + std::string(message));
	}
}

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine WebGPU Triangle Graphics Module") {}

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
		// Create swapchain
		void createSwapChain();

		// On window resize
		void resize();

	private:
		WGPUInstance m_instance;

		WGPUSurface m_surface;

		WGPUAdapter m_adapter;

		WGPUDevice m_device;
		WGPUQueue m_queue;

		WGPUSwapChain m_swapChain;
		WGPUTextureFormat m_swapChainFormat;
		uint32_t m_swapChainWidth;
		uint32_t m_swapChainHeight;

		WGPURenderPipeline m_renderPipeline;
	};

}