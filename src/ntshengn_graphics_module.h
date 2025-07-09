#pragma once
#include "../Common/modules/ntshengn_graphics_module_interface.h"
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
			NTSHENGN_MODULE_ERROR("WebGPU Error.\nFile: " + std::filesystem::path(__FILE__).filename().string() + "\nLine: " + std::to_string(__LINE__) + "\nError with object \"" + #o + "\" assignation."); \
		} \
	} while(0)

struct AdapterRequestUserData {
	WGPUAdapter adapter = nullptr;
};

void onAdapterRequestEndCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) {
	AdapterRequestUserData& userData = *reinterpret_cast<AdapterRequestUserData*>(pUserData);
	if (status != WGPURequestAdapterStatus_Success) {
		NTSHENGN_MODULE_ERROR(message);
	}
	userData.adapter = adapter;
}

struct DeviceRequestUserData {
	WGPUDevice device = nullptr;
};

void onDeviceRequestEndCallback(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
	DeviceRequestUserData& userData = *reinterpret_cast<DeviceRequestUserData*>(pUserData);
	if (status != WGPURequestDeviceStatus_Success) {
		NTSHENGN_MODULE_ERROR(message);
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
		void update(float dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

		// Sets the background color
		void setBackgroundColor(const Math::vec4& backgroundColor);

		// Plays an animation for an entity, indexed in the entity's model animation list
		void playAnimation(Entity entity, uint32_t animationIndex);
		// Pauses an animation played by an entity
		void pauseAnimation(Entity entity);
		// Stops an animation played by an entity
		void stopAnimation(Entity entity);
		// Sets the current playing time of an animation played by an entity
		void setAnimationCurrentTime(Entity entity, float time);

		// Returns true if the entity is currently playing the animation with index animationIndex, else, returns false
		bool isAnimationPlaying(Entity entity, uint32_t animationIndex);

		// Emits particles described by particleEmitter
		void emitParticles(const ParticleEmitter& particleEmitter);
		// Destroys all particles
		void destroyParticles();

		// Draws a text on the UI with the font in the fontID parameter using the position on screen, scale and color
		void drawUIText(FontID fontID, const std::wstring& text, const Math::vec2& position, const Math::vec2& scale, const Math::vec4& color);
		// Draws a line on the UI according to its start and end points and its color
		void drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color);
		// Draws a rectangle on the UI according to its position, its size (width and height) and its color
		void drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color);
		// Draws an image on the UI according to its sampler filter, position, rotation, scale and color to multiply the image with
		void drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color);

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

		Math::vec4 m_backgroundColor = Math::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	};

}