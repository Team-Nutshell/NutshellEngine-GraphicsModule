#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/modules/ntshengn_window_module_interface.h"
#include "../external/glslang/glslang/Include/ShHandle.h"
#include "../external/glslang/SPIRV/GlslangToSpv.h"
#include "../external/glslang/StandAlone/DirStackFileIncluder.h"
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
#include <random>
#include <cmath>

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

	// Create a queue supporting graphics
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

	// Enable features
	VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures = {};
	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	physicalDeviceDescriptorIndexingFeatures.pNext = nullptr;
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
		"VK_EXT_descriptor_indexing" };
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

		NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &imageCreateInfo, &imageAllocationCreateInfo, &m_drawImage, &m_drawImageAllocation, nullptr));

		// Create the image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_drawImage;
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
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_drawImageView));
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

	createColorAndDepthImages();

	createDescriptorSetLayout();

	createGraphicsPipeline();

	createToneMappingResources();

	createUIResources();

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

	// Create object storage buffers
	m_objectBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo objectBufferCreateInfo = {};
	objectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	objectBufferCreateInfo.pNext = nullptr;
	objectBufferCreateInfo.flags = 0;
	objectBufferCreateInfo.size = 32768;
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

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &meshBufferCreateInfo, &meshBufferAllocationCreateInfo, &m_meshBuffer, &m_meshBufferAllocation, nullptr));

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

	// Create material storage buffers
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

	// Create light storage buffers
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

	createDescriptorSets();

	createDefaultResources();

	createParticleResources();

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
	if (m_mainCamera != NTSHENGN_ENTITY_UNKNOWN) {
		const Camera& camera = ecs->getComponent<Camera>(m_mainCamera);
		const Transform& cameraTransform = ecs->getComponent<Transform>(m_mainCamera);

		const Math::mat4 cameraRotation = Math::rotate(cameraTransform.rotation.x, Math::vec3(1.0f, 0.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.y, Math::vec3(0.0f, 1.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.z, Math::vec3(0.0f, 0.0f, 1.0f));
		Math::mat4 cameraView = cameraRotation * Math::lookAtRH(cameraTransform.position, cameraTransform.position + camera.forward, camera.up);
		Math::mat4 cameraProjection = Math::perspectiveRH(camera.fov, m_viewport.width / m_viewport.height, camera.nearPlane, camera.farPlane);
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
	if (m_descriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateDescriptorSet(m_currentFrameInFlight);

		m_descriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
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

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
	swapchainOrDrawImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.levelCount = 1;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 undefinedToColorAttachmentImageMemoryBarrier = {};
	undefinedToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.image = m_colorImage;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 undefinedToDepthStencilAttachmentImageMemoryBarrier = {};
	undefinedToDepthStencilAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.pNext = nullptr;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.image = m_depthImage;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 3> imageMemoryBarriers = { swapchainOrDrawImageMemoryBarrier, undefinedToColorAttachmentImageMemoryBarrier, undefinedToDepthStencilAttachmentImageMemoryBarrier };
	VkDependencyInfo undefinedToAttachmentDependencyInfo = {};
	undefinedToAttachmentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToAttachmentDependencyInfo.pNext = nullptr;
	undefinedToAttachmentDependencyInfo.dependencyFlags = 0;
	undefinedToAttachmentDependencyInfo.memoryBarrierCount = 0;
	undefinedToAttachmentDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToAttachmentDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToAttachmentDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToAttachmentDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageMemoryBarriers.size());
	undefinedToAttachmentDependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &undefinedToAttachmentDependencyInfo);

	// Update particle buffer
	bool particleBufferUpdated = false;
	if (m_particleBuffersNeedUpdate[m_currentFrameInFlight]) {
		VkBufferMemoryBarrier2 beforeParticleUpdateBufferMemoryBarrier = {};
		beforeParticleUpdateBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeParticleUpdateBufferMemoryBarrier.pNext = nullptr;
		beforeParticleUpdateBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		beforeParticleUpdateBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		beforeParticleUpdateBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		beforeParticleUpdateBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		beforeParticleUpdateBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		beforeParticleUpdateBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		beforeParticleUpdateBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
		beforeParticleUpdateBufferMemoryBarrier.offset = 0;
		beforeParticleUpdateBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

		VkDependencyInfo beforeParticleUpdateBufferBufferDependencyInfo = {};
		beforeParticleUpdateBufferBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		beforeParticleUpdateBufferBufferDependencyInfo.pNext = nullptr;
		beforeParticleUpdateBufferBufferDependencyInfo.dependencyFlags = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.memoryBarrierCount = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.pMemoryBarriers = nullptr;
		beforeParticleUpdateBufferBufferDependencyInfo.bufferMemoryBarrierCount = 1;
		beforeParticleUpdateBufferBufferDependencyInfo.pBufferMemoryBarriers = &beforeParticleUpdateBufferMemoryBarrier;
		beforeParticleUpdateBufferBufferDependencyInfo.imageMemoryBarrierCount = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.pImageMemoryBarriers = nullptr;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &beforeParticleUpdateBufferBufferDependencyInfo);

		// Copy particles staging buffer
		VkBufferCopy particleStagingBufferCopy = {};
		particleStagingBufferCopy.srcOffset = 0;
		particleStagingBufferCopy.dstOffset = (m_maxParticlesNumber * sizeof(Particle)) - m_currentParticleHostSize;
		particleStagingBufferCopy.size = m_currentParticleHostSize;
		vkCmdCopyBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_particleStagingBuffers[m_currentFrameInFlight].handle, m_particleBuffers[m_inParticleBufferCurrentIndex].handle, 1, &particleStagingBufferCopy);

		m_currentParticleHostSize = 0;

		m_particleBuffersNeedUpdate[m_currentFrameInFlight] = false;
		particleBufferUpdated = true;
	}

	// Synchronize before fill particle draw indirect buffer
	VkBufferMemoryBarrier2 beforeFillParticleDrawIndirectBufferMemoryBarrier = {};
	beforeFillParticleDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.buffer = m_particleDrawIndirectBuffer.handle;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkDependencyInfo beforeFillParticleDrawIndirectBufferDependencyInfo = {};
	beforeFillParticleDrawIndirectBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pNext = nullptr;
	beforeFillParticleDrawIndirectBufferDependencyInfo.dependencyFlags = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.memoryBarrierCount = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pMemoryBarriers = nullptr;
	beforeFillParticleDrawIndirectBufferDependencyInfo.bufferMemoryBarrierCount = 1;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pBufferMemoryBarriers = &beforeFillParticleDrawIndirectBufferMemoryBarrier;
	beforeFillParticleDrawIndirectBufferDependencyInfo.imageMemoryBarrierCount = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &beforeFillParticleDrawIndirectBufferDependencyInfo);

	// Fill draw indirect particle buffer
	vkCmdFillBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_particleDrawIndirectBuffer.handle, 0, sizeof(uint32_t), 0);

	// Synchronize before particle compute
	VkBufferMemoryBarrier2 beforeDispatchParticleDrawIndirectBufferMemoryBarrier = {};
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.buffer = m_particleDrawIndirectBuffer.handle;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 drawToParticleComputeBufferMemoryBarrier = {};
	drawToParticleComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	drawToParticleComputeBufferMemoryBarrier.pNext = nullptr;
	if (particleBufferUpdated) {
		drawToParticleComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		drawToParticleComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else {
		drawToParticleComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		drawToParticleComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	}
	drawToParticleComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	drawToParticleComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	drawToParticleComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	drawToParticleComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	drawToParticleComputeBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle; 
	drawToParticleComputeBufferMemoryBarrier.offset = 0;
	drawToParticleComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkBufferMemoryBarrier2 computeBufferToComputeBufferMemoryBarrier = {};
	computeBufferToComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	computeBufferToComputeBufferMemoryBarrier.pNext = nullptr;
	computeBufferToComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	computeBufferToComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	computeBufferToComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	computeBufferToComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	computeBufferToComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	computeBufferToComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	computeBufferToComputeBufferMemoryBarrier.buffer = m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle;
	computeBufferToComputeBufferMemoryBarrier.offset = 0;
	computeBufferToComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	std::array<VkBufferMemoryBarrier2, 3> drawToParticleComputeBufferMemoryBarriers = { beforeDispatchParticleDrawIndirectBufferMemoryBarrier, drawToParticleComputeBufferMemoryBarrier, computeBufferToComputeBufferMemoryBarrier };
	VkDependencyInfo drawToParticleComputeBufferDependencyInfo = {};
	drawToParticleComputeBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	drawToParticleComputeBufferDependencyInfo.pNext = nullptr;
	drawToParticleComputeBufferDependencyInfo.dependencyFlags = 0;
	drawToParticleComputeBufferDependencyInfo.memoryBarrierCount = 0;
	drawToParticleComputeBufferDependencyInfo.pMemoryBarriers = nullptr;
	drawToParticleComputeBufferDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(drawToParticleComputeBufferMemoryBarriers.size());
	drawToParticleComputeBufferDependencyInfo.pBufferMemoryBarriers = drawToParticleComputeBufferMemoryBarriers.data();
	drawToParticleComputeBufferDependencyInfo.imageMemoryBarrierCount = 0;
	drawToParticleComputeBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &drawToParticleComputeBufferDependencyInfo);

	// Dispatch particles
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_COMPUTE, m_particleComputePipeline);

	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_COMPUTE, m_particleComputePipelineLayout, 0, 1, &m_particleComputeDescriptorSets[m_inParticleBufferCurrentIndex], 0, nullptr);

	float deltaTimeFloat = static_cast<float>(dt / 1000.0);
	vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_particleComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &deltaTimeFloat);

	vkCmdDispatch(m_renderingCommandBuffers[m_currentFrameInFlight], ((m_maxParticlesNumber + 64 - 1) / 64), 1, 1);

	// Synchronize before fill in particle
	VkBufferMemoryBarrier2 beforeFillInParticleBufferMemoryBarrier = {};
	beforeFillInParticleBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillInParticleBufferMemoryBarrier.pNext = nullptr;
	beforeFillInParticleBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeFillInParticleBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	beforeFillInParticleBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeFillInParticleBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeFillInParticleBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillInParticleBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillInParticleBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
	beforeFillInParticleBufferMemoryBarrier.offset = 0;
	beforeFillInParticleBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkDependencyInfo beforeFillInParticleBufferDependencyInfo = {};
	beforeFillInParticleBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeFillInParticleBufferDependencyInfo.pNext = nullptr;
	beforeFillInParticleBufferDependencyInfo.dependencyFlags = 0;
	beforeFillInParticleBufferDependencyInfo.memoryBarrierCount = 0;
	beforeFillInParticleBufferDependencyInfo.pMemoryBarriers = nullptr;
	beforeFillInParticleBufferDependencyInfo.bufferMemoryBarrierCount = 1;
	beforeFillInParticleBufferDependencyInfo.pBufferMemoryBarriers = &beforeFillInParticleBufferMemoryBarrier;
	beforeFillInParticleBufferDependencyInfo.imageMemoryBarrierCount = 0;
	beforeFillInParticleBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &beforeFillInParticleBufferDependencyInfo);

	// Fill in particle with 0
	vkCmdFillBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_particleBuffers[m_inParticleBufferCurrentIndex].handle, 0, m_maxParticlesNumber * sizeof(Particle), 0);

	// Synchronize before particle draw
	VkBufferMemoryBarrier2 beforeDrawIndirectBufferMemoryBarrier = {};
	beforeDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	beforeDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDrawIndirectBufferMemoryBarrier.buffer = m_particleDrawIndirectBuffer.handle;
	beforeDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 fillParticleBufferBeforeComputeBufferMemoryBarrier = {};
	fillParticleBufferBeforeComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.pNext = nullptr;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.offset = 0;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkBufferMemoryBarrier2 particleComputeToDrawBufferMemoryBarrier = {};
	particleComputeToDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	particleComputeToDrawBufferMemoryBarrier.pNext = nullptr;
	particleComputeToDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	particleComputeToDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	particleComputeToDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	particleComputeToDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	particleComputeToDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	particleComputeToDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	particleComputeToDrawBufferMemoryBarrier.buffer = m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle;
	particleComputeToDrawBufferMemoryBarrier.offset = 0;
	particleComputeToDrawBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	std::array<VkBufferMemoryBarrier2, 3> particleComputeToDrawBufferMemoryBarriers = { beforeDrawIndirectBufferMemoryBarrier, fillParticleBufferBeforeComputeBufferMemoryBarrier, particleComputeToDrawBufferMemoryBarrier };
	VkDependencyInfo particleComputeToDrawDependencyInfo = {};
	particleComputeToDrawDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	particleComputeToDrawDependencyInfo.pNext = nullptr;
	particleComputeToDrawDependencyInfo.dependencyFlags = 0;
	particleComputeToDrawDependencyInfo.memoryBarrierCount = 0;
	particleComputeToDrawDependencyInfo.pMemoryBarriers = nullptr;
	particleComputeToDrawDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(particleComputeToDrawBufferMemoryBarriers.size());
	particleComputeToDrawDependencyInfo.pBufferMemoryBarriers = particleComputeToDrawBufferMemoryBarriers.data();
	particleComputeToDrawDependencyInfo.imageMemoryBarrierCount = 0;
	particleComputeToDrawDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &particleComputeToDrawDependencyInfo);

	// Bind vertex and index buffers
	VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindVertexBuffers(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_vertexBuffer, &vertexBufferOffset);
	vkCmdBindIndexBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameInFlight], 0, nullptr);

	// Begin rendering
	VkRenderingAttachmentInfo renderingColorAttachmentInfo = {};
	renderingColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingColorAttachmentInfo.pNext = nullptr;
	renderingColorAttachmentInfo.imageView = m_colorImageView;
	renderingColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingColorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingColorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingColorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingColorAttachmentInfo.clearValue.color = { m_backgroundColor.x, m_backgroundColor.y, m_backgroundColor.z, m_backgroundColor.w };

	VkRenderingAttachmentInfo renderingDepthAttachmentInfo = {};
	renderingDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingDepthAttachmentInfo.pNext = nullptr;
	renderingDepthAttachmentInfo.imageView = m_depthImageView;
	renderingDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	renderingDepthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingDepthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingDepthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingDepthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.pNext = nullptr;
	renderingInfo.flags = 0;
	renderingInfo.renderArea = m_scissor;
	renderingInfo.layerCount = 1;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &renderingColorAttachmentInfo;
	renderingInfo.pDepthAttachment = &renderingDepthAttachmentInfo;
	renderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &renderingInfo);

	// Bind graphics pipeline
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	for (auto& it : m_objects) {
		// Object index as push constant
		vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &it.second.index);

		// Draw
		vkCmdDrawIndexed(m_renderingCommandBuffers[m_currentFrameInFlight], m_meshes[it.second.meshID].indexCount, 1, m_meshes[it.second.meshID].firstIndex, m_meshes[it.second.meshID].vertexOffset, 0);
	}

	// End rendering
	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Synchronization before particles
	VkImageMemoryBarrier2 colorAttachmentBeforeParticlesImageMemoryBarrier = {};
	colorAttachmentBeforeParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	colorAttachmentBeforeParticlesImageMemoryBarrier.pNext = nullptr;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentBeforeParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentBeforeParticlesImageMemoryBarrier.image = m_colorImage;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 depthAttachmentBeforeParticlesImageMemoryBarrier = {};
	depthAttachmentBeforeParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	depthAttachmentBeforeParticlesImageMemoryBarrier.pNext = nullptr;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentBeforeParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	depthAttachmentBeforeParticlesImageMemoryBarrier.image = m_depthImage;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> beforeParticlesImageMemoryBarriers = { colorAttachmentBeforeParticlesImageMemoryBarrier, depthAttachmentBeforeParticlesImageMemoryBarrier };
	VkDependencyInfo beforeParticlesDependencyInfo = {};
	beforeParticlesDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeParticlesDependencyInfo.pNext = nullptr;
	beforeParticlesDependencyInfo.dependencyFlags = 0;
	beforeParticlesDependencyInfo.memoryBarrierCount = 0;
	beforeParticlesDependencyInfo.pMemoryBarriers = nullptr;
	beforeParticlesDependencyInfo.bufferMemoryBarrierCount = 0;
	beforeParticlesDependencyInfo.pBufferMemoryBarriers = nullptr;
	beforeParticlesDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(beforeParticlesImageMemoryBarriers.size());
	beforeParticlesDependencyInfo.pImageMemoryBarriers = beforeParticlesImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &beforeParticlesDependencyInfo);

	// Draw particles
	VkRenderingAttachmentInfo particleRenderingColorAttachmentInfo = {};
	particleRenderingColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	particleRenderingColorAttachmentInfo.pNext = nullptr;
	particleRenderingColorAttachmentInfo.imageView = m_colorImageView;
	particleRenderingColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	particleRenderingColorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	particleRenderingColorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	particleRenderingColorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	particleRenderingColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	particleRenderingColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	particleRenderingColorAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo particleRenderingDepthAttachmentInfo = {};
	particleRenderingDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	particleRenderingDepthAttachmentInfo.pNext = nullptr;
	particleRenderingDepthAttachmentInfo.imageView = m_depthImageView;
	particleRenderingDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	particleRenderingDepthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	particleRenderingDepthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	particleRenderingDepthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	particleRenderingDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	particleRenderingDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	particleRenderingDepthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo particleRenderingInfo = {};
	particleRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	particleRenderingInfo.pNext = nullptr;
	particleRenderingInfo.flags = 0;
	particleRenderingInfo.renderArea = m_scissor;
	particleRenderingInfo.layerCount = 1;
	particleRenderingInfo.viewMask = 0;
	particleRenderingInfo.colorAttachmentCount = 1;
	particleRenderingInfo.pColorAttachments = &particleRenderingColorAttachmentInfo;
	particleRenderingInfo.pDepthAttachment = &particleRenderingDepthAttachmentInfo;
	particleRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &particleRenderingInfo);

	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_particleGraphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	vkCmdBindVertexBuffers(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle, &vertexBufferOffset);

	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_particleGraphicsPipelineLayout, 0, 1, &m_particleGraphicsDescriptorSets[m_currentFrameInFlight], 0, nullptr);

	vkCmdDrawIndirect(m_renderingCommandBuffers[m_currentFrameInFlight], m_particleDrawIndirectBuffer.handle, 0, 1, 0);

	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier2 colorAttachmentToShaderReadOnlyImageMemoryBarrier = {};
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.image = m_colorImage;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = 1;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	colorAttachmentToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo colorAttachmentToShaderReadOnlyDependencyInfo = {};
	colorAttachmentToShaderReadOnlyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	colorAttachmentToShaderReadOnlyDependencyInfo.pNext = nullptr;
	colorAttachmentToShaderReadOnlyDependencyInfo.dependencyFlags = 0;
	colorAttachmentToShaderReadOnlyDependencyInfo.memoryBarrierCount = 0;
	colorAttachmentToShaderReadOnlyDependencyInfo.pMemoryBarriers = nullptr;
	colorAttachmentToShaderReadOnlyDependencyInfo.bufferMemoryBarrierCount = 0;
	colorAttachmentToShaderReadOnlyDependencyInfo.pBufferMemoryBarriers = nullptr;
	colorAttachmentToShaderReadOnlyDependencyInfo.imageMemoryBarrierCount = 1;
	colorAttachmentToShaderReadOnlyDependencyInfo.pImageMemoryBarriers = &colorAttachmentToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentToShaderReadOnlyDependencyInfo);

	// Tonemapping
	VkRenderingAttachmentInfo renderingSwapchainAttachmentInfo = {};
	renderingSwapchainAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingSwapchainAttachmentInfo.pNext = nullptr;
	renderingSwapchainAttachmentInfo.imageView = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImageViews[imageIndex] : m_drawImageView;
	renderingSwapchainAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingSwapchainAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingSwapchainAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingSwapchainAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	renderingSwapchainAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingSwapchainAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo toneMappingRenderingInfo = {};
	toneMappingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	toneMappingRenderingInfo.pNext = nullptr;
	toneMappingRenderingInfo.flags = 0;
	toneMappingRenderingInfo.renderArea = m_scissor;
	toneMappingRenderingInfo.layerCount = 1;
	toneMappingRenderingInfo.viewMask = 0;
	toneMappingRenderingInfo.colorAttachmentCount = 1;
	toneMappingRenderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
	toneMappingRenderingInfo.pDepthAttachment = nullptr;
	toneMappingRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &toneMappingRenderingInfo);

	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipelineLayout, 0, 1, &m_toneMappingDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	if (!m_uiElements.empty()) {
		// Image memory barrier between tonemapping and UI
		VkImageMemoryBarrier2 tonemappingUIImageMemoryBarrier = {};
		tonemappingUIImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		tonemappingUIImageMemoryBarrier.pNext = nullptr;
		tonemappingUIImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		tonemappingUIImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		tonemappingUIImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		tonemappingUIImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		tonemappingUIImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		tonemappingUIImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		tonemappingUIImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		tonemappingUIImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		tonemappingUIImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage;
		tonemappingUIImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		tonemappingUIImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		tonemappingUIImageMemoryBarrier.subresourceRange.levelCount = 1;
		tonemappingUIImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		tonemappingUIImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo tonemappingUIDependencyInfo = {};
		tonemappingUIDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		tonemappingUIDependencyInfo.pNext = nullptr;
		tonemappingUIDependencyInfo.dependencyFlags = 0;
		tonemappingUIDependencyInfo.memoryBarrierCount = 0;
		tonemappingUIDependencyInfo.pMemoryBarriers = nullptr;
		tonemappingUIDependencyInfo.bufferMemoryBarrierCount = 0;
		tonemappingUIDependencyInfo.pBufferMemoryBarriers = nullptr;
		tonemappingUIDependencyInfo.imageMemoryBarrierCount = 1;
		tonemappingUIDependencyInfo.pImageMemoryBarriers = &tonemappingUIImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &tonemappingUIDependencyInfo);

		// UI
		renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

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

	m_inParticleBufferCurrentIndex = (m_inParticleBufferCurrentIndex + 1) % 2;

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
		vmaDestroyBuffer(m_allocator, m_lightBuffers[i].handle, m_lightBuffers[i].allocation);
	}

	// Destroy material buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_materialBuffers[i].handle, m_materialBuffers[i].allocation);
	}

	// Destroy joint transform buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_jointTransformBuffers[i].handle, m_jointTransformBuffers[i].allocation);
	}

	// Destroy mesh buffer
	vmaDestroyBuffer(m_allocator, m_meshBuffer, m_meshBufferAllocation);

	// Destroy object buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_objectBuffers[i].handle, m_objectBuffers[i].allocation);
	}

	// Destroy camera buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_cameraBuffers[i].handle, m_cameraBuffers[i].allocation);
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
		vmaDestroyBuffer(m_allocator, m_uiTextBuffers[i].handle, m_uiTextBuffers[i].allocation);
	}

	vkDestroySampler(m_device, m_uiLinearSampler, nullptr);
	vkDestroySampler(m_device, m_uiNearestSampler, nullptr);

	// Destroy tone mapping resources
	vkDestroyDescriptorPool(m_device, m_toneMappingDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_toneMappingGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_toneMappingGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_toneMappingDescriptorSetLayout, nullptr);
	vkDestroySampler(m_device, m_toneMappingSampler, nullptr);

	// Destroy particle resources
	vkDestroyDescriptorPool(m_device, m_particleGraphicsDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_particleGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_particleGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_particleGraphicsDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(m_device, m_particleComputeDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_particleComputePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_particleComputePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_particleComputeDescriptorSetLayout, nullptr);
	vmaDestroyBuffer(m_allocator, m_particleDrawIndirectBuffer.handle, m_particleDrawIndirectBuffer.allocation);
	for (size_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_particleStagingBuffers[i].handle, m_particleStagingBuffers[i].allocation);
	}
	for (size_t i = 0; i < m_particleBuffers.size(); i++) {
		vmaDestroyBuffer(m_allocator, m_particleBuffers[i].handle, m_particleBuffers[i].allocation);
	}

	// Destroy descriptor pool
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// Destroy graphics pipeline
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);

	// Destroy graphics pipeline layout
	vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);

	// Destroy descriptor set layout
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Destroy depth image and image view
	vkDestroyImageView(m_device, m_depthImageView, nullptr);
	vmaDestroyImage(m_allocator, m_depthImage, m_depthImageAllocation);

	// Destroy color image and image view
	vkDestroyImageView(m_device, m_colorImageView, nullptr);
	vmaDestroyImage(m_allocator, m_colorImage, m_colorImageAllocation);

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
	vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexBufferAllocation);

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
		vkDestroyImageView(m_device, m_drawImageView, nullptr);
		vmaDestroyImage(m_allocator, m_drawImage, m_drawImageAllocation);
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

	m_meshes.push_back({ static_cast<uint32_t>(mesh.indices.size()), m_currentIndexOffset, m_currentVertexOffset, static_cast<uint32_t>(mesh.skin.joints.size()) });
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
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_vertexBuffer, 1, &vertexBufferCopy);

	VkBufferCopy indexBufferCopy = {};
	indexBufferCopy.srcOffset = mesh.vertices.size() * sizeof(Vertex);
	indexBufferCopy.dstOffset = m_currentIndexOffset * sizeof(uint32_t);
	indexBufferCopy.size = mesh.indices.size() * sizeof(uint32_t);
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_indexBuffer, 1, &indexBufferCopy);

	VkBufferCopy meshBufferCopy = {};
	meshBufferCopy.srcOffset = 0;
	meshBufferCopy.dstOffset = (m_meshes.size() - 1) * sizeof(uint32_t);
	meshBufferCopy.size = sizeof(uint32_t);
	vkCmdCopyBuffer(m_initializationCommandBuffer, meshStagingBuffer, m_meshBuffer, 1, &meshBufferCopy);

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
		m_descriptorSetsNeedUpdate[i] = true;
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

	std::random_device r;
	std::default_random_engine randomEngine(r());
	std::uniform_real_distribution<float> randomDistribution(0.0f, 1.0f);

	std::vector<Particle> particles(particleEmitter.number);
	for (Particle& particle : particles) {
		particle.position = Math::vec3(Math::lerp(particleEmitter.positionRange[0].x, particleEmitter.positionRange[1].x, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.positionRange[0].y, particleEmitter.positionRange[1].y, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.positionRange[0].z, particleEmitter.positionRange[1].z, randomDistribution(randomEngine)));
		particle.size = Math::lerp(particleEmitter.sizeRange[0], particleEmitter.sizeRange[1], randomDistribution(randomEngine));
		particle.color = Math::vec4(Math::lerp(particleEmitter.colorRange[0].x, particleEmitter.colorRange[1].x, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.colorRange[0].y, particleEmitter.colorRange[1].y, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.colorRange[0].z, particleEmitter.colorRange[1].y, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.colorRange[0].w, particleEmitter.colorRange[1].w, randomDistribution(randomEngine)));
		Math::vec3 rotation = Math::vec3(Math::lerp(particleEmitter.rotationRange[0].x, particleEmitter.rotationRange[1].x, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.rotationRange[0].y, particleEmitter.rotationRange[1].y, randomDistribution(randomEngine)),
			Math::lerp(particleEmitter.rotationRange[0].z, particleEmitter.rotationRange[1].z, randomDistribution(randomEngine)));
		const Math::vec3 baseDirection = Math::normalize(particleEmitter.baseDirection);
		const float baseDirectionYaw = std::atan2(baseDirection.z, baseDirection.x);
		const float baseDirectionPitch = -std::asin(baseDirection.y);
		particle.direction = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + rotation.x) * std::cos(baseDirectionYaw + rotation.y),
			-std::sin(baseDirectionPitch + rotation.x),
			std::cos(baseDirectionPitch + rotation.x) * std::sin(baseDirectionYaw + rotation.y)
		));
		particle.speed = Math::lerp(particleEmitter.speedRange[0], particleEmitter.speedRange[1], randomDistribution(randomEngine));
		particle.duration = Math::lerp(particleEmitter.durationRange[0], particleEmitter.durationRange[1], randomDistribution(randomEngine));
	}

	size_t size = particles.size() * sizeof(Particle);
	memcpy(reinterpret_cast<uint8_t*>(m_particleStagingBuffers[m_currentFrameInFlight].address) + m_currentParticleHostSize, particles.data(), size);

	m_currentParticleHostSize += size;

	m_particleBuffersNeedUpdate[m_currentFrameInFlight] = true;
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

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.insert(entity);
			break;

		case LightType::Point:
			m_lights.pointLights.insert(entity);
			break;

		case LightType::Spot:
			m_lights.spotLights.insert(entity);
			break;

		case LightType::Ambient:
			m_lights.ambientLights.insert(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.insert(entity);
			break;
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

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.erase(entity);
			break;

		case LightType::Point:
			m_lights.pointLights.erase(entity);
			break;

		case LightType::Spot:
			m_lights.spotLights.erase(entity);
			break;

		case LightType::Ambient:
			m_lights.ambientLights.erase(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.erase(entity);
			break;
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
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_vertexBuffer, &m_vertexBufferAllocation, nullptr));

	vertexAndIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_indexBuffer, &m_indexBufferAllocation, nullptr));
}

