#include "ntshengn_graphics_module.h"
#include "../Common/module_interfaces/ntshengn_window_module_interface.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include "../Module/utils/ntshengn_dynamic_library.h"

void NtshEngn::GraphicsModule::init() {
	// Create instance
	WGPUInstanceDescriptor instanceDescriptor = {};
	instanceDescriptor.nextInChain = nullptr;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_instance, wgpuCreateInstance(&instanceDescriptor));

	// Create surface
	WGPUSurfaceDescriptor surfaceDescriptor = {};
	surfaceDescriptor.label = "Surface";

#if defined(NTSHENGN_OS_WINDOWS)
	WGPUSurfaceDescriptorFromWindowsHWND surfaceDescriptorWindows = {};
	surfaceDescriptorWindows.chain.next = nullptr;
	surfaceDescriptorWindows.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
	surfaceDescriptorWindows.hinstance = windowModule->getNativeAdditionalInformation(windowModule->getMainWindowID());
	surfaceDescriptorWindows.hwnd = windowModule->getNativeHandle(windowModule->getMainWindowID());

	surfaceDescriptor.nextInChain = &surfaceDescriptorWindows.chain;
#elif defined(NTSHENGN_OS_LINUX)
	WGPUSurfaceDescriptorFromXlibWindow surfaceDescriptorLinux = {};
	surfaceDescriptorLinux.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
	surfaceDescriptorLinux.chain.next = nullptr;
	surfaceDescriptorLinux.display = windowModule->getNativeAdditionalInformation(windowModule->getMainWindowID());
	surfaceDescriptorLinux.window = reinterpret_cast<uint32_t>(windowModule->getNativeHandle(windowModule->getMainWindowID()));

	surfaceDescriptor.nextInChain = &surfaceDescriptorLinux.chain;
