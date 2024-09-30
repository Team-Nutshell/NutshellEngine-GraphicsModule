#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/modules/ntshengn_window_module_interface.h"
#define VMA_IMPLEMENTATION
#if defined(NTSHENGN_COMPILER_MSVC)
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4127)
#pragma warning(disable : 4189)
#pragma warning(disable : 4324)
#pragma warning(disable : 4505)
#elif defined(NTSHENGN_COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(NTSHENGN_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "../external/VulkanMemoryAllocator/include/vk_mem_alloc.h"
#if defined(NTSHENGN_COMPILER_MSVC)
#pragma warning(pop)
#elif defined(NTSHENGN_COMPILER_GCC)
#pragma GCC diagnostic pop
#elif defined(NTSHENGN_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#include <array>
#include <algorithm>

void NtshEngn::GraphicsModule::init() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_framesInFlight = 2;
	}
	else {
		m_framesInFlight = 1;
	}

	// Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = getName().c_str();
	applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.pEngineName = "NutshellEngine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
#if defined(NTSHENGN_DEBUG)
	std::array<const char*, 1> explicitLayers = { "VK_LAYER_KHRONOS_validation" };
	bool foundValidationLayer = false;
	uint32_t instanceLayerPropertyCount;
	NTSHENGN_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, nullptr));
	std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerPropertyCount);
	NTSHENGN_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, instanceLayerProperties.data()));

	for (const VkLayerProperties& availableLayer : instanceLayerProperties) {
		if (strcmp(availableLayer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
			foundValidationLayer = true;
			break;
		}
	}

	if (foundValidationLayer) {
		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = explicitLayers.data();
	}
	else {
		NTSHENGN_MODULE_WARNING("Could not find validation layer VK_LAYER_KHRONOS_validation.");
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = nullptr;
	}
#else
	instanceCreateInfo.enabledLayerCount = 0;
	instanceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
	std::vector<const char*> instanceExtensions;
#if defined(NTSHENGN_DEBUG)
	instanceExtensions.push_back("VK_EXT_debug_utils");
#endif
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		instanceExtensions.push_back("VK_KHR_surface");
		instanceExtensions.push_back("VK_KHR_get_surface_capabilities2");
#if defined(NTSHENGN_OS_WINDOWS)
		instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
		instanceExtensions.push_back("VK_KHR_xlib_surface");
#endif
	}
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	NTSHENGN_VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

#if defined(NTSHENGN_DEBUG)
	// Create debug messenger
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {};
	debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugMessengerCreateInfo.pNext = nullptr;
	debugMessengerCreateInfo.flags = 0;
	debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMessengerCreateInfo.pfnUserCallback = debugCallback;
	debugMessengerCreateInfo.pUserData = nullptr;

	auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
	NTSHENGN_VK_CHECK(createDebugUtilsMessengerEXT(m_instance, &debugMessengerCreateInfo, nullptr, &m_debugMessenger));
#endif

	// Create surface
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
#if defined(NTSHENGN_OS_WINDOWS)
		HWND windowHandle = reinterpret_cast<HWND>(windowModule->getWindowNativeHandle(windowModule->getMainWindowID()));
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(windowModule->getWindowNativeAdditionalInformation(windowModule->getMainWindowID()));
		surfaceCreateInfo.hwnd = windowHandle;
		auto createWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateWin32SurfaceKHR");
		NTSHENGN_VK_CHECK(createWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#elif defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
		Window windowHandle = reinterpret_cast<Window>(windowModule->getWindowNativeHandle(windowModule->getMainWindowID()));
		VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.dpy = reinterpret_cast<Display*>(windowModule->getWindowNativeAdditionalInformation(windowModule->getMainWindowID()));
		surfaceCreateInfo.window = windowHandle;
		auto createXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR");
		NTSHENGN_VK_CHECK(createXlibSurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#endif
	}

	// Pick a physical device
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		NTSHENGN_MODULE_ERROR("Vulkan: Found no suitable GPU.", Result::ModuleError);
	}
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());
	m_physicalDevice = physicalDevices[0];

	VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {};
	physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties2.pNext = nullptr;
	vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties2);

	std::string physicalDeviceType;
	switch (physicalDeviceProperties2.properties.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		physicalDeviceType = "Other";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		physicalDeviceType = "Integrated";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		physicalDeviceType = "Discrete";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		physicalDeviceType = "Virtual";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		physicalDeviceType = "CPU";
		break;
	default:
		physicalDeviceType = "Unknown";
	}

	std::string driverVersion = std::to_string(VK_API_VERSION_MAJOR(physicalDeviceProperties2.properties.driverVersion)) + "."
		+ std::to_string(VK_API_VERSION_MINOR(physicalDeviceProperties2.properties.driverVersion)) + "."
		+ std::to_string(VK_API_VERSION_PATCH(physicalDeviceProperties2.properties.driverVersion));
	if (physicalDeviceProperties2.properties.vendorID == 4318) { // NVIDIA
		uint32_t major = (physicalDeviceProperties2.properties.driverVersion >> 22) & 0x3ff;
		uint32_t minor = (physicalDeviceProperties2.properties.driverVersion >> 14) & 0x0ff;
		uint32_t patch = (physicalDeviceProperties2.properties.driverVersion >> 6) & 0x0ff;
		driverVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
	}
#if defined(NTSHENGN_OS_WINDOWS)
	else if (physicalDeviceProperties2.properties.vendorID == 0x8086) { // Intel
		uint32_t major = (physicalDeviceProperties2.properties.driverVersion >> 14);
		uint32_t minor = (physicalDeviceProperties2.properties.driverVersion) & 0x3fff;
		driverVersion = std::to_string(major) + "." + std::to_string(minor);
	}
#endif

	NTSHENGN_MODULE_INFO("Physical Device Name: " + std::string(physicalDeviceProperties2.properties.deviceName));
	NTSHENGN_MODULE_INFO("Physical Device Type: " + physicalDeviceType);
	NTSHENGN_MODULE_INFO("Physical Device Driver Version: " + driverVersion);
	NTSHENGN_MODULE_INFO("Physical Device Vulkan API Version: " + std::to_string(VK_API_VERSION_MAJOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_MINOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_PATCH(physicalDeviceProperties2.properties.apiVersion)));

	// Find a queue family supporting graphics
	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
	m_graphicsComputeQueueFamilyIndex = 0;
	for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProperties) {
		if ((queueFamilyProperty.queueCount > 0) && (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
				VkBool32 presentSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsComputeQueueFamilyIndex, m_surface, &presentSupport);
				if (presentSupport) {
					break;
				}
			}
			else {
				break;
			}
		}
		m_graphicsComputeQueueFamilyIndex++;
	}

	// Create a queue supporting graphics and compute
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

	// Enable features
	VkPhysicalDeviceShaderDrawParametersFeatures physicalDeviceShaderDrawParametersFeatures;
	physicalDeviceShaderDrawParametersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	physicalDeviceShaderDrawParametersFeatures.pNext = nullptr;
	physicalDeviceShaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE;

	VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures = {};
	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	physicalDeviceDescriptorIndexingFeatures.pNext = &physicalDeviceShaderDrawParametersFeatures;
	physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;

	VkPhysicalDeviceDynamicRenderingFeatures physicalDeviceDynamicRenderingFeatures = {};
	physicalDeviceDynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	physicalDeviceDynamicRenderingFeatures.pNext = &physicalDeviceDescriptorIndexingFeatures;
	physicalDeviceDynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features = {};
	physicalDeviceSynchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	physicalDeviceSynchronization2Features.pNext = &physicalDeviceDynamicRenderingFeatures;
	physicalDeviceSynchronization2Features.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.multiDrawIndirect = VK_TRUE;
	physicalDeviceFeatures.depthClamp = VK_TRUE;
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

	// Create the logical device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &physicalDeviceSynchronization2Features;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	std::vector<const char*> deviceExtensions = { "VK_KHR_synchronization2",
		"VK_KHR_create_renderpass2",
		"VK_KHR_depth_stencil_resolve",
		"VK_KHR_dynamic_rendering",
		"VK_EXT_descriptor_indexing",
		"VK_KHR_draw_indirect_count" };
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		deviceExtensions.push_back("VK_KHR_swapchain");
	}
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
	NTSHENGN_VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, m_graphicsComputeQueueFamilyIndex, 0, &m_graphicsComputeQueue);

	// Get functions
	m_vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)vkGetDeviceProcAddr(m_device, "vkCmdPipelineBarrier2KHR");
	m_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdBeginRenderingKHR");
	m_vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdEndRenderingKHR");

	// Initialize VMA
	VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {};
	vmaAllocatorCreateInfo.flags = 0;
	vmaAllocatorCreateInfo.physicalDevice = m_physicalDevice;
	vmaAllocatorCreateInfo.device = m_device;
	vmaAllocatorCreateInfo.preferredLargeHeapBlockSize = 0;
	vmaAllocatorCreateInfo.pAllocationCallbacks = nullptr;
	vmaAllocatorCreateInfo.pDeviceMemoryCallbacks = nullptr;
	vmaAllocatorCreateInfo.pHeapSizeLimit = nullptr;
	vmaAllocatorCreateInfo.pVulkanFunctions = nullptr;
	vmaAllocatorCreateInfo.instance = m_instance;
	vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
	NTSHENGN_VK_CHECK(vmaCreateAllocator(&vmaAllocatorCreateInfo, &m_allocator));

	// Create the swapchain
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		createSwapchain(VK_NULL_HANDLE);
	}
	// Or create an image to draw on
	else {
		m_drawImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		m_imageCount = 1;

		m_viewport.x = 0.0f;
		m_viewport.y = 0.0f;
		m_viewport.width = 1280.0f;
		m_viewport.height = 720.0f;
		m_viewport.minDepth = 0.0f;
		m_viewport.maxDepth = 1.0f;

		m_scissor.offset.x = 0;
		m_scissor.offset.y = 0;
		m_scissor.extent.width = 1280;
		m_scissor.extent.height = 720;

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_drawImageFormat;
		imageCreateInfo.extent.width = 1280;
		imageCreateInfo.extent.height = 720;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo imageAllocationCreateInfo = {};
		imageAllocationCreateInfo.flags = 0;
		imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &imageCreateInfo, &imageAllocationCreateInfo, &m_drawImage.handle, &m_drawImage.allocation, nullptr));

		// Create the image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_drawImage.handle;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_drawImage.view));
	}

	// Create initialization command pool
	VkCommandPoolCreateInfo initializationCommandPoolCreateInfo = {};
	initializationCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	initializationCommandPoolCreateInfo.pNext = nullptr;
	initializationCommandPoolCreateInfo.flags = 0;
	initializationCommandPoolCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &initializationCommandPoolCreateInfo, nullptr, &m_initializationCommandPool));

	// Allocate initialization command pool
	VkCommandBufferAllocateInfo initializationCommandBufferAllocateInfo = {};
	initializationCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	initializationCommandBufferAllocateInfo.pNext = nullptr;
	initializationCommandBufferAllocateInfo.commandPool = m_initializationCommandPool;
	initializationCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	initializationCommandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &initializationCommandBufferAllocateInfo, &m_initializationCommandBuffer));

	// Create initialization fence
	VkFenceCreateInfo initializationFenceCreateInfo = {};
	initializationFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	initializationFenceCreateInfo.pNext = nullptr;
	initializationFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &initializationFenceCreateInfo, nullptr, &m_initializationFence));

	createVertexAndIndexBuffers();

	// Create camera uniform buffer
	m_cameraBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo cameraBufferCreateInfo = {};
	cameraBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cameraBufferCreateInfo.pNext = nullptr;
	cameraBufferCreateInfo.flags = 0;
	cameraBufferCreateInfo.size = sizeof(Math::mat4) * 2 + sizeof(Math::vec4);
	cameraBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cameraBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	cameraBufferCreateInfo.queueFamilyIndexCount = 1;
	cameraBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo bufferAllocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &cameraBufferCreateInfo, &bufferAllocationCreateInfo, &m_cameraBuffers[i].handle, &m_cameraBuffers[i].allocation, &bufferAllocationInfo));
		m_cameraBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create object storage buffer
	m_objectBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo objectBufferCreateInfo = {};
	objectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	objectBufferCreateInfo.pNext = nullptr;
	objectBufferCreateInfo.flags = 0;
	objectBufferCreateInfo.size = 262144;
	objectBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	objectBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	objectBufferCreateInfo.queueFamilyIndexCount = 1;
	objectBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &objectBufferCreateInfo, &bufferAllocationCreateInfo, &m_objectBuffers[i].handle, &m_objectBuffers[i].allocation, &bufferAllocationInfo));
		m_objectBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create mesh storage buffer
	VkBufferCreateInfo meshBufferCreateInfo = {};
	meshBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	meshBufferCreateInfo.pNext = nullptr;
	meshBufferCreateInfo.flags = 0;
	meshBufferCreateInfo.size = 32768;
	meshBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	meshBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	meshBufferCreateInfo.queueFamilyIndexCount = 1;
	meshBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo meshBufferAllocationCreateInfo = {};
	meshBufferAllocationCreateInfo.flags = 0;
	meshBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &meshBufferCreateInfo, &meshBufferAllocationCreateInfo, &m_meshBuffer.handle, &m_meshBuffer.allocation, &bufferAllocationInfo));

	// Create joint transform storage buffers
	m_jointTransformBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo jointTransformBufferCreateInfo = {};
	jointTransformBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	jointTransformBufferCreateInfo.pNext = nullptr;
	jointTransformBufferCreateInfo.flags = 0;
	jointTransformBufferCreateInfo.size = 4096 * sizeof(Math::mat4);
	jointTransformBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	jointTransformBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	jointTransformBufferCreateInfo.queueFamilyIndexCount = 1;
	jointTransformBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &jointTransformBufferCreateInfo, &bufferAllocationCreateInfo, &m_jointTransformBuffers[i].handle, &m_jointTransformBuffers[i].allocation, &bufferAllocationInfo));
		m_jointTransformBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create material storage buffer
	m_materialBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo materialBufferCreateInfo = {};
	materialBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	materialBufferCreateInfo.pNext = nullptr;
	materialBufferCreateInfo.flags = 0;
	materialBufferCreateInfo.size = 32768;
	materialBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	materialBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	materialBufferCreateInfo.queueFamilyIndexCount = 1;
	materialBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &materialBufferCreateInfo, &bufferAllocationCreateInfo, &m_materialBuffers[i].handle, &m_materialBuffers[i].allocation, &bufferAllocationInfo));
		m_materialBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create light storage buffer
	m_lightBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo lightBufferCreateInfo = {};
	lightBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	lightBufferCreateInfo.pNext = nullptr;
	lightBufferCreateInfo.flags = 0;
	lightBufferCreateInfo.size = 32768;
	lightBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	lightBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lightBufferCreateInfo.queueFamilyIndexCount = 1;
	lightBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &lightBufferCreateInfo, &bufferAllocationCreateInfo, &m_lightBuffers[i].handle, &m_lightBuffers[i].allocation, &bufferAllocationInfo));
		m_lightBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	glslang::InitializeProcess();

	m_frustumCulling.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_framesInFlight,
		m_vkCmdPipelineBarrier2KHR,
		jobSystem,
		ecs);

	m_gBuffer.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_initializationCommandPool,
		m_initializationCommandBuffer,
		m_initializationFence,
		m_viewport,
		m_scissor,
		m_framesInFlight,
		m_frustumCulling.getPerDrawBuffers(),
		m_cameraBuffers,
		m_objectBuffers,
		m_meshBuffer,
		m_jointTransformBuffers,
		m_materialBuffers,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR);

	m_ssao.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_initializationCommandPool,
		m_initializationCommandBuffer,
		m_initializationFence,
		m_gBuffer.getPosition().view,
		m_gBuffer.getNormal().view,
		m_viewport,
		m_scissor,
		m_framesInFlight,
		m_cameraBuffers,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR);

	m_shadowMapping.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_initializationCommandPool,
		m_initializationCommandBuffer,
		m_initializationFence,
		m_framesInFlight,
		m_objectBuffers,
		m_meshBuffer,
		m_jointTransformBuffers,
		m_materialBuffers,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR,
		ecs);

	createCompositingResources();

	m_particles.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_compositingImageFormat,
		m_initializationCommandPool,
		m_initializationCommandBuffer,
		m_initializationFence,
		m_viewport,
		m_scissor,
		m_framesInFlight,
		m_cameraBuffers,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR);

#if BLOOM_ENABLE == 1
	m_bloom.init(m_device,
		m_graphicsComputeQueue,
		m_graphicsComputeQueueFamilyIndex,
		m_allocator,
		m_compositingImage.view,
		m_compositingImageFormat,
		m_initializationCommandPool,
		m_initializationCommandBuffer,
		m_initializationFence,
		m_viewport,
		m_scissor,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR);