void NtshEngn::GraphicsModule::createColorAndDepthImages() {
	VkImageCreateInfo colorImageCreateInfo = {};
	colorImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	colorImageCreateInfo.pNext = nullptr;
	colorImageCreateInfo.flags = 0;
	colorImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	colorImageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		colorImageCreateInfo.extent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		colorImageCreateInfo.extent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
	}
	else {
		colorImageCreateInfo.extent.width = 1280;
		colorImageCreateInfo.extent.height = 720;
	}
	colorImageCreateInfo.extent.depth = 1;
	colorImageCreateInfo.mipLevels = 1;
	colorImageCreateInfo.arrayLayers = 1;
	colorImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	colorImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	colorImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	colorImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	colorImageCreateInfo.queueFamilyIndexCount = 1;
	colorImageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	colorImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo colorImageAllocationCreateInfo = {};
	colorImageAllocationCreateInfo.flags = 0;
	colorImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &colorImageCreateInfo, &colorImageAllocationCreateInfo, &m_colorImage, &m_colorImageAllocation, nullptr));

	VkImageViewCreateInfo colorImageViewCreateInfo = {};
	colorImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	colorImageViewCreateInfo.pNext = nullptr;
	colorImageViewCreateInfo.flags = 0;
	colorImageViewCreateInfo.image = m_colorImage;
	colorImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	colorImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	colorImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	colorImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	colorImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	colorImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	colorImageViewCreateInfo.subresourceRange.levelCount = 1;
	colorImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	colorImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &colorImageViewCreateInfo, nullptr, &m_colorImageView));

	VkImageCreateInfo depthImageCreateInfo = {};
	depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageCreateInfo.pNext = nullptr;
	depthImageCreateInfo.flags = 0;
	depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		depthImageCreateInfo.extent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		depthImageCreateInfo.extent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
	}
	else {
		depthImageCreateInfo.extent.width = 1280;
		depthImageCreateInfo.extent.height = 720;
	}
	depthImageCreateInfo.extent.depth = 1;
	depthImageCreateInfo.mipLevels = 1;
	depthImageCreateInfo.arrayLayers = 1;
	depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	depthImageCreateInfo.queueFamilyIndexCount = 1;
	depthImageCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;
	depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo depthImageAllocationCreateInfo = {};
	depthImageAllocationCreateInfo.flags = 0;
	depthImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &depthImageCreateInfo, &depthImageAllocationCreateInfo, &m_depthImage, &m_depthImageAllocation, nullptr));

	VkImageViewCreateInfo depthImageViewCreateInfo = {};
	depthImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthImageViewCreateInfo.pNext = nullptr;
	depthImageViewCreateInfo.flags = 0;
	depthImageViewCreateInfo.image = m_depthImage;
	depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthImageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	depthImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	depthImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	depthImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	depthImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	depthImageViewCreateInfo.subresourceRange.levelCount = 1;
	depthImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	depthImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &depthImageViewCreateInfo, nullptr, &m_depthImageView));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL and VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo colorAndDepthImagesTransitionBeginInfo = {};
	colorAndDepthImagesTransitionBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	colorAndDepthImagesTransitionBeginInfo.pNext = nullptr;
	colorAndDepthImagesTransitionBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	colorAndDepthImagesTransitionBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &colorAndDepthImagesTransitionBeginInfo));

	VkImageMemoryBarrier2 undefinedToColorAttachmentImageMemoryBarrier = {};
	undefinedToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.image = m_colorImage;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 undefinedToDepthStencilAttachmentImageMemoryBarrier = {};
	undefinedToDepthStencilAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.pNext = nullptr;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.image = m_depthImage;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToDepthStencilAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> imageMemoryBarriers = { undefinedToColorAttachmentImageMemoryBarrier, undefinedToDepthStencilAttachmentImageMemoryBarrier };
	VkDependencyInfo undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo = {};
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.pNext = nullptr;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.dependencyFlags = 0;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.memoryBarrierCount = 0;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageMemoryBarriers.size());
	undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToColorAttachmentAndUndefinedToDepthStencilAttachmentDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo colorAndDepthImagesTransitionSubmitInfo = {};
	colorAndDepthImagesTransitionSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	colorAndDepthImagesTransitionSubmitInfo.pNext = nullptr;
	colorAndDepthImagesTransitionSubmitInfo.waitSemaphoreCount = 0;
	colorAndDepthImagesTransitionSubmitInfo.pWaitSemaphores = nullptr;
	colorAndDepthImagesTransitionSubmitInfo.pWaitDstStageMask = nullptr;
	colorAndDepthImagesTransitionSubmitInfo.commandBufferCount = 1;
	colorAndDepthImagesTransitionSubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	colorAndDepthImagesTransitionSubmitInfo.signalSemaphoreCount = 0;
	colorAndDepthImagesTransitionSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &colorAndDepthImagesTransitionSubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));
}