#endif
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_surface, wgpuInstanceCreateSurface(m_instance, &surfaceDescriptor));

	// Request adapter
	AdapterRequestUserData adapterRequestUserData = {};
	WGPURequestAdapterOptions requestAdapterOptions = {};
	requestAdapterOptions.nextInChain = nullptr;
	requestAdapterOptions.compatibleSurface = m_surface;
	wgpuInstanceRequestAdapter(m_instance, &requestAdapterOptions, onAdapterRequestEndCallback, &adapterRequestUserData);
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_adapter, adapterRequestUserData.adapter);

	size_t featureCount = wgpuAdapterEnumerateFeatures(m_adapter, nullptr);
	std::vector<WGPUFeatureName> adapterFeatures(featureCount);
	wgpuAdapterEnumerateFeatures(m_adapter, adapterFeatures.data());

	NTSHENGN_MODULE_INFO("Adapter features: ");
	for (WGPUFeatureName adapterFeature : adapterFeatures) {
		switch (adapterFeature) {
		case WGPUFeatureName_Undefined:
			NTSHENGN_MODULE_INFO("Undefined");
			break;

		case WGPUFeatureName_DepthClipControl:
			NTSHENGN_MODULE_INFO("DepthClipControl");
			break;

		case WGPUFeatureName_Depth32FloatStencil8:
			NTSHENGN_MODULE_INFO("Depth32FloatStencil8");
			break;

		case WGPUFeatureName_TimestampQuery:
			NTSHENGN_MODULE_INFO("TimestampQuery");
			break;

		case WGPUFeatureName_PipelineStatisticsQuery:
			NTSHENGN_MODULE_INFO("PipelineStatisticsQuery");
			break;

		case WGPUFeatureName_TextureCompressionBC:
			NTSHENGN_MODULE_INFO("TextureCompressionBC");
			break;

		case WGPUFeatureName_TextureCompressionETC2:
			NTSHENGN_MODULE_INFO("TextureCompressionETC2");
			break;

		case WGPUFeatureName_TextureCompressionASTC:
			NTSHENGN_MODULE_INFO("TextureCompressionASTC");
			break;

		case WGPUFeatureName_IndirectFirstInstance:
			NTSHENGN_MODULE_INFO("IndirectFirstInstance");
			break;

		case WGPUFeatureName_ShaderF16:
			NTSHENGN_MODULE_INFO("ShaderF16");
			break;

		case WGPUFeatureName_RG11B10UfloatRenderable:
			NTSHENGN_MODULE_INFO("RG11B10UfloatRenderable");
			break;

		case WGPUFeatureName_BGRA8UnormStorage:
			NTSHENGN_MODULE_INFO("Undefined");
			break;

		case WGPUFeatureName_Force32:
			NTSHENGN_MODULE_INFO("Force32");
			break;

		default:
			if (adapterFeature == WGPUNativeFeature_PUSH_CONSTANTS) {
				NTSHENGN_MODULE_INFO("PUSH_CONSTANTS");
			}
			else if (adapterFeature == WGPUNativeFeature_TEXTURE_ADAPTER_SPECIFIC_FORMAT_FEATURES) {
				NTSHENGN_MODULE_INFO("TEXTURE_ADAPTER_SPECIFIC_FORMAT_FEATURES");
			}
			else if (adapterFeature == WGPUNativeFeature_MULTI_DRAW_INDIRECT) {
				NTSHENGN_MODULE_INFO("MULTI_DRAW_INDIRECT");
			}
			else if (adapterFeature == WGPUNativeFeature_MULTI_DRAW_INDIRECT_COUNT) {
				NTSHENGN_MODULE_INFO("MULTI_DRAW_INDIRECT_COUNT");
			}
			else if (adapterFeature == WGPUNativeFeature_VERTEX_WRITABLE_STORAGE) {
				NTSHENGN_MODULE_INFO("VERTEX_WRITABLE_STORAGE");
			}
			else {
				NTSHENGN_MODULE_INFO("Unknown feature: " + adapterFeature);
			}
		}
	}

	// Request device
	DeviceRequestUserData deviceRequestUserData = {};
	WGPUDeviceDescriptor deviceDescriptor = {};
	deviceDescriptor.nextInChain = nullptr;
	deviceDescriptor.label = "Device";
	deviceDescriptor.requiredFeaturesCount = 0;
	deviceDescriptor.requiredFeatures = nullptr;
	deviceDescriptor.requiredLimits = nullptr;
	deviceDescriptor.defaultQueue.nextInChain = nullptr;
	deviceDescriptor.defaultQueue.label = "Queue";
	wgpuAdapterRequestDevice(m_adapter, &deviceDescriptor, onDeviceRequestEndCallback, &deviceRequestUserData);
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_device, deviceRequestUserData.device);

	wgpuDeviceSetUncapturedErrorCallback(m_device, debugCallback, nullptr);

	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_queue, wgpuDeviceGetQueue(m_device));

	createSwapChain();

	// Create render pipeline
	const char* vertexShader = R"WGSL(
		var<private> positions: array<vec2f, 3> = array<vec2f, 3>(
			vec2f(0.0, 0.5),
			vec2f(-0.5, -0.5),
			vec2f(0.5, -0.5)
		);

		var<private> colors: array<vec3f, 3> = array<vec3f, 3>(
			vec3f(1.0, 0.0, 0.0),
			vec3f(0.0, 0.0, 1.0),
			vec3f(0.0, 1.0, 0.0)
		);

		struct VertexShaderOutput {
			@builtin(position) pos: vec4f,
			@location(0) color: vec3f
		}

		@vertex
		fn main(@builtin(vertex_index) vertexIndex: u32) -> VertexShaderOutput {
			var output: VertexShaderOutput;
			output.pos = vec4f(positions[vertexIndex], 0.0, 1.0);
			output.color = colors[vertexIndex];

			return output;
		}
	)WGSL";

	WGPUShaderModule vertexShaderModule = nullptr;
	WGPUShaderModuleWGSLDescriptor vertexShaderModuleWGSLDescriptor = {};
	vertexShaderModuleWGSLDescriptor.chain.next = nullptr;
	vertexShaderModuleWGSLDescriptor.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	vertexShaderModuleWGSLDescriptor.code = vertexShader;

	WGPUShaderModuleDescriptor vertexShaderModuleDescriptor = {};
	vertexShaderModuleDescriptor.nextInChain = &vertexShaderModuleWGSLDescriptor.chain;
	vertexShaderModuleDescriptor.label = "Vertex shader module";
	vertexShaderModuleDescriptor.hintCount = 0;
	vertexShaderModuleDescriptor.hints = nullptr;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(vertexShaderModule, wgpuDeviceCreateShaderModule(m_device, &vertexShaderModuleDescriptor));

	WGPUVertexState vertexState = {};
	vertexState.nextInChain = nullptr;
	vertexState.module = vertexShaderModule;
	vertexState.entryPoint = "main";
	vertexState.constantCount = 0;
	vertexState.constants = nullptr;
	vertexState.bufferCount = 0;
	vertexState.buffers = nullptr;

	const char* fragmentShader = R"WGSL(
		@fragment
		fn main(@location(0) color: vec3f) -> @location(0) vec4f {
			return vec4f(color, 1.0);
		}
	)WGSL";

	WGPUShaderModule fragmentShaderModule = nullptr;
	WGPUShaderModuleWGSLDescriptor fragmentShaderModuleWGSLDescriptor = {};
	fragmentShaderModuleWGSLDescriptor.chain.next = nullptr;
	fragmentShaderModuleWGSLDescriptor.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	fragmentShaderModuleWGSLDescriptor.code = fragmentShader;

	WGPUShaderModuleDescriptor fragmentShaderModuleDescriptor = {};
	fragmentShaderModuleDescriptor.nextInChain = &fragmentShaderModuleWGSLDescriptor.chain;
	fragmentShaderModuleDescriptor.label = "Fragment shader module";
	fragmentShaderModuleDescriptor.hintCount = 0;
	fragmentShaderModuleDescriptor.hints = nullptr;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(fragmentShaderModule, wgpuDeviceCreateShaderModule(m_device, &fragmentShaderModuleDescriptor));

	WGPUBlendState blendState = {};
	blendState.color.operation = WGPUBlendOperation_Add;
	blendState.color.srcFactor = WGPUBlendFactor_One;
	blendState.color.dstFactor = WGPUBlendFactor_Zero;
	blendState.alpha.operation = WGPUBlendOperation_Add;
	blendState.alpha.srcFactor = WGPUBlendFactor_One;
	blendState.alpha.dstFactor = WGPUBlendFactor_Zero;

	WGPUColorTargetState swapChainColorTargetState = {};
	swapChainColorTargetState.nextInChain = nullptr;
	swapChainColorTargetState.format = m_swapChainFormat;
	swapChainColorTargetState.blend = &blendState;
	swapChainColorTargetState.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragmentState = {};
	fragmentState.nextInChain = nullptr;
	fragmentState.module = fragmentShaderModule;
	fragmentState.entryPoint = "main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;
	fragmentState.targetCount = 1;
	fragmentState.targets = &swapChainColorTargetState;

	WGPURenderPipelineDescriptor renderPipelineDescriptor = {};
	renderPipelineDescriptor.nextInChain = nullptr;
	renderPipelineDescriptor.label = "Render pipeline";
	renderPipelineDescriptor.layout = nullptr;
	renderPipelineDescriptor.vertex = vertexState;
	renderPipelineDescriptor.primitive.nextInChain = nullptr;
	renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	renderPipelineDescriptor.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	renderPipelineDescriptor.primitive.frontFace = WGPUFrontFace_CCW;
	renderPipelineDescriptor.primitive.cullMode = WGPUCullMode_Back;
	renderPipelineDescriptor.depthStencil = nullptr;
	renderPipelineDescriptor.multisample.nextInChain = nullptr;
	renderPipelineDescriptor.multisample.count = 1;
	renderPipelineDescriptor.multisample.mask = ~0u;
	renderPipelineDescriptor.multisample.alphaToCoverageEnabled = false;
	renderPipelineDescriptor.fragment = &fragmentState;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_renderPipeline, wgpuDeviceCreateRenderPipeline(m_device, &renderPipelineDescriptor));

	wgpuShaderModuleDrop(vertexShaderModule);
	wgpuShaderModuleDrop(fragmentShaderModule);
}