#endif

	createToneMappingResources();

	m_fxaa.init(m_device,
		m_graphicsComputeQueueFamilyIndex,
		m_toneMappingImage.view,
		m_drawImageFormat,
		m_viewport,
		m_scissor,
		m_vkCmdBeginRenderingKHR,
		m_vkCmdEndRenderingKHR,
		m_vkCmdPipelineBarrier2KHR);

	createUIResources();

	createDefaultResources();

	// Resize buffers according to number of frames in flight and swapchain size
	m_renderingCommandPools.resize(m_framesInFlight);
	m_renderingCommandBuffers.resize(m_framesInFlight);

	m_fences.resize(m_framesInFlight);
	m_imageAvailableSemaphores.resize(m_framesInFlight);
	m_renderFinishedSemaphores.resize(m_imageCount);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		// Create rendering command pools and buffers
		NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_renderingCommandPools[i]));

		commandBufferAllocateInfo.commandPool = m_renderingCommandPools[i];
		NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_renderingCommandBuffers[i]));

		// Create sync objects
		NTSHENGN_VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_fences[i]));

		NTSHENGN_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]));
	}
	for (uint32_t i = 0; i < m_imageCount; i++) {
		NTSHENGN_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]));
	}

	// Set current frame-in-flight to 0
	m_currentFrameInFlight = 0;
}

void NtshEngn::GraphicsModule::update(double dt) {
	if (windowModule && (!windowModule->isWindowOpen(windowModule->getMainWindowID()) || ((windowModule->getWindowWidth(windowModule->getMainWindowID()) == 0) || (windowModule->getWindowHeight(windowModule->getMainWindowID()) == 0)))) {
		// Do not update if the main window got closed or the window size is 0
		return;
	}

	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_fences[m_currentFrameInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max()));

	uint32_t imageIndex = m_imageCount - 1;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[m_currentFrameInFlight], VK_NULL_HANDLE, &imageIndex);
		if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
			resize();
		}
		else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
			NTSHENGN_MODULE_ERROR("Next swapchain image acquire failed.", Result::ModuleError);
		}
	}
	else {
		VkSubmitInfo emptySignalSubmitInfo = {};
		emptySignalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		emptySignalSubmitInfo.pNext = nullptr;
		emptySignalSubmitInfo.waitSemaphoreCount = 0;
		emptySignalSubmitInfo.pWaitSemaphores = nullptr;
		emptySignalSubmitInfo.pWaitDstStageMask = nullptr;
		emptySignalSubmitInfo.commandBufferCount = 0;
		emptySignalSubmitInfo.pCommandBuffers = nullptr;
		emptySignalSubmitInfo.signalSemaphoreCount = 1;
		emptySignalSubmitInfo.pSignalSemaphores = &m_imageAvailableSemaphores[m_currentFrameInFlight];
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &emptySignalSubmitInfo, VK_NULL_HANDLE));
	}

	// Update camera buffer
	float cameraNearPlane = 0.0f;
	float cameraFarPlane = 0.0f;
	Math::mat4 cameraView = Math::mat4::identity();
	Math::mat4 cameraProjection = Math::mat4::identity();
	if (m_mainCamera != NTSHENGN_ENTITY_UNKNOWN) {
		const Camera& camera = ecs->getComponent<Camera>(m_mainCamera);
		const Transform& cameraTransform = ecs->getComponent<Transform>(m_mainCamera);

		cameraNearPlane = camera.nearPlane;
		cameraFarPlane = camera.farPlane;

		const Math::mat4 cameraRotation = Math::rotate(cameraTransform.rotation.x, Math::vec3(1.0f, 0.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.y, Math::vec3(0.0f, 1.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.z, Math::vec3(0.0f, 0.0f, 1.0f));
		cameraView = cameraRotation * Math::lookAtRH(cameraTransform.position, cameraTransform.position + camera.forward, camera.up);
		cameraProjection = Math::perspectiveRH(camera.fov, m_viewport.width / m_viewport.height, camera.nearPlane, camera.farPlane);
		cameraProjection[1][1] *= -1.0f;
		std::array<Math::mat4, 2> cameraMatrices{ cameraView, cameraProjection };
		Math::vec4 cameraPositionAsVec4 = { cameraTransform.position, 0.0f };

		memcpy(m_cameraBuffers[m_currentFrameInFlight].address, cameraMatrices.data(), sizeof(Math::mat4) * 2);
		memcpy(reinterpret_cast<char*>(m_cameraBuffers[m_currentFrameInFlight].address) + sizeof(Math::mat4) * 2, cameraPositionAsVec4.data(), sizeof(Math::vec4));
	}

	// Update objects buffer
	for (auto& it : m_objects) {
		loadRenderableForEntity(it.first);

		if (it.second.meshID == 0) {
			continue;
		}

		const Transform& objectTransform = ecs->getComponent<Transform>(it.first);

		Math::mat4 objectModel = Math::translate(objectTransform.position) *
			Math::rotate(objectTransform.rotation.x, Math::vec3(1.0f, 0.0f, 0.0f)) *
			Math::rotate(objectTransform.rotation.y, Math::vec3(0.0f, 1.0f, 0.0f)) *
			Math::rotate(objectTransform.rotation.z, Math::vec3(0.0f, 0.0f, 1.0f)) *
			Math::scale(objectTransform.scale);
		Math::mat4 transposeInverseObjectModel = Math::transpose(Math::inverse(objectModel));

		size_t offset = (it.second.index * ((sizeof(Math::mat4) * 2) + sizeof(Math::vec4))); // vec4 is used here for padding

		memcpy(reinterpret_cast<char*>(m_objectBuffers[m_currentFrameInFlight].address) + offset, objectModel.data(), sizeof(Math::mat4));
		memcpy(reinterpret_cast<char*>(m_objectBuffers[m_currentFrameInFlight].address) + offset + sizeof(Math::mat4), transposeInverseObjectModel.data(), sizeof(Math::mat4));
		const std::array<uint32_t, 3> objectIndices = { (it.second.meshID < m_meshes.size()) ? it.second.meshID : 0, it.second.jointTransformOffset, (it.second.materialIndex < m_materials.size()) ? it.second.materialIndex : 0 };
		memcpy(reinterpret_cast<char*>(m_objectBuffers[m_currentFrameInFlight].address) + offset + (sizeof(Math::mat4) * 2), objectIndices.data(), 3 * sizeof(uint32_t));
	}

	// Update joint transforms buffer
	for (auto& it : m_objects) {
		const Renderable& objectRenderable = ecs->getComponent<Renderable>(it.first);

		if (objectRenderable.mesh) {
			const Skin& skin = objectRenderable.mesh->skin;

			if (!skin.joints.empty()) {
				std::vector<Math::mat4> jointTransformMatrices(skin.joints.size(), Math::mat4::identity());
				std::vector<Math::mat4> parentJointTransformMatrices(skin.joints.size(), Math::mat4::identity());

				if (m_playingAnimations.find(&it.second) != m_playingAnimations.end()) {
					PlayingAnimation& playingAnimation = m_playingAnimations[&it.second];
					const Animation& animation = objectRenderable.mesh->animations[m_playingAnimations[&it.second].animationIndex];

					std::queue<std::pair<uint32_t, uint32_t>> jointsAndParents;
					jointsAndParents.push({ skin.rootJoint, std::numeric_limits<uint32_t>::max() });
					while (!jointsAndParents.empty()) {
						uint32_t jointIndex = jointsAndParents.front().first;
						Math::mat4 parentJointTransformMatrix;
						if (jointsAndParents.front().second != std::numeric_limits<uint32_t>::max()) {
							parentJointTransformMatrix = parentJointTransformMatrices[jointsAndParents.front().second];
						}
						else {
							parentJointTransformMatrix = skin.baseMatrix;
						}

						if (animation.jointChannels.find(jointIndex) != animation.jointChannels.end()) {
							const std::vector<AnimationChannel>& channels = animation.jointChannels.at(jointIndex);

							Math::vec3 translation;
							Math::quat rotation;
							Math::vec3 scale;
							Math::decomposeTransform(skin.joints[jointIndex].localTransform, translation, rotation, scale);
							for (const AnimationChannel& channel : channels) {
								// Find previous keyframe
								uint32_t keyframe = findPreviousAnimationKeyframe(playingAnimation.time, channel.keyframes);

								if (keyframe == std::numeric_limits<uint32_t>::max()) {
									continue;
								}

								const AnimationChannelKeyframe& previousKeyframe = channel.keyframes[keyframe];

								if (channel.interpolationType == AnimationChannelInterpolationType::Step) {
									const Math::vec4 channelPrevious = previousKeyframe.value;

									// Step interpolation
									if (channel.transformType == AnimationChannelTransformType::Translation) {
										translation = Math::vec3(channelPrevious);
									}
									else if (channel.transformType == AnimationChannelTransformType::Rotation) {
										rotation = Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w);
									}
									else if (channel.transformType == AnimationChannelTransformType::Scale) {
										scale = Math::vec3(channelPrevious);
									}
								}
								else if (channel.interpolationType == AnimationChannelInterpolationType::Linear) {
									// Linear interpolation
									const Math::vec4& channelPrevious = previousKeyframe.value;

									if ((keyframe + 1) >= channel.keyframes.size()) {
										// Last keyframe
										if (channel.transformType == AnimationChannelTransformType::Translation) {
											translation = Math::vec3(channelPrevious);
										}
										else if (channel.transformType == AnimationChannelTransformType::Rotation) {
											rotation = Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w);
										}
										else if (channel.transformType == AnimationChannelTransformType::Scale) {
											scale = Math::vec3(channelPrevious);
										}
									}
									else {
										const AnimationChannelKeyframe& nextKeyframe = channel.keyframes[keyframe + 1];
										const Math::vec4& channelNext = nextKeyframe.value;

										const float timestampPrevious = previousKeyframe.timestamp;
										const float timestampNext = nextKeyframe.timestamp;
										const float interpolationValue = (playingAnimation.time - timestampPrevious) / (timestampNext - timestampPrevious);

										if (channel.transformType == AnimationChannelTransformType::Translation) {
											translation = Math::vec3(Math::lerp(channelPrevious.x, channelNext.x, interpolationValue),
												Math::lerp(channelPrevious.y, channelNext.y, interpolationValue),
												Math::lerp(channelPrevious.z, channelNext.z, interpolationValue));
										}
										else if (channel.transformType == AnimationChannelTransformType::Rotation) {
											rotation = Math::normalize(Math::slerp(Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w),
												Math::quat(channelNext.x, channelNext.y, channelNext.z, channelNext.w),
												interpolationValue));
										}
										else if (channel.transformType == AnimationChannelTransformType::Scale) {
											scale = Math::vec3(Math::lerp(channelPrevious.x, channelNext.x, interpolationValue),
												Math::lerp(channelPrevious.y, channelNext.y, interpolationValue),
												Math::lerp(channelPrevious.z, channelNext.z, interpolationValue));
										}
									}
								}
							}

							const Math::mat4 jointTransformMatrix = Math::translate(translation) *
								Math::quatToRotationMatrix(rotation) *
								Math::scale(scale);

							jointTransformMatrices[jointIndex] = parentJointTransformMatrix * jointTransformMatrix;
							parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
							jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
							jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];
						}
						else {
							jointTransformMatrices[jointIndex] = parentJointTransformMatrix * skin.joints[jointIndex].localTransform;
							parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
							jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
							jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];
						}

						for (uint32_t jointChild : skin.joints[jointIndex].children) {
							jointsAndParents.push({ jointChild, jointIndex });
						}

						jointsAndParents.pop();
					}

					if (m_playingAnimations[&it.second].isPlaying) {
						playingAnimation.time += static_cast<float>(dt) / 1000.0f;
					}

					// End animation
					if (playingAnimation.time >= animation.duration) {
						m_playingAnimations.erase(&it.second);
					}
				}
				else {
					std::queue<std::pair<uint32_t, uint32_t>> jointsAndParents;
					jointsAndParents.push({ skin.rootJoint, std::numeric_limits<uint32_t>::max() });
					while (!jointsAndParents.empty()) {
						uint32_t jointIndex = jointsAndParents.front().first;
						Math::mat4 parentJointTransformMatrix;
						if (jointsAndParents.front().second != std::numeric_limits<uint32_t>::max()) {
							parentJointTransformMatrix = parentJointTransformMatrices[jointsAndParents.front().second];
						}
						else {
							parentJointTransformMatrix = skin.baseMatrix;
						}

						jointTransformMatrices[jointIndex] = parentJointTransformMatrix * skin.joints[jointIndex].localTransform;
						parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
						jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
						jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];

						for (uint32_t jointChild : skin.joints[jointIndex].children) {
							jointsAndParents.push({ jointChild, jointIndex });
						}

						jointsAndParents.pop();
					}
				}

				memcpy(reinterpret_cast<char*>(m_jointTransformBuffers[m_currentFrameInFlight].address) + (sizeof(Math::mat4) * it.second.jointTransformOffset), jointTransformMatrices.data(), sizeof(Math::mat4) * m_meshes[it.second.meshID].jointCount);
			}
		}
	}

	// Update materials buffer
	for (size_t i = 0; i < m_materials.size(); i++) {
		size_t offset = i * sizeof(InternalMaterial);

		memcpy(reinterpret_cast<char*>(m_materialBuffers[m_currentFrameInFlight].address) + offset, &m_materials[i], sizeof(InternalMaterial));
	}

	// Update lights buffer
	std::array<uint32_t, 4> lightsCount = { static_cast<uint32_t>(m_lights.directionalLights.size()), static_cast<uint32_t>(m_lights.pointLights.size()), static_cast<uint32_t>(m_lights.spotLights.size()), static_cast<uint32_t>(m_lights.ambientLights.size()) };
	memcpy(m_lightBuffers[m_currentFrameInFlight].address, lightsCount.data(), 4 * sizeof(uint32_t));

	size_t offset = sizeof(Math::vec4);
	for (Entity light : m_lights.directionalLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		const Math::vec3 baseLightDirection = Math::normalize(lightLight.direction);
		const float baseDirectionYaw = std::atan2(baseLightDirection.z, baseLightDirection.x);
		const float baseDirectionPitch = -std::asin(baseLightDirection.y);
		const Math::vec3 lightDirection = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::cos(baseDirectionYaw + lightTransform.rotation.y),
			-std::sin(baseDirectionPitch + lightTransform.rotation.x),
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::sin(baseDirectionYaw + lightTransform.rotation.y)
		));

		InternalLight internalLight;
		internalLight.direction = Math::vec4(lightDirection, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.pointLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.position = Math::vec4(lightTransform.position, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.spotLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		const Math::vec3 baseLightDirection = Math::normalize(lightLight.direction);
		const float baseDirectionYaw = std::atan2(baseLightDirection.z, baseLightDirection.x);
		const float baseDirectionPitch = -std::asin(baseLightDirection.y);
		const Math::vec3 lightDirection = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::cos(baseDirectionYaw + lightTransform.rotation.y),
			-std::sin(baseDirectionPitch + lightTransform.rotation.x),
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::sin(baseDirectionYaw + lightTransform.rotation.y)
		));

		InternalLight internalLight;
		internalLight.position = Math::vec4(lightTransform.position, 0.0f);
		internalLight.direction = Math::vec4(lightDirection, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);
		internalLight.cutoff = Math::vec4(lightLight.cutoff, 0.0f, 0.0f);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.ambientLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);

		InternalLight internalLight;
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}

	// Update descriptor sets if needed
	m_gBuffer.updateDescriptorSets(m_currentFrameInFlight, m_textures, m_textureImageViews, m_textureSamplers);
	m_shadowMapping.updateDescriptorSets(m_currentFrameInFlight, m_textures, m_textureImageViews, m_textureSamplers);
	if (m_compositingDescriptorSetsNeedShadowUpdate[m_currentFrameInFlight]) {
		updateCompositingDescriptorSetsShadow(m_currentFrameInFlight);

		m_compositingDescriptorSetsNeedShadowUpdate[m_currentFrameInFlight] = false;
	}
	if (m_uiTextDescriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateUITextDescriptorSet(m_currentFrameInFlight);

		m_uiTextDescriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
	}
	if (m_uiImageDescriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateUIImageDescriptorSet(m_currentFrameInFlight);

		m_uiImageDescriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
	}

	// Record rendering commands
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_renderingCommandPools[m_currentFrameInFlight], 0));

	// Begin command buffer recording
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], &commandBufferBeginInfo));

	// Layout transitions
	VkImageMemoryBarrier2 swapchainOrDrawImageMemoryBarrier = {};
	swapchainOrDrawImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	swapchainOrDrawImageMemoryBarrier.pNext = nullptr;
	swapchainOrDrawImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainOrDrawImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	swapchainOrDrawImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainOrDrawImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	swapchainOrDrawImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainOrDrawImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainOrDrawImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	swapchainOrDrawImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	swapchainOrDrawImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage.handle;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.levelCount = 1;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 compositingFragmentToColorAttachmentImageMemoryBarrier = {};
	compositingFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	compositingFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	compositingFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	compositingFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	compositingFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositingFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingFragmentToColorAttachmentImageMemoryBarrier.image = m_compositingImage.handle;
	compositingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	compositingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	compositingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	compositingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 toneMappingFragmentToColorAttachmentImageMemoryBarrier = {};
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.image = m_toneMappingImage.handle;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	toneMappingFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 3> startFrameImageMemoryBarriers = { swapchainOrDrawImageMemoryBarrier, compositingFragmentToColorAttachmentImageMemoryBarrier, toneMappingFragmentToColorAttachmentImageMemoryBarrier };
	VkDependencyInfo startFrameDependencyInfo = {};
	startFrameDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	startFrameDependencyInfo.pNext = nullptr;
	startFrameDependencyInfo.dependencyFlags = 0;
	startFrameDependencyInfo.memoryBarrierCount = 0;
	startFrameDependencyInfo.pMemoryBarriers = nullptr;
	startFrameDependencyInfo.bufferMemoryBarrierCount = 0;
	startFrameDependencyInfo.pBufferMemoryBarriers = nullptr;
	startFrameDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(startFrameImageMemoryBarriers.size());
	startFrameDependencyInfo.pImageMemoryBarriers = startFrameImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &startFrameDependencyInfo);

	// Frustum culling
	uint32_t drawCount = 0;
	if (m_mainCamera != NTSHENGN_ENTITY_UNKNOWN) {
		drawCount = m_frustumCulling.cull(m_renderingCommandBuffers[m_currentFrameInFlight],
			m_currentFrameInFlight,
			cameraView,
			cameraProjection,
			m_objects,
			m_meshes
		);
	}

	// Draw G-Buffer
	m_gBuffer.draw(m_renderingCommandBuffers[m_currentFrameInFlight],
		m_currentFrameInFlight,
		m_frustumCulling.getDrawIndirectBuffer(m_currentFrameInFlight),
		drawCount,
		m_vertexBuffer,
		m_indexBuffer
	);

	// Draw SSAO
	m_ssao.draw(m_renderingCommandBuffers[m_currentFrameInFlight], m_currentFrameInFlight);

	// Draw shadow mapping
	m_shadowMapping.draw(m_renderingCommandBuffers[m_currentFrameInFlight],
		m_currentFrameInFlight,
		cameraNearPlane,
		cameraFarPlane,
		cameraView,
		cameraProjection,
		m_objects,
		m_meshes,
		m_vertexBuffer,
		m_indexBuffer
	);

	// Compositing
	VkRenderingAttachmentInfo compositingAttachmentInfo = {};
	compositingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	compositingAttachmentInfo.pNext = nullptr;
	compositingAttachmentInfo.imageView = m_compositingImage.view;
	compositingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	compositingAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	compositingAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	compositingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	compositingAttachmentInfo.clearValue.color = { m_backgroundColor.x, m_backgroundColor.y, m_backgroundColor.z, m_backgroundColor.w };

	VkRenderingInfo compositingRenderingInfo = {};
	compositingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	compositingRenderingInfo.pNext = nullptr;
	compositingRenderingInfo.flags = 0;
	compositingRenderingInfo.renderArea = m_scissor;
	compositingRenderingInfo.layerCount = 1;
	compositingRenderingInfo.viewMask = 0;
	compositingRenderingInfo.colorAttachmentCount = 1;
	compositingRenderingInfo.pColorAttachments = &compositingAttachmentInfo;
	compositingRenderingInfo.pDepthAttachment = nullptr;
	compositingRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &compositingRenderingInfo);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositingGraphicsPipelineLayout, 0, 1, &m_compositingDescriptorSets[m_currentFrameInFlight], 0, nullptr);

	// Bind graphics pipeline
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositingGraphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 3, 1, 0, 0);

	// End compositing rendering
	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Compositing synchronization before particles
	VkImageMemoryBarrier2 compositingBeforeParticlesImageMemoryBarrier = {};
	compositingBeforeParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	compositingBeforeParticlesImageMemoryBarrier.pNext = nullptr;
	compositingBeforeParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingBeforeParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingBeforeParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingBeforeParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingBeforeParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingBeforeParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingBeforeParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingBeforeParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingBeforeParticlesImageMemoryBarrier.image = m_compositingImage.handle;
	compositingBeforeParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingBeforeParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	compositingBeforeParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	compositingBeforeParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	compositingBeforeParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo compositingBeforeParticlesDependencyInfo = {};
	compositingBeforeParticlesDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	compositingBeforeParticlesDependencyInfo.pNext = nullptr;
	compositingBeforeParticlesDependencyInfo.dependencyFlags = 0;
	compositingBeforeParticlesDependencyInfo.memoryBarrierCount = 0;
	compositingBeforeParticlesDependencyInfo.pMemoryBarriers = nullptr;
	compositingBeforeParticlesDependencyInfo.bufferMemoryBarrierCount = 0;
	compositingBeforeParticlesDependencyInfo.pBufferMemoryBarriers = nullptr;
	compositingBeforeParticlesDependencyInfo.imageMemoryBarrierCount = 1;
	compositingBeforeParticlesDependencyInfo.pImageMemoryBarriers = &compositingBeforeParticlesImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &compositingBeforeParticlesDependencyInfo);

	m_particles.draw(m_renderingCommandBuffers[m_currentFrameInFlight], m_compositingImage.handle, m_compositingImage.view, m_gBuffer.getDepth().handle, m_gBuffer.getDepth().view, m_currentFrameInFlight, static_cast<float>(dt / 1000.0));

	// Compositing synchronization after particles
	VkImageMemoryBarrier2 compositingAfterParticlesImageMemoryBarrier = {};
	compositingAfterParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	compositingAfterParticlesImageMemoryBarrier.pNext = nullptr;
	compositingAfterParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingAfterParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingAfterParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	compositingAfterParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	compositingAfterParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingAfterParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	compositingAfterParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingAfterParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingAfterParticlesImageMemoryBarrier.image = m_compositingImage.handle;
	compositingAfterParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingAfterParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	compositingAfterParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	compositingAfterParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	compositingAfterParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo compositingAfterParticlesDependencyInfo = {};
	compositingAfterParticlesDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	compositingAfterParticlesDependencyInfo.pNext = nullptr;
	compositingAfterParticlesDependencyInfo.dependencyFlags = 0;
	compositingAfterParticlesDependencyInfo.memoryBarrierCount = 0;
	compositingAfterParticlesDependencyInfo.pMemoryBarriers = nullptr;
	compositingAfterParticlesDependencyInfo.bufferMemoryBarrierCount = 0;
	compositingAfterParticlesDependencyInfo.pBufferMemoryBarriers = nullptr;
	compositingAfterParticlesDependencyInfo.imageMemoryBarrierCount = 1;
	compositingAfterParticlesDependencyInfo.pImageMemoryBarriers = &compositingAfterParticlesImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &compositingAfterParticlesDependencyInfo);