void NtshEngn::GraphicsModule::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding objectsDescriptorSetLayoutBinding = {};
	objectsDescriptorSetLayoutBinding.binding = 1;
	objectsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorSetLayoutBinding.descriptorCount = 1;
	objectsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	objectsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding meshesDescriptorSetLayoutBinding = {};
	meshesDescriptorSetLayoutBinding.binding = 2;
	meshesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshesDescriptorSetLayoutBinding.descriptorCount = 1;
	meshesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	meshesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding jointTransformsDescriptorSetLayoutBinding = {};
	jointTransformsDescriptorSetLayoutBinding.binding = 3;
	jointTransformsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	jointTransformsDescriptorSetLayoutBinding.descriptorCount = 1;
	jointTransformsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	jointTransformsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialsDescriptorSetLayoutBinding = {};
	materialsDescriptorSetLayoutBinding.binding = 4;
	materialsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorSetLayoutBinding.descriptorCount = 1;
	materialsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 5;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = {};
	texturesDescriptorSetLayoutBinding.binding = 6;
	texturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	texturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 7> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 7> descriptorSetLayoutBindings = { cameraDescriptorSetLayoutBinding, objectsDescriptorSetLayoutBinding, meshesDescriptorSetLayoutBinding, jointTransformsDescriptorSetLayoutBinding, materialsDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, texturesDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

std::vector<uint32_t> NtshEngn::GraphicsModule::compileShader(const std::string& shaderCode, ShaderType type) {
	if (!m_glslangInitialized) {
		glslang::InitializeProcess();
		m_glslangInitialized = true;
	}

	std::vector<uint32_t> spvCode;

	const char* shaderCodeCharPtr = shaderCode.c_str();

	EShLanguage shaderType = EShLangVertex;
	switch (type) {
	case ShaderType::Vertex:
		shaderType = EShLangVertex;
		break;

	case ShaderType::TesselationControl:
		shaderType = EShLangTessControl;
		break;

	case ShaderType::TesselationEvaluation:
		shaderType = EShLangTessEvaluation;
		break;

	case ShaderType::Geometry:
		shaderType = EShLangGeometry;
		break;

	case ShaderType::Fragment:
		shaderType = EShLangFragment;
		break;

	case ShaderType::Compute:
		shaderType = EShLangCompute;
		break;
	}

	glslang::TShader shader(shaderType);
	shader.setStrings(&shaderCodeCharPtr, 1);
	int clientInputSemanticsVersion = 110;
	glslang::EshTargetClientVersion vulkanClientVersion = glslang::EShTargetVulkan_1_1;
	glslang::EShTargetLanguageVersion spvLanguageVersion = glslang::EShTargetSpv_1_2;
	shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientVulkan, clientInputSemanticsVersion);
	shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVersion);
	shader.setEnvTarget(glslang::EshTargetSpv, spvLanguageVersion);
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
	int defaultVersion = 460;

	// Preprocess
	const TBuiltInResource defaultTBuiltInResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxMeshOutputVerticesEXT = */ 256,
		/* .maxMeshOutputPrimitivesEXT = */ 256,
		/* .maxMeshWorkGroupSizeX_EXT = */ 128,
		/* .maxMeshWorkGroupSizeY_EXT = */ 128,
		/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
		/* .maxTaskWorkGroupSizeX_EXT = */ 128,
		/* .maxTaskWorkGroupSizeY_EXT = */ 128,
		/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
		/* .maxMeshViewCountEXT = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		} };
	DirStackFileIncluder includer;
	std::string preprocess;
	if (!shader.preprocess(&defaultTBuiltInResource, defaultVersion, ENoProfile, false, false, messages, &preprocess, includer)) {
		NTSHENGN_MODULE_WARNING("Shader preprocessing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Parse
	const char* preprocessCharPtr = preprocess.c_str();
	shader.setStrings(&preprocessCharPtr, 1);
	if (!shader.parse(&defaultTBuiltInResource, defaultVersion, false, messages)) {
		NTSHENGN_MODULE_WARNING("Shader parsing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Link
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		NTSHENGN_MODULE_WARNING("Shader linking failed.");
		return spvCode;
	}

	// Compile
	spv::SpvBuildLogger buildLogger;
	glslang::SpvOptions spvOptions;
	glslang::GlslangToSpv(*program.getIntermediate(shaderType), spvCode, &buildLogger, &spvOptions);

	return spvCode;
}

void NtshEngn::GraphicsModule::createGraphicsPipeline() {
	VkFormat pipelineRenderingColorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct ObjectInfo {
			mat4 model;
			mat4 transposeInverseModel;
			uint meshID;
			uint jointTransformOffset;
			uint materialID;
		};

		struct MeshInfo {
			uint hasSkin;
		};

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(std430, set = 0, binding = 1) restrict readonly buffer Objects {
			ObjectInfo info[];
		} objects;

		layout(std430, set = 0, binding = 2) restrict readonly buffer Meshes {
			MeshInfo info[];
		} meshes;

		layout(set = 0, binding = 3) restrict readonly buffer JointTransforms {
			mat4 matrix[];
		} jointTransforms;

		layout(push_constant) uniform ObjectID {
			uint objectID;
		} oID;

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec3 normal;
		layout(location = 2) in vec2 uv;
		layout(location = 3) in vec3 color;
		layout(location = 4) in vec4 tangent;
		layout(location = 5) in uvec4 joints;
		layout(location = 6) in vec4 weights;

		layout(location = 0) out vec3 outPosition;
		layout(location = 1) out vec2 outUV;
		layout(location = 2) out flat uint outMaterialID;
		layout(location = 3) out flat vec3 outCameraPosition;
		layout(location = 4) out mat3 outTBN;

		void main() {
			mat4 skinMatrix = mat4(1.0);
			if (meshes.info[objects.info[oID.objectID].meshID].hasSkin == 1) {
				uint jointTransformOffset = objects.info[oID.objectID].jointTransformOffset;

				skinMatrix = (weights.x * jointTransforms.matrix[jointTransformOffset + joints.x]) +
					(weights.y * jointTransforms.matrix[jointTransformOffset + joints.y]) +
					(weights.z * jointTransforms.matrix[jointTransformOffset + joints.z]) +
					(weights.w * jointTransforms.matrix[jointTransformOffset + joints.w]);
			}
			outPosition = vec3(objects.info[oID.objectID].model * skinMatrix * vec4(position, 1.0));
			outUV = uv;
			outMaterialID = objects.info[oID.objectID].materialID;
			outCameraPosition = camera.position;

			vec3 skinnedNormal = vec3(transpose(inverse(skinMatrix)) * vec4(normal, 0.0));
			vec3 skinnedTangent = vec3(skinMatrix * vec4(tangent.xyz, 0.0));

			vec3 bitangent = cross(skinnedNormal, skinnedTangent) * tangent.w;
			vec3 T = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(skinnedTangent, 0.0));
			vec3 B = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(bitangent, 0.0));
			vec3 N = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(skinnedNormal, 0.0));
			outTBN = mat3(T, B, N);

			gl_Position = camera.projection * camera.view * vec4(outPosition, 1.0);
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

		#define M_PI 3.1415926535897932384626433832795

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

		struct MaterialInfo {
			uint diffuseTextureIndex;
			uint normalTextureIndex;
			uint metalnessTextureIndex;
			uint roughnessTextureIndex;
			uint occlusionTextureIndex;
			uint emissiveTextureIndex;
			float emissiveFactor;
			float alphaCutoff;
		};

		struct LightInfo {
			vec3 position;
			vec3 direction;
			vec3 color;
			float intensity;
			vec2 cutoff;
		};

		layout(set = 0, binding = 4) restrict readonly buffer Materials {
			MaterialInfo info[];
		} materials;

		layout(set = 0, binding = 5) restrict readonly buffer Lights {
			uvec4 count;
			LightInfo info[];
		} lights;

		layout(set = 0, binding = 6) uniform sampler2D textures[];

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec2 uv;
		layout(location = 2) in flat uint materialID;
		layout(location = 3) in flat vec3 cameraPosition;
		layout(location = 4) in mat3 TBN;

		layout(location = 0) out vec4 outColor;

		void main() {
			const MaterialInfo material = materials.info[materialID];
			const vec4 diffuseSample = texture(textures[nonuniformEXT(material.diffuseTextureIndex)], uv);
			if (diffuseSample.a < material.alphaCutoff) {
				discard;
			}
			const vec3 normalSample = texture(textures[nonuniformEXT(material.normalTextureIndex)], uv).xyz;
			const float metalnessSample = texture(textures[nonuniformEXT(material.metalnessTextureIndex)], uv).b;
			const float roughnessSample = texture(textures[nonuniformEXT(material.roughnessTextureIndex)], uv).g;
			const float occlusionSample = texture(textures[nonuniformEXT(material.occlusionTextureIndex)], uv).r;
			const vec3 emissiveSample = texture(textures[nonuniformEXT(material.emissiveTextureIndex)], uv).rgb;

			const vec3 d = vec3(diffuseSample);
			const vec3 n = normalize(TBN * (normalSample * 2.0 - 1.0));
			const vec3 v = normalize(cameraPosition - position);

			vec3 color = vec3(0.0);

			uint lightIndex = 0;
			// Directional Lights
			for (uint i = 0; i < lights.count.x; i++) {
				const vec3 l = -lights.info[lightIndex].direction;
				color += shade(n, v, l, lights.info[lightIndex].color * lights.info[lightIndex].intensity, d, metalnessSample, roughnessSample);

				lightIndex++;
			}
			// Point Lights
			for (uint i = 0; i < lights.count.y; i++) {
				const vec3 l = normalize(lights.info[lightIndex].position - position);
				const float distance = length(lights.info[lightIndex].position - position);
				const float attenuation = 1.0 / (distance * distance);
				const vec3 radiance = (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * attenuation;
				color += shade(n, v, l, radiance, d, metalnessSample, roughnessSample);

				lightIndex++;
			}
			// Spot Lights
			for (uint i = 0; i < lights.count.z; i++) {
				const vec3 l = normalize(lights.info[lightIndex].position - position);
				const float theta = dot(l, -lights.info[lightIndex].direction);
				const float epsilon = cos(lights.info[lightIndex].cutoff.y) - cos(lights.info[lightIndex].cutoff.x);
				float intensity = clamp((theta - cos(lights.info[lightIndex].cutoff.x)) / epsilon, 0.0, 1.0);
				intensity = 1.0 - intensity;
				color += shade(n, v, l, (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * intensity, d * intensity, metalnessSample, roughnessSample);

				lightIndex++;
			}
			// Ambient Lights
			for (uint i = 0; i < lights.count.w; i++) {
				color += (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * d;

				lightIndex++;
			}

			color *= occlusionSample;
			color += emissiveSample * material.emissiveFactor;

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

	VkVertexInputBindingDescription vertexInputBindingDescription = {};
	vertexInputBindingDescription.binding = 0;
	vertexInputBindingDescription.stride = sizeof(Vertex);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexPositionInputAttributeDescription = {};
	vertexPositionInputAttributeDescription.location = 0;
	vertexPositionInputAttributeDescription.binding = 0;
	vertexPositionInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexPositionInputAttributeDescription.offset = 0;

	VkVertexInputAttributeDescription vertexNormalInputAttributeDescription = {};
	vertexNormalInputAttributeDescription.location = 1;
	vertexNormalInputAttributeDescription.binding = 0;
	vertexNormalInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexNormalInputAttributeDescription.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription vertexUVInputAttributeDescription = {};
	vertexUVInputAttributeDescription.location = 2;
	vertexUVInputAttributeDescription.binding = 0;
	vertexUVInputAttributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
	vertexUVInputAttributeDescription.offset = offsetof(Vertex, uv);

	VkVertexInputAttributeDescription vertexColorInputAttributeDescription = {};
	vertexColorInputAttributeDescription.location = 3;
	vertexColorInputAttributeDescription.binding = 0;
	vertexColorInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexColorInputAttributeDescription.offset = offsetof(Vertex, color);

	VkVertexInputAttributeDescription vertexTangentInputAttributeDescription = {};
	vertexTangentInputAttributeDescription.location = 4;
	vertexTangentInputAttributeDescription.binding = 0;
	vertexTangentInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexTangentInputAttributeDescription.offset = offsetof(Vertex, tangent);

	VkVertexInputAttributeDescription vertexJointsInputAttributeDescription = {};
	vertexJointsInputAttributeDescription.location = 5;
	vertexJointsInputAttributeDescription.binding = 0;
	vertexJointsInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_UINT;
	vertexJointsInputAttributeDescription.offset = offsetof(Vertex, joints);

	VkVertexInputAttributeDescription vertexWeightsInputAttributeDescription = {};
	vertexWeightsInputAttributeDescription.location = 6;
	vertexWeightsInputAttributeDescription.binding = 0;
	vertexWeightsInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexWeightsInputAttributeDescription.offset = offsetof(Vertex, weights);

	std::array<VkVertexInputAttributeDescription, 7> vertexInputAttributeDescriptions = { vertexPositionInputAttributeDescription, vertexNormalInputAttributeDescription, vertexUVInputAttributeDescription, vertexColorInputAttributeDescription, vertexTangentInputAttributeDescription, vertexJointsInputAttributeDescription, vertexWeightsInputAttributeDescription };
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();

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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
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

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(uint32_t);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_graphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createDescriptorSets() {
	// Create descriptor pool
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize objectsDescriptorPoolSize = {};
	objectsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize meshesDescriptorPoolSize = {};
	meshesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshesDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize jointTransformsDescriptorPoolSize = {};
	jointTransformsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	jointTransformsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize materialsDescriptorPoolSize = {};
	materialsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize texturesDescriptorPoolSize = {};
	texturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 7> descriptorPoolSizes = { cameraDescriptorPoolSize, objectsDescriptorPoolSize, meshesDescriptorPoolSize, jointTransformsDescriptorPoolSize, materialsDescriptorPoolSize, lightsDescriptorPoolSize, texturesDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	// Allocate descriptor sets
	m_descriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = m_cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(Math::mat4) * 2 + sizeof(Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 0;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(cameraDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo objectsDescriptorBufferInfo;
		objectsDescriptorBufferInfo.buffer = m_objectBuffers[i].handle;
		objectsDescriptorBufferInfo.offset = 0;
		objectsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet objectsDescriptorWriteDescriptorSet = {};
		objectsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		objectsDescriptorWriteDescriptorSet.pNext = nullptr;
		objectsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		objectsDescriptorWriteDescriptorSet.dstBinding = 1;
		objectsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		objectsDescriptorWriteDescriptorSet.descriptorCount = 1;
		objectsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		objectsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		objectsDescriptorWriteDescriptorSet.pBufferInfo = &objectsDescriptorBufferInfo;
		objectsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(objectsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo meshesDescriptorBufferInfo;
		meshesDescriptorBufferInfo.buffer = m_meshBuffer;
		meshesDescriptorBufferInfo.offset = 0;
		meshesDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet meshesDescriptorWriteDescriptorSet = {};
		meshesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		meshesDescriptorWriteDescriptorSet.pNext = nullptr;
		meshesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		meshesDescriptorWriteDescriptorSet.dstBinding = 2;
		meshesDescriptorWriteDescriptorSet.dstArrayElement = 0;
		meshesDescriptorWriteDescriptorSet.descriptorCount = 1;
		meshesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		meshesDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		meshesDescriptorWriteDescriptorSet.pBufferInfo = &meshesDescriptorBufferInfo;
		meshesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(meshesDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo jointTransformsDescriptorBufferInfo;
		jointTransformsDescriptorBufferInfo.buffer = m_jointTransformBuffers[i].handle;
		jointTransformsDescriptorBufferInfo.offset = 0;
		jointTransformsDescriptorBufferInfo.range = 4096 * sizeof(Math::mat4);

		VkWriteDescriptorSet jointTransformsDescriptorWriteDescriptorSet = {};
		jointTransformsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		jointTransformsDescriptorWriteDescriptorSet.pNext = nullptr;
		jointTransformsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		jointTransformsDescriptorWriteDescriptorSet.dstBinding = 3;
		jointTransformsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		jointTransformsDescriptorWriteDescriptorSet.descriptorCount = 1;
		jointTransformsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		jointTransformsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		jointTransformsDescriptorWriteDescriptorSet.pBufferInfo = &jointTransformsDescriptorBufferInfo;
		jointTransformsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(jointTransformsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo materialsDescriptorBufferInfo;
		materialsDescriptorBufferInfo.buffer = m_materialBuffers[i].handle;
		materialsDescriptorBufferInfo.offset = 0;
		materialsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet materialsDescriptorWriteDescriptorSet = {};
		materialsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialsDescriptorWriteDescriptorSet.pNext = nullptr;
		materialsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		materialsDescriptorWriteDescriptorSet.dstBinding = 4;
		materialsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		materialsDescriptorWriteDescriptorSet.descriptorCount = 1;
		materialsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materialsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		materialsDescriptorWriteDescriptorSet.pBufferInfo = &materialsDescriptorBufferInfo;
		materialsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(materialsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo lightsDescriptorBufferInfo;
		lightsDescriptorBufferInfo.buffer = m_lightBuffers[i].handle;
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 5;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(lightsDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	m_descriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> texturesDescriptorImageInfos(m_textures.size());
	for (size_t i = 0; i < m_textures.size(); i++) {
		texturesDescriptorImageInfos[i].sampler = m_textureSamplers[m_textures[i].samplerKey];
		texturesDescriptorImageInfos[i].imageView = m_textureImageViews[m_textures[i].imageID];
		texturesDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 6;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createParticleResources() {
	// Create particle buffer
	VkBufferCreateInfo particleBufferCreateInfo = {};
	particleBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	particleBufferCreateInfo.pNext = nullptr;
	particleBufferCreateInfo.flags = 0;
	particleBufferCreateInfo.size = m_maxParticlesNumber * sizeof(Particle);
	particleBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	particleBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	particleBufferCreateInfo.queueFamilyIndexCount = 1;
	particleBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo particleBufferAllocationInfo;

	VmaAllocationCreateInfo particleBufferAllocationCreateInfo = {};
	particleBufferAllocationCreateInfo.flags = 0;
	particleBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleBufferCreateInfo, &particleBufferAllocationCreateInfo, &m_particleBuffers[0].handle, &m_particleBuffers[0].allocation, &particleBufferAllocationInfo));
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleBufferCreateInfo, &particleBufferAllocationCreateInfo, &m_particleBuffers[1].handle, &m_particleBuffers[1].allocation, &particleBufferAllocationInfo));

	// Create staging buffers
	m_particleStagingBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo particleStagingBufferCreateInfo = {};
	particleStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	particleStagingBufferCreateInfo.pNext = nullptr;
	particleStagingBufferCreateInfo.flags = 0;
	particleStagingBufferCreateInfo.size = m_maxParticlesNumber * sizeof(Particle);
	particleStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	particleStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	particleStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	particleStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo particleStagingBufferAllocationInfo;

	VmaAllocationCreateInfo particleStagingBufferAllocationCreateInfo = {};
	particleStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	particleStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleStagingBufferCreateInfo, &particleStagingBufferAllocationCreateInfo, &m_particleStagingBuffers[i].handle, &m_particleStagingBuffers[i].allocation, &particleStagingBufferAllocationInfo));
		m_particleStagingBuffers[i].address = particleStagingBufferAllocationInfo.pMappedData;
	}

	m_particleBuffersNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_particleBuffersNeedUpdate[i] = false;
	}

	// Create draw indirect buffer
	VkBufferCreateInfo particleDrawIndirectBufferCreateInfo = {};
	particleDrawIndirectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	particleDrawIndirectBufferCreateInfo.pNext = nullptr;
	particleDrawIndirectBufferCreateInfo.flags = 0;
	particleDrawIndirectBufferCreateInfo.size = 4 * sizeof(uint32_t);
	particleDrawIndirectBufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	particleDrawIndirectBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	particleDrawIndirectBufferCreateInfo.queueFamilyIndexCount = 1;
	particleDrawIndirectBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo particleDrawIndirectBufferAllocationInfo;

	VmaAllocationCreateInfo particleDrawIndirectBufferAllocationCreateInfo = {};
	particleDrawIndirectBufferAllocationCreateInfo.flags = 0;
	particleDrawIndirectBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleDrawIndirectBufferCreateInfo, &particleDrawIndirectBufferAllocationCreateInfo, &m_particleDrawIndirectBuffer.handle, &m_particleDrawIndirectBuffer.allocation, &particleDrawIndirectBufferAllocationInfo));

	// Fill draw indirect buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo fillDrawIndirectBufferBeginInfo = {};
	fillDrawIndirectBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	fillDrawIndirectBufferBeginInfo.pNext = nullptr;
	fillDrawIndirectBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	fillDrawIndirectBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &fillDrawIndirectBufferBeginInfo));

	std::vector<uint32_t> drawIndirectData = { 1, 0, 0 };
	vkCmdUpdateBuffer(m_initializationCommandBuffer, m_particleDrawIndirectBuffer.handle, sizeof(uint32_t), 3 * sizeof(uint32_t), drawIndirectData.data());

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo fillDrawIndirectBufferSubmitInfo = {};
	fillDrawIndirectBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	fillDrawIndirectBufferSubmitInfo.pNext = nullptr;
	fillDrawIndirectBufferSubmitInfo.waitSemaphoreCount = 0;
	fillDrawIndirectBufferSubmitInfo.pWaitSemaphores = nullptr;
	fillDrawIndirectBufferSubmitInfo.pWaitDstStageMask = nullptr;
	fillDrawIndirectBufferSubmitInfo.commandBufferCount = 1;
	fillDrawIndirectBufferSubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	fillDrawIndirectBufferSubmitInfo.signalSemaphoreCount = 0;
	fillDrawIndirectBufferSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &fillDrawIndirectBufferSubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	// Create descriptor set layouts
	VkDescriptorSetLayoutBinding inParticleBufferDescriptorSetLayoutBinding = {};
	inParticleBufferDescriptorSetLayoutBinding.binding = 0;
	inParticleBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticleBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	inParticleBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inParticleBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outParticleBufferDescriptorSetLayoutBinding = {};
	outParticleBufferDescriptorSetLayoutBinding.binding = 1;
	outParticleBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticleBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	outParticleBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outParticleBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outDrawIndirectBufferDescriptorSetLayoutBinding = {};
	outDrawIndirectBufferDescriptorSetLayoutBinding.binding = 2;
	outDrawIndirectBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	outDrawIndirectBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outDrawIndirectBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> computeDescriptorSetLayoutBindings = { inParticleBufferDescriptorSetLayoutBinding, outParticleBufferDescriptorSetLayoutBinding, outDrawIndirectBufferDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo computeDescriptorSetLayoutCreateInfo = {};
	computeDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeDescriptorSetLayoutCreateInfo.pNext = nullptr;
	computeDescriptorSetLayoutCreateInfo.flags = 0;
	computeDescriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(computeDescriptorSetLayoutBindings.size());
	computeDescriptorSetLayoutCreateInfo.pBindings = computeDescriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &computeDescriptorSetLayoutCreateInfo, nullptr, &m_particleComputeDescriptorSetLayout));

	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo graphicsDescriptorSetLayoutCreateInfo = {};
	graphicsDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	graphicsDescriptorSetLayoutCreateInfo.pNext = nullptr;
	graphicsDescriptorSetLayoutCreateInfo.flags = 0;
	graphicsDescriptorSetLayoutCreateInfo.bindingCount = 1;
	graphicsDescriptorSetLayoutCreateInfo.pBindings = &cameraDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &graphicsDescriptorSetLayoutCreateInfo, nullptr, &m_particleGraphicsDescriptorSetLayout));

	// Create compute pipeline
	const std::string computeShaderCode = R"GLSL(
		#version 460

		layout(local_size_x = 64) in;

		struct Particle {
			vec3 position;
			float size;
			vec4 color;
			vec3 direction;
			float speed;
			float duration;
		};

		layout(set = 0, binding = 0) restrict readonly buffer InParticles {
			Particle particles[];
		} inParticles;

		layout(set = 0, binding = 1) restrict writeonly buffer OutParticles {
			Particle particles[];
		} outParticles;

		layout(set = 0, binding = 2) buffer OutDrawIndirect {
			uint vertexCount;
		} outDrawIndirect;

		layout(push_constant) uniform DeltaTime {
			float deltaTime;
		} dT;

		void main() {
			uint index = gl_GlobalInvocationID.x;

			Particle inParticle = inParticles.particles[index];

			float newDuration = inParticle.duration - dT.deltaTime;
			if (newDuration >= 0.0) {
				uint particleIndex = atomicAdd(outDrawIndirect.vertexCount, 1);

				outParticles.particles[particleIndex].position = inParticle.position + (inParticle.direction * inParticle.speed * dT.deltaTime);
				outParticles.particles[particleIndex].size = inParticle.size;
				outParticles.particles[particleIndex].color = inParticle.color;
				outParticles.particles[particleIndex].direction = inParticle.direction;
				outParticles.particles[particleIndex].speed = inParticle.speed;
				outParticles.particles[particleIndex].duration = newDuration;
			}
		}
	)GLSL";
	const std::vector<uint32_t> computeShaderSpv = compileShader(computeShaderCode, ShaderType::Compute);

	VkShaderModule computeShaderModule;
	VkShaderModuleCreateInfo computeShaderModuleCreateInfo = {};
	computeShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	computeShaderModuleCreateInfo.pNext = nullptr;
	computeShaderModuleCreateInfo.flags = 0;
	computeShaderModuleCreateInfo.codeSize = computeShaderSpv.size() * sizeof(uint32_t);
	computeShaderModuleCreateInfo.pCode = computeShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &computeShaderModuleCreateInfo, nullptr, &computeShaderModule));

	VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = {};
	computeShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageCreateInfo.pNext = nullptr;
	computeShaderStageCreateInfo.flags = 0;
	computeShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageCreateInfo.module = computeShaderModule;
	computeShaderStageCreateInfo.pName = "main";
	computeShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float);

	VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo = {};
	computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipelineLayoutCreateInfo.pNext = nullptr;
	computePipelineLayoutCreateInfo.flags = 0;
	computePipelineLayoutCreateInfo.setLayoutCount = 1;
	computePipelineLayoutCreateInfo.pSetLayouts = &m_particleComputeDescriptorSetLayout;
	computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &computePipelineLayoutCreateInfo, nullptr, &m_particleComputePipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.flags = 0;
	computePipelineCreateInfo.stage = computeShaderStageCreateInfo;
	computePipelineCreateInfo.layout = m_particleComputePipelineLayout;
	computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_particleComputePipeline));

	vkDestroyShaderModule(m_device, computeShaderModule, nullptr);

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
		} camera;

		layout(location = 0) in vec3 position;
		layout(location = 1) in float size;
		layout(location = 2) in vec4 color;

		layout(location = 0) out vec4 fragColor;

		void main() {
			gl_PointSize = size;
			gl_Position = camera.projection * camera.view * vec4(position, 1.0);
			fragColor = color;
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

		layout(location = 0) in vec4 fragColor;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = fragColor;
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

	VkVertexInputBindingDescription vertexInputBindingDescription = {};
	vertexInputBindingDescription.binding = 0;
	vertexInputBindingDescription.stride = sizeof(Particle);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription particlePositionInputAttributeDescription = {};
	particlePositionInputAttributeDescription.location = 0;
	particlePositionInputAttributeDescription.binding = 0;
	particlePositionInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	particlePositionInputAttributeDescription.offset = 0;

	VkVertexInputAttributeDescription particleSizeInputAttributeDescription = {};
	particleSizeInputAttributeDescription.location = 1;
	particleSizeInputAttributeDescription.binding = 0;
	particleSizeInputAttributeDescription.format = VK_FORMAT_R32_SFLOAT;
	particleSizeInputAttributeDescription.offset = offsetof(Particle, size);

	VkVertexInputAttributeDescription particleColorInputAttributeDescription = {};
	particleColorInputAttributeDescription.location = 2;
	particleColorInputAttributeDescription.binding = 0;
	particleColorInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	particleColorInputAttributeDescription.offset = offsetof(Particle, color);

	std::array<VkVertexInputAttributeDescription, 3> vertexInputAttributeDescriptions = { particlePositionInputAttributeDescription, particleSizeInputAttributeDescription, particleColorInputAttributeDescription };
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
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

	VkPipelineLayoutCreateInfo graphicsPipelineLayoutCreateInfo = {};
	graphicsPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	graphicsPipelineLayoutCreateInfo.pNext = nullptr;
	graphicsPipelineLayoutCreateInfo.flags = 0;
	graphicsPipelineLayoutCreateInfo.setLayoutCount = 1;
	graphicsPipelineLayoutCreateInfo.pSetLayouts = &m_particleGraphicsDescriptorSetLayout;
	graphicsPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	graphicsPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &graphicsPipelineLayoutCreateInfo, nullptr, &m_particleGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_particleGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_particleGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pools
	VkDescriptorPoolSize inParticleDescriptorPoolSize = {};
	inParticleDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticleDescriptorPoolSize.descriptorCount = 2;

	VkDescriptorPoolSize outParticleDescriptorPoolSize = {};
	outParticleDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticleDescriptorPoolSize.descriptorCount = 2;

	VkDescriptorPoolSize outDrawIndirectDescriptorPoolSize = {};
	outDrawIndirectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectDescriptorPoolSize.descriptorCount = 2;

	std::array<VkDescriptorPoolSize, 3> computeDescriptorPoolSizes = { inParticleDescriptorPoolSize, outParticleDescriptorPoolSize, outDrawIndirectDescriptorPoolSize };
	VkDescriptorPoolCreateInfo computeDescriptorPoolCreateInfo = {};
	computeDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	computeDescriptorPoolCreateInfo.pNext = nullptr;
	computeDescriptorPoolCreateInfo.flags = 0;
	computeDescriptorPoolCreateInfo.maxSets = 2;
	computeDescriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(computeDescriptorPoolSizes.size());
	computeDescriptorPoolCreateInfo.pPoolSizes = computeDescriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &computeDescriptorPoolCreateInfo, nullptr, &m_particleComputeDescriptorPool));
	
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolCreateInfo graphicsDescriptorPoolCreateInfo = {};
	graphicsDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	graphicsDescriptorPoolCreateInfo.pNext = nullptr;
	graphicsDescriptorPoolCreateInfo.flags = 0;
	graphicsDescriptorPoolCreateInfo.maxSets = m_framesInFlight;
	graphicsDescriptorPoolCreateInfo.poolSizeCount = 1;
	graphicsDescriptorPoolCreateInfo.pPoolSizes = &cameraDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &graphicsDescriptorPoolCreateInfo, nullptr, &m_particleGraphicsDescriptorPool));

	// Allocate descriptor sets
	for (size_t i = 0; i < m_particleComputeDescriptorSets.size(); i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_particleComputeDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_particleComputeDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_particleComputeDescriptorSets[i]));
	}

	m_particleGraphicsDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_particleGraphicsDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_particleGraphicsDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_particleGraphicsDescriptorSets[i]));
	}

	// Update descriptor sets
	VkDescriptorBufferInfo particle0DescriptorBufferInfo;
	particle0DescriptorBufferInfo.buffer = m_particleBuffers[0].handle;
	particle0DescriptorBufferInfo.offset = 0;
	particle0DescriptorBufferInfo.range = m_maxParticlesNumber * sizeof(Particle);

	VkWriteDescriptorSet inParticle0DescriptorWriteDescriptorSet = {};
	inParticle0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	inParticle0DescriptorWriteDescriptorSet.pNext = nullptr;
	inParticle0DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[0];
	inParticle0DescriptorWriteDescriptorSet.dstBinding = 0;
	inParticle0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	inParticle0DescriptorWriteDescriptorSet.descriptorCount = 1;
	inParticle0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticle0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	inParticle0DescriptorWriteDescriptorSet.pBufferInfo = &particle0DescriptorBufferInfo;
	inParticle0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outParticle0DescriptorWriteDescriptorSet = {};
	outParticle0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outParticle0DescriptorWriteDescriptorSet.pNext = nullptr;
	outParticle0DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[1];
	outParticle0DescriptorWriteDescriptorSet.dstBinding = 1;
	outParticle0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outParticle0DescriptorWriteDescriptorSet.descriptorCount = 1;
	outParticle0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticle0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outParticle0DescriptorWriteDescriptorSet.pBufferInfo = &particle0DescriptorBufferInfo;
	outParticle0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo particle1DescriptorBufferInfo;
	particle1DescriptorBufferInfo.buffer = m_particleBuffers[1].handle;
	particle1DescriptorBufferInfo.offset = 0;
	particle1DescriptorBufferInfo.range = m_maxParticlesNumber * sizeof(Particle);

	VkWriteDescriptorSet inParticle1DescriptorWriteDescriptorSet = {};
	inParticle1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	inParticle1DescriptorWriteDescriptorSet.pNext = nullptr;
	inParticle1DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[1];
	inParticle1DescriptorWriteDescriptorSet.dstBinding = 0;
	inParticle1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	inParticle1DescriptorWriteDescriptorSet.descriptorCount = 1;
	inParticle1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticle1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	inParticle1DescriptorWriteDescriptorSet.pBufferInfo = &particle1DescriptorBufferInfo;
	inParticle1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outParticle1DescriptorWriteDescriptorSet = {};
	outParticle1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outParticle1DescriptorWriteDescriptorSet.pNext = nullptr;
	outParticle1DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[0];
	outParticle1DescriptorWriteDescriptorSet.dstBinding = 1;
	outParticle1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outParticle1DescriptorWriteDescriptorSet.descriptorCount = 1;
	outParticle1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticle1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outParticle1DescriptorWriteDescriptorSet.pBufferInfo = &particle1DescriptorBufferInfo;
	outParticle1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo outDrawIndirectDescriptorBufferInfo;
	outDrawIndirectDescriptorBufferInfo.buffer = m_particleDrawIndirectBuffer.handle;
	outDrawIndirectDescriptorBufferInfo.offset = 0;
	outDrawIndirectDescriptorBufferInfo.range = sizeof(uint32_t);

	VkWriteDescriptorSet outDrawIndirect0DescriptorWriteDescriptorSet = {};
	outDrawIndirect0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outDrawIndirect0DescriptorWriteDescriptorSet.pNext = nullptr;
	outDrawIndirect0DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[0];
	outDrawIndirect0DescriptorWriteDescriptorSet.dstBinding = 2;
	outDrawIndirect0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outDrawIndirect0DescriptorWriteDescriptorSet.descriptorCount = 1;
	outDrawIndirect0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirect0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outDrawIndirect0DescriptorWriteDescriptorSet.pBufferInfo = &outDrawIndirectDescriptorBufferInfo;
	outDrawIndirect0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outDrawIndirect1DescriptorWriteDescriptorSet = {};
	outDrawIndirect1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outDrawIndirect1DescriptorWriteDescriptorSet.pNext = nullptr;
	outDrawIndirect1DescriptorWriteDescriptorSet.dstSet = m_particleComputeDescriptorSets[1];
	outDrawIndirect1DescriptorWriteDescriptorSet.dstBinding = 2;
	outDrawIndirect1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outDrawIndirect1DescriptorWriteDescriptorSet.descriptorCount = 1;
	outDrawIndirect1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirect1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outDrawIndirect1DescriptorWriteDescriptorSet.pBufferInfo = &outDrawIndirectDescriptorBufferInfo;
	outDrawIndirect1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	std::array<VkWriteDescriptorSet, 6> computeWriteDescriptorSets = { inParticle0DescriptorWriteDescriptorSet, outParticle0DescriptorWriteDescriptorSet, inParticle1DescriptorWriteDescriptorSet, outParticle1DescriptorWriteDescriptorSet, outDrawIndirect0DescriptorWriteDescriptorSet, outDrawIndirect1DescriptorWriteDescriptorSet };
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = m_cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(Math::mat4) * 2;

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_particleGraphicsDescriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 0;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &cameraDescriptorWriteDescriptorSet, 0, nullptr);
	}
}

void NtshEngn::GraphicsModule::createToneMappingResources() {
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

	// Update descriptor set
	VkDescriptorImageInfo colorImageDescriptorImageInfo;
	colorImageDescriptorImageInfo.sampler = m_toneMappingSampler;
	colorImageDescriptorImageInfo.imageView = m_colorImageView;
	colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
	colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
	colorImageDescriptorWriteDescriptorSet.dstSet = m_toneMappingDescriptorSet;
	colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
	colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
	colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
	colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &colorImageDescriptorWriteDescriptorSet, 0, nullptr);
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
		#extension GL_EXT_nonuniform_qualifier : enable

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

		// Destroy depth image and image view
		vkDestroyImageView(m_device, m_depthImageView, nullptr);
		vmaDestroyImage(m_allocator, m_depthImage, m_depthImageAllocation);

		// Destroy color image and image view
		vkDestroyImageView(m_device, m_colorImageView, nullptr);
		vmaDestroyImage(m_allocator, m_colorImage, m_colorImageAllocation);

		// Recreate color and depth images
		createColorAndDepthImages();

		// Update tone mapping descriptor set
		VkDescriptorImageInfo colorImageDescriptorImageInfo;
		colorImageDescriptorImageInfo.sampler = m_toneMappingSampler;
		colorImageDescriptorImageInfo.imageView = m_colorImageView;
		colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
		colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
		colorImageDescriptorWriteDescriptorSet.dstSet = m_toneMappingDescriptorSet;
		colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
		colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
		colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &colorImageDescriptorWriteDescriptorSet, 0, nullptr);
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

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}