void NtshEngn::GraphicsModule::update(double dt) {
	NTSHENGN_UNUSED(dt);

	if (m_swapChainWidth != static_cast<uint32_t>(windowModule->getWidth(windowModule->getMainWindowID())) ||
		m_swapChainHeight != static_cast<uint32_t>(windowModule->getHeight(windowModule->getMainWindowID()))) {
		resize();
	}

	WGPUTextureView swapChainTextureView = wgpuSwapChainGetCurrentTextureView(m_swapChain);
	if (!swapChainTextureView) {
		resize();
		NTSHENGN_WEBGPU_ASSIGN_CHECK(swapChainTextureView, wgpuSwapChainGetCurrentTextureView(m_swapChain));
	}

	WGPUCommandEncoder commandEncoder;
	WGPUCommandEncoderDescriptor commandEncoderDescriptor = {};
	commandEncoderDescriptor.nextInChain = nullptr;
	commandEncoderDescriptor.label = "Command encoder";
	NTSHENGN_WEBGPU_ASSIGN_CHECK(commandEncoder, wgpuDeviceCreateCommandEncoder(m_device, &commandEncoderDescriptor));

	WGPURenderPassEncoder renderPassEncoder = nullptr;
	WGPURenderPassColorAttachment swapChainRenderPassColorAttachment = {};
	swapChainRenderPassColorAttachment.view = swapChainTextureView;
	swapChainRenderPassColorAttachment.resolveTarget = nullptr;
	swapChainRenderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	swapChainRenderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	swapChainRenderPassColorAttachment.clearValue.r = 0.0f;
	swapChainRenderPassColorAttachment.clearValue.g = 0.0f;
	swapChainRenderPassColorAttachment.clearValue.b = 0.0f;
	swapChainRenderPassColorAttachment.clearValue.a = 0.0f;

	WGPURenderPassDescriptor renderPassDescriptor = {};
	renderPassDescriptor.nextInChain = nullptr;
	renderPassDescriptor.label = "Render pass";
	renderPassDescriptor.colorAttachmentCount = 1;
	renderPassDescriptor.colorAttachments = &swapChainRenderPassColorAttachment;
	renderPassDescriptor.depthStencilAttachment = nullptr;
	renderPassDescriptor.occlusionQuerySet = nullptr;
	renderPassDescriptor.timestampWriteCount = 0;
	renderPassDescriptor.timestampWrites = nullptr;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(renderPassEncoder, wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDescriptor));

	wgpuRenderPassEncoderSetPipeline(renderPassEncoder, m_renderPipeline);

	wgpuRenderPassEncoderDraw(renderPassEncoder, 3, 1, 0, 0);

	wgpuRenderPassEncoderEnd(renderPassEncoder);

	wgpuTextureViewDrop(swapChainTextureView);

	WGPUCommandBuffer commandBuffer;
	WGPUCommandBufferDescriptor commandBufferDescriptor = {};
	commandBufferDescriptor.nextInChain = nullptr;
	commandBufferDescriptor.label = "Command buffer";
	NTSHENGN_WEBGPU_ASSIGN_CHECK(commandBuffer, wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor));
	wgpuQueueSubmit(m_queue, 1, &commandBuffer);

	wgpuSwapChainPresent(m_swapChain);
}