#if BLOOM_ENABLE == 1
	// Bloom
	m_bloom.draw(m_renderingCommandBuffers[m_currentFrameInFlight], m_compositingImage.handle, m_compositingImage.view);
#endif

	// Tone mapping
	VkRenderingAttachmentInfo toneMappingAttachmentInfo = {};
	toneMappingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	toneMappingAttachmentInfo.pNext = nullptr;
	toneMappingAttachmentInfo.imageView = m_toneMappingImage.view;
	toneMappingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	toneMappingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	toneMappingAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	toneMappingAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toneMappingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	toneMappingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	toneMappingAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo toneMappingRenderingInfo = {};
	toneMappingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	toneMappingRenderingInfo.pNext = nullptr;
	toneMappingRenderingInfo.flags = 0;
	toneMappingRenderingInfo.renderArea = m_scissor;
	toneMappingRenderingInfo.layerCount = 1;
	toneMappingRenderingInfo.viewMask = 0;
	toneMappingRenderingInfo.colorAttachmentCount = 1;
	toneMappingRenderingInfo.pColorAttachments = &toneMappingAttachmentInfo;
	toneMappingRenderingInfo.pDepthAttachment = nullptr;
	toneMappingRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &toneMappingRenderingInfo);

	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipelineLayout, 0, 1, &m_toneMappingDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Tone mapping layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier2 toneMappingColorAttachmentToFragmentImageMemoryBarrier = {};
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.image = m_toneMappingImage.handle;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	toneMappingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo toneMappingDependencyInfo = {};
	toneMappingDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	toneMappingDependencyInfo.pNext = nullptr;
	toneMappingDependencyInfo.dependencyFlags = 0;
	toneMappingDependencyInfo.memoryBarrierCount = 0;
	toneMappingDependencyInfo.pMemoryBarriers = nullptr;
	toneMappingDependencyInfo.bufferMemoryBarrierCount = 0;
	toneMappingDependencyInfo.pBufferMemoryBarriers = nullptr;
	toneMappingDependencyInfo.imageMemoryBarrierCount = 1;
	toneMappingDependencyInfo.pImageMemoryBarriers = &toneMappingColorAttachmentToFragmentImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &toneMappingDependencyInfo);

	// FXAA
	m_fxaa.draw(m_renderingCommandBuffers[m_currentFrameInFlight], (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage.handle, (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImageViews[imageIndex] : m_drawImage.view);

	// UI
	if (!m_uiElements.empty()) {
		VkRenderingAttachmentInfo renderingSwapchainAttachmentInfo = {};
		renderingSwapchainAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		renderingSwapchainAttachmentInfo.pNext = nullptr;
		renderingSwapchainAttachmentInfo.imageView = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImageViews[imageIndex] : m_drawImage.view;
		renderingSwapchainAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		renderingSwapchainAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
		renderingSwapchainAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
		renderingSwapchainAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderingSwapchainAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		renderingSwapchainAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

		VkRenderingInfo uiRenderingInfo = {};
		uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		uiRenderingInfo.pNext = nullptr;
		uiRenderingInfo.flags = 0;
		uiRenderingInfo.renderArea = m_scissor;
		uiRenderingInfo.layerCount = 1;
		uiRenderingInfo.viewMask = 0;
		uiRenderingInfo.colorAttachmentCount = 1;
		uiRenderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
		uiRenderingInfo.pDepthAttachment = nullptr;
		uiRenderingInfo.pStencilAttachment = nullptr;
		m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &uiRenderingInfo);

		while (!m_uiElements.empty()) {
			const UIElement uiElement = m_uiElements.front();
			if (uiElement == UIElement::Text) {
				const InternalUIText& uiText = m_uiTexts.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipeline);
				vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipelineLayout, 0, 1, &m_uiTextDescriptorSets[m_currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiTextGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &uiText.bufferOffset);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiTextGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec4), sizeof(Math::vec4) + sizeof(uint32_t), &uiText.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], uiText.charactersCount * 6, 1, 0, 0);

				m_uiTexts.pop();
			}
			else if (uiElement == UIElement::Line) {
				const InternalUILine& uiLine = m_uiLines.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiLineGraphicsPipeline);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiLineGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Math::vec2) + sizeof(Math::vec2), &uiLine.positions);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiLineGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4), &uiLine.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 2, 1, 0, 0);

				m_uiLines.pop();
			}
			else if (uiElement == UIElement::Rectangle) {
				const InternalUIRectangle& uiRectangle = m_uiRectangles.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiRectangleGraphicsPipeline);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiRectangleGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Math::vec2) + sizeof(Math::vec2), &uiRectangle.positions);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiRectangleGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4), &uiRectangle.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 6, 1, 0, 0);

				m_uiRectangles.pop();
			}
			else if (uiElement == UIElement::Image) {
				const InternalUIImage& uiImage = m_uiImages.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiImageGraphicsPipeline);
				vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiImageGraphicsPipelineLayout, 0, 1, &m_uiImageDescriptorSets[m_currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiImageGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 5 * sizeof(Math::vec2), &uiImage.v0);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiImageGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 5 * sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4) + sizeof(uint32_t), &uiImage.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 6, 1, 0, 0);

				m_uiImages.pop();
			}

			m_uiElements.pop();
		}

		m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);
	}

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkImageMemoryBarrier2 colorAttachmentToPresentSrcImageMemoryBarrier = {};
		colorAttachmentToPresentSrcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		colorAttachmentToPresentSrcImageMemoryBarrier.pNext = nullptr;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_NONE;
		colorAttachmentToPresentSrcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentToPresentSrcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		colorAttachmentToPresentSrcImageMemoryBarrier.image = m_swapchainImages[imageIndex];
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.levelCount = 1;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo colorAttachmentToPresentSrcDependencyInfo = {};
		colorAttachmentToPresentSrcDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		colorAttachmentToPresentSrcDependencyInfo.pNext = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.dependencyFlags = 0;
		colorAttachmentToPresentSrcDependencyInfo.memoryBarrierCount = 0;
		colorAttachmentToPresentSrcDependencyInfo.pMemoryBarriers = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.bufferMemoryBarrierCount = 0;
		colorAttachmentToPresentSrcDependencyInfo.pBufferMemoryBarriers = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.imageMemoryBarrierCount = 1;
		colorAttachmentToPresentSrcDependencyInfo.pImageMemoryBarriers = &colorAttachmentToPresentSrcImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentToPresentSrcDependencyInfo);
	}

	// End command buffer recording
	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_renderingCommandBuffers[m_currentFrameInFlight]));

	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_fences[m_currentFrameInFlight]));

	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrameInFlight];
	submitInfo.pWaitDstStageMask = &waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_renderingCommandBuffers[m_currentFrameInFlight];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex];
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &submitInfo, m_fences[m_currentFrameInFlight]));

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;
		VkResult queuePresentResult = vkQueuePresentKHR(m_graphicsComputeQueue, &presentInfo);
		if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR) {
			resize();
		}
		else if (queuePresentResult != VK_SUCCESS) {
			NTSHENGN_MODULE_ERROR("Queue present swapchain image failed.", Result::ModuleError);
		}
	}
	else {
		VkPipelineStageFlags emptyWaitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSubmitInfo emptyWaitSubmitInfo = {};
		emptyWaitSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		emptyWaitSubmitInfo.pNext = nullptr;
		emptyWaitSubmitInfo.waitSemaphoreCount = 1;
		emptyWaitSubmitInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
		emptyWaitSubmitInfo.pWaitDstStageMask = &emptyWaitDstStageMask;
		emptyWaitSubmitInfo.commandBufferCount = 0;
		emptyWaitSubmitInfo.pCommandBuffers = nullptr;
		emptyWaitSubmitInfo.signalSemaphoreCount = 0;
		emptyWaitSubmitInfo.pSignalSemaphores = nullptr;
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &emptyWaitSubmitInfo, VK_NULL_HANDLE));
	}

	m_uiTextBufferOffset = 0;

	m_currentFrameInFlight = (m_currentFrameInFlight + 1) % m_framesInFlight;
}

void NtshEngn::GraphicsModule::destroy() {
	NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsComputeQueue));

	// Destroy sync objects
	for (uint32_t i = 0; i < m_imageCount; i++) {
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
	}
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);

		vkDestroyFence(m_device, m_fences[i], nullptr);

		// Destroy rendering command pools
		vkDestroyCommandPool(m_device, m_renderingCommandPools[i], nullptr);
	}

	// Destroy light buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_lightBuffers[i].destroy(m_allocator);
	}

	// Destroy material buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_materialBuffers[i].destroy(m_allocator);
	}

	// Destroy joint transform buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_jointTransformBuffers[i].handle, m_jointTransformBuffers[i].allocation);
	}

	// Destroy mesh buffer
	vmaDestroyBuffer(m_allocator, m_meshBuffer.handle, m_meshBuffer.allocation);

	// Destroy object buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_objectBuffers[i].destroy(m_allocator);
	}

	// Destroy camera buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_cameraBuffers[i].destroy(m_allocator);
	}

	// Destroy UI resources
	vkDestroyDescriptorPool(m_device, m_uiImageDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_uiImageGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiImageGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_uiImageDescriptorSetLayout, nullptr);

	vkDestroyPipeline(m_device, m_uiRectangleGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiRectangleGraphicsPipelineLayout, nullptr);

	vkDestroyPipeline(m_device, m_uiLineGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiLineGraphicsPipelineLayout, nullptr);

	for (size_t i = 0; i < m_fonts.size(); i++) {
		vkDestroyImageView(m_device, m_fonts[i].imageView, nullptr);
		vmaDestroyImage(m_allocator, m_fonts[i].image, m_fonts[i].imageAllocation);
	}
	vkDestroyDescriptorPool(m_device, m_uiTextDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_uiTextGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiTextGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_uiTextDescriptorSetLayout, nullptr);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiTextBuffers[i].destroy(m_allocator);
	}

	vkDestroySampler(m_device, m_uiLinearSampler, nullptr);
	vkDestroySampler(m_device, m_uiNearestSampler, nullptr);

	// Destroy FXAA
	m_fxaa.destroy();

	// Destroy tone mapping resources
	vkDestroyDescriptorPool(m_device, m_toneMappingDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_toneMappingGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_toneMappingGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_toneMappingDescriptorSetLayout, nullptr);
	vkDestroySampler(m_device, m_toneMappingSampler, nullptr);
	m_toneMappingImage.destroy(m_device, m_allocator);

#if BLOOM_ENABLE == 1
	// Destroy bloom
	m_bloom.destroy();
#endif

	// Destroy particles
	m_particles.destroy();

	// Destroy compositing resources
	vkDestroyDescriptorPool(m_device, m_compositingDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_compositingGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_compositingGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_compositingDescriptorSetLayout, nullptr);
	vkDestroySampler(m_device, m_compositingShadowSampler, nullptr);
	vkDestroySampler(m_device, m_compositingSampler, nullptr);
	m_compositingImage.destroy(m_device, m_allocator);

	// Destroy shadow mapping
	m_shadowMapping.destroy();

	// Destroy SSAO
	m_ssao.destroy();

	// Destroy G-Buffer
	m_gBuffer.destroy();

	// Destroy frustum culling
	m_frustumCulling.destroy();

	// Destroy samplers
	for (const auto& sampler : m_textureSamplers) {
		vkDestroySampler(m_device, sampler.second, nullptr);
	}

	// Destroy textures
	for (size_t i = 0; i < m_textureImages.size(); i++) {
		vkDestroyImageView(m_device, m_textureImageViews[i], nullptr);
		vmaDestroyImage(m_allocator, m_textureImages[i], m_textureImageAllocations[i]);
	}

	// Destroy vertex and index buffers
	m_indexBuffer.destroy(m_allocator);
	m_vertexBuffer.destroy(m_allocator);

	// Destroy initialization fence
	vkDestroyFence(m_device, m_initializationFence, nullptr);

	// Destroy initialization command pool
	vkDestroyCommandPool(m_device, m_initializationCommandPool, nullptr);

	// Destroy swapchain
	if (m_swapchain != VK_NULL_HANDLE) {
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}
	// Or destroy the image
	else {
		m_drawImage.destroy(m_device, m_allocator);
	}

	// Destroy VMA Allocator
	vmaDestroyAllocator(m_allocator);

	// Destroy device
	vkDestroyDevice(m_device, nullptr);

	// Destroy surface
	if (m_surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

#if defined(NTSHENGN_DEBUG)
	// Destroy debug messenger
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

	// Destroy instance
	vkDestroyInstance(m_instance, nullptr);
}

NtshEngn::MeshID NtshEngn::GraphicsModule::load(const Mesh& mesh) {
	if (m_meshAddresses.find(&mesh) != m_meshAddresses.end()) {
		return m_meshAddresses[&mesh];
	}

	std::array<Math::vec3, 2> meshAABB = assetManager->calculateAABB(mesh);
	if (!mesh.skin.joints.empty()) {
		// Scale the AABB to try to cover the animation
		const Math::vec3 aabbCenter = (meshAABB[0] + meshAABB[1]) / 2.0f;
		const Math::vec3 aabbSize = (meshAABB[1] - meshAABB[0]) / 2.0f;
		meshAABB[0] = aabbCenter - (aabbSize * 3.0f);
		meshAABB[1] = aabbCenter + (aabbSize * 3.0f);
	}

	m_meshes.push_back({ static_cast<uint32_t>(mesh.indices.size()), m_currentIndexOffset, m_currentVertexOffset, static_cast<uint32_t>(mesh.skin.joints.size()), meshAABB[0], meshAABB[1] });
	m_meshAddresses[&mesh] = static_cast<MeshID>(m_meshes.size() - 1);

	// Vertex and Index staging buffer
	VkBuffer vertexAndIndexStagingBuffer;
	VmaAllocation vertexAndIndexStagingBufferAllocation;

	VkBufferCreateInfo vertexAndIndexStagingBufferCreateInfo = {};
	vertexAndIndexStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexAndIndexStagingBufferCreateInfo.pNext = nullptr;
	vertexAndIndexStagingBufferCreateInfo.flags = 0;
	vertexAndIndexStagingBufferCreateInfo.size = (mesh.vertices.size() * sizeof(Vertex)) + (mesh.indices.size() * sizeof(uint32_t));
	vertexAndIndexStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vertexAndIndexStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexAndIndexStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexAndIndexStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo vertexAndIndexStagingBufferAllocationCreateInfo = {};
	vertexAndIndexStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	vertexAndIndexStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexStagingBufferCreateInfo, &vertexAndIndexStagingBufferAllocationCreateInfo, &vertexAndIndexStagingBuffer, &vertexAndIndexStagingBufferAllocation, nullptr));

	void* data;

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, vertexAndIndexStagingBufferAllocation, &data));
	memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	memcpy(reinterpret_cast<char*>(data) + (mesh.vertices.size() * sizeof(Vertex)), mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(m_allocator, vertexAndIndexStagingBufferAllocation);

	// Mesh buffer
	VkBuffer meshStagingBuffer;
	VmaAllocation meshStagingBufferAllocation;

	VkBufferCreateInfo meshStagingBufferCreateInfo = {};
	meshStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	meshStagingBufferCreateInfo.pNext = nullptr;
	meshStagingBufferCreateInfo.flags = 0;
	meshStagingBufferCreateInfo.size = sizeof(uint32_t);
	meshStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	meshStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	meshStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	meshStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo meshStagingBufferAllocationCreateInfo = {};
	meshStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	meshStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &meshStagingBufferCreateInfo, &meshStagingBufferAllocationCreateInfo, &meshStagingBuffer, &meshStagingBufferAllocation, nullptr));

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, meshStagingBufferAllocation, &data));
	uint32_t meshSkin = { !mesh.skin.joints.empty() ? static_cast<uint32_t>(1) : static_cast<uint32_t>(0) };
	memcpy(data, &meshSkin, sizeof(uint32_t));
	vmaUnmapMemory(m_allocator, meshStagingBufferAllocation);

	// Copy staging buffers
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo stagingBuffersCopyBeginInfo = {};
	stagingBuffersCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	stagingBuffersCopyBeginInfo.pNext = nullptr;
	stagingBuffersCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	stagingBuffersCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &stagingBuffersCopyBeginInfo));

	VkBufferCopy vertexBufferCopy = {};
	vertexBufferCopy.srcOffset = 0;
	vertexBufferCopy.dstOffset = m_currentVertexOffset * sizeof(Vertex);
	vertexBufferCopy.size = mesh.vertices.size() * sizeof(Vertex);
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_vertexBuffer.handle, 1, &vertexBufferCopy);

	VkBufferCopy indexBufferCopy = {};
	indexBufferCopy.srcOffset = mesh.vertices.size() * sizeof(Vertex);
	indexBufferCopy.dstOffset = m_currentIndexOffset * sizeof(uint32_t);
	indexBufferCopy.size = mesh.indices.size() * sizeof(uint32_t);
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_indexBuffer.handle, 1, &indexBufferCopy);

	VkBufferCopy meshBufferCopy = {};
	meshBufferCopy.srcOffset = 0;
	meshBufferCopy.dstOffset = (m_meshes.size() - 1) * sizeof(uint32_t);
	meshBufferCopy.size = sizeof(uint32_t);
	vkCmdCopyBuffer(m_initializationCommandBuffer, meshStagingBuffer, m_meshBuffer.handle, 1, &meshBufferCopy);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, meshStagingBuffer, meshStagingBufferAllocation);
	vmaDestroyBuffer(m_allocator, vertexAndIndexStagingBuffer, vertexAndIndexStagingBufferAllocation);

	m_currentVertexOffset += static_cast<int32_t>(mesh.vertices.size());
	m_currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
	m_currentJointOffset += m_meshes.back().jointCount;

	return static_cast<MeshID>(m_meshes.size() - 1);
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	if (m_imageAddresses.find(&image) != m_imageAddresses.end()) {
		return m_imageAddresses[&image];
	}

	m_imageAddresses[&image] = static_cast<ImageID>(m_textureImages.size());

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	size_t numComponents = 4;
	size_t sizeComponent = 1;

	if (image.colorSpace == ImageColorSpace::SRGB) {
		switch (image.format) {
		case ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_SRGB;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_SRGB;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_SRGB;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_SFLOAT;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_SFLOAT;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_SFLOAT;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", Result::ModuleError);
		}
	}
	else if (image.colorSpace == ImageColorSpace::Linear) {
		switch (image.format) {
		case ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_UNORM;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_UNORM;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_UNORM;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_UNORM;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_UNORM;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_UNORM;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_UNORM;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", Result::ModuleError);
		}
	}

	// Create texture
	VkImage textureImage;
	VmaAllocation textureImageAllocation;
	VkImageView textureImageView;

	VkImageCreateInfo textureImageCreateInfo = {};
	textureImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureImageCreateInfo.pNext = nullptr;
	textureImageCreateInfo.flags = 0;
	textureImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureImageCreateInfo.format = imageFormat;
	textureImageCreateInfo.extent.width = image.width;
	textureImageCreateInfo.extent.height = image.height;
	textureImageCreateInfo.extent.depth = 1;
	textureImageCreateInfo.mipLevels = findMipLevels(image.width, image.height);
	textureImageCreateInfo.arrayLayers = 1;
	textureImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	textureImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureImageCreateInfo.queueFamilyIndexCount = 1;
	textureImageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo textureImageAllocationCreateInfo = {};
	textureImageAllocationCreateInfo.flags = 0;
	textureImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &textureImageCreateInfo, &textureImageAllocationCreateInfo, &textureImage, &textureImageAllocation, nullptr));

	VkImageViewCreateInfo textureImageViewCreateInfo = {};
	textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureImageViewCreateInfo.pNext = nullptr;
	textureImageViewCreateInfo.flags = 0;
	textureImageViewCreateInfo.image = textureImage;
	textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureImageViewCreateInfo.format = imageFormat;
	textureImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	textureImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	textureImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	textureImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	textureImageViewCreateInfo.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	textureImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &textureImageViewCreateInfo, nullptr, &textureImageView));

	// Create texture staging buffer
	VkBuffer textureStagingBuffer;
	VmaAllocation textureStagingBufferAllocation;

	VkBufferCreateInfo textureStagingBufferCreateInfo = {};
	textureStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	textureStagingBufferCreateInfo.pNext = nullptr;
	textureStagingBufferCreateInfo.flags = 0;
	textureStagingBufferCreateInfo.size = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * numComponents * sizeComponent;
	textureStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	textureStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	textureStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo textureStagingBufferAllocationCreateInfo = {};
	textureStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	textureStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &textureStagingBufferCreateInfo, &textureStagingBufferAllocationCreateInfo, &textureStagingBuffer, &textureStagingBufferAllocation, nullptr));

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, textureStagingBufferAllocation, &data));
	memcpy(data, image.data.data(), static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * numComponents * sizeComponent);
	vmaUnmapMemory(m_allocator, textureStagingBufferAllocation);

	// Copy staging buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo textureCopyBeginInfo = {};
	textureCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	textureCopyBeginInfo.pNext = nullptr;
	textureCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	textureCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &textureCopyBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstImageMemoryBarrier = {};
	undefinedToTransferDstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
	undefinedToTransferDstImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.image = textureImage;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstDependencyInfo = {};
	undefinedToTransferDstDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstDependencyInfo.pNext = nullptr;
	undefinedToTransferDstDependencyInfo.dependencyFlags = 0;
	undefinedToTransferDstDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToTransferDstDependencyInfo);

	VkBufferImageCopy textureBufferCopy = {};
	textureBufferCopy.bufferOffset = 0;
	textureBufferCopy.bufferRowLength = 0;
	textureBufferCopy.bufferImageHeight = 0;
	textureBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureBufferCopy.imageSubresource.mipLevel = 0;
	textureBufferCopy.imageSubresource.baseArrayLayer = 0;
	textureBufferCopy.imageSubresource.layerCount = 1;
	textureBufferCopy.imageOffset.x = 0;
	textureBufferCopy.imageOffset.y = 0;
	textureBufferCopy.imageOffset.z = 0;
	textureBufferCopy.imageExtent.width = image.width;
	textureBufferCopy.imageExtent.height = image.height;
	textureBufferCopy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(m_initializationCommandBuffer, textureStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureBufferCopy);

	VkImageMemoryBarrier2 mipmapGenerationImageMemoryBarrier = {};
	mipmapGenerationImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	mipmapGenerationImageMemoryBarrier.pNext = nullptr;
	mipmapGenerationImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	mipmapGenerationImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	mipmapGenerationImageMemoryBarrier.image = textureImage;
	mipmapGenerationImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	mipmapGenerationImageMemoryBarrier.subresourceRange.levelCount = 1;
	mipmapGenerationImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	mipmapGenerationImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo mipmapGenerationDependencyInfo = {};
	mipmapGenerationDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	mipmapGenerationDependencyInfo.pNext = nullptr;
	mipmapGenerationDependencyInfo.dependencyFlags = 0;
	mipmapGenerationDependencyInfo.memoryBarrierCount = 0;
	mipmapGenerationDependencyInfo.pMemoryBarriers = nullptr;
	mipmapGenerationDependencyInfo.bufferMemoryBarrierCount = 0;
	mipmapGenerationDependencyInfo.pBufferMemoryBarriers = nullptr;
	mipmapGenerationDependencyInfo.imageMemoryBarrierCount = 1;
	mipmapGenerationDependencyInfo.pImageMemoryBarriers = &mipmapGenerationImageMemoryBarrier;

	uint32_t mipWidth = image.width;
	uint32_t mipHeight = image.height;
	for (uint32_t i = 1; i < textureImageCreateInfo.mipLevels; i++) {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;

		m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

		VkImageBlit imageBlit = {};
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcOffsets[0].x = 0;
		imageBlit.srcOffsets[0].y = 0;
		imageBlit.srcOffsets[0].z = 0;
		imageBlit.srcOffsets[1].x = mipWidth;
		imageBlit.srcOffsets[1].y = mipHeight;
		imageBlit.srcOffsets[1].z = 1;
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstOffsets[0].x = 0;
		imageBlit.dstOffsets[0].y = 0;
		imageBlit.dstOffsets[0].z = 0;
		imageBlit.dstOffsets[1].x = mipWidth > 1 ? mipWidth / 2 : 1;
		imageBlit.dstOffsets[1].y = mipHeight > 1 ? mipHeight / 2 : 1;
		imageBlit.dstOffsets[1].z = 1;
		vkCmdBlitImage(m_initializationCommandBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

		mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
		mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
	}

	if (textureImageCreateInfo.mipLevels == 1) {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	}
	else {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
	}
	mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mipmapGenerationImageMemoryBarrier.subresourceRange.baseMipLevel = textureImageCreateInfo.mipLevels - 1;

	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, textureStagingBuffer, textureStagingBufferAllocation);

	m_textureImages.push_back(textureImage);
	m_textureImageAllocations.push_back(textureImageAllocation);
	m_textureImageViews.push_back(textureImageView);
	m_textureSizes.push_back({ static_cast<float>(image.width), static_cast<float>(image.height) });

	// Mark descriptor sets for update
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_gBuffer.descriptorSetNeedsUpdate(i);
		m_shadowMapping.descriptorSetNeedsUpdate(i);
	}

	return static_cast<ImageID>(m_textureImages.size() - 1);
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	if (m_fontAddresses.find(&font) != m_fontAddresses.end()) {
		return m_fontAddresses[&font];
	}

	m_fontAddresses[&font] = static_cast<FontID>(m_fonts.size());

	// Create texture
	VkImage textureImage;
	VmaAllocation textureImageAllocation;
	VkImageView textureImageView;

	VkImageCreateInfo textureImageCreateInfo = {};
	textureImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureImageCreateInfo.pNext = nullptr;
	textureImageCreateInfo.flags = 0;
	textureImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureImageCreateInfo.format = VK_FORMAT_R8_UNORM;
	textureImageCreateInfo.extent.width = font.image->width;
	textureImageCreateInfo.extent.height = font.image->height;
	textureImageCreateInfo.extent.depth = 1;
	textureImageCreateInfo.mipLevels = 1;
	textureImageCreateInfo.arrayLayers = 1;
	textureImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	textureImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureImageCreateInfo.queueFamilyIndexCount = 1;
	textureImageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo textureImageAllocationCreateInfo = {};
	textureImageAllocationCreateInfo.flags = 0;
	textureImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &textureImageCreateInfo, &textureImageAllocationCreateInfo, &textureImage, &textureImageAllocation, nullptr));

	VkImageViewCreateInfo textureImageViewCreateInfo = {};
	textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureImageViewCreateInfo.pNext = nullptr;
	textureImageViewCreateInfo.flags = 0;
	textureImageViewCreateInfo.image = textureImage;
	textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureImageViewCreateInfo.format = VK_FORMAT_R8_UNORM;
	textureImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	textureImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	textureImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	textureImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	textureImageViewCreateInfo.subresourceRange.levelCount = 1;
	textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	textureImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &textureImageViewCreateInfo, nullptr, &textureImageView));

	// Create texture staging buffer
	VkBuffer textureStagingBuffer;
	VmaAllocation textureStagingBufferAllocation;

	VkBufferCreateInfo textureStagingBufferCreateInfo = {};
	textureStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	textureStagingBufferCreateInfo.pNext = nullptr;
	textureStagingBufferCreateInfo.flags = 0;
	textureStagingBufferCreateInfo.size = static_cast<size_t>(font.image->width) * static_cast<size_t>(font.image->height);
	textureStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	textureStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	textureStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo textureStagingBufferAllocationCreateInfo = {};
	textureStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	textureStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &textureStagingBufferCreateInfo, &textureStagingBufferAllocationCreateInfo, &textureStagingBuffer, &textureStagingBufferAllocation, nullptr));

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, textureStagingBufferAllocation, &data));
	memcpy(data, font.image->data.data(), static_cast<size_t>(font.image->width) * static_cast<size_t>(font.image->height));
	vmaUnmapMemory(m_allocator, textureStagingBufferAllocation);

	// Copy staging buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo textureCopyBeginInfo = {};
	textureCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	textureCopyBeginInfo.pNext = nullptr;
	textureCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	textureCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &textureCopyBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstImageMemoryBarrier = {};
	undefinedToTransferDstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	undefinedToTransferDstImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.image = textureImage;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstDependencyInfo = {};
	undefinedToTransferDstDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstDependencyInfo.pNext = nullptr;
	undefinedToTransferDstDependencyInfo.dependencyFlags = 0;
	undefinedToTransferDstDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToTransferDstDependencyInfo);

	VkBufferImageCopy textureBufferCopy = {};
	textureBufferCopy.bufferOffset = 0;
	textureBufferCopy.bufferRowLength = 0;
	textureBufferCopy.bufferImageHeight = 0;
	textureBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureBufferCopy.imageSubresource.mipLevel = 0;
	textureBufferCopy.imageSubresource.baseArrayLayer = 0;
	textureBufferCopy.imageSubresource.layerCount = 1;
	textureBufferCopy.imageOffset.x = 0;
	textureBufferCopy.imageOffset.y = 0;
	textureBufferCopy.imageOffset.z = 0;
	textureBufferCopy.imageExtent.width = font.image->width;
	textureBufferCopy.imageExtent.height = font.image->height;
	textureBufferCopy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(m_initializationCommandBuffer, textureStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureBufferCopy);

	VkImageMemoryBarrier2 transferDstToShaderReadOnlyImageMemoryBarrier = {};
	transferDstToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	transferDstToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.image = textureImage;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo transferDstToShaderReadOnlyDependencyInfo = {};
	transferDstToShaderReadOnlyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	transferDstToShaderReadOnlyDependencyInfo.pNext = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.dependencyFlags = 0;
	transferDstToShaderReadOnlyDependencyInfo.memoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.bufferMemoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pBufferMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.imageMemoryBarrierCount = 1;
	transferDstToShaderReadOnlyDependencyInfo.pImageMemoryBarriers = &transferDstToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &transferDstToShaderReadOnlyDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, textureStagingBuffer, textureStagingBufferAllocation);

	m_fonts.push_back({ textureImage, textureImageAllocation, textureImageView, font.imageSamplerFilter, font.glyphs });

	// Mark descriptor sets for update
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiTextDescriptorSetsNeedUpdate[i] = true;
	}

	return static_cast<FontID>(m_fonts.size() - 1);
}