void NtshEngn::GraphicsModule::destroy() {
	// Destroy render pipeline
	wgpuRenderPipelineDrop(m_renderPipeline);

	// Destroy swapchain
	wgpuSwapChainDrop(m_swapChain);

	// Destroy device
	wgpuDeviceDrop(m_device);

	// Destroy adapter
	wgpuAdapterDrop(m_adapter);

	// Destroy surface
	wgpuSurfaceDrop(m_surface);

	// Destroy instance
	wgpuInstanceDrop(m_instance);
}

NtshEngn::MeshID NtshEngn::GraphicsModule::load(const Mesh& mesh) {
	NTSHENGN_UNUSED(mesh);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return std::numeric_limits<MeshID>::max();
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	NTSHENGN_UNUSED(image);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return std::numeric_limits<ImageID>::max();
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	NTSHENGN_UNUSED(font);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return std::numeric_limits<FontID>::max();
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

void NtshEngn::GraphicsModule::createSwapChain() {
	// Create swapchain
	m_swapChainFormat = WGPUTextureFormat_BGRA8UnormSrgb;
	m_swapChainWidth = static_cast<uint32_t>(windowModule->getWidth(windowModule->getMainWindowID()));
	m_swapChainHeight = static_cast<uint32_t>(windowModule->getHeight(windowModule->getMainWindowID()));
	WGPUSwapChainDescriptor swapChainDescriptor = {};
	swapChainDescriptor.nextInChain = nullptr;
	swapChainDescriptor.label = "SwapChain";
	swapChainDescriptor.usage = WGPUTextureUsage_RenderAttachment;
	swapChainDescriptor.format = m_swapChainFormat;
	swapChainDescriptor.width = m_swapChainWidth;
	swapChainDescriptor.height = m_swapChainHeight;
	swapChainDescriptor.presentMode = WGPUPresentMode_Mailbox;
	NTSHENGN_WEBGPU_ASSIGN_CHECK(m_swapChain, wgpuDeviceCreateSwapChain(m_device, m_surface, &swapChainDescriptor));
}

void NtshEngn::GraphicsModule::resize() {
	while ((windowModule->getWidth(windowModule->getMainWindowID()) == 0) || (windowModule->getHeight(windowModule->getMainWindowID()) == 0)) {
		windowModule->pollEvents();
	}

	// Destroy swapchain
	wgpuSwapChainDrop(m_swapChain);

	// Recreate swapchain
	createSwapChain();
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}