void NtshEngn::GraphicsModule::setBackgroundColor(const Math::vec4& backgroundColor) {
	m_backgroundColor = backgroundColor;
}

void NtshEngn::GraphicsModule::playAnimation(Entity entity, uint32_t animationIndex) {
	if (!ecs->hasComponent<Renderable>(entity)) {
		NTSHENGN_MODULE_WARNING("Entity " + (ecs->entityHasName(entity) ? ("\"" + ecs->getEntityName(entity) + "\"") : std::to_string(entity)) + " does not have a Renderable component, when trying to play animation " + std::to_string(animationIndex) + ".");
		return;
	}

	const Renderable& renderable = ecs->getComponent<Renderable>(entity);

	if (animationIndex >= renderable.mesh->animations.size()) {
		NTSHENGN_MODULE_WARNING("Animation " + std::to_string(animationIndex) + " does not exist for Entity " + (ecs->entityHasName(entity) ? ("\"" + ecs->getEntityName(entity) + "\"") : std::to_string(entity)) + "\'s mesh.");
		return;
	}

	if (m_playingAnimations.find(&m_objects[entity]) != m_playingAnimations.end()) {
		if (m_playingAnimations[&m_objects[entity]].animationIndex == animationIndex) {
			m_playingAnimations[&m_objects[entity]].isPlaying = true;
			return;
		}
	}

	PlayingAnimation playingAnimation;
	playingAnimation.animationIndex = animationIndex;
	m_playingAnimations[&m_objects[entity]] = playingAnimation;
}

void NtshEngn::GraphicsModule::pauseAnimation(Entity entity) {
	if (m_playingAnimations.find(&m_objects[entity]) != m_playingAnimations.end()) {
		if (m_playingAnimations[&m_objects[entity]].isPlaying) {
			m_playingAnimations[&m_objects[entity]].isPlaying = false;
		}
	}
}

void NtshEngn::GraphicsModule::stopAnimation(Entity entity) {
	if (m_playingAnimations.find(&m_objects[entity]) != m_playingAnimations.end()) {
		m_playingAnimations.erase(&m_objects[entity]);
	}
}

bool NtshEngn::GraphicsModule::isAnimationPlaying(Entity entity, uint32_t animationIndex) {
	if (m_playingAnimations.find(&m_objects[entity]) != m_playingAnimations.end()) {
		if (m_playingAnimations[&m_objects[entity]].animationIndex == animationIndex) {
			return m_playingAnimations[&m_objects[entity]].isPlaying;
		}
	}

	return false;
}

void NtshEngn::GraphicsModule::emitParticles(const ParticleEmitter& particleEmitter) {
	if (particleEmitter.number == 0) {
		return;
	}

	m_particles.emitParticles(particleEmitter, m_currentFrameInFlight);
}

void NtshEngn::GraphicsModule::drawUIText(FontID fontID, const std::string& text, const Math::vec2& position, const Math::vec4& color) {
	NTSHENGN_ASSERT(fontID < m_fonts.size());

	float positionAdvance = 0.0f;
	std::vector<Math::vec2> positionsAndUVs;
	for (const char& c : text) {
		const Math::vec2 topLeft = position + m_fonts[fontID].glyphs[c].positionTopLeft + Math::vec2(positionAdvance, 0.0f);
		const Math::vec2 bottomRight = position + m_fonts[fontID].glyphs[c].positionBottomRight + Math::vec2(positionAdvance, 0.0f);
		positionsAndUVs.push_back(Math::vec2((topLeft.x / m_viewport.width) * 2.0f - 1.0f, (topLeft.y / m_viewport.height) * 2.0f - 1.0f));
		positionsAndUVs.push_back(Math::vec2((bottomRight.x / m_viewport.width) * 2.0f - 1.0f, (bottomRight.y / m_viewport.height) * 2.0f - 1.0f));
		positionsAndUVs.push_back(m_fonts[fontID].glyphs[c].uvTopLeft);
		positionsAndUVs.push_back(m_fonts[fontID].glyphs[c].uvBottomRight);

		positionAdvance += m_fonts[fontID].glyphs[c].positionAdvance;
	}

	size_t offset = m_uiTextBufferOffset * sizeof(Math::vec2) * 4;
	memcpy(reinterpret_cast<uint8_t*>(m_uiTextBuffers[m_currentFrameInFlight].address) + offset, positionsAndUVs.data(), sizeof(Math::vec2) * 4 * text.size());

	InternalUIText uiText;
	uiText.fontID = fontID;
	uiText.color = color;
	uiText.charactersCount = static_cast<uint32_t>(text.size());
	uiText.bufferOffset = m_uiTextBufferOffset;
	m_uiTexts.push(uiText);

	m_uiTextBufferOffset += static_cast<uint32_t>(uiText.charactersCount);

	m_uiElements.push(UIElement::Text);
}

void NtshEngn::GraphicsModule::drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color) {
	InternalUILine uiLine;
	uiLine.positions = Math::vec4((start.x / m_viewport.width) * 2.0f - 1.0f, (start.y / m_viewport.height) * 2.0f - 1.0f, (end.x / m_viewport.width) * 2.0f - 1.0f, (end.y / m_viewport.height) * 2.0f - 1.0f);
	uiLine.color = color;
	m_uiLines.push(uiLine);

	m_uiElements.push(UIElement::Line);
}

void NtshEngn::GraphicsModule::drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color) {
	InternalUIRectangle uiRectangle;
	const Math::vec2 topLeft = Math::vec2((position.x / m_viewport.width) * 2.0f - 1.0f, (position.y / m_viewport.height) * 2.0f - 1.0f);
	const Math::vec2 bottomRight = Math::vec2(((position.x + size.x) / m_viewport.width) * 2.0f - 1.0f, ((position.y + size.y) / m_viewport.height) * 2.0f - 1.0f);
	uiRectangle.positions = Math::vec4(topLeft, bottomRight);
	uiRectangle.color = color;
	m_uiRectangles.push(uiRectangle);

	m_uiElements.push(UIElement::Rectangle);
}

void NtshEngn::GraphicsModule::drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color) {
	NTSHENGN_ASSERT(imageID < m_textureImages.size());

	const Math::mat3 transform = Math::translate(position) * Math::rotate(rotation) * Math::scale(Math::vec2(std::abs(scale.x), std::abs(scale.y)));
	const float x = (m_textureSizes[imageID].x) / 2.0f;
	const float y = (m_textureSizes[imageID].y) / 2.0f;

	InternalUIImage uiImage;
	bool foundUITexture = false;
	for (size_t i = 0; i < m_uiTextures.size(); i++) {
		if ((m_uiTextures[i].first == imageID) &&
			(m_uiTextures[i].second == imageSamplerFilter)) {
			uiImage.uiTextureIndex = static_cast<uint32_t>(i);

			foundUITexture = true;
			break;
		}
	}
	if (!foundUITexture) {
		m_uiTextures.push_back({ imageID, imageSamplerFilter });

		uiImage.uiTextureIndex = static_cast<uint32_t>(m_uiTextures.size() - 1);

		for (uint32_t i = 0; i < m_framesInFlight; i++) {
			m_uiImageDescriptorSetsNeedUpdate[i] = true;
		}
	}

	const Math::vec2 v0 = Math::vec2(transform * Math::vec3(x, -y, 1.0f));
	const Math::vec2 v1 = Math::vec2(transform * Math::vec3(-x, -y, 1.0f));
	const Math::vec2 v2 = Math::vec2(transform * Math::vec3(-x, y, 1.0f));
	const Math::vec2 v3 = Math::vec2(transform * Math::vec3(x, y, 1.0f));

	uiImage.v0 = Math::vec2((v0.x / m_viewport.width) * 2.0f - 1.0f, (v0.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v1 = Math::vec2((v1.x / m_viewport.width) * 2.0f - 1.0f, (v1.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v2 = Math::vec2((v2.x / m_viewport.width) * 2.0f - 1.0f, (v2.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v3 = Math::vec2((v3.x / m_viewport.width) * 2.0f - 1.0f, (v3.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.reverseUV.x = (scale.x >= 0.0f) ? 0.0f : 1.0f;
	uiImage.reverseUV.y = (scale.y >= 0.0f) ? 0.0f : 1.0f;
	uiImage.color = color;
	m_uiImages.push(uiImage);

	m_uiElements.push(UIElement::Image);
}

const NtshEngn::ComponentMask NtshEngn::GraphicsModule::getComponentMask() const {
	ComponentMask componentMask;
	componentMask.set(ecs->getComponentID<Renderable>());
	componentMask.set(ecs->getComponentID<Camera>());
	componentMask.set(ecs->getComponentID<Light>());

	return componentMask;
}

void NtshEngn::GraphicsModule::onEntityComponentAdded(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Renderable>()) {
		InternalObject object;
		object.index = attributeObjectIndex();
		m_objects[entity] = object;

		m_lastKnownMaterial[entity] = Material();
		loadRenderableForEntity(entity);
	}
	else if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == NTSHENGN_ENTITY_UNKNOWN) {
			m_mainCamera = entity;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		bool compositingShadowDescriptorSetsNeedUpdate = false;
		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.insert(entity);
			m_shadowMapping.createDirectionalLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Point:
			m_lights.pointLights.insert(entity);
			m_shadowMapping.createPointLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Spot:
			m_lights.spotLights.insert(entity);
			m_shadowMapping.createSpotLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Ambient:
			m_lights.ambientLights.insert(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.insert(entity);
			m_shadowMapping.createDirectionalLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;
		}

		if (compositingShadowDescriptorSetsNeedUpdate) {
			for (uint32_t i = 0; i < m_framesInFlight; i++) {
				m_compositingDescriptorSetsNeedShadowUpdate[i] = true;
			}
		}
	}
}

void NtshEngn::GraphicsModule::onEntityComponentRemoved(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Renderable>()) {
		const InternalObject& object = m_objects[entity];

		retrieveObjectIndex(object.index);

		if (m_meshes[object.meshID].jointCount > 0) {
			m_freeJointTransformOffsets.freeBlock(static_cast<size_t>(object.jointTransformOffset), static_cast<size_t>(m_meshes[object.meshID].jointCount));
		}

		m_lastKnownMaterial.erase(entity);

		m_objects.erase(entity);
	}
	else if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == entity) {
			m_mainCamera = NTSHENGN_ENTITY_UNKNOWN;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		bool compositingShadowDescriptorSetsNeedUpdate = false;
		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.erase(entity);
			m_shadowMapping.destroyDirectionalLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Point:
			m_lights.pointLights.erase(entity);
			m_shadowMapping.destroyPointLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Spot:
			m_lights.spotLights.erase(entity);
			m_shadowMapping.destroySpotLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;

		case LightType::Ambient:
			m_lights.ambientLights.erase(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.erase(entity);
			m_shadowMapping.destroyDirectionalLightShadowMap(entity);
			compositingShadowDescriptorSetsNeedUpdate = true;
			break;
		}

		if (compositingShadowDescriptorSetsNeedUpdate) {
			for (uint32_t i = 0; i < m_framesInFlight; i++) {
				m_compositingDescriptorSetsNeedShadowUpdate[i] = true;
			}
		}
	}
}

VkSurfaceCapabilitiesKHR NtshEngn::GraphicsModule::getSurfaceCapabilities() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	VkSurfaceCapabilities2KHR surfaceCapabilities;
	surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	surfaceCapabilities.pNext = nullptr;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(m_physicalDevice, &surfaceInfo, &surfaceCapabilities));

	return surfaceCapabilities.surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> NtshEngn::GraphicsModule::getSurfaceFormats() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	uint32_t surfaceFormatsCount;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, nullptr));
	std::vector<VkSurfaceFormat2KHR> surfaceFormats2(surfaceFormatsCount);
	for (VkSurfaceFormat2KHR& surfaceFormat2 : surfaceFormats2) {
		surfaceFormat2.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		surfaceFormat2.pNext = nullptr;
	}
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, surfaceFormats2.data()));

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	for (const VkSurfaceFormat2KHR surfaceFormat2 : surfaceFormats2) {
		surfaceFormats.push_back(surfaceFormat2.surfaceFormat);
	}

	return surfaceFormats;
}

std::vector<VkPresentModeKHR> NtshEngn::GraphicsModule::getSurfacePresentModes() {
	uint32_t presentModesCount;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModesCount);
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, presentModes.data()));

	return presentModes;
}

VkPhysicalDeviceMemoryProperties NtshEngn::GraphicsModule::getMemoryProperties() {
	VkPhysicalDeviceMemoryProperties2 memoryProperties = {};
	memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	memoryProperties.pNext = nullptr;
	vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &memoryProperties);

	return memoryProperties.memoryProperties;
}

uint32_t NtshEngn::GraphicsModule::findMipLevels(uint32_t width, uint32_t height) {
	return static_cast<uint32_t>(std::floor(std::log2(std::min(width, height)) + 1));
}

void NtshEngn::GraphicsModule::createSwapchain(VkSwapchainKHR oldSwapchain) {
	VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities();
	uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) {
		minImageCount = surfaceCapabilities.maxImageCount;
	}

	std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats();
	m_drawImageFormat = surfaceFormats[0].format;
	VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
	for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			m_drawImageFormat = surfaceFormat.format;
			swapchainColorSpace = surfaceFormat.colorSpace;
			break;
		}
	}

	std::vector<VkPresentModeKHR> presentModes = getSurfacePresentModes();
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const VkPresentModeKHR& presentMode : presentModes) {
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			swapchainPresentMode = presentMode;
			break;
		}
		else if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			swapchainPresentMode = presentMode;
		}
	}

	VkExtent2D swapchainExtent = {};
	swapchainExtent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
	swapchainExtent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));

	m_viewport.x = 0.0f;
	m_viewport.y = 0.0f;
	m_viewport.width = static_cast<float>(swapchainExtent.width);
	m_viewport.height = static_cast<float>(swapchainExtent.height);
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;

	m_scissor.offset.x = 0;
	m_scissor.offset.y = 0;
	m_scissor.extent.width = swapchainExtent.width;
	m_scissor.extent.height = swapchainExtent.height;

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = m_surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = m_drawImageFormat;
	swapchainCreateInfo.imageColorSpace = swapchainColorSpace;
	swapchainCreateInfo.imageExtent = swapchainExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = swapchainPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = oldSwapchain;
	NTSHENGN_VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

	if (oldSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, oldSwapchain, nullptr);
	}

	NTSHENGN_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr));
	m_swapchainImages.resize(m_imageCount);
	NTSHENGN_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data()));

	// Create the swapchain image views
	m_swapchainImageViews.resize(m_imageCount);
	for (uint32_t i = 0; i < m_imageCount; i++) {
		VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
		swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		swapchainImageViewCreateInfo.pNext = nullptr;
		swapchainImageViewCreateInfo.flags = 0;
		swapchainImageViewCreateInfo.image = m_swapchainImages[i];
		swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		swapchainImageViewCreateInfo.format = m_drawImageFormat;
		swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
		swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &swapchainImageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
	}
}

void NtshEngn::GraphicsModule::createVertexAndIndexBuffers() {
	VkBufferCreateInfo vertexAndIndexBufferCreateInfo = {};
	vertexAndIndexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexAndIndexBufferCreateInfo.pNext = nullptr;
	vertexAndIndexBufferCreateInfo.flags = 0;
	vertexAndIndexBufferCreateInfo.size = 267108864;
	vertexAndIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vertexAndIndexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexAndIndexBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexAndIndexBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationCreateInfo vertexAndIndexBufferAllocationCreateInfo = {};
	vertexAndIndexBufferAllocationCreateInfo.flags = 0;
	vertexAndIndexBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_vertexBuffer.handle, &m_vertexBuffer.allocation, nullptr));

	vertexAndIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_indexBuffer.handle, &m_indexBuffer.allocation, nullptr));
}

void NtshEngn::GraphicsModule::createCompositingResources() {
	createCompositingImage();

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 1;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferPositionDescriptorSetLayoutBinding = {};
	gBufferPositionDescriptorSetLayoutBinding.binding = 2;
	gBufferPositionDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferPositionDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferPositionDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferPositionDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferNormalDescriptorSetLayoutBinding = {};
	gBufferNormalDescriptorSetLayoutBinding.binding = 3;
	gBufferNormalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferNormalDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferNormalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferNormalDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferDiffuseDescriptorSetLayoutBinding = {};
	gBufferDiffuseDescriptorSetLayoutBinding.binding = 4;
	gBufferDiffuseDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferDiffuseDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferDiffuseDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferDiffuseDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferMaterialDescriptorSetLayoutBinding = {};
	gBufferMaterialDescriptorSetLayoutBinding.binding = 5;
	gBufferMaterialDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferMaterialDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferMaterialDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferMaterialDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferEmissiveDescriptorSetLayoutBinding = {};
	gBufferEmissiveDescriptorSetLayoutBinding.binding = 6;
	gBufferEmissiveDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferEmissiveDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferEmissiveDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferEmissiveDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding ssaoDescriptorSetLayoutBinding = {};
	ssaoDescriptorSetLayoutBinding.binding = 7;
	ssaoDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorSetLayoutBinding.descriptorCount = 1;
	ssaoDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ssaoDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cascadeSceneDescriptorSetLayoutBinding = {};
	cascadeSceneDescriptorSetLayoutBinding.binding = 8;
	cascadeSceneDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cascadeSceneDescriptorSetLayoutBinding.descriptorCount = 1;
	cascadeSceneDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cascadeSceneDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowMapsDescriptorSetLayoutBinding = {};
	shadowMapsDescriptorSetLayoutBinding.binding = 9;
	shadowMapsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorSetLayoutBinding.descriptorCount = 131072;
	shadowMapsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowMapsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 10> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 10> compositingDescriptorSetLayoutBindings = { cameraDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, gBufferPositionDescriptorSetLayoutBinding, gBufferNormalDescriptorSetLayoutBinding, gBufferDiffuseDescriptorSetLayoutBinding, gBufferMaterialDescriptorSetLayoutBinding, gBufferEmissiveDescriptorSetLayoutBinding, ssaoDescriptorSetLayoutBinding, cascadeSceneDescriptorSetLayoutBinding, shadowMapsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(compositingDescriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = compositingDescriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_compositingDescriptorSetLayout));

	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_compositingImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(location = 0) out vec2 outUv;

		void main() {
			outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
			gl_Position = vec4(outUv * 2.0 + -1.0, 0.0, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		#define SHADOW_MAPPING_CASCADE_COUNT )GLSL";
	fragmentShaderCode += std::to_string(SHADOW_MAPPING_CASCADE_COUNT);
	fragmentShaderCode += R"GLSL(
		#define M_PI 3.1415926535897932384626433832795

		const mat4 shadowOffset = mat4(
			0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.5, 0.5, 0.0, 1.0
		);

		// BRDF
		float distribution(float NdotH, float roughness) {
			const float a = roughness * roughness;
			const float aSquare = a * a;
			const float NdotHSquare = NdotH * NdotH;
			const float denom = NdotHSquare * (aSquare - 1.0) + 1.0;

			return aSquare / (M_PI * denom * denom);
		}

		vec3 fresnel(float cosTheta, vec3 f0) {
			return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
		}

		float g(float NdotV, float roughness) {
			const float r = roughness + 1.0;
			const float k = (r * r) / 8.0;
			const float denom = NdotV * (1.0 - k) + k;

			return NdotV / denom;
		}

		float smith(float LdotN, float VdotN, float roughness) {
			const float gv = g(VdotN, roughness);
			const float gl = g(LdotN, roughness);

			return gv * gl;
		}

		vec3 diffuseFresnelCorrection(vec3 ior) {
			const vec3 iorSquare = ior * ior;
			const bvec3 TIR = lessThan(ior, vec3(1.0));
			const vec3 invDenum = mix(vec3(1.0), vec3(1.0) / (iorSquare * iorSquare * (vec3(554.33) * 380.7 * ior)), TIR);
			vec3 num = ior * mix(vec3(0.1921156102251088), ior * 298.25 - 261.38 * iorSquare + 138.43, TIR);
			num += mix(vec3(0.8078843897748912), vec3(-1.07), TIR);

			return num * invDenum;
		}

		vec3 brdf(float LdotH, float NdotH, float VdotH, float LdotN, float VdotN, vec3 diffuse, float metalness, float roughness) {
			const float d = distribution(NdotH, roughness);
			const vec3 f = fresnel(LdotH, mix(vec3(0.04), diffuse, metalness));
			const vec3 fT = fresnel(LdotN, mix(vec3(0.04), diffuse, metalness));
			const vec3 fTIR = fresnel(VdotN, mix(vec3(0.04), diffuse, metalness));
			const float g = smith(LdotN, VdotN, roughness);
			const vec3 dfc = diffuseFresnelCorrection(vec3(1.05));

			const vec3 lambertian = diffuse / M_PI;

			return (d * f * g) / max(4.0 * LdotN * VdotN, 0.001) + ((vec3(1.0) - fT) * (vec3(1.0 - fTIR)) * lambertian) * dfc;
		}

		vec3 shade(vec3 n, vec3 v, vec3 l, vec3 lc, vec3 diffuse, float metalness, float roughness) {
			const vec3 h = normalize(v + l);

			const float LdotH = max(dot(l, h), 0.0);
			const float NdotH = max(dot(n, h), 0.0);
			const float VdotH = max(dot(v, h), 0.0);
			const float LdotN = max(dot(l, n), 0.0);
			const float VdotN = max(dot(v, n), 0.0);

			const vec3 brdf = brdf(LdotH, NdotH, VdotH, LdotN, VdotN, diffuse, metalness, roughness);
	
			return lc * brdf * LdotN;
		}

		struct LightInfo {
			vec3 position;
			vec3 direction;
			vec3 color;
			float intensity;
			vec2 cutoffs;
		};

		struct ShadowInfo {
			mat4 viewProj;
			float splitDepth;
		};

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(set = 0, binding = 1) restrict readonly buffer Lights {
			uvec4 count;
			LightInfo info[];
		} lights;

		layout(set = 0, binding = 2) uniform sampler2D gBufferPositionSampler;
		layout(set = 0, binding = 3) uniform sampler2D gBufferNormalSampler;
		layout(set = 0, binding = 4) uniform sampler2D gBufferDiffuseSampler;
		layout(set = 0, binding = 5) uniform sampler2D gBufferMaterialSampler;
		layout(set = 0, binding = 6) uniform sampler2D gBufferEmissiveSampler;

		layout(set = 0, binding = 7) uniform sampler2D ssaoSampler;

		layout(set = 0, binding = 8) restrict readonly buffer Shadows {
			ShadowInfo info[];
		} shadows;

		layout(set = 0, binding = 9) uniform sampler2DArray shadowMaps[];
		layout(set = 0, binding = 9) uniform samplerCube shadowCubeMaps[];

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		// Shadows
		float shadowValue(uint lightIndex, uint layerIndex, vec4 shadowCoord, float bias) {
			float shadow = 0.0;
			if ((shadowCoord.z < -1.0) || (shadowCoord.z > 1.0)) {
				return 1.0;
			}

			const vec2 texelSize = 0.75 * (1.0 / vec2(textureSize(shadowMaps[nonuniformEXT(lightIndex)], 0).xy));
			for (int x = -1; x <= 1; x++) {
				for (int y = -1; y <= 1; y++) {
					const float depth = texture(shadowMaps[nonuniformEXT(lightIndex)], vec3(shadowCoord.xy + (vec2(x, y) * texelSize), layerIndex)).r;
					if (depth >= (shadowCoord.z - bias)) {
						shadow += 1.0;
					}
				}
			}

			return shadow / 9.0;
		}

		float shadowCubeValue(uint lightIndex, vec3 direction, float bias) {
			const float lengthDirection = length(direction);
			const float depth = texture(shadowCubeMaps[nonuniformEXT(lightIndex)], direction).r;
			if ((depth * 50.0) >= (lengthDirection - bias)) {
				return 1.0;
			}

			return 0.0;
		}

		void main() {
			const vec4 diffuseSample = texture(gBufferDiffuseSampler, uv);
			if (diffuseSample.a == 0.0f) {
				discard;
			}
			const vec3 positionSample = texture(gBufferPositionSampler, uv).xyz;
			const vec3 normalSample = texture(gBufferNormalSampler, uv).xyz;
			const vec3 materialSample = texture(gBufferMaterialSampler, uv).xyz;
			const float metalnessSample = materialSample.b;
			const float roughnessSample = materialSample.g;
			const float occlusionSample = materialSample.r;
			const vec3 emissiveSample = texture(gBufferEmissiveSampler, uv).rgb;

			const float ssaoSample = texture(ssaoSampler, uv).r;

			const vec3 position = positionSample;
			const vec3 viewPosition = vec3(camera.view * vec4(position, 1.0));
			const vec3 n = normalSample;
			const vec3 d = vec3(diffuseSample);
			const vec3 v = normalize(camera.position - position);

			vec3 color = vec3(0.0);

			uint lightIndex = 0;
			// Directional Lights
			for (uint i = 0; i < lights.count.x; i++) {
				const vec3 l = -lights.info[lightIndex].direction;

				uint cascadeIndex = 0;
				for (uint j = 0; j < SHADOW_MAPPING_CASCADE_COUNT - 1; j++) {
					if (viewPosition.z < shadows.info[i * SHADOW_MAPPING_CASCADE_COUNT + j].splitDepth) {
						cascadeIndex = j + 1;
					}
				}

				const vec4 shadowCoord = (shadowOffset * shadows.info[(i * SHADOW_MAPPING_CASCADE_COUNT) + cascadeIndex].viewProj) * vec4(position, 1.0);

				color += shade(n, v, l, lights.info[lightIndex].color * lights.info[lightIndex].intensity, d, metalnessSample, roughnessSample) * shadowValue(i, cascadeIndex, shadowCoord / shadowCoord.w, 0.005);

				lightIndex++;
			}
			// Point Lights
			for (uint i = 0; i < lights.count.y; i++) {
				const vec3 l = normalize(lights.info[lightIndex].position - position);

				const float distance = length(lights.info[lightIndex].position - position);
				const float attenuation = 1.0 / (distance * distance);
				const vec3 radiance = (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * attenuation;

				color += shade(n, v, l, radiance, d, metalnessSample, roughnessSample) * shadowCubeValue(lightIndex, position - lights.info[lightIndex].position, 0.05);

				lightIndex++;
			}
			// Spot Lights
			for (uint i = 0; i < lights.count.z; i++) {
				const vec3 l = normalize(lights.info[lightIndex].position - position);
				const float theta = dot(l, -lights.info[lightIndex].direction);
				const float epsilon = cos(lights.info[lightIndex].cutoffs.y) - cos(lights.info[lightIndex].cutoffs.x);
				float intensity = clamp((theta - cos(lights.info[lightIndex].cutoffs.x)) / epsilon, 0.0, 1.0);
				intensity = 1.0 - intensity;

				const vec4 shadowCoord = (shadowOffset * shadows.info[(lights.count.x * SHADOW_MAPPING_CASCADE_COUNT) + (lights.count.y * 6) + i].viewProj) * vec4(position, 1.0);

				color += shade(n, v, l, (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * intensity, d * intensity, metalnessSample, roughnessSample) * shadowValue(lightIndex, 0, shadowCoord / shadowCoord.w, 0.00005);

				lightIndex++;
			}
			// Ambient Lights
			for (uint i = 0; i < lights.count.w; i++) {
				color += (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * d;

				lightIndex++;
			}

			color *= occlusionSample;
			color *= ssaoSample;
			color += emissiveSample;

			outColor = vec4(color, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_compositingDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_compositingGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_compositingGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_compositingGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_compositingSampler));

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_compositingShadowSampler));

	// Create descriptor pool
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize gBufferImagesDescriptorPoolSize = {};
	gBufferImagesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferImagesDescriptorPoolSize.descriptorCount = 5 * m_framesInFlight;

	VkDescriptorPoolSize ssaoDescriptorPoolSize = {};
	ssaoDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize cascadeSceneDescriptorPoolSize = {};
	cascadeSceneDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cascadeSceneDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize shadowMapsDescriptorPoolSize = {};
	shadowMapsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 6> descriptorPoolSizes = { cameraDescriptorPoolSize, lightsDescriptorPoolSize, gBufferImagesDescriptorPoolSize, ssaoDescriptorPoolSize, cascadeSceneDescriptorPoolSize, shadowMapsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_compositingDescriptorPool));

	// Allocate descriptor sets
	m_compositingDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_compositingDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_compositingDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_compositingDescriptorSets[i]));
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = m_cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(Math::mat4) * 2 + sizeof(Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 0;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorBufferInfo lightsDescriptorBufferInfo;
		lightsDescriptorBufferInfo.buffer = m_lightBuffers[i].handle;
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 1;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorBufferInfo shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorBufferInfo.buffer = m_shadowMapping.getShadowSceneBuffer(i).handle;
		shadowSceneDescriptorBufferInfo.offset = 0;
		shadowSceneDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet shadowSceneDescriptorWriteDescriptorSet = {};
		shadowSceneDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowSceneDescriptorWriteDescriptorSet.pNext = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		shadowSceneDescriptorWriteDescriptorSet.dstBinding = 8;
		shadowSceneDescriptorWriteDescriptorSet.dstArrayElement = 0;
		shadowSceneDescriptorWriteDescriptorSet.descriptorCount = 1;
		shadowSceneDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowSceneDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.pBufferInfo = &shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 3> writeDescriptorSets = { cameraDescriptorWriteDescriptorSet, lightsDescriptorWriteDescriptorSet, shadowSceneDescriptorWriteDescriptorSet };

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	updateCompositingDescriptorSets();

	m_compositingDescriptorSetsNeedShadowUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_compositingDescriptorSetsNeedShadowUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::createCompositingImage() {
	// Create image
	VkExtent3D imageExtent;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		imageExtent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		imageExtent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
	}
	else {
		imageExtent.width = 1280;
		imageExtent.height = 720;
	}
	imageExtent.depth = 1;

	m_compositingImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	VkImageCreateInfo compositingImageCreateInfo = {};
	compositingImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	compositingImageCreateInfo.pNext = nullptr;
	compositingImageCreateInfo.flags = 0;
	compositingImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	compositingImageCreateInfo.format = m_compositingImageFormat;
	compositingImageCreateInfo.extent.width = imageExtent.width;
	compositingImageCreateInfo.extent.height = imageExtent.height;
	compositingImageCreateInfo.extent.depth = 1;
	compositingImageCreateInfo.mipLevels = 1;
	compositingImageCreateInfo.arrayLayers = 1;
	compositingImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	compositingImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	compositingImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	compositingImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	compositingImageCreateInfo.queueFamilyIndexCount = 1;
	compositingImageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	compositingImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo compositingImageAllocationCreateInfo = {};
	compositingImageAllocationCreateInfo.flags = 0;
	compositingImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &compositingImageCreateInfo, &compositingImageAllocationCreateInfo, &m_compositingImage.handle, &m_compositingImage.allocation, nullptr));

	VkImageViewCreateInfo compositingImageViewCreateInfo = {};
	compositingImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	compositingImageViewCreateInfo.pNext = nullptr;
	compositingImageViewCreateInfo.flags = 0;
	compositingImageViewCreateInfo.image = m_compositingImage.handle;
	compositingImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	compositingImageViewCreateInfo.format = m_compositingImageFormat;
	compositingImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	compositingImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	compositingImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	compositingImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	compositingImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	compositingImageViewCreateInfo.subresourceRange.levelCount = 1;
	compositingImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	compositingImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &compositingImageViewCreateInfo, nullptr, &m_compositingImage.view));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkCommandPool commandPool;

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 compositingImageMemoryBarrier = {};
	compositingImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	compositingImageMemoryBarrier.pNext = nullptr;
	compositingImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	compositingImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	compositingImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositingImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	compositingImageMemoryBarrier.image = m_compositingImage.handle;
	compositingImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	compositingImageMemoryBarrier.subresourceRange.levelCount = 1;
	compositingImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	compositingImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;
	dependencyInfo.dependencyFlags = 0;
	dependencyInfo.memoryBarrierCount = 0;
	dependencyInfo.pMemoryBarriers = nullptr;
	dependencyInfo.bufferMemoryBarrierCount = 0;
	dependencyInfo.pBufferMemoryBarriers = nullptr;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &compositingImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &dependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &submitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vkDestroyCommandPool(m_device, commandPool, nullptr);
}

void NtshEngn::GraphicsModule::updateCompositingDescriptorSets() {
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorImageInfo gBufferPositionImageDescriptorImageInfo;
		gBufferPositionImageDescriptorImageInfo.sampler = m_compositingSampler;
		gBufferPositionImageDescriptorImageInfo.imageView = m_gBuffer.getPosition().view;
		gBufferPositionImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferPositionImageDescriptorWriteDescriptorSet = {};
		gBufferPositionImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferPositionImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferPositionImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		gBufferPositionImageDescriptorWriteDescriptorSet.dstBinding = 2;
		gBufferPositionImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferPositionImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferPositionImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferPositionImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferPositionImageDescriptorImageInfo;
		gBufferPositionImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferPositionImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferNormalImageDescriptorImageInfo;
		gBufferNormalImageDescriptorImageInfo.sampler = m_compositingSampler;
		gBufferNormalImageDescriptorImageInfo.imageView = m_gBuffer.getNormal().view;
		gBufferNormalImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferNormalImageDescriptorWriteDescriptorSet = {};
		gBufferNormalImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferNormalImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferNormalImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		gBufferNormalImageDescriptorWriteDescriptorSet.dstBinding = 3;
		gBufferNormalImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferNormalImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferNormalImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferNormalImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferNormalImageDescriptorImageInfo;
		gBufferNormalImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferNormalImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferDiffuseImageDescriptorImageInfo;
		gBufferDiffuseImageDescriptorImageInfo.sampler = m_compositingSampler;
		gBufferDiffuseImageDescriptorImageInfo.imageView = m_gBuffer.getDiffuse().view;
		gBufferDiffuseImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferDiffuseImageDescriptorWriteDescriptorSet = {};
		gBufferDiffuseImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstBinding = 4;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferDiffuseImageDescriptorImageInfo;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferMaterialImageDescriptorImageInfo;
		gBufferMaterialImageDescriptorImageInfo.sampler = m_compositingSampler;
		gBufferMaterialImageDescriptorImageInfo.imageView = m_gBuffer.getMaterial().view;
		gBufferMaterialImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferMaterialImageDescriptorWriteDescriptorSet = {};
		gBufferMaterialImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstBinding = 5;
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferMaterialImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferMaterialImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferMaterialImageDescriptorImageInfo;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferEmissiveImageDescriptorImageInfo;
		gBufferEmissiveImageDescriptorImageInfo.sampler = m_compositingSampler;
		gBufferEmissiveImageDescriptorImageInfo.imageView = m_gBuffer.getEmissive().view;
		gBufferEmissiveImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferEmissiveImageDescriptorWriteDescriptorSet = {};
		gBufferEmissiveImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstBinding = 6;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferEmissiveImageDescriptorImageInfo;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo ssaoDescriptorImageInfo;
		ssaoDescriptorImageInfo.sampler = m_compositingSampler;
		ssaoDescriptorImageInfo.imageView = m_ssao.getSSAO().view;
		ssaoDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet ssaoDescriptorWriteDescriptorSet = {};
		ssaoDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ssaoDescriptorWriteDescriptorSet.pNext = nullptr;
		ssaoDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[i];
		ssaoDescriptorWriteDescriptorSet.dstBinding = 7;
		ssaoDescriptorWriteDescriptorSet.dstArrayElement = 0;
		ssaoDescriptorWriteDescriptorSet.descriptorCount = 1;
		ssaoDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssaoDescriptorWriteDescriptorSet.pImageInfo = &ssaoDescriptorImageInfo;
		ssaoDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		ssaoDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 6> writeDescriptorSets = { gBufferPositionImageDescriptorWriteDescriptorSet, gBufferNormalImageDescriptorWriteDescriptorSet, gBufferDiffuseImageDescriptorWriteDescriptorSet, gBufferMaterialImageDescriptorWriteDescriptorSet, gBufferEmissiveImageDescriptorWriteDescriptorSet, ssaoDescriptorWriteDescriptorSet };
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void NtshEngn::GraphicsModule::updateCompositingDescriptorSetsShadow(uint32_t frameInFlight) {
	const std::vector<VulkanImage> shadowMaps = m_shadowMapping.getShadowMapImages();

	std::vector<VkDescriptorImageInfo> shadowMapImageDescriptorImageInfos(shadowMaps.size());
	for (uint32_t i = 0; i < shadowMaps.size(); i++) {
		shadowMapImageDescriptorImageInfos[i].sampler = m_compositingShadowSampler;
		shadowMapImageDescriptorImageInfos[i].imageView = shadowMaps[i].view;
		shadowMapImageDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet shadowMapImageDescriptorWriteDescriptorSet = {};
	shadowMapImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowMapImageDescriptorWriteDescriptorSet.pNext = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.dstSet = m_compositingDescriptorSets[frameInFlight];
	shadowMapImageDescriptorWriteDescriptorSet.dstBinding = 9;
	shadowMapImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	shadowMapImageDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(shadowMapImageDescriptorImageInfos.size());
	shadowMapImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapImageDescriptorWriteDescriptorSet.pImageInfo = shadowMapImageDescriptorImageInfos.data();
	shadowMapImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &shadowMapImageDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createToneMappingResources() {
	createToneMappingImage();

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding colorImageDescriptorSetLayoutBinding = {};
	colorImageDescriptorSetLayoutBinding.binding = 0;
	colorImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorSetLayoutBinding.descriptorCount = 1;
	colorImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	colorImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &colorImageDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_toneMappingDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(location = 0) out vec2 outUv;

		void main() {
			outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
			gl_Position = vec4(outUv * 2.0 + -1.0, 0.0, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform sampler2D imageSampler;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			vec4 color = texture(imageSampler, uv);
			color.rgb /= color.rgb + vec3(1.0);

			outColor = color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_toneMappingDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_toneMappingGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_toneMappingGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_toneMappingGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_toneMappingSampler));

	// Create descriptor pool
	VkDescriptorPoolSize colorImageDescriptorPoolSize = {};
	colorImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &colorImageDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_toneMappingDescriptorPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_toneMappingDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_toneMappingDescriptorSetLayout;
	NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_toneMappingDescriptorSet));

	updateToneMappingDescriptorSet();
}

void NtshEngn::GraphicsModule::createToneMappingImage() {
	// Create image
	VkExtent3D imageExtent;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		imageExtent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		imageExtent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
	}
	else {
		imageExtent.width = 1280;
		imageExtent.height = 720;
	}
	imageExtent.depth = 1;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_drawImageFormat;
	imageCreateInfo.extent.width = imageExtent.width;
	imageCreateInfo.extent.height = imageExtent.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 1;
	imageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo imageAllocationCreateInfo = {};
	imageAllocationCreateInfo.flags = 0;
	imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &imageCreateInfo, &imageAllocationCreateInfo, &m_toneMappingImage.handle, &m_toneMappingImage.allocation, nullptr));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.image = m_toneMappingImage.handle;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = m_drawImageFormat;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_toneMappingImage.view));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkCommandPool commandPool;

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	imageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	imageMemoryBarrier.image = m_toneMappingImage.handle;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;
	dependencyInfo.dependencyFlags = 0;
	dependencyInfo.memoryBarrierCount = 0;
	dependencyInfo.pMemoryBarriers = nullptr;
	dependencyInfo.bufferMemoryBarrierCount = 0;
	dependencyInfo.pBufferMemoryBarriers = nullptr;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &dependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &submitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vkDestroyCommandPool(m_device, commandPool, nullptr);
}

void NtshEngn::GraphicsModule::updateToneMappingDescriptorSet() {
	VkDescriptorImageInfo compositingImageDescriptorImageInfo;
	compositingImageDescriptorImageInfo.sampler = m_toneMappingSampler;
	compositingImageDescriptorImageInfo.imageView = m_compositingImage.view;
	compositingImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet compositingImageDescriptorWriteDescriptorSet = {};
	compositingImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	compositingImageDescriptorWriteDescriptorSet.pNext = nullptr;
	compositingImageDescriptorWriteDescriptorSet.dstSet = m_toneMappingDescriptorSet;
	compositingImageDescriptorWriteDescriptorSet.dstBinding = 0;
	compositingImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	compositingImageDescriptorWriteDescriptorSet.descriptorCount = 1;
	compositingImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositingImageDescriptorWriteDescriptorSet.pImageInfo = &compositingImageDescriptorImageInfo;
	compositingImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	compositingImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &compositingImageDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createUIResources() {
	// Create samplers
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_uiNearestSampler));

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_uiLinearSampler));

	createUITextResources();
	createUILineResources();
	createUIRectangleResources();
	createUIImageResources();
}

void NtshEngn::GraphicsModule::createUITextResources() {
	// Create text buffers
	m_uiTextBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo uiTextBufferCreateInfo = {};
	uiTextBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uiTextBufferCreateInfo.pNext = nullptr;
	uiTextBufferCreateInfo.flags = 0;
	uiTextBufferCreateInfo.size = 32768;
	uiTextBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	uiTextBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	uiTextBufferCreateInfo.queueFamilyIndexCount = 1;
	uiTextBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo bufferAllocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &uiTextBufferCreateInfo, &bufferAllocationCreateInfo, &m_uiTextBuffers[i].handle, &m_uiTextBuffers[i].allocation, &bufferAllocationInfo));
		m_uiTextBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding textBufferDescriptorSetLayoutBinding = {};
	textBufferDescriptorSetLayoutBinding.binding = 0;
	textBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	textBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	textBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding fontsDescriptorSetLayoutBinding = {};
	fontsDescriptorSetLayoutBinding.binding = 1;
	fontsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorSetLayoutBinding.descriptorCount = 131072;
	fontsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fontsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 2> descriptorBindingFlags = { 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings = { textBufferDescriptorSetLayoutBinding, fontsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_uiTextDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct CharacterInfo {
			vec2 positionTopLeft;
			vec2 positionBottomRight;
			vec2 uvTopLeft;
			vec2 uvBottomRight;
		};

		layout(std430, set = 0, binding = 0) restrict readonly buffer Text {
			CharacterInfo char[];
		} text;

		layout(push_constant) uniform TextBufferInfo {
			uint bufferOffset;
		} tBI;

		layout(location = 0) out vec2 outUv;

		void main() {
			const uint charIndex = tBI.bufferOffset + uint(floor(gl_VertexIndex / 6));
			const uint vertexID = uint(mod(float(gl_VertexIndex), 6.0));
			if ((vertexID == 0) || (vertexID == 3)) {
				gl_Position = vec4(text.char[charIndex].positionBottomRight.x, text.char[charIndex].positionTopLeft.y, 0.0, 1.0);
				outUv = vec2(text.char[charIndex].uvBottomRight.x, text.char[charIndex].uvTopLeft.y);
			}
			else if (vertexID == 1) {
				gl_Position = vec4(text.char[charIndex].positionTopLeft, 0.0, 1.0);
				outUv = text.char[charIndex].uvTopLeft;
			}
			else if ((vertexID == 2) || (vertexID == 4)) {
				gl_Position = vec4(text.char[charIndex].positionTopLeft.x, text.char[charIndex].positionBottomRight.y, 0.0, 1.0);
				outUv = vec2(text.char[charIndex].uvTopLeft.x, text.char[charIndex].uvBottomRight.y);
			}
			else if (vertexID == 5) {
				gl_Position = vec4(text.char[charIndex].positionBottomRight, 0.0, 1.0);
				outUv = text.char[charIndex].uvBottomRight;
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(set = 0, binding = 1) uniform sampler2D fonts[];

		layout(push_constant) uniform TextInfo {
			layout(offset = 16) vec4 color;
			uint fontID;
		} tI;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = vec4(1.0, 1.0, 1.0, texture(fonts[nonuniformEXT(tI.fontID)], uv).r) * tI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(uint32_t);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4) + sizeof(uint32_t);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_uiTextDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiTextGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiTextGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiTextGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize textBufferDescriptorPoolSize = {};
	textBufferDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize fontsDescriptorPoolSize = {};
	fontsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes = { textBufferDescriptorPoolSize, fontsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_uiTextDescriptorPool));

	// Allocate descriptor sets
	m_uiTextDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_uiTextDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_uiTextDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_uiTextDescriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo textDescriptorBufferInfo;
		textDescriptorBufferInfo.buffer = m_uiTextBuffers[i].handle;
		textDescriptorBufferInfo.offset = 0;
		textDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet textDescriptorWriteDescriptorSet = {};
		textDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textDescriptorWriteDescriptorSet.pNext = nullptr;
		textDescriptorWriteDescriptorSet.dstSet = m_uiTextDescriptorSets[i];
		textDescriptorWriteDescriptorSet.dstBinding = 0;
		textDescriptorWriteDescriptorSet.dstArrayElement = 0;
		textDescriptorWriteDescriptorSet.descriptorCount = 1;
		textDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		textDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		textDescriptorWriteDescriptorSet.pBufferInfo = &textDescriptorBufferInfo;
		textDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &textDescriptorWriteDescriptorSet, 0, nullptr);
	}

	m_uiTextDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiTextDescriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateUITextDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> fontsDescriptorImageInfos(m_fonts.size());
	for (size_t i = 0; i < m_fonts.size(); i++) {
		fontsDescriptorImageInfos[i].sampler = (m_fonts[i].filter == ImageSamplerFilter::Nearest) ? m_uiNearestSampler : m_uiLinearSampler;
		fontsDescriptorImageInfos[i].imageView = m_fonts[i].imageView;
		fontsDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet fontsDescriptorWriteDescriptorSet = {};
	fontsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	fontsDescriptorWriteDescriptorSet.pNext = nullptr;
	fontsDescriptorWriteDescriptorSet.dstSet = m_uiTextDescriptorSets[frameInFlight];
	fontsDescriptorWriteDescriptorSet.dstBinding = 1;
	fontsDescriptorWriteDescriptorSet.dstArrayElement = 0;
	fontsDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(fontsDescriptorImageInfos.size());
	fontsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorWriteDescriptorSet.pImageInfo = fontsDescriptorImageInfos.data();
	fontsDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	fontsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &fontsDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createUILineResources() {
	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LinePositionInfo {
			vec2 start;
			vec2 end;
		} lPI;

		void main() {
			if (gl_VertexIndex == 0) {
				gl_Position = vec4(lPI.start, 0.0, 1.0);
			}
			else {
				gl_Position = vec4(lPI.end, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LineColorInfo {
			layout(offset = 16) vec4 color;
		} lCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = lCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(Math::vec2) + sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiLineGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiLineGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiLineGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createUIRectangleResources() {
	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectanglePositionInfo {
			vec2 topLeft;
			vec2 bottomRight;
		} rPI;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(rPI.bottomRight.x, rPI.topLeft.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(rPI.topLeft, 0.0, 1.0);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(rPI.topLeft.x, rPI.bottomRight.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(rPI.bottomRight, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectangleColorInfo {
			layout(offset = 16) vec4 color;
		} rCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = rCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(Math::vec2) + sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiRectangleGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiRectangleGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiRectangleGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createUIImageResources() {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding uiTexturesDescriptorSetLayoutBinding = {};
	uiTexturesDescriptorSetLayoutBinding.binding = 0;
	uiTexturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	uiTexturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	uiTexturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags descriptorBindingFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = 1;
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = &descriptorBindingFlag;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &uiTexturesDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_uiImageDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform UITexturePositionInfo {
			vec2 v0;
			vec2 v1;
			vec2 v2;
			vec2 v3;
			vec2 reverseUV;
		} uTPI;

		layout(location = 0) out vec2 outUv;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(uTPI.v0, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(uTPI.v1, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(uTPI.v2, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(uTPI.v3, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(push_constant) uniform UITextureInfo {
			layout(offset = 48) vec4 color;
			uint uiTextureIndex;
		} uTI;

		layout(set = 0, binding = 0) uniform sampler2D uiTextures[];

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = texture(uiTextures[nonuniformEXT(uTI.uiTextureIndex)], uv) * uTI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = 5 * sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = 5 * sizeof(Math::vec2) + sizeof(Math::vec2);
	fragmentPushConstantRange.size = sizeof(Math::vec4) + sizeof(uint32_t);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_uiImageDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiImageGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiImageGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiImageGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize uiTexturesDescriptorPoolSize = {};
	uiTexturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &uiTexturesDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_uiImageDescriptorPool));

	// Allocate descriptor sets
	m_uiImageDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_uiImageDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_uiImageDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_uiImageDescriptorSets[i]));
	}

	m_uiImageDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiImageDescriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateUIImageDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> uiTexturesDescriptorImageInfos(m_uiTextures.size());
	for (size_t i = 0; i < m_uiTextures.size(); i++) {
		uiTexturesDescriptorImageInfos[i].sampler = (m_uiTextures[i].second == ImageSamplerFilter::Nearest) ? m_uiNearestSampler : m_uiLinearSampler;
		uiTexturesDescriptorImageInfos[i].imageView = m_textureImageViews[m_uiTextures[i].first];
		uiTexturesDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet uiTexturesDescriptorWriteDescriptorSet = {};
	uiTexturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uiTexturesDescriptorWriteDescriptorSet.pNext = nullptr;
	uiTexturesDescriptorWriteDescriptorSet.dstSet = m_uiImageDescriptorSets[frameInFlight];
	uiTexturesDescriptorWriteDescriptorSet.dstBinding = 0;
	uiTexturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	uiTexturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(uiTexturesDescriptorImageInfos.size());
	uiTexturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorWriteDescriptorSet.pImageInfo = uiTexturesDescriptorImageInfos.data();
	uiTexturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	uiTexturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &uiTexturesDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createDefaultResources() {
	// Default mesh
	m_meshes.push_back({ 0, 0, 0, 0 });

	// Create texture sampler
	VkSampler defaultTextureSampler;
	VkSamplerCreateInfo textureSamplerCreateInfo = {};
	textureSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	textureSamplerCreateInfo.pNext = nullptr;
	textureSamplerCreateInfo.flags = 0;
	textureSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	textureSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.mipLodBias = 0.0f;
	textureSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	textureSamplerCreateInfo.maxAnisotropy = 16.0f;
	textureSamplerCreateInfo.compareEnable = VK_FALSE;
	textureSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	textureSamplerCreateInfo.minLod = 0.0f;
	textureSamplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	textureSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	textureSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &textureSamplerCreateInfo, nullptr, &defaultTextureSampler));

	m_textureSamplers["defaultSampler"] = defaultTextureSampler;

	// Default diffuse texture
	m_defaultDiffuseTexture.width = 16;
	m_defaultDiffuseTexture.height = 16;
	m_defaultDiffuseTexture.format = ImageFormat::R8G8B8A8;
	m_defaultDiffuseTexture.colorSpace = ImageColorSpace::SRGB;
	m_defaultDiffuseTexture.data.resize(m_defaultDiffuseTexture.width * m_defaultDiffuseTexture.height * 4 * 1);
	for (size_t i = 0; i < 256; i++) {
		m_defaultDiffuseTexture.data[i * 4 + 0] = static_cast<uint8_t>(255 - i);
		m_defaultDiffuseTexture.data[i * 4 + 1] = static_cast<uint8_t>(i % 128);
		m_defaultDiffuseTexture.data[i * 4 + 2] = static_cast<uint8_t>(i);
		m_defaultDiffuseTexture.data[i * 4 + 3] = static_cast<uint8_t>(255);
	}

	load(m_defaultDiffuseTexture);
	m_textures.push_back({ 0, "defaultSampler" });

	// Default normal texture
	m_defaultNormalTexture.width = 1;
	m_defaultNormalTexture.height = 1;
	m_defaultNormalTexture.format = ImageFormat::R8G8B8A8;
	m_defaultNormalTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultNormalTexture.data = { 127, 127, 255, 255 };

	load(m_defaultNormalTexture);
	m_textures.push_back({ 1, "defaultSampler" });

	// Default metalness texture
	m_defaultMetalnessTexture.width = 1;
	m_defaultMetalnessTexture.height = 1;
	m_defaultMetalnessTexture.format = ImageFormat::R8G8B8A8;
	m_defaultMetalnessTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultMetalnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultMetalnessTexture);
	m_textures.push_back({ 2, "defaultSampler" });

	// Default roughness texture
	m_defaultRoughnessTexture.width = 1;
	m_defaultRoughnessTexture.height = 1;
	m_defaultRoughnessTexture.format = ImageFormat::R8G8B8A8;
	m_defaultRoughnessTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultRoughnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultRoughnessTexture);
	m_textures.push_back({ 3, "defaultSampler" });

	// Default occlusion texture
	m_defaultOcclusionTexture.width = 1;
	m_defaultOcclusionTexture.height = 1;
	m_defaultOcclusionTexture.format = ImageFormat::R8G8B8A8;
	m_defaultOcclusionTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultOcclusionTexture.data = { 255, 255, 255, 255 };

	load(m_defaultOcclusionTexture);
	m_textures.push_back({ 4, "defaultSampler" });

	// Default emissive texture
	m_defaultEmissiveTexture.width = 1;
	m_defaultEmissiveTexture.height = 1;
	m_defaultEmissiveTexture.format = ImageFormat::R8G8B8A8;
	m_defaultEmissiveTexture.colorSpace = ImageColorSpace::SRGB;
	m_defaultEmissiveTexture.data = { 0, 0, 0, 255 };

	load(m_defaultEmissiveTexture);
	m_textures.push_back({ 5, "defaultSampler" });

	m_materials.push_back(InternalMaterial());
}

void NtshEngn::GraphicsModule::resize() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsComputeQueue));

		// Destroy swapchain image views
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}

		// Recreate the swapchain
		createSwapchain(m_swapchain);

		// Recreate compositing image and image view
		m_compositingImage.destroy(m_device, m_allocator);
		createCompositingImage();

		// Recreate tone mapping image and image view
		m_toneMappingImage.destroy(m_device, m_allocator);
		createToneMappingImage();

		// Resize G-Buffer
		m_gBuffer.onResize(static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID())), static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID())));

		// Resize SSAO
		m_ssao.onResize(static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID())), static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID())), m_gBuffer.getPosition().view, m_gBuffer.getNormal().view);

		// Resize particles
		m_particles.onResize(static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID())), static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID())));

#if BLOOM_ENABLE == 1
		// Resize bloom
		m_bloom.onResize(static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID())), static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID())), m_compositingImage.view);
#endif

		// Resize FXAA
		m_fxaa.onResize(static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID())), static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID())), m_toneMappingImage.view);

		// Update descriptor sets using these images
		updateCompositingDescriptorSets();
		updateToneMappingDescriptorSet();
	}
}

void NtshEngn::GraphicsModule::loadRenderableForEntity(Entity entity) {
	const Renderable& renderable = ecs->getComponent<Renderable>(entity);
	InternalObject& object = m_objects[entity];

	if (renderable.mesh && !renderable.mesh->vertices.empty()) {
		const std::unordered_map<const Mesh*, MeshID>::const_iterator newMesh = m_meshAddresses.find(renderable.mesh);
		if ((newMesh == m_meshAddresses.end()) || (newMesh->second != object.meshID)) {
			object.meshID = load(*renderable.mesh);
			if (!renderable.mesh->skin.joints.empty()) {
				object.jointTransformOffset = static_cast<uint32_t>(m_freeJointTransformOffsets.addBlock(static_cast<size_t>(m_meshes[object.meshID].jointCount)));
			}
		}
	}
	else {
		object.meshID = 0;
	}

	if (renderable.material == m_lastKnownMaterial[entity]) {
		return;
	}

	InternalMaterial material = m_materials[object.materialIndex];
	if (renderable.material.diffuseTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.diffuseTexture.image);
		ImageID imageID = m_textures[material.diffuseTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.diffuseTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.diffuseTexture.imageSampler);
		if (samplerKey != m_textures[material.diffuseTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.diffuseTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.normalTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.normalTexture.image);
		ImageID imageID = m_textures[material.normalTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.normalTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.normalTexture.imageSampler);
		if (samplerKey != m_textures[material.normalTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.normalTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.metalnessTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.metalnessTexture.image);
		ImageID imageID = m_textures[material.metalnessTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.metalnessTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.metalnessTexture.imageSampler);
		if (samplerKey != m_textures[material.metalnessTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.metalnessTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.roughnessTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.roughnessTexture.image);
		ImageID imageID = m_textures[material.roughnessTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.roughnessTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.roughnessTexture.imageSampler);
		if (samplerKey != m_textures[material.roughnessTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.roughnessTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.occlusionTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.occlusionTexture.image);
		ImageID imageID = m_textures[material.occlusionTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.occlusionTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.occlusionTexture.imageSampler);
		if (samplerKey != m_textures[material.occlusionTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.occlusionTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.emissiveTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.emissiveTexture.image);
		ImageID imageID = m_textures[material.emissiveTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.emissiveTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.emissiveTexture.imageSampler);
		if (samplerKey != m_textures[material.emissiveTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.emissiveTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if ((renderable.material.emissiveFactor != material.emissiveFactor) ||
		(renderable.material.alphaCutoff != material.alphaCutoff)) {
		material.emissiveFactor = renderable.material.emissiveFactor;
		material.alphaCutoff = renderable.material.alphaCutoff;
	}

	uint32_t materialID = addToMaterials(material);
	object.materialIndex = materialID;

	m_lastKnownMaterial[entity] = renderable.material;
}

std::string NtshEngn::GraphicsModule::createSampler(const ImageSampler& sampler) {
	const std::string samplerKey = "mag:" + std::to_string(m_filterMap.at(sampler.magFilter)) +
		"/min:" + std::to_string(m_filterMap.at(sampler.minFilter)) +
		"/mip:" + std::to_string(m_mipmapFilterMap.at(sampler.mipmapFilter)) +
		"/aU:" + std::to_string(m_addressModeMap.at(sampler.addressModeU)) +
		"/aV:" + std::to_string(m_addressModeMap.at(sampler.addressModeV)) +
		"/aW:" + std::to_string(m_addressModeMap.at(sampler.addressModeW)) +
		"/mlb:" + std::to_string(0.0f) +
		"/aE:" + std::to_string(sampler.anisotropyLevel > 0.0f ? VK_TRUE : VK_FALSE) +
		"/cE:" + std::to_string(VK_FALSE) +
		"/cO:" + std::to_string(VK_COMPARE_OP_NEVER) +
		"/mL:" + std::to_string(0.0f) +
		"/ML:" + std::to_string(VK_LOD_CLAMP_NONE) +
		"/bC:" + std::to_string(m_borderColorMap.at(sampler.borderColor)) +
		"/unC:" + std::to_string(VK_FALSE);

	if (m_textureSamplers.find(samplerKey) != m_textureSamplers.end()) {
		return samplerKey;
	}

	VkSampler newSampler;

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = m_filterMap.at(sampler.magFilter);
	samplerCreateInfo.minFilter = m_filterMap.at(sampler.minFilter);
	samplerCreateInfo.mipmapMode = m_mipmapFilterMap.at(sampler.mipmapFilter);
	samplerCreateInfo.addressModeU = m_addressModeMap.at(sampler.addressModeU);
	samplerCreateInfo.addressModeV = m_addressModeMap.at(sampler.addressModeV);
	samplerCreateInfo.addressModeW = m_addressModeMap.at(sampler.addressModeW);
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = sampler.anisotropyLevel > 0.0f ? VK_TRUE : VK_FALSE;
	samplerCreateInfo.maxAnisotropy = sampler.anisotropyLevel;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = m_borderColorMap.at(sampler.borderColor);
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &newSampler));

	m_textureSamplers[samplerKey] = newSampler;

	return samplerKey;
}

uint32_t NtshEngn::GraphicsModule::addToTextures(const InternalTexture& texture) {
	for (size_t i = 0; i < m_textures.size(); i++) {
		const InternalTexture& tex = m_textures[i];
		if ((tex.imageID == texture.imageID) &&
			(tex.samplerKey == texture.samplerKey)) {
			return static_cast<uint32_t>(i);
		}
	}

	m_textures.push_back(texture);

	return static_cast<uint32_t>(m_textures.size()) - 1;
}

uint32_t NtshEngn::GraphicsModule::addToMaterials(const InternalMaterial& material) {
	for (size_t i = 0; i < m_materials.size(); i++) {
		const InternalMaterial& mat = m_materials[i];
		if ((mat.diffuseTextureIndex == material.diffuseTextureIndex) &&
			(mat.normalTextureIndex == material.normalTextureIndex) &&
			(mat.metalnessTextureIndex == material.metalnessTextureIndex) &&
			(mat.roughnessTextureIndex == material.roughnessTextureIndex) &&
			(mat.occlusionTextureIndex == material.occlusionTextureIndex) &&
			(mat.emissiveTextureIndex == material.emissiveTextureIndex) &&
			(mat.emissiveFactor == material.emissiveFactor) &&
			(mat.alphaCutoff == material.alphaCutoff)) {
			return static_cast<uint32_t>(i);
		}
	}

	m_materials.push_back(material);

	return static_cast<uint32_t>(m_materials.size()) - 1;
}

uint32_t NtshEngn::GraphicsModule::findPreviousAnimationKeyframe(float time, const std::vector<AnimationChannelKeyframe>& keyframes) {
	const std::vector<AnimationChannelKeyframe>::const_iterator previousKeyframe = std::lower_bound(keyframes.begin(), keyframes.end(), time, [](const AnimationChannelKeyframe& keyframe, float time) {
		return keyframe.timestamp < time;
		});

	if (previousKeyframe != keyframes.end()) {
		return static_cast<uint32_t>(std::distance(keyframes.begin(), previousKeyframe));
	}

	return std::numeric_limits<uint32_t>::max();
}

uint32_t NtshEngn::GraphicsModule::attributeObjectIndex() {
	uint32_t objectID = m_freeObjectsIndices[0];
	m_freeObjectsIndices.erase(m_freeObjectsIndices.begin());
	if (m_freeObjectsIndices.empty()) {
		m_freeObjectsIndices.push_back(objectID + 1);
	}

	return objectID;
}

void NtshEngn::GraphicsModule::retrieveObjectIndex(uint32_t objectIndex) {
	m_freeObjectsIndices.insert(m_freeObjectsIndices.begin(), objectIndex);
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface * createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface * m) {
	delete m;
}