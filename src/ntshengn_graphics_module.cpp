#include "ntshengn_graphics_module.h"
#include "../external/Module/utils/ntshengn_dynamic_library.h"
#include "../external/Common/module_interfaces/ntshengn_window_module_interface.h"
#include <limits>
#include <array>

void NtshEngn::GraphicsModule::init() {
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		m_framesInFlight = 2;
	}
	else {
		m_framesInFlight = 1;
	}

	// Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = m_name.c_str();
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
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		instanceExtensions.push_back("VK_KHR_surface");
		instanceExtensions.push_back("VK_KHR_get_surface_capabilities2");
		instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
#if defined(NTSHENGN_OS_WINDOWS)
		instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(NTSHENGN_OS_LINUX)
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
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
#if defined(NTSHENGN_OS_WINDOWS)
		HWND windowHandle = m_windowModule->getNativeHandle(NTSHENGN_MAIN_WINDOW);
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(windowHandle, GWLP_HINSTANCE));
		surfaceCreateInfo.hwnd = windowHandle;
		auto createWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateWin32SurfaceKHR");
		NTSHENGN_VK_CHECK(createWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#elif defined(NTSHENGN_OS_LINUX)
		m_display = XOpenDisplay(NULL);
		Window windowHandle = m_windowModule->getNativeHandle(NTSHENGN_MAIN_WINDOW);
		VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.dpy = m_display;
		surfaceCreateInfo.window = windowHandle;
		auto createXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR");
		NTSHENGN_VK_CHECK(createXlibSurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#endif
	}

	// Pick a physical device
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		NTSHENGN_MODULE_ERROR("Vulkan: Found no suitable GPU.", NTSHENGN_RESULT_UNKNOWN_ERROR);
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
	m_graphicsQueueFamilyIndex = 0;
	for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProperties) {
		if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
				VkBool32 presentSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueFamilyIndex, m_surface, &presentSupport);
				if (presentSupport) {
					break;
				}
			}
			else {
				break;
			}
		}
		m_graphicsQueueFamilyIndex++;
	}

	// Create a queue supporting graphics
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
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
		"VK_KHR_maintenance3",
		"VK_EXT_descriptor_indexing" };
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		deviceExtensions.push_back("VK_KHR_swapchain");
	}
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
	NTSHENGN_VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

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
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		createSwapchain(VK_NULL_HANDLE);
	}
	// Or create an image to draw on
	else {
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
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
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

	createVertexAndIndexBuffers();

	createDepthImage();

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

	VkDescriptorSetLayoutBinding materialsDescriptorSetLayoutBinding = {};
	materialsDescriptorSetLayoutBinding.binding = 2;
	materialsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorSetLayoutBinding.descriptorCount = 1;
	materialsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 3;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = {};
	texturesDescriptorSetLayoutBinding.binding = 4;
	texturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	texturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 5> descriptorBindingFlags = { 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 5> descriptorSetLayoutBindings = { cameraDescriptorSetLayoutBinding, objectsDescriptorSetLayoutBinding, materialsDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, texturesDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));

	createGraphicsPipeline();

	// Create camera uniform buffer
	m_cameraBuffers.resize(m_framesInFlight);
	m_cameraBufferAllocations.resize(m_framesInFlight);
	VkBufferCreateInfo cameraBufferCreateInfo = {};
	cameraBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cameraBufferCreateInfo.pNext = nullptr;
	cameraBufferCreateInfo.flags = 0;
	cameraBufferCreateInfo.size = sizeof(nml::mat4) * 2 + sizeof(nml::vec4);
	cameraBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cameraBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	cameraBufferCreateInfo.queueFamilyIndexCount = 1;
	cameraBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &cameraBufferCreateInfo, &bufferAllocationCreateInfo, &m_cameraBuffers[i], &m_cameraBufferAllocations[i], nullptr));
	}

	// Create object storage buffer
	m_objectBuffers.resize(m_framesInFlight);
	m_objectBufferAllocations.resize(m_framesInFlight);
	VkBufferCreateInfo objectBufferCreateInfo = {};
	objectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	objectBufferCreateInfo.pNext = nullptr;
	objectBufferCreateInfo.flags = 0;
	objectBufferCreateInfo.size = 32768;
	objectBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	objectBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	objectBufferCreateInfo.queueFamilyIndexCount = 1;
	objectBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &objectBufferCreateInfo, &bufferAllocationCreateInfo, &m_objectBuffers[i], &m_objectBufferAllocations[i], nullptr));
	}

	// Create material storage buffer
	m_materialBuffers.resize(m_framesInFlight);
	m_materialBufferAllocations.resize(m_framesInFlight);
	VkBufferCreateInfo materialBufferCreateInfo = {};
	materialBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	materialBufferCreateInfo.pNext = nullptr;
	materialBufferCreateInfo.flags = 0;
	materialBufferCreateInfo.size = 32768;
	materialBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	materialBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	materialBufferCreateInfo.queueFamilyIndexCount = 1;
	materialBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &materialBufferCreateInfo, &bufferAllocationCreateInfo, &m_materialBuffers[i], &m_materialBufferAllocations[i], nullptr));
	}

	// Create light storage buffer
	m_lightBuffers.resize(m_framesInFlight);
	m_lightBufferAllocations.resize(m_framesInFlight);
	VkBufferCreateInfo lightBufferCreateInfo = {};
	lightBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	lightBufferCreateInfo.pNext = nullptr;
	lightBufferCreateInfo.flags = 0;
	lightBufferCreateInfo.size = 32768;
	lightBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	lightBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lightBufferCreateInfo.queueFamilyIndexCount = 1;
	lightBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &lightBufferCreateInfo, &bufferAllocationCreateInfo, &m_lightBuffers[i], &m_lightBufferAllocations[i], nullptr));
	}

	createDescriptorSets();

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
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

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
	NTSHENGN_UNUSED(dt);

	if (m_windowModule && !m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		// Do not update if the main window got closed
		return;
	}

	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_fences[m_currentFrameInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max()));

	uint32_t imageIndex = m_imageCount - 1;
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[m_currentFrameInFlight], VK_NULL_HANDLE, &imageIndex);
		if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
			resize();
		}
		else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
			NTSHENGN_MODULE_ERROR("Next swapchain image acquire failed.", NTSHENGN_RESULT_MODULE_ERROR);
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
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptySignalSubmitInfo, VK_NULL_HANDLE));
	}

	void* data;

	// Update camera buffer
	if (m_mainCamera != std::numeric_limits<uint32_t>::max()) {
		Camera camera = m_ecs->getComponent<Camera>(m_mainCamera);
		Transform cameraTransform = m_ecs->getComponent<Transform>(m_mainCamera);
		nml::vec3 cameraPosition = nml::vec3(cameraTransform.position[0], cameraTransform.position[1], cameraTransform.position[2]);
		nml::vec3 cameraRotation = nml::vec3(cameraTransform.rotation[0], cameraTransform.rotation[1], cameraTransform.rotation[2]);

		nml::mat4 cameraView = nml::lookAtRH(cameraPosition, cameraPosition + cameraRotation, nml::vec3(0.0f, 1.0f, 0.0));
		nml::mat4 cameraProjection = nml::perspectiveRH(camera.fov * toRad, m_viewport.width / m_viewport.height, camera.nearPlane, camera.farPlane);
		cameraProjection[1][1] *= -1.0f;
		std::array<nml::mat4, 2> cameraMatrices{ cameraView, cameraProjection };
		nml::vec4 cameraPositionAsVec4 = { cameraPosition, 0.0f };

		NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_cameraBufferAllocations[m_currentFrameInFlight], &data));
		memcpy(data, cameraMatrices.data(), sizeof(nml::mat4) * 2);
		memcpy(reinterpret_cast<char*>(data) + sizeof(nml::mat4) * 2, cameraPositionAsVec4.data(), sizeof(nml::vec4));
		vmaUnmapMemory(m_allocator, m_cameraBufferAllocations[m_currentFrameInFlight]);
	}

	// Update objects buffer
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_objectBufferAllocations[m_currentFrameInFlight], &data));
	for (auto& it : m_objects) {
		Transform objectTransform = m_ecs->getComponent<Transform>(it.first);
		nml::vec3 objectPosition = nml::vec3(objectTransform.position[0], objectTransform.position[1], objectTransform.position[2]);
		nml::vec3 objectRotation = nml::vec3(objectTransform.rotation[0], objectTransform.rotation[1], objectTransform.rotation[2]);
		nml::vec3 objectScale = nml::vec3(objectTransform.scale[0], objectTransform.scale[1], objectTransform.scale[2]);

		nml::mat4 objectModel = nml::translate(objectPosition) *
			nml::rotate(objectRotation.x, nml::vec3(1.0f, 0.0f, 0.0f)) *
			nml::rotate(objectRotation.y, nml::vec3(0.0f, 1.0f, 0.0f)) *
			nml::rotate(objectRotation.z, nml::vec3(0.0f, 0.0f, 1.0f)) *
			nml::scale(objectScale);

		size_t offset = (it.second.index * (sizeof(nml::mat4) + sizeof(nml::vec4))); // vec4 is used here for padding

		memcpy(reinterpret_cast<char*>(data) + offset, objectModel.data(), sizeof(nml::mat4));
		const uint32_t textureID = (it.second.materialIndex < m_materials.size()) ? it.second.materialIndex : 0;
		memcpy(reinterpret_cast<char*>(data) + offset + sizeof(nml::mat4), &textureID, sizeof(uint32_t));
	}
	vmaUnmapMemory(m_allocator, m_objectBufferAllocations[m_currentFrameInFlight]);

	// Update material buffer
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_materialBufferAllocations[m_currentFrameInFlight], &data));
	for (size_t i = 0; i < m_materials.size(); i++) {
		size_t offset = i * sizeof(InternalMaterial);

		memcpy(reinterpret_cast<char*>(data) + offset, &m_materials[i], sizeof(InternalMaterial));
	}
	vmaUnmapMemory(m_allocator, m_materialBufferAllocations[m_currentFrameInFlight]);

	// Update lights buffer
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_lightBufferAllocations[m_currentFrameInFlight], &data));
	std::array<uint32_t, 4> lightsCount = { static_cast<uint32_t>(m_lights.directionalLights.size()), static_cast<uint32_t>(m_lights.pointLights.size()), static_cast<uint32_t>(m_lights.spotLights.size()), 0 };
	memcpy(data, lightsCount.data(), 4 * sizeof(uint32_t));

	size_t offset = sizeof(nml::vec4);
	for (Entity light : m_lights.directionalLights) {
		const Light& lightLight = m_ecs->getComponent<Light>(light);
		const Transform& lightTransform = m_ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.direction = nml::vec4(lightTransform.rotation.data(), 0.0f);
		internalLight.color = nml::vec4(lightLight.color.data(), 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.pointLights) {
		const Light& lightLight = m_ecs->getComponent<Light>(light);
		const Transform& lightTransform = m_ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.position = nml::vec4(lightTransform.position.data(), 0.0f);
		internalLight.color = nml::vec4(lightLight.color.data(), 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.spotLights) {
		const Light& lightLight = m_ecs->getComponent<Light>(light);
		const Transform& lightTransform = m_ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.position = nml::vec4(lightTransform.position.data(), 0.0f);
		internalLight.direction = nml::vec4(lightTransform.rotation.data(), 0.0f);
		internalLight.color = nml::vec4(lightLight.color.data(), 0.0f);
		internalLight.cutoffs = nml::vec4(lightTransform.scale.data(), 0.0f, 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	vmaUnmapMemory(m_allocator, m_lightBufferAllocations[m_currentFrameInFlight]);

	// Update descriptor set if needed
	if (m_descriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateDescriptorSet(m_currentFrameInFlight);

		m_descriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
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
	VkImageMemoryBarrier2 undefinedToColorAttachmentOptimalImageMemoryBarrier = {};
	undefinedToColorAttachmentOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcAccessMask = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_swapchainImages[imageIndex];
	}
	else {
		undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_drawImage;
	}
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToColorAttachmentOptimalDependencyInfo = {};
	undefinedToColorAttachmentOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToColorAttachmentOptimalDependencyInfo.pNext = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToColorAttachmentOptimalDependencyInfo.memoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToColorAttachmentOptimalDependencyInfo.pImageMemoryBarriers = &undefinedToColorAttachmentOptimalImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &undefinedToColorAttachmentOptimalDependencyInfo);

	// Bind vertex and index buffers
	VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindVertexBuffers(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_vertexBuffer, &vertexBufferOffset);
	vkCmdBindIndexBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameInFlight], 0, nullptr);

	// Begin rendering
	VkRenderingAttachmentInfo renderingSwapchainAttachmentInfo = {};
	renderingSwapchainAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingSwapchainAttachmentInfo.pNext = nullptr;
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		renderingSwapchainAttachmentInfo.imageView = m_swapchainImageViews[imageIndex];
	}
	else {
		renderingSwapchainAttachmentInfo.imageView = m_drawImageView;
	}
	renderingSwapchainAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingSwapchainAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingSwapchainAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingSwapchainAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingSwapchainAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingSwapchainAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };
	renderingSwapchainAttachmentInfo.clearValue.depthStencil = { 0.0f, 0 };

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
	renderingDepthAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };
	renderingDepthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.pNext = nullptr;
	renderingInfo.flags = 0;
	renderingInfo.renderArea = m_scissor;
	renderingInfo.layerCount = 1;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
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
		vkCmdDrawIndexed(m_renderingCommandBuffers[m_currentFrameInFlight], m_meshes[it.second.meshIndex].indexCount, 1, m_meshes[it.second.meshIndex].firstIndex, m_meshes[it.second.meshIndex].vertexOffset, 0);
	}

	// End rendering
	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		VkImageMemoryBarrier2 colorAttachmentOptimalToPresentSrcImageMemoryBarrier = {};
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.pNext = nullptr;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstAccessMask = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.image = m_swapchainImages[imageIndex];
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.levelCount = 1;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo colorAttachmentOptimalToPresentSrcDependencyInfo = {};
		colorAttachmentOptimalToPresentSrcDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pNext = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		colorAttachmentOptimalToPresentSrcDependencyInfo.memoryBarrierCount = 0;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pMemoryBarriers = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.bufferMemoryBarrierCount = 0;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pBufferMemoryBarriers = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.imageMemoryBarrierCount = 1;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pImageMemoryBarriers = &colorAttachmentOptimalToPresentSrcImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentOptimalToPresentSrcDependencyInfo);
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
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fences[m_currentFrameInFlight]));

	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;
		VkResult queuePresentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
		if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR) {
			resize();
		}
		else if (queuePresentResult != VK_SUCCESS) {
			NTSHENGN_MODULE_ERROR("Queue present swapchain image failed.", NTSHENGN_RESULT_MODULE_ERROR);
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
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptyWaitSubmitInfo, VK_NULL_HANDLE));
	}

	m_currentFrameInFlight = (m_currentFrameInFlight + 1) % m_framesInFlight;
}

void NtshEngn::GraphicsModule::destroy() {
	NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

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

	// Destroy lights buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_lightBuffers[i], m_lightBufferAllocations[i]);
	}

	// Destroy materials buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_materialBuffers[i], m_materialBufferAllocations[i]);
	}

	// Destroy objects buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_objectBuffers[i], m_objectBufferAllocations[i]);
	}

	// Destroy camera buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_cameraBuffers[i], m_cameraBufferAllocations[i]);
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

	// Destroy samplers
	for (size_t i = 0; i < m_textureSamplers.size(); i++) {
		vkDestroySampler(m_device, m_textureSamplers[i], nullptr);
	}

	// Destroy textures
	for (size_t i = 0; i < m_textureImages.size(); i++) {
		vkDestroyImageView(m_device, m_textureImageViews[i], nullptr);
		vmaDestroyImage(m_allocator, m_textureImages[i], m_textureImageAllocations[i]);
	}

	// Destroy vertex and index buffers
	vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexBufferAllocation);

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

#if defined(NTSHENGN_OS_LINUX)
	// Close X display
	XCloseDisplay(m_display);
#endif

#if defined(NTSHENGN_DEBUG)
	// Destroy debug messenger
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

	// Destroy instance
	vkDestroyInstance(m_instance, nullptr);
}

NtshEngn::MeshId NtshEngn::GraphicsModule::load(const NtshEngn::Mesh& mesh) {
	if (m_meshAddresses.find(&mesh) != m_meshAddresses.end()) {
		return m_meshAddresses[&mesh];
	}

	m_meshes.push_back({ static_cast<uint32_t>(mesh.indices.size()), m_currentIndexOffset, m_currentVertexOffset });
	m_meshAddresses[&mesh] = static_cast<uint32_t>(m_meshes.size() - 1);

	// Vertex and Index staging buffer
	VkBuffer vertexAndIndexStagingBuffer;
	VmaAllocation vertexAndIndexStagingBufferAllocation;

	VkBufferCreateInfo vertexAndIndexStagingBufferCreateInfo = {};
	vertexAndIndexStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexAndIndexStagingBufferCreateInfo.pNext = nullptr;
	vertexAndIndexStagingBufferCreateInfo.flags = 0;
	vertexAndIndexStagingBufferCreateInfo.size = (mesh.vertices.size() * sizeof(NtshEngn::Vertex)) + (mesh.indices.size() * sizeof(uint32_t));
	vertexAndIndexStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vertexAndIndexStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexAndIndexStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexAndIndexStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo vertexAndIndexStagingBufferAllocationCreateInfo = {};
	vertexAndIndexStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	vertexAndIndexStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexStagingBufferCreateInfo, &vertexAndIndexStagingBufferAllocationCreateInfo, &vertexAndIndexStagingBuffer, &vertexAndIndexStagingBufferAllocation, nullptr));

	void* data;

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, vertexAndIndexStagingBufferAllocation, &data));
	memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(NtshEngn::Vertex));
	memcpy(reinterpret_cast<char*>(data) + (mesh.vertices.size() * sizeof(NtshEngn::Vertex)), mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(m_allocator, vertexAndIndexStagingBufferAllocation);

	// Copy staging buffer
	VkCommandPool buffersCopyCommandPool;

	VkCommandPoolCreateInfo buffersCopyCommandPoolCreateInfo = {};
	buffersCopyCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	buffersCopyCommandPoolCreateInfo.pNext = nullptr;
	buffersCopyCommandPoolCreateInfo.flags = 0;
	buffersCopyCommandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &buffersCopyCommandPoolCreateInfo, nullptr, &buffersCopyCommandPool));

	VkCommandBuffer buffersCopyCommandBuffer;

	VkCommandBufferAllocateInfo buffersCopyCommandBufferAllocateInfo = {};
	buffersCopyCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	buffersCopyCommandBufferAllocateInfo.pNext = nullptr;
	buffersCopyCommandBufferAllocateInfo.commandPool = buffersCopyCommandPool;
	buffersCopyCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	buffersCopyCommandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &buffersCopyCommandBufferAllocateInfo, &buffersCopyCommandBuffer));

	VkCommandBufferBeginInfo vertexAndIndexBuffersCopyBeginInfo = {};
	vertexAndIndexBuffersCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vertexAndIndexBuffersCopyBeginInfo.pNext = nullptr;
	vertexAndIndexBuffersCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vertexAndIndexBuffersCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(buffersCopyCommandBuffer, &vertexAndIndexBuffersCopyBeginInfo));

	VkBufferCopy vertexBufferCopy = {};
	vertexBufferCopy.srcOffset = 0;
	vertexBufferCopy.dstOffset = m_currentVertexOffset * sizeof(NtshEngn::Vertex);
	vertexBufferCopy.size = mesh.vertices.size() * sizeof(NtshEngn::Vertex);
	vkCmdCopyBuffer(buffersCopyCommandBuffer, vertexAndIndexStagingBuffer, m_vertexBuffer, 1, &vertexBufferCopy);

	VkBufferCopy indexBufferCopy = {};
	indexBufferCopy.srcOffset = mesh.vertices.size() * sizeof(NtshEngn::Vertex);
	indexBufferCopy.dstOffset = m_currentIndexOffset * sizeof(uint32_t);
	indexBufferCopy.size = mesh.indices.size() * sizeof(uint32_t);
	vkCmdCopyBuffer(buffersCopyCommandBuffer, vertexAndIndexStagingBuffer, m_indexBuffer, 1, &indexBufferCopy);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(buffersCopyCommandBuffer));

	VkFence buffersCopyFence;

	VkFenceCreateInfo buffersCopyFenceCreateInfo = {};
	buffersCopyFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	buffersCopyFenceCreateInfo.pNext = nullptr;
	buffersCopyFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &buffersCopyFenceCreateInfo, nullptr, &buffersCopyFence));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &buffersCopyCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &buffersCopySubmitInfo, buffersCopyFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &buffersCopyFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));

	vkDestroyFence(m_device, buffersCopyFence, nullptr);
	vkDestroyCommandPool(m_device, buffersCopyCommandPool, nullptr);
	vmaDestroyBuffer(m_allocator, vertexAndIndexStagingBuffer, vertexAndIndexStagingBufferAllocation);

	m_currentVertexOffset += static_cast<int32_t>(mesh.vertices.size());
	m_currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());

	return static_cast<uint32_t>(m_meshes.size() - 1);
}

NtshEngn::ImageId NtshEngn::GraphicsModule::load(const NtshEngn::Image& image) {
	if (m_imageAddresses.find(&image) != m_imageAddresses.end()) {
		return m_imageAddresses[&image];
	}

	m_imageAddresses[&image] = static_cast<uint32_t>(m_textureImages.size());

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	size_t numComponents = 4;
	size_t sizeComponent = 1;

	if (image.colorSpace == NtshEngn::ImageColorSpace::SRGB) {
		switch (image.format) {
		case NtshEngn::ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_SRGB;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_SRGB;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_SRGB;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_SFLOAT;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_SFLOAT;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_SFLOAT;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", NtshEngn::Result::ModuleError);
		}
	}
	else if (image.colorSpace == NtshEngn::ImageColorSpace::Linear) {
		switch (image.format) {
		case NtshEngn::ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_UNORM;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_UNORM;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_UNORM;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case NtshEngn::ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_UNORM;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_UNORM;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_UNORM;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_UNORM;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case NtshEngn::ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case NtshEngn::ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", NtshEngn::Result::ModuleError);
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
	textureImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo cubeTextureImageAllocationCreateInfo = {};
	cubeTextureImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &textureImageCreateInfo, &cubeTextureImageAllocationCreateInfo, &textureImage, &textureImageAllocation, nullptr));

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
	textureStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo textureStagingBufferAllocationCreateInfo = {};
	textureStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	textureStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &textureStagingBufferCreateInfo, &textureStagingBufferAllocationCreateInfo, &textureStagingBuffer, &textureStagingBufferAllocation, nullptr));

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, textureStagingBufferAllocation, &data));
	memcpy(data, image.data.data(), static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * numComponents * sizeComponent);
	vmaUnmapMemory(m_allocator, textureStagingBufferAllocation);

	// Copy staging buffer
	VkCommandPool commandPool;

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer));

	VkCommandBufferBeginInfo textureCopyBeginInfo = {};
	textureCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	textureCopyBeginInfo.pNext = nullptr;
	textureCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	textureCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &textureCopyBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstOptimalImageMemoryBarrier = {};
	undefinedToTransferDstOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstOptimalImageMemoryBarrier.srcAccessMask = 0;
	undefinedToTransferDstOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	undefinedToTransferDstOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstOptimalImageMemoryBarrier.image = textureImage;
	undefinedToTransferDstOptimalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstOptimalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstOptimalImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	undefinedToTransferDstOptimalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstOptimalImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstOptimalDependencyInfo = {};
	undefinedToTransferDstOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstOptimalDependencyInfo.pNext = nullptr;
	undefinedToTransferDstOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToTransferDstOptimalDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstOptimalDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstOptimalDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstOptimalDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstOptimalImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &undefinedToTransferDstOptimalDependencyInfo);

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
	vkCmdCopyBufferToImage(commandBuffer, textureStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureBufferCopy);

	VkImageMemoryBarrier2 mipMapGenerationImageMemoryBarrier = {};
	mipMapGenerationImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	mipMapGenerationImageMemoryBarrier.pNext = nullptr;
	mipMapGenerationImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	mipMapGenerationImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	mipMapGenerationImageMemoryBarrier.image = textureImage;
	mipMapGenerationImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	mipMapGenerationImageMemoryBarrier.subresourceRange.levelCount = 1;
	mipMapGenerationImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	mipMapGenerationImageMemoryBarrier.subresourceRange.layerCount = 1;

	uint32_t mipWidth = image.width;
	uint32_t mipHeight = image.height;
	for (size_t i = 1; i < textureImageCreateInfo.mipLevels; i++) {
		mipMapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		mipMapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		mipMapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		mipMapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipMapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipMapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipMapGenerationImageMemoryBarrier.subresourceRange.baseMipLevel = static_cast<uint32_t>(i) - 1;

		VkDependencyInfo mipMapGenerationDependencyInfo = {};
		mipMapGenerationDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		mipMapGenerationDependencyInfo.pNext = nullptr;
		mipMapGenerationDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		mipMapGenerationDependencyInfo.memoryBarrierCount = 0;
		mipMapGenerationDependencyInfo.pMemoryBarriers = nullptr;
		mipMapGenerationDependencyInfo.bufferMemoryBarrierCount = 0;
		mipMapGenerationDependencyInfo.pBufferMemoryBarriers = nullptr;
		mipMapGenerationDependencyInfo.imageMemoryBarrierCount = 1;
		mipMapGenerationDependencyInfo.pImageMemoryBarriers = &mipMapGenerationImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(commandBuffer, &mipMapGenerationDependencyInfo);

		VkImageBlit imageBlit = {};
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.mipLevel = static_cast<uint32_t>(i) - 1;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcOffsets[0].x = 0;
		imageBlit.srcOffsets[0].y = 0;
		imageBlit.srcOffsets[0].z = 0;
		imageBlit.srcOffsets[1].x = mipWidth;
		imageBlit.srcOffsets[1].y = mipHeight;
		imageBlit.srcOffsets[1].z = 1;
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.mipLevel = static_cast<uint32_t>(i);
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstOffsets[0].x = 0;
		imageBlit.dstOffsets[0].y = 0;
		imageBlit.dstOffsets[0].z = 0;
		imageBlit.dstOffsets[1].x = mipWidth > 1 ? mipWidth / 2 : 1;
		imageBlit.dstOffsets[1].y = mipHeight > 1 ? mipHeight / 2 : 1;
		imageBlit.dstOffsets[1].z = 1;
		vkCmdBlitImage(commandBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

		mipMapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		mipMapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipMapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		mipMapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		mipMapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipMapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		mipMapGenerationDependencyInfo.pImageMemoryBarriers = &mipMapGenerationImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(commandBuffer, &mipMapGenerationDependencyInfo);

		mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
		mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
	}

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkFence buffersCopyFence;

	VkFenceCreateInfo buffersCopyFenceCreateInfo = {};
	buffersCopyFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	buffersCopyFenceCreateInfo.pNext = nullptr;
	buffersCopyFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &buffersCopyFenceCreateInfo, nullptr, &buffersCopyFence));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &commandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &buffersCopySubmitInfo, buffersCopyFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &buffersCopyFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));

	vkDestroyFence(m_device, buffersCopyFence, nullptr);
	vkDestroyCommandPool(m_device, commandPool, nullptr);
	vmaDestroyBuffer(m_allocator, textureStagingBuffer, textureStagingBufferAllocation);

	m_textureImages.push_back(textureImage);
	m_textureImageAllocations.push_back(textureImageAllocation);
	m_textureImageViews.push_back(textureImageView);

	// Mark descriptor sets for update
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = true;
	}

	return static_cast<uint32_t>(m_textureImages.size() - 1);
}

const NtshEngn::ComponentMask NtshEngn::GraphicsModule::getComponentMask() const {
	ComponentMask componentMask;
	componentMask.set(m_ecs->getComponentId<Renderable>());
	componentMask.set(m_ecs->getComponentId<Camera>());
	componentMask.set(m_ecs->getComponentId<Light>());

	return componentMask;
}

void NtshEngn::GraphicsModule::onEntityComponentAdded(Entity entity, Component componentID) {
	if (componentID == m_ecs->getComponentId<Renderable>()) {
		const Renderable& renderable = m_ecs->getComponent<Renderable>(entity);

		InternalObject object;
		object.index = attributeObjectIndex();
		if (renderable.mesh->vertices.size() != 0) {
			object.meshIndex = load(*renderable.mesh);
		}
		InternalMaterial material;
		if (renderable.material->diffuseTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->diffuseTexture.first));
			uint32_t samplerId = createSampler(renderable.material->diffuseTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.diffuseTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		if (renderable.material->normalTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->normalTexture.first));
			uint32_t samplerId = createSampler(renderable.material->normalTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.normalTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		if (renderable.material->metalnessTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->metalnessTexture.first));
			uint32_t samplerId = createSampler(renderable.material->metalnessTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.metalnessTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		if (renderable.material->roughnessTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->roughnessTexture.first));
			uint32_t samplerId = createSampler(renderable.material->roughnessTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.roughnessTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		if (renderable.material->occlusionTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->occlusionTexture.first));
			uint32_t samplerId = createSampler(renderable.material->occlusionTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.occlusionTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		if (renderable.material->emissiveTexture.first) {
			ImageId imageId = static_cast<uint32_t>(load(*renderable.material->emissiveTexture.first));
			uint32_t samplerId = createSampler(renderable.material->emissiveTexture.second);
			m_textures.push_back({ static_cast<uint32_t>(imageId), samplerId });
			material.emissiveTextureIndex = static_cast<uint32_t>(m_textures.size()) - 1;
		}
		m_materials.push_back(material);
		object.materialIndex = static_cast<uint32_t>(m_materials.size() - 1);
		m_objects[entity] = object;
	}
	else if (componentID == m_ecs->getComponentId<Camera>()) {
		if (m_mainCamera == std::numeric_limits<uint32_t>::max()) {
			m_mainCamera = entity;
		}
	}
	else if (componentID == m_ecs->getComponentId<Light>()) {
		const Light& light = m_ecs->getComponent<Light>(entity);

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

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.insert(entity);
			break;
		}
	}
}

void NtshEngn::GraphicsModule::onEntityComponentRemoved(Entity entity, Component componentID) {
	if (componentID == m_ecs->getComponentId<Renderable>()) {
		const InternalObject& object = m_objects[entity];
		retrieveObjectIndex(object.index);

		m_objects.erase(entity);
	}
	else if (componentID == m_ecs->getComponentId<Camera>()) {
		if (m_mainCamera == entity) {
			m_mainCamera = std::numeric_limits<uint32_t>::max();
		}
	}
	else if (componentID == m_ecs->getComponentId<Light>()) {
		const Light& light = m_ecs->getComponent<Light>(entity);

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
	m_swapchainFormat = surfaceFormats[0].format;
	VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
	for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			m_swapchainFormat = surfaceFormat.format;
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
	swapchainExtent.width = static_cast<uint32_t>(m_windowModule->getWidth(NTSHENGN_MAIN_WINDOW));
	swapchainExtent.height = static_cast<uint32_t>(m_windowModule->getHeight(NTSHENGN_MAIN_WINDOW));

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
	swapchainCreateInfo.imageFormat = m_swapchainFormat;
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
		swapchainImageViewCreateInfo.format = m_swapchainFormat;
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
	// Vertex and index buffers
	VkBufferCreateInfo vertexAndIndexBufferCreateInfo = {};
	vertexAndIndexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexAndIndexBufferCreateInfo.pNext = nullptr;
	vertexAndIndexBufferCreateInfo.flags = 0;
	vertexAndIndexBufferCreateInfo.size = 67108864;
	vertexAndIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vertexAndIndexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexAndIndexBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexAndIndexBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo vertexAndIndexBufferAllocationCreateInfo = {};
	vertexAndIndexBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_vertexBuffer, &m_vertexBufferAllocation, nullptr));

	vertexAndIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexBufferCreateInfo, &vertexAndIndexBufferAllocationCreateInfo, &m_indexBuffer, &m_indexBufferAllocation, nullptr));
}

void NtshEngn::GraphicsModule::createDepthImage() {
	VkImageCreateInfo depthImageCreateInfo = {};
	depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageCreateInfo.pNext = nullptr;
	depthImageCreateInfo.flags = 0;
	depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		depthImageCreateInfo.extent.width = static_cast<uint32_t>(m_windowModule->getWidth(NTSHENGN_MAIN_WINDOW));
		depthImageCreateInfo.extent.height = static_cast<uint32_t>(m_windowModule->getHeight(NTSHENGN_MAIN_WINDOW));
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
	depthImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo depthImageAllocationCreateInfo = {};
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

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	VkCommandPool depthImageTransitionCommandPool;

	VkCommandPoolCreateInfo depthImageTransitionCommandPoolCreateInfo = {};
	depthImageTransitionCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	depthImageTransitionCommandPoolCreateInfo.pNext = nullptr;
	depthImageTransitionCommandPoolCreateInfo.flags = 0;
	depthImageTransitionCommandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &depthImageTransitionCommandPoolCreateInfo, nullptr, &depthImageTransitionCommandPool));

	VkCommandBuffer depthImageTransitionCommandBuffer;

	VkCommandBufferAllocateInfo depthImageTransitionCommandBufferAllocateInfo = {};
	depthImageTransitionCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	depthImageTransitionCommandBufferAllocateInfo.pNext = nullptr;
	depthImageTransitionCommandBufferAllocateInfo.commandPool = depthImageTransitionCommandPool;
	depthImageTransitionCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	depthImageTransitionCommandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &depthImageTransitionCommandBufferAllocateInfo, &depthImageTransitionCommandBuffer));

	VkCommandBufferBeginInfo depthImageTransitionBeginInfo = {};
	depthImageTransitionBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	depthImageTransitionBeginInfo.pNext = nullptr;
	depthImageTransitionBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	depthImageTransitionBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(depthImageTransitionCommandBuffer, &depthImageTransitionBeginInfo));

	VkImageMemoryBarrier2 undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier = {};
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.srcAccessMask = 0;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.image = m_depthImage;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToDepthStencilAttachmentOptimalDependencyInfo = {};
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.pNext = nullptr;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.memoryBarrierCount = 0;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToDepthStencilAttachmentOptimalDependencyInfo.pImageMemoryBarriers = &undefinedToDepthStencilAttachmentOptimalImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(depthImageTransitionCommandBuffer, &undefinedToDepthStencilAttachmentOptimalDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(depthImageTransitionCommandBuffer));

	VkFence depthImageTransitionFence;

	VkFenceCreateInfo depthImageTransitionFenceCreateInfo = {};
	depthImageTransitionFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	depthImageTransitionFenceCreateInfo.pNext = nullptr;
	depthImageTransitionFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &depthImageTransitionFenceCreateInfo, nullptr, &depthImageTransitionFence));

	VkSubmitInfo depthImageTransitionSubmitInfo = {};
	depthImageTransitionSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	depthImageTransitionSubmitInfo.pNext = nullptr;
	depthImageTransitionSubmitInfo.waitSemaphoreCount = 0;
	depthImageTransitionSubmitInfo.pWaitSemaphores = nullptr;
	depthImageTransitionSubmitInfo.pWaitDstStageMask = nullptr;
	depthImageTransitionSubmitInfo.commandBufferCount = 1;
	depthImageTransitionSubmitInfo.pCommandBuffers = &depthImageTransitionCommandBuffer;
	depthImageTransitionSubmitInfo.signalSemaphoreCount = 0;
	depthImageTransitionSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &depthImageTransitionSubmitInfo, depthImageTransitionFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &depthImageTransitionFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));

	vkDestroyFence(m_device, depthImageTransitionFence, nullptr);
	vkDestroyCommandPool(m_device, depthImageTransitionCommandPool, nullptr);
}

void NtshEngn::GraphicsModule::createGraphicsPipeline() {
	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = VK_FORMAT_R8G8B8A8_SRGB;
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		pipelineRenderingColorFormat = m_swapchainFormat;
	}

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::vector<uint32_t> vertexShaderCode = { 0x07230203,0x00010000,0x0008000b,0x000000a3,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0010000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000001e,0x0000002c,
	0x0000002e,0x00000031,0x00000038,0x00000042,0x00000045,0x0000007f,0x00000094,0x000000a2,
	0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
	0x00000009,0x5074756f,0x7469736f,0x006e6f69,0x00050005,0x0000000d,0x656a624f,0x6e497463,
	0x00006f66,0x00050006,0x0000000d,0x00000000,0x65646f6d,0x0000006c,0x00060006,0x0000000d,
	0x00000001,0x6574616d,0x6c616972,0x00004449,0x00040005,0x0000000f,0x656a624f,0x00737463,
	0x00050006,0x0000000f,0x00000000,0x6f666e69,0x00000000,0x00040005,0x00000011,0x656a626f,
	0x00737463,0x00050005,0x00000014,0x656a624f,0x44497463,0x00000000,0x00060006,0x00000014,
	0x00000000,0x656a626f,0x44497463,0x00000000,0x00030005,0x00000016,0x0044496f,0x00050005,
	0x0000001e,0x69736f70,0x6e6f6974,0x00000000,0x00040005,0x0000002c,0x5574756f,0x00000056,
	0x00030005,0x0000002e,0x00007675,0x00060005,0x00000031,0x4d74756f,0x72657461,0x496c6169,
	0x00000044,0x00070005,0x00000038,0x4374756f,0x72656d61,0x736f5061,0x6f697469,0x0000006e,
	0x00040005,0x00000039,0x656d6143,0x00006172,0x00050006,0x00000039,0x00000000,0x77656976,
	0x00000000,0x00060006,0x00000039,0x00000001,0x6a6f7270,0x69746365,0x00006e6f,0x00060006,
	0x00000039,0x00000002,0x69736f70,0x6e6f6974,0x00000000,0x00040005,0x0000003b,0x656d6163,
	0x00006172,0x00050005,0x00000041,0x61746962,0x6e65676e,0x00000074,0x00040005,0x00000042,
	0x6d726f6e,0x00006c61,0x00040005,0x00000045,0x676e6174,0x00746e65,0x00030005,0x0000004e,
	0x00000054,0x00030005,0x0000005f,0x00000042,0x00030005,0x0000006e,0x0000004e,0x00040005,
	0x0000007f,0x5474756f,0x00004e42,0x00060005,0x00000092,0x505f6c67,0x65567265,0x78657472,
	0x00000000,0x00060006,0x00000092,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,
	0x00000092,0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x00000092,
	0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00070006,0x00000092,0x00000003,
	0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,0x00000094,0x00000000,0x00040005,
	0x000000a2,0x6f6c6f63,0x00000072,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040048,
	0x0000000d,0x00000000,0x00000005,0x00050048,0x0000000d,0x00000000,0x00000023,0x00000000,
	0x00050048,0x0000000d,0x00000000,0x00000007,0x00000010,0x00050048,0x0000000d,0x00000001,
	0x00000023,0x00000040,0x00040047,0x0000000e,0x00000006,0x00000050,0x00040048,0x0000000f,
	0x00000000,0x00000013,0x00040048,0x0000000f,0x00000000,0x00000018,0x00050048,0x0000000f,
	0x00000000,0x00000023,0x00000000,0x00030047,0x0000000f,0x00000003,0x00040047,0x00000011,
	0x00000022,0x00000000,0x00040047,0x00000011,0x00000021,0x00000001,0x00050048,0x00000014,
	0x00000000,0x00000023,0x00000000,0x00030047,0x00000014,0x00000002,0x00040047,0x0000001e,
	0x0000001e,0x00000000,0x00040047,0x0000002c,0x0000001e,0x00000001,0x00040047,0x0000002e,
	0x0000001e,0x00000002,0x00030047,0x00000031,0x0000000e,0x00040047,0x00000031,0x0000001e,
	0x00000002,0x00030047,0x00000038,0x0000000e,0x00040047,0x00000038,0x0000001e,0x00000003,
	0x00040048,0x00000039,0x00000000,0x00000005,0x00050048,0x00000039,0x00000000,0x00000023,
	0x00000000,0x00050048,0x00000039,0x00000000,0x00000007,0x00000010,0x00040048,0x00000039,
	0x00000001,0x00000005,0x00050048,0x00000039,0x00000001,0x00000023,0x00000040,0x00050048,
	0x00000039,0x00000001,0x00000007,0x00000010,0x00050048,0x00000039,0x00000002,0x00000023,
	0x00000080,0x00030047,0x00000039,0x00000002,0x00040047,0x0000003b,0x00000022,0x00000000,
	0x00040047,0x0000003b,0x00000021,0x00000000,0x00040047,0x00000042,0x0000001e,0x00000001,
	0x00040047,0x00000045,0x0000001e,0x00000004,0x00040047,0x0000007f,0x0000001e,0x00000004,
	0x00050048,0x00000092,0x00000000,0x0000000b,0x00000000,0x00050048,0x00000092,0x00000001,
	0x0000000b,0x00000001,0x00050048,0x00000092,0x00000002,0x0000000b,0x00000003,0x00050048,
	0x00000092,0x00000003,0x0000000b,0x00000004,0x00030047,0x00000092,0x00000002,0x00040047,
	0x000000a2,0x0000001e,0x00000003,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
	0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000003,0x00040020,
	0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,
	0x0000000a,0x00000006,0x00000004,0x00040018,0x0000000b,0x0000000a,0x00000004,0x00040015,
	0x0000000c,0x00000020,0x00000000,0x0004001e,0x0000000d,0x0000000b,0x0000000c,0x0003001d,
	0x0000000e,0x0000000d,0x0003001e,0x0000000f,0x0000000e,0x00040020,0x00000010,0x00000002,
	0x0000000f,0x0004003b,0x00000010,0x00000011,0x00000002,0x00040015,0x00000012,0x00000020,
	0x00000001,0x0004002b,0x00000012,0x00000013,0x00000000,0x0003001e,0x00000014,0x0000000c,
	0x00040020,0x00000015,0x00000009,0x00000014,0x0004003b,0x00000015,0x00000016,0x00000009,
	0x00040020,0x00000017,0x00000009,0x0000000c,0x00040020,0x0000001a,0x00000002,0x0000000b,
	0x00040020,0x0000001d,0x00000001,0x00000007,0x0004003b,0x0000001d,0x0000001e,0x00000001,
	0x0004002b,0x00000006,0x00000020,0x3f800000,0x00040017,0x0000002a,0x00000006,0x00000002,
	0x00040020,0x0000002b,0x00000003,0x0000002a,0x0004003b,0x0000002b,0x0000002c,0x00000003,
	0x00040020,0x0000002d,0x00000001,0x0000002a,0x0004003b,0x0000002d,0x0000002e,0x00000001,
	0x00040020,0x00000030,0x00000003,0x0000000c,0x0004003b,0x00000030,0x00000031,0x00000003,
	0x0004002b,0x00000012,0x00000034,0x00000001,0x00040020,0x00000035,0x00000002,0x0000000c,
	0x0004003b,0x00000008,0x00000038,0x00000003,0x0005001e,0x00000039,0x0000000b,0x0000000b,
	0x00000007,0x00040020,0x0000003a,0x00000002,0x00000039,0x0004003b,0x0000003a,0x0000003b,
	0x00000002,0x0004002b,0x00000012,0x0000003c,0x00000002,0x00040020,0x0000003d,0x00000002,
	0x00000007,0x00040020,0x00000040,0x00000007,0x00000007,0x0004003b,0x0000001d,0x00000042,
	0x00000001,0x00040020,0x00000044,0x00000001,0x0000000a,0x0004003b,0x00000044,0x00000045,
	0x00000001,0x0004002b,0x0000000c,0x00000049,0x00000003,0x00040020,0x0000004a,0x00000001,
	0x00000006,0x0004002b,0x00000006,0x00000055,0x00000000,0x00040018,0x0000007d,0x00000007,
	0x00000003,0x00040020,0x0000007e,0x00000003,0x0000007d,0x0004003b,0x0000007e,0x0000007f,
	0x00000003,0x0004002b,0x0000000c,0x00000090,0x00000001,0x0004001c,0x00000091,0x00000006,
	0x00000090,0x0006001e,0x00000092,0x0000000a,0x00000006,0x00000091,0x00000091,0x00040020,
	0x00000093,0x00000003,0x00000092,0x0004003b,0x00000093,0x00000094,0x00000003,0x00040020,
	0x000000a0,0x00000003,0x0000000a,0x0004003b,0x0000001d,0x000000a2,0x00000001,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000040,
	0x00000041,0x00000007,0x0004003b,0x00000040,0x0000004e,0x00000007,0x0004003b,0x00000040,
	0x0000005f,0x00000007,0x0004003b,0x00000040,0x0000006e,0x00000007,0x00050041,0x00000017,
	0x00000018,0x00000016,0x00000013,0x0004003d,0x0000000c,0x00000019,0x00000018,0x00070041,
	0x0000001a,0x0000001b,0x00000011,0x00000013,0x00000019,0x00000013,0x0004003d,0x0000000b,
	0x0000001c,0x0000001b,0x0004003d,0x00000007,0x0000001f,0x0000001e,0x00050051,0x00000006,
	0x00000021,0x0000001f,0x00000000,0x00050051,0x00000006,0x00000022,0x0000001f,0x00000001,
	0x00050051,0x00000006,0x00000023,0x0000001f,0x00000002,0x00070050,0x0000000a,0x00000024,
	0x00000021,0x00000022,0x00000023,0x00000020,0x00050091,0x0000000a,0x00000025,0x0000001c,
	0x00000024,0x00050051,0x00000006,0x00000026,0x00000025,0x00000000,0x00050051,0x00000006,
	0x00000027,0x00000025,0x00000001,0x00050051,0x00000006,0x00000028,0x00000025,0x00000002,
	0x00060050,0x00000007,0x00000029,0x00000026,0x00000027,0x00000028,0x0003003e,0x00000009,
	0x00000029,0x0004003d,0x0000002a,0x0000002f,0x0000002e,0x0003003e,0x0000002c,0x0000002f,
	0x00050041,0x00000017,0x00000032,0x00000016,0x00000013,0x0004003d,0x0000000c,0x00000033,
	0x00000032,0x00070041,0x00000035,0x00000036,0x00000011,0x00000013,0x00000033,0x00000034,
	0x0004003d,0x0000000c,0x00000037,0x00000036,0x0003003e,0x00000031,0x00000037,0x00050041,
	0x0000003d,0x0000003e,0x0000003b,0x0000003c,0x0004003d,0x00000007,0x0000003f,0x0000003e,
	0x0003003e,0x00000038,0x0000003f,0x0004003d,0x00000007,0x00000043,0x00000042,0x0004003d,
	0x0000000a,0x00000046,0x00000045,0x0008004f,0x00000007,0x00000047,0x00000046,0x00000046,
	0x00000000,0x00000001,0x00000002,0x0007000c,0x00000007,0x00000048,0x00000001,0x00000044,
	0x00000043,0x00000047,0x00050041,0x0000004a,0x0000004b,0x00000045,0x00000049,0x0004003d,
	0x00000006,0x0000004c,0x0000004b,0x0005008e,0x00000007,0x0000004d,0x00000048,0x0000004c,
	0x0003003e,0x00000041,0x0000004d,0x00050041,0x00000017,0x0000004f,0x00000016,0x00000013,
	0x0004003d,0x0000000c,0x00000050,0x0000004f,0x00070041,0x0000001a,0x00000051,0x00000011,
	0x00000013,0x00000050,0x00000013,0x0004003d,0x0000000b,0x00000052,0x00000051,0x0004003d,
	0x0000000a,0x00000053,0x00000045,0x0008004f,0x00000007,0x00000054,0x00000053,0x00000053,
	0x00000000,0x00000001,0x00000002,0x00050051,0x00000006,0x00000056,0x00000054,0x00000000,
	0x00050051,0x00000006,0x00000057,0x00000054,0x00000001,0x00050051,0x00000006,0x00000058,
	0x00000054,0x00000002,0x00070050,0x0000000a,0x00000059,0x00000056,0x00000057,0x00000058,
	0x00000055,0x00050091,0x0000000a,0x0000005a,0x00000052,0x00000059,0x00050051,0x00000006,
	0x0000005b,0x0000005a,0x00000000,0x00050051,0x00000006,0x0000005c,0x0000005a,0x00000001,
	0x00050051,0x00000006,0x0000005d,0x0000005a,0x00000002,0x00060050,0x00000007,0x0000005e,
	0x0000005b,0x0000005c,0x0000005d,0x0003003e,0x0000004e,0x0000005e,0x00050041,0x00000017,
	0x00000060,0x00000016,0x00000013,0x0004003d,0x0000000c,0x00000061,0x00000060,0x00070041,
	0x0000001a,0x00000062,0x00000011,0x00000013,0x00000061,0x00000013,0x0004003d,0x0000000b,
	0x00000063,0x00000062,0x0004003d,0x00000007,0x00000064,0x00000041,0x00050051,0x00000006,
	0x00000065,0x00000064,0x00000000,0x00050051,0x00000006,0x00000066,0x00000064,0x00000001,
	0x00050051,0x00000006,0x00000067,0x00000064,0x00000002,0x00070050,0x0000000a,0x00000068,
	0x00000065,0x00000066,0x00000067,0x00000055,0x00050091,0x0000000a,0x00000069,0x00000063,
	0x00000068,0x00050051,0x00000006,0x0000006a,0x00000069,0x00000000,0x00050051,0x00000006,
	0x0000006b,0x00000069,0x00000001,0x00050051,0x00000006,0x0000006c,0x00000069,0x00000002,
	0x00060050,0x00000007,0x0000006d,0x0000006a,0x0000006b,0x0000006c,0x0003003e,0x0000005f,
	0x0000006d,0x00050041,0x00000017,0x0000006f,0x00000016,0x00000013,0x0004003d,0x0000000c,
	0x00000070,0x0000006f,0x00070041,0x0000001a,0x00000071,0x00000011,0x00000013,0x00000070,
	0x00000013,0x0004003d,0x0000000b,0x00000072,0x00000071,0x0004003d,0x00000007,0x00000073,
	0x00000042,0x00050051,0x00000006,0x00000074,0x00000073,0x00000000,0x00050051,0x00000006,
	0x00000075,0x00000073,0x00000001,0x00050051,0x00000006,0x00000076,0x00000073,0x00000002,
	0x00070050,0x0000000a,0x00000077,0x00000074,0x00000075,0x00000076,0x00000055,0x00050091,
	0x0000000a,0x00000078,0x00000072,0x00000077,0x00050051,0x00000006,0x00000079,0x00000078,
	0x00000000,0x00050051,0x00000006,0x0000007a,0x00000078,0x00000001,0x00050051,0x00000006,
	0x0000007b,0x00000078,0x00000002,0x00060050,0x00000007,0x0000007c,0x00000079,0x0000007a,
	0x0000007b,0x0003003e,0x0000006e,0x0000007c,0x0004003d,0x00000007,0x00000080,0x0000004e,
	0x0004003d,0x00000007,0x00000081,0x0000005f,0x0004003d,0x00000007,0x00000082,0x0000006e,
	0x00050051,0x00000006,0x00000083,0x00000080,0x00000000,0x00050051,0x00000006,0x00000084,
	0x00000080,0x00000001,0x00050051,0x00000006,0x00000085,0x00000080,0x00000002,0x00050051,
	0x00000006,0x00000086,0x00000081,0x00000000,0x00050051,0x00000006,0x00000087,0x00000081,
	0x00000001,0x00050051,0x00000006,0x00000088,0x00000081,0x00000002,0x00050051,0x00000006,
	0x00000089,0x00000082,0x00000000,0x00050051,0x00000006,0x0000008a,0x00000082,0x00000001,
	0x00050051,0x00000006,0x0000008b,0x00000082,0x00000002,0x00060050,0x00000007,0x0000008c,
	0x00000083,0x00000084,0x00000085,0x00060050,0x00000007,0x0000008d,0x00000086,0x00000087,
	0x00000088,0x00060050,0x00000007,0x0000008e,0x00000089,0x0000008a,0x0000008b,0x00060050,
	0x0000007d,0x0000008f,0x0000008c,0x0000008d,0x0000008e,0x0003003e,0x0000007f,0x0000008f,
	0x00050041,0x0000001a,0x00000095,0x0000003b,0x00000034,0x0004003d,0x0000000b,0x00000096,
	0x00000095,0x00050041,0x0000001a,0x00000097,0x0000003b,0x00000013,0x0004003d,0x0000000b,
	0x00000098,0x00000097,0x00050092,0x0000000b,0x00000099,0x00000096,0x00000098,0x0004003d,
	0x00000007,0x0000009a,0x00000009,0x00050051,0x00000006,0x0000009b,0x0000009a,0x00000000,
	0x00050051,0x00000006,0x0000009c,0x0000009a,0x00000001,0x00050051,0x00000006,0x0000009d,
	0x0000009a,0x00000002,0x00070050,0x0000000a,0x0000009e,0x0000009b,0x0000009c,0x0000009d,
	0x00000020,0x00050091,0x0000000a,0x0000009f,0x00000099,0x0000009e,0x00050041,0x000000a0,
	0x000000a1,0x00000094,0x00000013,0x0003003e,0x000000a1,0x0000009f,0x000100fd,0x00010038 };

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderCode.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderCode.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::vector<uint32_t> fragmentShaderCode = {
		0x07230203,0x00010000,0x0008000b,0x00000270,0x00000000,0x00020011,0x00000001,0x00020011,
	0x000014b6,0x0008000a,0x5f565053,0x5f545845,0x63736564,0x74706972,0x695f726f,0x7865646e,
	0x00676e69,0x0006000b,0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,
	0x00000000,0x00000001,0x000b000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000156,
	0x00000160,0x000001a1,0x000001a8,0x000001aa,0x0000026a,0x00030010,0x00000004,0x00000007,
	0x00030003,0x00000002,0x000001cc,0x00080004,0x455f4c47,0x6e5f5458,0x6e756e6f,0x726f6669,
	0x75715f6d,0x66696c61,0x00726569,0x000a0004,0x475f4c47,0x4c474f4f,0x70635f45,0x74735f70,
	0x5f656c79,0x656e696c,0x7269645f,0x69746365,0x00006576,0x00080004,0x475f4c47,0x4c474f4f,
	0x6e695f45,0x64756c63,0x69645f65,0x74636572,0x00657669,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00070005,0x0000000b,0x74736964,0x75626972,0x6e6f6974,0x3b316628,0x003b3166,
	0x00040005,0x00000009,0x746f644e,0x00000048,0x00050005,0x0000000a,0x67756f72,0x73656e68,
	0x00000073,0x00060005,0x00000012,0x73657266,0x286c656e,0x763b3166,0x003b3366,0x00050005,
	0x00000010,0x54736f63,0x61746568,0x00000000,0x00030005,0x00000011,0x00003066,0x00050005,
	0x00000016,0x31662867,0x3b31663b,0x00000000,0x00040005,0x00000014,0x746f644e,0x00000056,
	0x00050005,0x00000015,0x67756f72,0x73656e68,0x00000073,0x00060005,0x0000001c,0x74696d73,
	0x31662868,0x3b31663b,0x003b3166,0x00040005,0x00000019,0x746f644c,0x0000004e,0x00040005,
	0x0000001a,0x746f6456,0x0000004e,0x00050005,0x0000001b,0x67756f72,0x73656e68,0x00000073,
	0x000a0005,0x00000020,0x66666964,0x46657375,0x6e736572,0x6f436c65,0x63657272,0x6e6f6974,
	0x33667628,0x0000003b,0x00030005,0x0000001f,0x00726f69,0x000a0005,0x0000002b,0x66647262,
	0x3b316628,0x663b3166,0x31663b31,0x3b31663b,0x3b336676,0x663b3166,0x00003b31,0x00040005,
	0x00000023,0x746f644c,0x00000048,0x00040005,0x00000024,0x746f644e,0x00000048,0x00040005,
	0x00000025,0x746f6456,0x00000048,0x00040005,0x00000026,0x746f644c,0x0000004e,0x00040005,
	0x00000027,0x746f6456,0x0000004e,0x00040005,0x00000028,0x66666964,0x00657375,0x00050005,
	0x00000029,0x6174656d,0x73656e6c,0x00000073,0x00050005,0x0000002a,0x67756f72,0x73656e68,
	0x00000073,0x000b0005,0x00000035,0x64616873,0x66762865,0x66763b33,0x66763b33,0x66763b33,
	0x66763b33,0x31663b33,0x3b31663b,0x00000000,0x00030005,0x0000002e,0x0000006e,0x00030005,
	0x0000002f,0x00000076,0x00030005,0x00000030,0x0000006c,0x00030005,0x00000031,0x0000636c,
	0x00040005,0x00000032,0x66666964,0x00657375,0x00050005,0x00000033,0x6174656d,0x73656e6c,
	0x00000073,0x00050005,0x00000034,0x67756f72,0x73656e68,0x00000073,0x00030005,0x00000037,
	0x00000061,0x00040005,0x0000003b,0x75715361,0x00657261,0x00050005,0x0000003f,0x746f644e,
	0x75715348,0x00657261,0x00040005,0x00000043,0x6f6e6564,0x0000006d,0x00030005,0x0000005f,
	0x00000072,0x00030005,0x00000062,0x0000006b,0x00040005,0x00000068,0x6f6e6564,0x0000006d,
	0x00030005,0x00000074,0x00007667,0x00040005,0x00000075,0x61726170,0x0000006d,0x00040005,
	0x00000077,0x61726170,0x0000006d,0x00030005,0x0000007a,0x00006c67,0x00040005,0x0000007b,
	0x61726170,0x0000006d,0x00040005,0x0000007d,0x61726170,0x0000006d,0x00050005,0x00000085,
	0x53726f69,0x72617571,0x00000065,0x00030005,0x0000008c,0x00524954,0x00050005,0x00000090,
	0x44766e69,0x6d756e65,0x00000000,0x00030005,0x0000009c,0x006d756e,0x00030005,0x000000ba,
	0x00000064,0x00040005,0x000000bb,0x61726170,0x0000006d,0x00040005,0x000000bd,0x61726170,
	0x0000006d,0x00030005,0x000000c0,0x00000066,0x00040005,0x000000c7,0x61726170,0x0000006d,
	0x00040005,0x000000c9,0x61726170,0x0000006d,0x00030005,0x000000cb,0x00005466,0x00040005,
	0x000000d0,0x61726170,0x0000006d,0x00040005,0x000000d2,0x61726170,0x0000006d,0x00040005,
	0x000000d4,0x52495466,0x00000000,0x00040005,0x000000d9,0x61726170,0x0000006d,0x00040005,
	0x000000db,0x61726170,0x0000006d,0x00030005,0x000000dd,0x00000067,0x00040005,0x000000de,
	0x61726170,0x0000006d,0x00040005,0x000000e0,0x61726170,0x0000006d,0x00040005,0x000000e2,
	0x61726170,0x0000006d,0x00030005,0x000000e5,0x00636664,0x00040005,0x000000e8,0x61726170,
	0x0000006d,0x00050005,0x000000ea,0x626d616c,0x69747265,0x00006e61,0x00030005,0x0000010d,
	0x00000068,0x00040005,0x00000112,0x746f644c,0x00000048,0x00040005,0x00000118,0x746f644e,
	0x00000048,0x00040005,0x0000011d,0x746f6456,0x00000048,0x00040005,0x00000122,0x746f644c,
	0x0000004e,0x00040005,0x00000127,0x746f6456,0x0000004e,0x00040005,0x0000012c,0x66647262,
	0x00000000,0x00040005,0x0000012d,0x61726170,0x0000006d,0x00040005,0x0000012f,0x61726170,
	0x0000006d,0x00040005,0x00000131,0x61726170,0x0000006d,0x00040005,0x00000133,0x61726170,
	0x0000006d,0x00040005,0x00000135,0x61726170,0x0000006d,0x00040005,0x00000137,0x61726170,
	0x0000006d,0x00040005,0x00000139,0x61726170,0x0000006d,0x00040005,0x0000013b,0x61726170,
	0x0000006d,0x00060005,0x00000147,0x66666964,0x53657375,0x6c706d61,0x00000065,0x00050005,
	0x0000014c,0x74786574,0x73657275,0x00000000,0x00060005,0x0000014e,0x6574614d,0x6c616972,
	0x6f666e49,0x00000000,0x00080006,0x0000014e,0x00000000,0x66666964,0x54657375,0x75747865,
	0x6e496572,0x00786564,0x00080006,0x0000014e,0x00000001,0x6d726f6e,0x65546c61,0x72757478,
	0x646e4965,0x00007865,0x00090006,0x0000014e,0x00000002,0x6174656d,0x73656e6c,0x78655473,
	0x65727574,0x65646e49,0x00000078,0x00090006,0x0000014e,0x00000003,0x67756f72,0x73656e68,
	0x78655473,0x65727574,0x65646e49,0x00000078,0x00090006,0x0000014e,0x00000004,0x6c63636f,
	0x6f697375,0x7865546e,0x65727574,0x65646e49,0x00000078,0x00090006,0x0000014e,0x00000005,
	0x73696d65,0x65766973,0x74786554,0x49657275,0x7865646e,0x00000000,0x00050005,0x00000150,
	0x6574614d,0x6c616972,0x00000073,0x00050006,0x00000150,0x00000000,0x6f666e69,0x00000000,
	0x00050005,0x00000152,0x6574616d,0x6c616972,0x00000073,0x00050005,0x00000156,0x6574616d,
	0x6c616972,0x00004449,0x00030005,0x00000160,0x00007675,0x00060005,0x00000163,0x6d726f6e,
	0x61536c61,0x656c706d,0x00000000,0x00060005,0x0000016d,0x6174656d,0x73656e6c,0x6d615373,
	0x00656c70,0x00060005,0x00000178,0x67756f72,0x73656e68,0x6d615373,0x00656c70,0x00060005,
	0x00000183,0x6c63636f,0x6f697375,0x6d61536e,0x00656c70,0x00060005,0x0000018e,0x73696d65,
	0x65766973,0x706d6153,0x0000656c,0x00030005,0x00000198,0x00000064,0x00030005,0x0000019e,
	0x0000006e,0x00030005,0x000001a1,0x004e4254,0x00030005,0x000001a6,0x00000076,0x00060005,
	0x000001a8,0x656d6163,0x6f506172,0x69746973,0x00006e6f,0x00050005,0x000001aa,0x69736f70,
	0x6e6f6974,0x00000000,0x00040005,0x000001ae,0x6f6c6f63,0x00000072,0x00050005,0x000001b1,
	0x6867696c,0x646e4974,0x00007865,0x00030005,0x000001b2,0x00000069,0x00050005,0x000001ba,
	0x6867694c,0x666e4974,0x0000006f,0x00060006,0x000001ba,0x00000000,0x69736f70,0x6e6f6974,
	0x00000000,0x00060006,0x000001ba,0x00000001,0x65726964,0x6f697463,0x0000006e,0x00050006,
	0x000001ba,0x00000002,0x6f6c6f63,0x00000072,0x00050006,0x000001ba,0x00000003,0x6f747563,
	0x00736666,0x00040005,0x000001bc,0x6867694c,0x00007374,0x00050006,0x000001bc,0x00000000,
	0x6e756f63,0x00000074,0x00050006,0x000001bc,0x00000001,0x6f666e69,0x00000000,0x00040005,
	0x000001be,0x6867696c,0x00007374,0x00030005,0x000001c2,0x0000006c,0x00040005,0x000001ca,
	0x61726170,0x0000006d,0x00040005,0x000001cc,0x61726170,0x0000006d,0x00040005,0x000001ce,
	0x61726170,0x0000006d,0x00040005,0x000001d0,0x61726170,0x0000006d,0x00040005,0x000001d3,
	0x61726170,0x0000006d,0x00040005,0x000001d5,0x61726170,0x0000006d,0x00040005,0x000001d7,
	0x61726170,0x0000006d,0x00030005,0x000001e0,0x00000069,0x00030005,0x000001ea,0x0000006c,
	0x00050005,0x000001f1,0x74736964,0x65636e61,0x00000000,0x00050005,0x000001f8,0x65747461,
	0x7461756e,0x006e6f69,0x00050005,0x000001fd,0x69646172,0x65636e61,0x00000000,0x00040005,
	0x00000203,0x61726170,0x0000006d,0x00040005,0x00000205,0x61726170,0x0000006d,0x00040005,
	0x00000207,0x61726170,0x0000006d,0x00040005,0x00000209,0x61726170,0x0000006d,0x00040005,
	0x0000020b,0x61726170,0x0000006d,0x00040005,0x0000020d,0x61726170,0x0000006d,0x00040005,
	0x0000020f,0x61726170,0x0000006d,0x00030005,0x00000218,0x00000069,0x00030005,0x00000222,
	0x0000006c,0x00040005,0x00000229,0x74656874,0x00000061,0x00040005,0x00000231,0x69737065,
	0x006e6f6c,0x00050005,0x0000023c,0x65746e69,0x7469736e,0x00000079,0x00040005,0x00000250,
	0x61726170,0x0000006d,0x00040005,0x00000252,0x61726170,0x0000006d,0x00040005,0x00000254,
	0x61726170,0x0000006d,0x00040005,0x00000256,0x61726170,0x0000006d,0x00040005,0x00000257,
	0x61726170,0x0000006d,0x00040005,0x00000258,0x61726170,0x0000006d,0x00040005,0x0000025a,
	0x61726170,0x0000006d,0x00050005,0x0000026a,0x4374756f,0x726f6c6f,0x00000000,0x00040047,
	0x0000014c,0x00000022,0x00000000,0x00040047,0x0000014c,0x00000021,0x00000004,0x00050048,
	0x0000014e,0x00000000,0x00000023,0x00000000,0x00050048,0x0000014e,0x00000001,0x00000023,
	0x00000004,0x00050048,0x0000014e,0x00000002,0x00000023,0x00000008,0x00050048,0x0000014e,
	0x00000003,0x00000023,0x0000000c,0x00050048,0x0000014e,0x00000004,0x00000023,0x00000010,
	0x00050048,0x0000014e,0x00000005,0x00000023,0x00000014,0x00040047,0x0000014f,0x00000006,
	0x00000018,0x00040048,0x00000150,0x00000000,0x00000013,0x00040048,0x00000150,0x00000000,
	0x00000018,0x00050048,0x00000150,0x00000000,0x00000023,0x00000000,0x00030047,0x00000150,
	0x00000003,0x00040047,0x00000152,0x00000022,0x00000000,0x00040047,0x00000152,0x00000021,
	0x00000002,0x00030047,0x00000156,0x0000000e,0x00040047,0x00000156,0x0000001e,0x00000002,
	0x00040047,0x00000160,0x0000001e,0x00000001,0x00040047,0x000001a1,0x0000001e,0x00000004,
	0x00030047,0x000001a8,0x0000000e,0x00040047,0x000001a8,0x0000001e,0x00000003,0x00040047,
	0x000001aa,0x0000001e,0x00000000,0x00050048,0x000001ba,0x00000000,0x00000023,0x00000000,
	0x00050048,0x000001ba,0x00000001,0x00000023,0x00000010,0x00050048,0x000001ba,0x00000002,
	0x00000023,0x00000020,0x00050048,0x000001ba,0x00000003,0x00000023,0x00000030,0x00040047,
	0x000001bb,0x00000006,0x00000040,0x00040048,0x000001bc,0x00000000,0x00000013,0x00040048,
	0x000001bc,0x00000000,0x00000018,0x00050048,0x000001bc,0x00000000,0x00000023,0x00000000,
	0x00040048,0x000001bc,0x00000001,0x00000013,0x00040048,0x000001bc,0x00000001,0x00000018,
	0x00050048,0x000001bc,0x00000001,0x00000023,0x00000010,0x00030047,0x000001bc,0x00000003,
	0x00040047,0x000001be,0x00000022,0x00000000,0x00040047,0x000001be,0x00000021,0x00000003,
	0x00040047,0x0000026a,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,
	0x00000002,0x00030016,0x00000006,0x00000020,0x00040020,0x00000007,0x00000007,0x00000006,
	0x00050021,0x00000008,0x00000006,0x00000007,0x00000007,0x00040017,0x0000000d,0x00000006,
	0x00000003,0x00040020,0x0000000e,0x00000007,0x0000000d,0x00050021,0x0000000f,0x0000000d,
	0x00000007,0x0000000e,0x00060021,0x00000018,0x00000006,0x00000007,0x00000007,0x00000007,
	0x00040021,0x0000001e,0x0000000d,0x0000000e,0x000b0021,0x00000022,0x0000000d,0x00000007,
	0x00000007,0x00000007,0x00000007,0x00000007,0x0000000e,0x00000007,0x00000007,0x000a0021,
	0x0000002d,0x0000000d,0x0000000e,0x0000000e,0x0000000e,0x0000000e,0x0000000e,0x00000007,
	0x00000007,0x0004002b,0x00000006,0x00000046,0x3f800000,0x0004002b,0x00000006,0x0000004b,
	0x40490fdb,0x0004002b,0x00000006,0x00000059,0x40a00000,0x0004002b,0x00000006,0x00000066,
	0x41000000,0x00020014,0x00000089,0x00040017,0x0000008a,0x00000089,0x00000003,0x00040020,
	0x0000008b,0x00000007,0x0000008a,0x0006002c,0x0000000d,0x0000008e,0x00000046,0x00000046,
	0x00000046,0x0004002b,0x00000006,0x00000094,0x484e165c,0x0006002c,0x0000000d,0x00000095,
	0x00000094,0x00000094,0x00000094,0x0004002b,0x00000006,0x0000009e,0x3e44b9f4,0x0006002c,
	0x0000000d,0x0000009f,0x0000009e,0x0000009e,0x0000009e,0x0004002b,0x00000006,0x000000a1,
	0x43952000,0x0004002b,0x00000006,0x000000a3,0x4382b0a4,0x0004002b,0x00000006,0x000000a7,
	0x430a6e14,0x0004002b,0x00000006,0x000000ad,0x3f4ed183,0x0006002c,0x0000000d,0x000000ae,
	0x000000ad,0x000000ad,0x000000ad,0x0004002b,0x00000006,0x000000af,0xbf88f5c3,0x0006002c,
	0x0000000d,0x000000b0,0x000000af,0x000000af,0x000000af,0x0004002b,0x00000006,0x000000c1,
	0x3d23d70a,0x0006002c,0x0000000d,0x000000c2,0x000000c1,0x000000c1,0x000000c1,0x0004002b,
	0x00000006,0x000000e6,0x3f866666,0x0006002c,0x0000000d,0x000000e7,0x000000e6,0x000000e6,
	0x000000e6,0x0004002b,0x00000006,0x000000f3,0x40800000,0x0004002b,0x00000006,0x000000f8,
	0x3a83126f,0x0004002b,0x00000006,0x00000116,0x00000000,0x00040017,0x00000145,0x00000006,
	0x00000004,0x00040020,0x00000146,0x00000007,0x00000145,0x00090019,0x00000148,0x00000006,
	0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x00000149,
	0x00000148,0x0003001d,0x0000014a,0x00000149,0x00040020,0x0000014b,0x00000000,0x0000014a,
	0x0004003b,0x0000014b,0x0000014c,0x00000000,0x00040015,0x0000014d,0x00000020,0x00000000,
	0x0008001e,0x0000014e,0x0000014d,0x0000014d,0x0000014d,0x0000014d,0x0000014d,0x0000014d,
	0x0003001d,0x0000014f,0x0000014e,0x0003001e,0x00000150,0x0000014f,0x00040020,0x00000151,
	0x00000002,0x00000150,0x0004003b,0x00000151,0x00000152,0x00000002,0x00040015,0x00000153,
	0x00000020,0x00000001,0x0004002b,0x00000153,0x00000154,0x00000000,0x00040020,0x00000155,
	0x00000001,0x0000014d,0x0004003b,0x00000155,0x00000156,0x00000001,0x00040020,0x00000158,
	0x00000002,0x0000014d,0x00040020,0x0000015b,0x00000000,0x00000149,0x00040017,0x0000015e,
	0x00000006,0x00000002,0x00040020,0x0000015f,0x00000001,0x0000015e,0x0004003b,0x0000015f,
	0x00000160,0x00000001,0x0004002b,0x00000153,0x00000165,0x00000001,0x0004002b,0x00000153,
	0x0000016f,0x00000002,0x0004002b,0x0000014d,0x00000176,0x00000002,0x0004002b,0x00000153,
	0x0000017a,0x00000003,0x0004002b,0x0000014d,0x00000181,0x00000001,0x0004002b,0x00000153,
	0x00000185,0x00000004,0x0004002b,0x0000014d,0x0000018c,0x00000000,0x0004002b,0x00000153,
	0x00000190,0x00000005,0x00040018,0x0000019f,0x0000000d,0x00000003,0x00040020,0x000001a0,
	0x00000001,0x0000019f,0x0004003b,0x000001a0,0x000001a1,0x00000001,0x00040020,0x000001a7,
	0x00000001,0x0000000d,0x0004003b,0x000001a7,0x000001a8,0x00000001,0x0004003b,0x000001a7,
	0x000001aa,0x00000001,0x0006002c,0x0000000d,0x000001af,0x00000116,0x00000116,0x00000116,
	0x00040020,0x000001b0,0x00000007,0x0000014d,0x00040017,0x000001b9,0x0000014d,0x00000003,
	0x0006001e,0x000001ba,0x0000000d,0x0000000d,0x0000000d,0x0000015e,0x0003001d,0x000001bb,
	0x000001ba,0x0004001e,0x000001bc,0x000001b9,0x000001bb,0x00040020,0x000001bd,0x00000002,
	0x000001bc,0x0004003b,0x000001bd,0x000001be,0x00000002,0x00040020,0x000001c4,0x00000002,
	0x0000000d,0x00040020,0x00000233,0x00000002,0x00000006,0x00040020,0x00000269,0x00000003,
	0x00000145,0x0004003b,0x00000269,0x0000026a,0x00000003,0x00050036,0x00000002,0x00000004,
	0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000146,0x00000147,0x00000007,
	0x0004003b,0x0000000e,0x00000163,0x00000007,0x0004003b,0x00000007,0x0000016d,0x00000007,
	0x0004003b,0x00000007,0x00000178,0x00000007,0x0004003b,0x00000007,0x00000183,0x00000007,
	0x0004003b,0x0000000e,0x0000018e,0x00000007,0x0004003b,0x0000000e,0x00000198,0x00000007,
	0x0004003b,0x0000000e,0x0000019e,0x00000007,0x0004003b,0x0000000e,0x000001a6,0x00000007,
	0x0004003b,0x0000000e,0x000001ae,0x00000007,0x0004003b,0x000001b0,0x000001b1,0x00000007,
	0x0004003b,0x000001b0,0x000001b2,0x00000007,0x0004003b,0x0000000e,0x000001c2,0x00000007,
	0x0004003b,0x0000000e,0x000001ca,0x00000007,0x0004003b,0x0000000e,0x000001cc,0x00000007,
	0x0004003b,0x0000000e,0x000001ce,0x00000007,0x0004003b,0x0000000e,0x000001d0,0x00000007,
	0x0004003b,0x0000000e,0x000001d3,0x00000007,0x0004003b,0x00000007,0x000001d5,0x00000007,
	0x0004003b,0x00000007,0x000001d7,0x00000007,0x0004003b,0x000001b0,0x000001e0,0x00000007,
	0x0004003b,0x0000000e,0x000001ea,0x00000007,0x0004003b,0x00000007,0x000001f1,0x00000007,
	0x0004003b,0x00000007,0x000001f8,0x00000007,0x0004003b,0x0000000e,0x000001fd,0x00000007,
	0x0004003b,0x0000000e,0x00000203,0x00000007,0x0004003b,0x0000000e,0x00000205,0x00000007,
	0x0004003b,0x0000000e,0x00000207,0x00000007,0x0004003b,0x0000000e,0x00000209,0x00000007,
	0x0004003b,0x0000000e,0x0000020b,0x00000007,0x0004003b,0x00000007,0x0000020d,0x00000007,
	0x0004003b,0x00000007,0x0000020f,0x00000007,0x0004003b,0x000001b0,0x00000218,0x00000007,
	0x0004003b,0x0000000e,0x00000222,0x00000007,0x0004003b,0x00000007,0x00000229,0x00000007,
	0x0004003b,0x00000007,0x00000231,0x00000007,0x0004003b,0x00000007,0x0000023c,0x00000007,
	0x0004003b,0x0000000e,0x00000250,0x00000007,0x0004003b,0x0000000e,0x00000252,0x00000007,
	0x0004003b,0x0000000e,0x00000254,0x00000007,0x0004003b,0x0000000e,0x00000256,0x00000007,
	0x0004003b,0x0000000e,0x00000257,0x00000007,0x0004003b,0x00000007,0x00000258,0x00000007,
	0x0004003b,0x00000007,0x0000025a,0x00000007,0x0004003d,0x0000014d,0x00000157,0x00000156,
	0x00070041,0x00000158,0x00000159,0x00000152,0x00000154,0x00000157,0x00000154,0x0004003d,
	0x0000014d,0x0000015a,0x00000159,0x00050041,0x0000015b,0x0000015c,0x0000014c,0x0000015a,
	0x0004003d,0x00000149,0x0000015d,0x0000015c,0x0004003d,0x0000015e,0x00000161,0x00000160,
	0x00050057,0x00000145,0x00000162,0x0000015d,0x00000161,0x0003003e,0x00000147,0x00000162,
	0x0004003d,0x0000014d,0x00000164,0x00000156,0x00070041,0x00000158,0x00000166,0x00000152,
	0x00000154,0x00000164,0x00000165,0x0004003d,0x0000014d,0x00000167,0x00000166,0x00050041,
	0x0000015b,0x00000168,0x0000014c,0x00000167,0x0004003d,0x00000149,0x00000169,0x00000168,
	0x0004003d,0x0000015e,0x0000016a,0x00000160,0x00050057,0x00000145,0x0000016b,0x00000169,
	0x0000016a,0x0008004f,0x0000000d,0x0000016c,0x0000016b,0x0000016b,0x00000000,0x00000001,
	0x00000002,0x0003003e,0x00000163,0x0000016c,0x0004003d,0x0000014d,0x0000016e,0x00000156,
	0x00070041,0x00000158,0x00000170,0x00000152,0x00000154,0x0000016e,0x0000016f,0x0004003d,
	0x0000014d,0x00000171,0x00000170,0x00050041,0x0000015b,0x00000172,0x0000014c,0x00000171,
	0x0004003d,0x00000149,0x00000173,0x00000172,0x0004003d,0x0000015e,0x00000174,0x00000160,
	0x00050057,0x00000145,0x00000175,0x00000173,0x00000174,0x00050051,0x00000006,0x00000177,
	0x00000175,0x00000002,0x0003003e,0x0000016d,0x00000177,0x0004003d,0x0000014d,0x00000179,
	0x00000156,0x00070041,0x00000158,0x0000017b,0x00000152,0x00000154,0x00000179,0x0000017a,
	0x0004003d,0x0000014d,0x0000017c,0x0000017b,0x00050041,0x0000015b,0x0000017d,0x0000014c,
	0x0000017c,0x0004003d,0x00000149,0x0000017e,0x0000017d,0x0004003d,0x0000015e,0x0000017f,
	0x00000160,0x00050057,0x00000145,0x00000180,0x0000017e,0x0000017f,0x00050051,0x00000006,
	0x00000182,0x00000180,0x00000001,0x0003003e,0x00000178,0x00000182,0x0004003d,0x0000014d,
	0x00000184,0x00000156,0x00070041,0x00000158,0x00000186,0x00000152,0x00000154,0x00000184,
	0x00000185,0x0004003d,0x0000014d,0x00000187,0x00000186,0x00050041,0x0000015b,0x00000188,
	0x0000014c,0x00000187,0x0004003d,0x00000149,0x00000189,0x00000188,0x0004003d,0x0000015e,
	0x0000018a,0x00000160,0x00050057,0x00000145,0x0000018b,0x00000189,0x0000018a,0x00050051,
	0x00000006,0x0000018d,0x0000018b,0x00000000,0x0003003e,0x00000183,0x0000018d,0x0004003d,
	0x0000014d,0x0000018f,0x00000156,0x00070041,0x00000158,0x00000191,0x00000152,0x00000154,
	0x0000018f,0x00000190,0x0004003d,0x0000014d,0x00000192,0x00000191,0x00050041,0x0000015b,
	0x00000193,0x0000014c,0x00000192,0x0004003d,0x00000149,0x00000194,0x00000193,0x0004003d,
	0x0000015e,0x00000195,0x00000160,0x00050057,0x00000145,0x00000196,0x00000194,0x00000195,
	0x0008004f,0x0000000d,0x00000197,0x00000196,0x00000196,0x00000000,0x00000001,0x00000002,
	0x0003003e,0x0000018e,0x00000197,0x0004003d,0x00000145,0x00000199,0x00000147,0x00050051,
	0x00000006,0x0000019a,0x00000199,0x00000000,0x00050051,0x00000006,0x0000019b,0x00000199,
	0x00000001,0x00050051,0x00000006,0x0000019c,0x00000199,0x00000002,0x00060050,0x0000000d,
	0x0000019d,0x0000019a,0x0000019b,0x0000019c,0x0003003e,0x00000198,0x0000019d,0x0004003d,
	0x0000019f,0x000001a2,0x000001a1,0x0004003d,0x0000000d,0x000001a3,0x00000163,0x00050091,
	0x0000000d,0x000001a4,0x000001a2,0x000001a3,0x0006000c,0x0000000d,0x000001a5,0x00000001,
	0x00000045,0x000001a4,0x0003003e,0x0000019e,0x000001a5,0x0004003d,0x0000000d,0x000001a9,
	0x000001a8,0x0004003d,0x0000000d,0x000001ab,0x000001aa,0x00050083,0x0000000d,0x000001ac,
	0x000001a9,0x000001ab,0x0006000c,0x0000000d,0x000001ad,0x00000001,0x00000045,0x000001ac,
	0x0003003e,0x000001a6,0x000001ad,0x0003003e,0x000001ae,0x000001af,0x0003003e,0x000001b1,
	0x0000018c,0x0003003e,0x000001b2,0x0000018c,0x000200f9,0x000001b3,0x000200f8,0x000001b3,
	0x000400f6,0x000001b5,0x000001b6,0x00000000,0x000200f9,0x000001b7,0x000200f8,0x000001b7,
	0x0004003d,0x0000014d,0x000001b8,0x000001b2,0x00060041,0x00000158,0x000001bf,0x000001be,
	0x00000154,0x0000018c,0x0004003d,0x0000014d,0x000001c0,0x000001bf,0x000500b0,0x00000089,
	0x000001c1,0x000001b8,0x000001c0,0x000400fa,0x000001c1,0x000001b4,0x000001b5,0x000200f8,
	0x000001b4,0x0004003d,0x0000014d,0x000001c3,0x000001b1,0x00070041,0x000001c4,0x000001c5,
	0x000001be,0x00000165,0x000001c3,0x00000165,0x0004003d,0x0000000d,0x000001c6,0x000001c5,
	0x0004007f,0x0000000d,0x000001c7,0x000001c6,0x0006000c,0x0000000d,0x000001c8,0x00000001,
	0x00000045,0x000001c7,0x0003003e,0x000001c2,0x000001c8,0x0004003d,0x0000014d,0x000001c9,
	0x000001b1,0x0004003d,0x0000000d,0x000001cb,0x0000019e,0x0003003e,0x000001ca,0x000001cb,
	0x0004003d,0x0000000d,0x000001cd,0x000001a6,0x0003003e,0x000001cc,0x000001cd,0x0004003d,
	0x0000000d,0x000001cf,0x000001c2,0x0003003e,0x000001ce,0x000001cf,0x00070041,0x000001c4,
	0x000001d1,0x000001be,0x00000165,0x000001c9,0x0000016f,0x0004003d,0x0000000d,0x000001d2,
	0x000001d1,0x0003003e,0x000001d0,0x000001d2,0x0004003d,0x0000000d,0x000001d4,0x00000198,
	0x0003003e,0x000001d3,0x000001d4,0x0004003d,0x00000006,0x000001d6,0x0000016d,0x0003003e,
	0x000001d5,0x000001d6,0x0004003d,0x00000006,0x000001d8,0x00000178,0x0003003e,0x000001d7,
	0x000001d8,0x000b0039,0x0000000d,0x000001d9,0x00000035,0x000001ca,0x000001cc,0x000001ce,
	0x000001d0,0x000001d3,0x000001d5,0x000001d7,0x0004003d,0x0000000d,0x000001da,0x000001ae,
	0x00050081,0x0000000d,0x000001db,0x000001da,0x000001d9,0x0003003e,0x000001ae,0x000001db,
	0x0004003d,0x0000014d,0x000001dc,0x000001b1,0x00050080,0x0000014d,0x000001dd,0x000001dc,
	0x00000165,0x0003003e,0x000001b1,0x000001dd,0x000200f9,0x000001b6,0x000200f8,0x000001b6,
	0x0004003d,0x0000014d,0x000001de,0x000001b2,0x00050080,0x0000014d,0x000001df,0x000001de,
	0x00000165,0x0003003e,0x000001b2,0x000001df,0x000200f9,0x000001b3,0x000200f8,0x000001b5,
	0x0003003e,0x000001e0,0x0000018c,0x000200f9,0x000001e1,0x000200f8,0x000001e1,0x000400f6,
	0x000001e3,0x000001e4,0x00000000,0x000200f9,0x000001e5,0x000200f8,0x000001e5,0x0004003d,
	0x0000014d,0x000001e6,0x000001e0,0x00060041,0x00000158,0x000001e7,0x000001be,0x00000154,
	0x00000181,0x0004003d,0x0000014d,0x000001e8,0x000001e7,0x000500b0,0x00000089,0x000001e9,
	0x000001e6,0x000001e8,0x000400fa,0x000001e9,0x000001e2,0x000001e3,0x000200f8,0x000001e2,
	0x0004003d,0x0000014d,0x000001eb,0x000001b1,0x00070041,0x000001c4,0x000001ec,0x000001be,
	0x00000165,0x000001eb,0x00000154,0x0004003d,0x0000000d,0x000001ed,0x000001ec,0x0004003d,
	0x0000000d,0x000001ee,0x000001aa,0x00050083,0x0000000d,0x000001ef,0x000001ed,0x000001ee,
	0x0006000c,0x0000000d,0x000001f0,0x00000001,0x00000045,0x000001ef,0x0003003e,0x000001ea,
	0x000001f0,0x0004003d,0x0000014d,0x000001f2,0x000001b1,0x00070041,0x000001c4,0x000001f3,
	0x000001be,0x00000165,0x000001f2,0x00000154,0x0004003d,0x0000000d,0x000001f4,0x000001f3,
	0x0004003d,0x0000000d,0x000001f5,0x000001aa,0x00050083,0x0000000d,0x000001f6,0x000001f4,
	0x000001f5,0x0006000c,0x00000006,0x000001f7,0x00000001,0x00000042,0x000001f6,0x0003003e,
	0x000001f1,0x000001f7,0x0004003d,0x00000006,0x000001f9,0x000001f1,0x0004003d,0x00000006,
	0x000001fa,0x000001f1,0x00050085,0x00000006,0x000001fb,0x000001f9,0x000001fa,0x00050088,
	0x00000006,0x000001fc,0x00000046,0x000001fb,0x0003003e,0x000001f8,0x000001fc,0x0004003d,
	0x0000014d,0x000001fe,0x000001b1,0x00070041,0x000001c4,0x000001ff,0x000001be,0x00000165,
	0x000001fe,0x0000016f,0x0004003d,0x0000000d,0x00000200,0x000001ff,0x0004003d,0x00000006,
	0x00000201,0x000001f8,0x0005008e,0x0000000d,0x00000202,0x00000200,0x00000201,0x0003003e,
	0x000001fd,0x00000202,0x0004003d,0x0000000d,0x00000204,0x0000019e,0x0003003e,0x00000203,
	0x00000204,0x0004003d,0x0000000d,0x00000206,0x000001a6,0x0003003e,0x00000205,0x00000206,
	0x0004003d,0x0000000d,0x00000208,0x000001ea,0x0003003e,0x00000207,0x00000208,0x0004003d,
	0x0000000d,0x0000020a,0x000001fd,0x0003003e,0x00000209,0x0000020a,0x0004003d,0x0000000d,
	0x0000020c,0x00000198,0x0003003e,0x0000020b,0x0000020c,0x0004003d,0x00000006,0x0000020e,
	0x0000016d,0x0003003e,0x0000020d,0x0000020e,0x0004003d,0x00000006,0x00000210,0x00000178,
	0x0003003e,0x0000020f,0x00000210,0x000b0039,0x0000000d,0x00000211,0x00000035,0x00000203,
	0x00000205,0x00000207,0x00000209,0x0000020b,0x0000020d,0x0000020f,0x0004003d,0x0000000d,
	0x00000212,0x000001ae,0x00050081,0x0000000d,0x00000213,0x00000212,0x00000211,0x0003003e,
	0x000001ae,0x00000213,0x0004003d,0x0000014d,0x00000214,0x000001b1,0x00050080,0x0000014d,
	0x00000215,0x00000214,0x00000165,0x0003003e,0x000001b1,0x00000215,0x000200f9,0x000001e4,
	0x000200f8,0x000001e4,0x0004003d,0x0000014d,0x00000216,0x000001e0,0x00050080,0x0000014d,
	0x00000217,0x00000216,0x00000165,0x0003003e,0x000001e0,0x00000217,0x000200f9,0x000001e1,
	0x000200f8,0x000001e3,0x0003003e,0x00000218,0x0000018c,0x000200f9,0x00000219,0x000200f8,
	0x00000219,0x000400f6,0x0000021b,0x0000021c,0x00000000,0x000200f9,0x0000021d,0x000200f8,
	0x0000021d,0x0004003d,0x0000014d,0x0000021e,0x00000218,0x00060041,0x00000158,0x0000021f,
	0x000001be,0x00000154,0x00000176,0x0004003d,0x0000014d,0x00000220,0x0000021f,0x000500b0,
	0x00000089,0x00000221,0x0000021e,0x00000220,0x000400fa,0x00000221,0x0000021a,0x0000021b,
	0x000200f8,0x0000021a,0x0004003d,0x0000014d,0x00000223,0x000001b1,0x00070041,0x000001c4,
	0x00000224,0x000001be,0x00000165,0x00000223,0x00000154,0x0004003d,0x0000000d,0x00000225,
	0x00000224,0x0004003d,0x0000000d,0x00000226,0x000001aa,0x00050083,0x0000000d,0x00000227,
	0x00000225,0x00000226,0x0006000c,0x0000000d,0x00000228,0x00000001,0x00000045,0x00000227,
	0x0003003e,0x00000222,0x00000228,0x0004003d,0x0000000d,0x0000022a,0x00000222,0x0004003d,
	0x0000014d,0x0000022b,0x000001b1,0x00070041,0x000001c4,0x0000022c,0x000001be,0x00000165,
	0x0000022b,0x00000165,0x0004003d,0x0000000d,0x0000022d,0x0000022c,0x0004007f,0x0000000d,
	0x0000022e,0x0000022d,0x0006000c,0x0000000d,0x0000022f,0x00000001,0x00000045,0x0000022e,
	0x00050094,0x00000006,0x00000230,0x0000022a,0x0000022f,0x0003003e,0x00000229,0x00000230,
	0x0004003d,0x0000014d,0x00000232,0x000001b1,0x00080041,0x00000233,0x00000234,0x000001be,
	0x00000165,0x00000232,0x0000017a,0x00000181,0x0004003d,0x00000006,0x00000235,0x00000234,
	0x0006000c,0x00000006,0x00000236,0x00000001,0x0000000e,0x00000235,0x0004003d,0x0000014d,
	0x00000237,0x000001b1,0x00080041,0x00000233,0x00000238,0x000001be,0x00000165,0x00000237,
	0x0000017a,0x0000018c,0x0004003d,0x00000006,0x00000239,0x00000238,0x0006000c,0x00000006,
	0x0000023a,0x00000001,0x0000000e,0x00000239,0x00050083,0x00000006,0x0000023b,0x00000236,
	0x0000023a,0x0003003e,0x00000231,0x0000023b,0x0004003d,0x00000006,0x0000023d,0x00000229,
	0x0004003d,0x0000014d,0x0000023e,0x000001b1,0x00080041,0x00000233,0x0000023f,0x000001be,
	0x00000165,0x0000023e,0x0000017a,0x0000018c,0x0004003d,0x00000006,0x00000240,0x0000023f,
	0x0006000c,0x00000006,0x00000241,0x00000001,0x0000000e,0x00000240,0x00050083,0x00000006,
	0x00000242,0x0000023d,0x00000241,0x0004003d,0x00000006,0x00000243,0x00000231,0x00050088,
	0x00000006,0x00000244,0x00000242,0x00000243,0x0008000c,0x00000006,0x00000245,0x00000001,
	0x0000002b,0x00000244,0x00000116,0x00000046,0x0003003e,0x0000023c,0x00000245,0x0004003d,
	0x00000006,0x00000246,0x0000023c,0x00050083,0x00000006,0x00000247,0x00000046,0x00000246,
	0x0003003e,0x0000023c,0x00000247,0x0004003d,0x0000014d,0x00000248,0x000001b1,0x00070041,
	0x000001c4,0x00000249,0x000001be,0x00000165,0x00000248,0x0000016f,0x0004003d,0x0000000d,
	0x0000024a,0x00000249,0x0004003d,0x00000006,0x0000024b,0x0000023c,0x0005008e,0x0000000d,
	0x0000024c,0x0000024a,0x0000024b,0x0004003d,0x0000000d,0x0000024d,0x00000198,0x0004003d,
	0x00000006,0x0000024e,0x0000023c,0x0005008e,0x0000000d,0x0000024f,0x0000024d,0x0000024e,
	0x0004003d,0x0000000d,0x00000251,0x0000019e,0x0003003e,0x00000250,0x00000251,0x0004003d,
	0x0000000d,0x00000253,0x000001a6,0x0003003e,0x00000252,0x00000253,0x0004003d,0x0000000d,
	0x00000255,0x00000222,0x0003003e,0x00000254,0x00000255,0x0003003e,0x00000256,0x0000024c,
	0x0003003e,0x00000257,0x0000024f,0x0004003d,0x00000006,0x00000259,0x0000016d,0x0003003e,
	0x00000258,0x00000259,0x0004003d,0x00000006,0x0000025b,0x00000178,0x0003003e,0x0000025a,
	0x0000025b,0x000b0039,0x0000000d,0x0000025c,0x00000035,0x00000250,0x00000252,0x00000254,
	0x00000256,0x00000257,0x00000258,0x0000025a,0x0004003d,0x0000000d,0x0000025d,0x000001ae,
	0x00050081,0x0000000d,0x0000025e,0x0000025d,0x0000025c,0x0003003e,0x000001ae,0x0000025e,
	0x0004003d,0x0000014d,0x0000025f,0x000001b1,0x00050080,0x0000014d,0x00000260,0x0000025f,
	0x00000165,0x0003003e,0x000001b1,0x00000260,0x000200f9,0x0000021c,0x000200f8,0x0000021c,
	0x0004003d,0x0000014d,0x00000261,0x00000218,0x00050080,0x0000014d,0x00000262,0x00000261,
	0x00000165,0x0003003e,0x00000218,0x00000262,0x000200f9,0x00000219,0x000200f8,0x0000021b,
	0x0004003d,0x00000006,0x00000263,0x00000183,0x0004003d,0x0000000d,0x00000264,0x000001ae,
	0x0005008e,0x0000000d,0x00000265,0x00000264,0x00000263,0x0003003e,0x000001ae,0x00000265,
	0x0004003d,0x0000000d,0x00000266,0x0000018e,0x0004003d,0x0000000d,0x00000267,0x000001ae,
	0x00050081,0x0000000d,0x00000268,0x00000267,0x00000266,0x0003003e,0x000001ae,0x00000268,
	0x0004003d,0x0000000d,0x0000026b,0x000001ae,0x00050051,0x00000006,0x0000026c,0x0000026b,
	0x00000000,0x00050051,0x00000006,0x0000026d,0x0000026b,0x00000001,0x00050051,0x00000006,
	0x0000026e,0x0000026b,0x00000002,0x00070050,0x00000145,0x0000026f,0x0000026c,0x0000026d,
	0x0000026e,0x00000046,0x0003003e,0x0000026a,0x0000026f,0x000100fd,0x00010038,0x00050036,
	0x00000006,0x0000000b,0x00000000,0x00000008,0x00030037,0x00000007,0x00000009,0x00030037,
	0x00000007,0x0000000a,0x000200f8,0x0000000c,0x0004003b,0x00000007,0x00000037,0x00000007,
	0x0004003b,0x00000007,0x0000003b,0x00000007,0x0004003b,0x00000007,0x0000003f,0x00000007,
	0x0004003b,0x00000007,0x00000043,0x00000007,0x0004003d,0x00000006,0x00000038,0x0000000a,
	0x0004003d,0x00000006,0x00000039,0x0000000a,0x00050085,0x00000006,0x0000003a,0x00000038,
	0x00000039,0x0003003e,0x00000037,0x0000003a,0x0004003d,0x00000006,0x0000003c,0x00000037,
	0x0004003d,0x00000006,0x0000003d,0x00000037,0x00050085,0x00000006,0x0000003e,0x0000003c,
	0x0000003d,0x0003003e,0x0000003b,0x0000003e,0x0004003d,0x00000006,0x00000040,0x00000009,
	0x0004003d,0x00000006,0x00000041,0x00000009,0x00050085,0x00000006,0x00000042,0x00000040,
	0x00000041,0x0003003e,0x0000003f,0x00000042,0x0004003d,0x00000006,0x00000044,0x0000003f,
	0x0004003d,0x00000006,0x00000045,0x0000003b,0x00050083,0x00000006,0x00000047,0x00000045,
	0x00000046,0x00050085,0x00000006,0x00000048,0x00000044,0x00000047,0x00050081,0x00000006,
	0x00000049,0x00000048,0x00000046,0x0003003e,0x00000043,0x00000049,0x0004003d,0x00000006,
	0x0000004a,0x0000003b,0x0004003d,0x00000006,0x0000004c,0x00000043,0x00050085,0x00000006,
	0x0000004d,0x0000004b,0x0000004c,0x0004003d,0x00000006,0x0000004e,0x00000043,0x00050085,
	0x00000006,0x0000004f,0x0000004d,0x0000004e,0x00050088,0x00000006,0x00000050,0x0000004a,
	0x0000004f,0x000200fe,0x00000050,0x00010038,0x00050036,0x0000000d,0x00000012,0x00000000,
	0x0000000f,0x00030037,0x00000007,0x00000010,0x00030037,0x0000000e,0x00000011,0x000200f8,
	0x00000013,0x0004003d,0x0000000d,0x00000053,0x00000011,0x0004003d,0x0000000d,0x00000054,
	0x00000011,0x00060050,0x0000000d,0x00000055,0x00000046,0x00000046,0x00000046,0x00050083,
	0x0000000d,0x00000056,0x00000055,0x00000054,0x0004003d,0x00000006,0x00000057,0x00000010,
	0x00050083,0x00000006,0x00000058,0x00000046,0x00000057,0x0007000c,0x00000006,0x0000005a,
	0x00000001,0x0000001a,0x00000058,0x00000059,0x0005008e,0x0000000d,0x0000005b,0x00000056,
	0x0000005a,0x00050081,0x0000000d,0x0000005c,0x00000053,0x0000005b,0x000200fe,0x0000005c,
	0x00010038,0x00050036,0x00000006,0x00000016,0x00000000,0x00000008,0x00030037,0x00000007,
	0x00000014,0x00030037,0x00000007,0x00000015,0x000200f8,0x00000017,0x0004003b,0x00000007,
	0x0000005f,0x00000007,0x0004003b,0x00000007,0x00000062,0x00000007,0x0004003b,0x00000007,
	0x00000068,0x00000007,0x0004003d,0x00000006,0x00000060,0x00000015,0x00050081,0x00000006,
	0x00000061,0x00000060,0x00000046,0x0003003e,0x0000005f,0x00000061,0x0004003d,0x00000006,
	0x00000063,0x0000005f,0x0004003d,0x00000006,0x00000064,0x0000005f,0x00050085,0x00000006,
	0x00000065,0x00000063,0x00000064,0x00050088,0x00000006,0x00000067,0x00000065,0x00000066,
	0x0003003e,0x00000062,0x00000067,0x0004003d,0x00000006,0x00000069,0x00000014,0x0004003d,
	0x00000006,0x0000006a,0x00000062,0x00050083,0x00000006,0x0000006b,0x00000046,0x0000006a,
	0x00050085,0x00000006,0x0000006c,0x00000069,0x0000006b,0x0004003d,0x00000006,0x0000006d,
	0x00000062,0x00050081,0x00000006,0x0000006e,0x0000006c,0x0000006d,0x0003003e,0x00000068,
	0x0000006e,0x0004003d,0x00000006,0x0000006f,0x00000014,0x0004003d,0x00000006,0x00000070,
	0x00000068,0x00050088,0x00000006,0x00000071,0x0000006f,0x00000070,0x000200fe,0x00000071,
	0x00010038,0x00050036,0x00000006,0x0000001c,0x00000000,0x00000018,0x00030037,0x00000007,
	0x00000019,0x00030037,0x00000007,0x0000001a,0x00030037,0x00000007,0x0000001b,0x000200f8,
	0x0000001d,0x0004003b,0x00000007,0x00000074,0x00000007,0x0004003b,0x00000007,0x00000075,
	0x00000007,0x0004003b,0x00000007,0x00000077,0x00000007,0x0004003b,0x00000007,0x0000007a,
	0x00000007,0x0004003b,0x00000007,0x0000007b,0x00000007,0x0004003b,0x00000007,0x0000007d,
	0x00000007,0x0004003d,0x00000006,0x00000076,0x0000001a,0x0003003e,0x00000075,0x00000076,
	0x0004003d,0x00000006,0x00000078,0x0000001b,0x0003003e,0x00000077,0x00000078,0x00060039,
	0x00000006,0x00000079,0x00000016,0x00000075,0x00000077,0x0003003e,0x00000074,0x00000079,
	0x0004003d,0x00000006,0x0000007c,0x00000019,0x0003003e,0x0000007b,0x0000007c,0x0004003d,
	0x00000006,0x0000007e,0x0000001b,0x0003003e,0x0000007d,0x0000007e,0x00060039,0x00000006,
	0x0000007f,0x00000016,0x0000007b,0x0000007d,0x0003003e,0x0000007a,0x0000007f,0x0004003d,
	0x00000006,0x00000080,0x00000074,0x0004003d,0x00000006,0x00000081,0x0000007a,0x00050085,
	0x00000006,0x00000082,0x00000080,0x00000081,0x000200fe,0x00000082,0x00010038,0x00050036,
	0x0000000d,0x00000020,0x00000000,0x0000001e,0x00030037,0x0000000e,0x0000001f,0x000200f8,
	0x00000021,0x0004003b,0x0000000e,0x00000085,0x00000007,0x0004003b,0x0000008b,0x0000008c,
	0x00000007,0x0004003b,0x0000000e,0x00000090,0x00000007,0x0004003b,0x0000000e,0x0000009c,
	0x00000007,0x0004003d,0x0000000d,0x00000086,0x0000001f,0x0004003d,0x0000000d,0x00000087,
	0x0000001f,0x00050085,0x0000000d,0x00000088,0x00000086,0x00000087,0x0003003e,0x00000085,
	0x00000088,0x0004003d,0x0000000d,0x0000008d,0x0000001f,0x000500b8,0x0000008a,0x0000008f,
	0x0000008d,0x0000008e,0x0003003e,0x0000008c,0x0000008f,0x0004003d,0x0000000d,0x00000091,
	0x00000085,0x0004003d,0x0000000d,0x00000092,0x00000085,0x00050085,0x0000000d,0x00000093,
	0x00000091,0x00000092,0x0004003d,0x0000000d,0x00000096,0x0000001f,0x00050085,0x0000000d,
	0x00000097,0x00000095,0x00000096,0x00050085,0x0000000d,0x00000098,0x00000093,0x00000097,
	0x00050088,0x0000000d,0x00000099,0x0000008e,0x00000098,0x0004003d,0x0000008a,0x0000009a,
	0x0000008c,0x000600a9,0x0000000d,0x0000009b,0x0000009a,0x00000099,0x0000008e,0x0003003e,
	0x00000090,0x0000009b,0x0004003d,0x0000000d,0x0000009d,0x0000001f,0x0004003d,0x0000000d,
	0x000000a0,0x0000001f,0x0005008e,0x0000000d,0x000000a2,0x000000a0,0x000000a1,0x0004003d,
	0x0000000d,0x000000a4,0x00000085,0x0005008e,0x0000000d,0x000000a5,0x000000a4,0x000000a3,
	0x00050083,0x0000000d,0x000000a6,0x000000a2,0x000000a5,0x00060050,0x0000000d,0x000000a8,
	0x000000a7,0x000000a7,0x000000a7,0x00050081,0x0000000d,0x000000a9,0x000000a6,0x000000a8,
	0x0004003d,0x0000008a,0x000000aa,0x0000008c,0x000600a9,0x0000000d,0x000000ab,0x000000aa,
	0x000000a9,0x0000009f,0x00050085,0x0000000d,0x000000ac,0x0000009d,0x000000ab,0x0003003e,
	0x0000009c,0x000000ac,0x0004003d,0x0000008a,0x000000b1,0x0000008c,0x000600a9,0x0000000d,
	0x000000b2,0x000000b1,0x000000b0,0x000000ae,0x0004003d,0x0000000d,0x000000b3,0x0000009c,
	0x00050081,0x0000000d,0x000000b4,0x000000b3,0x000000b2,0x0003003e,0x0000009c,0x000000b4,
	0x0004003d,0x0000000d,0x000000b5,0x0000009c,0x0004003d,0x0000000d,0x000000b6,0x00000090,
	0x00050085,0x0000000d,0x000000b7,0x000000b5,0x000000b6,0x000200fe,0x000000b7,0x00010038,
	0x00050036,0x0000000d,0x0000002b,0x00000000,0x00000022,0x00030037,0x00000007,0x00000023,
	0x00030037,0x00000007,0x00000024,0x00030037,0x00000007,0x00000025,0x00030037,0x00000007,
	0x00000026,0x00030037,0x00000007,0x00000027,0x00030037,0x0000000e,0x00000028,0x00030037,
	0x00000007,0x00000029,0x00030037,0x00000007,0x0000002a,0x000200f8,0x0000002c,0x0004003b,
	0x00000007,0x000000ba,0x00000007,0x0004003b,0x00000007,0x000000bb,0x00000007,0x0004003b,
	0x00000007,0x000000bd,0x00000007,0x0004003b,0x0000000e,0x000000c0,0x00000007,0x0004003b,
	0x00000007,0x000000c7,0x00000007,0x0004003b,0x0000000e,0x000000c9,0x00000007,0x0004003b,
	0x0000000e,0x000000cb,0x00000007,0x0004003b,0x00000007,0x000000d0,0x00000007,0x0004003b,
	0x0000000e,0x000000d2,0x00000007,0x0004003b,0x0000000e,0x000000d4,0x00000007,0x0004003b,
	0x00000007,0x000000d9,0x00000007,0x0004003b,0x0000000e,0x000000db,0x00000007,0x0004003b,
	0x00000007,0x000000dd,0x00000007,0x0004003b,0x00000007,0x000000de,0x00000007,0x0004003b,
	0x00000007,0x000000e0,0x00000007,0x0004003b,0x00000007,0x000000e2,0x00000007,0x0004003b,
	0x0000000e,0x000000e5,0x00000007,0x0004003b,0x0000000e,0x000000e8,0x00000007,0x0004003b,
	0x0000000e,0x000000ea,0x00000007,0x0004003d,0x00000006,0x000000bc,0x00000024,0x0003003e,
	0x000000bb,0x000000bc,0x0004003d,0x00000006,0x000000be,0x0000002a,0x0003003e,0x000000bd,
	0x000000be,0x00060039,0x00000006,0x000000bf,0x0000000b,0x000000bb,0x000000bd,0x0003003e,
	0x000000ba,0x000000bf,0x0004003d,0x0000000d,0x000000c3,0x00000028,0x0004003d,0x00000006,
	0x000000c4,0x00000029,0x00060050,0x0000000d,0x000000c5,0x000000c4,0x000000c4,0x000000c4,
	0x0008000c,0x0000000d,0x000000c6,0x00000001,0x0000002e,0x000000c2,0x000000c3,0x000000c5,
	0x0004003d,0x00000006,0x000000c8,0x00000023,0x0003003e,0x000000c7,0x000000c8,0x0003003e,
	0x000000c9,0x000000c6,0x00060039,0x0000000d,0x000000ca,0x00000012,0x000000c7,0x000000c9,
	0x0003003e,0x000000c0,0x000000ca,0x0004003d,0x0000000d,0x000000cc,0x00000028,0x0004003d,
	0x00000006,0x000000cd,0x00000029,0x00060050,0x0000000d,0x000000ce,0x000000cd,0x000000cd,
	0x000000cd,0x0008000c,0x0000000d,0x000000cf,0x00000001,0x0000002e,0x000000c2,0x000000cc,
	0x000000ce,0x0004003d,0x00000006,0x000000d1,0x00000026,0x0003003e,0x000000d0,0x000000d1,
	0x0003003e,0x000000d2,0x000000cf,0x00060039,0x0000000d,0x000000d3,0x00000012,0x000000d0,
	0x000000d2,0x0003003e,0x000000cb,0x000000d3,0x0004003d,0x0000000d,0x000000d5,0x00000028,
	0x0004003d,0x00000006,0x000000d6,0x00000029,0x00060050,0x0000000d,0x000000d7,0x000000d6,
	0x000000d6,0x000000d6,0x0008000c,0x0000000d,0x000000d8,0x00000001,0x0000002e,0x000000c2,
	0x000000d5,0x000000d7,0x0004003d,0x00000006,0x000000da,0x00000027,0x0003003e,0x000000d9,
	0x000000da,0x0003003e,0x000000db,0x000000d8,0x00060039,0x0000000d,0x000000dc,0x00000012,
	0x000000d9,0x000000db,0x0003003e,0x000000d4,0x000000dc,0x0004003d,0x00000006,0x000000df,
	0x00000026,0x0003003e,0x000000de,0x000000df,0x0004003d,0x00000006,0x000000e1,0x00000027,
	0x0003003e,0x000000e0,0x000000e1,0x0004003d,0x00000006,0x000000e3,0x0000002a,0x0003003e,
	0x000000e2,0x000000e3,0x00070039,0x00000006,0x000000e4,0x0000001c,0x000000de,0x000000e0,
	0x000000e2,0x0003003e,0x000000dd,0x000000e4,0x0003003e,0x000000e8,0x000000e7,0x00050039,
	0x0000000d,0x000000e9,0x00000020,0x000000e8,0x0003003e,0x000000e5,0x000000e9,0x0004003d,
	0x0000000d,0x000000eb,0x00000028,0x00060050,0x0000000d,0x000000ec,0x0000004b,0x0000004b,
	0x0000004b,0x00050088,0x0000000d,0x000000ed,0x000000eb,0x000000ec,0x0003003e,0x000000ea,
	0x000000ed,0x0004003d,0x00000006,0x000000ee,0x000000ba,0x0004003d,0x0000000d,0x000000ef,
	0x000000c0,0x0005008e,0x0000000d,0x000000f0,0x000000ef,0x000000ee,0x0004003d,0x00000006,
	0x000000f1,0x000000dd,0x0005008e,0x0000000d,0x000000f2,0x000000f0,0x000000f1,0x0004003d,
	0x00000006,0x000000f4,0x00000026,0x00050085,0x00000006,0x000000f5,0x000000f3,0x000000f4,
	0x0004003d,0x00000006,0x000000f6,0x00000027,0x00050085,0x00000006,0x000000f7,0x000000f5,
	0x000000f6,0x0007000c,0x00000006,0x000000f9,0x00000001,0x00000028,0x000000f7,0x000000f8,
	0x00060050,0x0000000d,0x000000fa,0x000000f9,0x000000f9,0x000000f9,0x00050088,0x0000000d,
	0x000000fb,0x000000f2,0x000000fa,0x0004003d,0x0000000d,0x000000fc,0x000000cb,0x00050083,
	0x0000000d,0x000000fd,0x0000008e,0x000000fc,0x0004003d,0x0000000d,0x000000fe,0x000000d4,
	0x00060050,0x0000000d,0x000000ff,0x00000046,0x00000046,0x00000046,0x00050083,0x0000000d,
	0x00000100,0x000000ff,0x000000fe,0x00050051,0x00000006,0x00000101,0x00000100,0x00000000,
	0x00050051,0x00000006,0x00000102,0x00000100,0x00000001,0x00050051,0x00000006,0x00000103,
	0x00000100,0x00000002,0x00060050,0x0000000d,0x00000104,0x00000101,0x00000102,0x00000103,
	0x00050085,0x0000000d,0x00000105,0x000000fd,0x00000104,0x0004003d,0x0000000d,0x00000106,
	0x000000ea,0x00050085,0x0000000d,0x00000107,0x00000105,0x00000106,0x0004003d,0x0000000d,
	0x00000108,0x000000e5,0x00050085,0x0000000d,0x00000109,0x00000107,0x00000108,0x00050081,
	0x0000000d,0x0000010a,0x000000fb,0x00000109,0x000200fe,0x0000010a,0x00010038,0x00050036,
	0x0000000d,0x00000035,0x00000000,0x0000002d,0x00030037,0x0000000e,0x0000002e,0x00030037,
	0x0000000e,0x0000002f,0x00030037,0x0000000e,0x00000030,0x00030037,0x0000000e,0x00000031,
	0x00030037,0x0000000e,0x00000032,0x00030037,0x00000007,0x00000033,0x00030037,0x00000007,
	0x00000034,0x000200f8,0x00000036,0x0004003b,0x0000000e,0x0000010d,0x00000007,0x0004003b,
	0x00000007,0x00000112,0x00000007,0x0004003b,0x00000007,0x00000118,0x00000007,0x0004003b,
	0x00000007,0x0000011d,0x00000007,0x0004003b,0x00000007,0x00000122,0x00000007,0x0004003b,
	0x00000007,0x00000127,0x00000007,0x0004003b,0x0000000e,0x0000012c,0x00000007,0x0004003b,
	0x00000007,0x0000012d,0x00000007,0x0004003b,0x00000007,0x0000012f,0x00000007,0x0004003b,
	0x00000007,0x00000131,0x00000007,0x0004003b,0x00000007,0x00000133,0x00000007,0x0004003b,
	0x00000007,0x00000135,0x00000007,0x0004003b,0x0000000e,0x00000137,0x00000007,0x0004003b,
	0x00000007,0x00000139,0x00000007,0x0004003b,0x00000007,0x0000013b,0x00000007,0x0004003d,
	0x0000000d,0x0000010e,0x0000002f,0x0004003d,0x0000000d,0x0000010f,0x00000030,0x00050081,
	0x0000000d,0x00000110,0x0000010e,0x0000010f,0x0006000c,0x0000000d,0x00000111,0x00000001,
	0x00000045,0x00000110,0x0003003e,0x0000010d,0x00000111,0x0004003d,0x0000000d,0x00000113,
	0x00000030,0x0004003d,0x0000000d,0x00000114,0x0000010d,0x00050094,0x00000006,0x00000115,
	0x00000113,0x00000114,0x0007000c,0x00000006,0x00000117,0x00000001,0x00000028,0x00000115,
	0x00000116,0x0003003e,0x00000112,0x00000117,0x0004003d,0x0000000d,0x00000119,0x0000002e,
	0x0004003d,0x0000000d,0x0000011a,0x0000010d,0x00050094,0x00000006,0x0000011b,0x00000119,
	0x0000011a,0x0007000c,0x00000006,0x0000011c,0x00000001,0x00000028,0x0000011b,0x00000116,
	0x0003003e,0x00000118,0x0000011c,0x0004003d,0x0000000d,0x0000011e,0x0000002f,0x0004003d,
	0x0000000d,0x0000011f,0x0000010d,0x00050094,0x00000006,0x00000120,0x0000011e,0x0000011f,
	0x0007000c,0x00000006,0x00000121,0x00000001,0x00000028,0x00000120,0x00000116,0x0003003e,
	0x0000011d,0x00000121,0x0004003d,0x0000000d,0x00000123,0x00000030,0x0004003d,0x0000000d,
	0x00000124,0x0000002e,0x00050094,0x00000006,0x00000125,0x00000123,0x00000124,0x0007000c,
	0x00000006,0x00000126,0x00000001,0x00000028,0x00000125,0x00000116,0x0003003e,0x00000122,
	0x00000126,0x0004003d,0x0000000d,0x00000128,0x0000002f,0x0004003d,0x0000000d,0x00000129,
	0x0000002e,0x00050094,0x00000006,0x0000012a,0x00000128,0x00000129,0x0007000c,0x00000006,
	0x0000012b,0x00000001,0x00000028,0x0000012a,0x00000116,0x0003003e,0x00000127,0x0000012b,
	0x0004003d,0x00000006,0x0000012e,0x00000112,0x0003003e,0x0000012d,0x0000012e,0x0004003d,
	0x00000006,0x00000130,0x00000118,0x0003003e,0x0000012f,0x00000130,0x0004003d,0x00000006,
	0x00000132,0x0000011d,0x0003003e,0x00000131,0x00000132,0x0004003d,0x00000006,0x00000134,
	0x00000122,0x0003003e,0x00000133,0x00000134,0x0004003d,0x00000006,0x00000136,0x00000127,
	0x0003003e,0x00000135,0x00000136,0x0004003d,0x0000000d,0x00000138,0x00000032,0x0003003e,
	0x00000137,0x00000138,0x0004003d,0x00000006,0x0000013a,0x00000033,0x0003003e,0x00000139,
	0x0000013a,0x0004003d,0x00000006,0x0000013c,0x00000034,0x0003003e,0x0000013b,0x0000013c,
	0x000c0039,0x0000000d,0x0000013d,0x0000002b,0x0000012d,0x0000012f,0x00000131,0x00000133,
	0x00000135,0x00000137,0x00000139,0x0000013b,0x0003003e,0x0000012c,0x0000013d,0x0004003d,
	0x0000000d,0x0000013e,0x00000031,0x0004003d,0x0000000d,0x0000013f,0x0000012c,0x00050085,
	0x0000000d,0x00000140,0x0000013e,0x0000013f,0x0004003d,0x00000006,0x00000141,0x00000122,
	0x0005008e,0x0000000d,0x00000142,0x00000140,0x00000141,0x000200fe,0x00000142,0x00010038
	};

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderCode.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderCode.data();
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
	vertexInputBindingDescription.stride = sizeof(NtshEngn::Vertex);
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
	vertexNormalInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, normal);

	VkVertexInputAttributeDescription vertexUVInputAttributeDescription = {};
	vertexUVInputAttributeDescription.location = 2;
	vertexUVInputAttributeDescription.binding = 0;
	vertexUVInputAttributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
	vertexUVInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, uv);

	VkVertexInputAttributeDescription vertexColorInputAttributeDescription = {};
	vertexColorInputAttributeDescription.location = 3;
	vertexColorInputAttributeDescription.binding = 0;
	vertexColorInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexColorInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, color);

	VkVertexInputAttributeDescription vertexTangentInputAttributeDescription = {};
	vertexTangentInputAttributeDescription.location = 4;
	vertexTangentInputAttributeDescription.binding = 0;
	vertexTangentInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexTangentInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, tangent);

	std::array<VkVertexInputAttributeDescription, 5> vertexInputAttributeDescriptions = { vertexPositionInputAttributeDescription, vertexNormalInputAttributeDescription, vertexUVInputAttributeDescription, vertexColorInputAttributeDescription, vertexTangentInputAttributeDescription };

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
	colorBlendAttachmentState.colorWriteMask = { VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT };

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
	dynamicStateCreateInfo.dynamicStateCount = 2;
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
	
	VkDescriptorPoolSize materialsDescriptorPoolSize = {};
	materialsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize texturesDescriptorPoolSize = {};
	texturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 5> descriptorPoolSizes = { cameraDescriptorPoolSize, objectsDescriptorPoolSize, materialsDescriptorPoolSize, lightsDescriptorPoolSize, texturesDescriptorPoolSize };
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
		VkDescriptorBufferInfo objectsDescriptorBufferInfo;
		VkDescriptorBufferInfo materialsDescriptorBufferInfo;
		VkDescriptorBufferInfo lightsDescriptorBufferInfo;

		cameraDescriptorBufferInfo.buffer = m_cameraBuffers[i];
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(nml::mat4) * 2 + sizeof(nml::vec4);

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

		objectsDescriptorBufferInfo.buffer = m_objectBuffers[i];
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

		materialsDescriptorBufferInfo.buffer = m_materialBuffers[i];
		materialsDescriptorBufferInfo.offset = 0;
		materialsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet materialsDescriptorWriteDescriptorSet = {};
		materialsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialsDescriptorWriteDescriptorSet.pNext = nullptr;
		materialsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		materialsDescriptorWriteDescriptorSet.dstBinding = 2;
		materialsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		materialsDescriptorWriteDescriptorSet.descriptorCount = 1;
		materialsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materialsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		materialsDescriptorWriteDescriptorSet.pBufferInfo = &materialsDescriptorBufferInfo;
		materialsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(materialsDescriptorWriteDescriptorSet);

		lightsDescriptorBufferInfo.buffer = m_lightBuffers[i];
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 3;
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
	for (size_t j = 0; j < m_textures.size(); j++) {
		texturesDescriptorImageInfos[j].sampler = m_textureSamplers[m_textures[j].samplerIndex];
		texturesDescriptorImageInfos[j].imageView = m_textureImageViews[m_textures[j].imageIndex];
		texturesDescriptorImageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 4;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createDefaultResources() {
	// Default mesh
	m_defaultMesh.vertices = {
		{ {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f} },
		{ {0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f} },
		{ {-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f} }
	};
	m_defaultMesh.indices = {
		0,
		1,
		2
	};

	load(m_defaultMesh);

	// Create texture sampler
	VkSampler defaultTextureSampler;
	VkSamplerCreateInfo textureSamplerCreateInfo = {};
	textureSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	textureSamplerCreateInfo.pNext = nullptr;
	textureSamplerCreateInfo.flags = 0;
	textureSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	textureSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	textureSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	textureSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
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

	m_textureSamplers.push_back(defaultTextureSampler);

	// Default diffuse texture
	m_defaultDiffuseTexture.width = 16;
	m_defaultDiffuseTexture.height = 16;
	m_defaultDiffuseTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultDiffuseTexture.colorSpace = NtshEngn::ImageColorSpace::SRGB;
	m_defaultDiffuseTexture.data.resize(m_defaultDiffuseTexture.width * m_defaultDiffuseTexture.height * 4 * 1);
	for (size_t i = 0; i < 256; i++) {
		m_defaultDiffuseTexture.data[i * 4 + 0] = static_cast<uint8_t>(255 - i);
		m_defaultDiffuseTexture.data[i * 4 + 1] = static_cast<uint8_t>(i % 128);
		m_defaultDiffuseTexture.data[i * 4 + 2] = static_cast<uint8_t>(i);
		m_defaultDiffuseTexture.data[i * 4 + 3] = static_cast<uint8_t>(255);
	}

	load(m_defaultDiffuseTexture);
	m_textures.push_back({ 0, 0 });

	// Default normal texture
	m_defaultNormalTexture.width = 1;
	m_defaultNormalTexture.height = 1;
	m_defaultNormalTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultNormalTexture.colorSpace = NtshEngn::ImageColorSpace::Linear;
	m_defaultNormalTexture.data = { 127, 127, 255, 255 };

	load(m_defaultNormalTexture);
	m_textures.push_back({ 1, 0 });

	// Default metalness texture
	m_defaultMetalnessTexture.width = 1;
	m_defaultMetalnessTexture.height = 1;
	m_defaultMetalnessTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultMetalnessTexture.colorSpace = NtshEngn::ImageColorSpace::Linear;
	m_defaultMetalnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultMetalnessTexture);
	m_textures.push_back({ 2, 0 });

	// Default roughness texture
	m_defaultRoughnessTexture.width = 1;
	m_defaultRoughnessTexture.height = 1;
	m_defaultRoughnessTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultRoughnessTexture.colorSpace = NtshEngn::ImageColorSpace::Linear;
	m_defaultRoughnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultRoughnessTexture);
	m_textures.push_back({ 3, 0 });

	// Default occlusion texture
	m_defaultOcclusionTexture.width = 1;
	m_defaultOcclusionTexture.height = 1;
	m_defaultOcclusionTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultOcclusionTexture.colorSpace = NtshEngn::ImageColorSpace::Linear;
	m_defaultOcclusionTexture.data = { 255, 255, 255, 255 };

	load(m_defaultOcclusionTexture);
	m_textures.push_back({ 4, 0 });

	// Default emissive texture
	m_defaultEmissiveTexture.width = 1;
	m_defaultEmissiveTexture.height = 1;
	m_defaultEmissiveTexture.format = NtshEngn::ImageFormat::R8G8B8A8;
	m_defaultEmissiveTexture.colorSpace = NtshEngn::ImageColorSpace::SRGB;
	m_defaultEmissiveTexture.data = { 0, 0, 0, 255 };

	load(m_defaultEmissiveTexture);
	m_textures.push_back({ 5, 0 });
}

void NtshEngn::GraphicsModule::resize() {
	if (m_windowModule && m_windowModule->isOpen(NTSHENGN_MAIN_WINDOW)) {
		while (m_windowModule->getWidth(NTSHENGN_MAIN_WINDOW) == 0 || m_windowModule->getHeight(NTSHENGN_MAIN_WINDOW) == 0) {
			m_windowModule->pollEvents();
		}

		NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

		// Destroy swapchain image views
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}

		// Recreate the swapchain
		createSwapchain(m_swapchain);
		
		// Destroy depth image and image view
		vkDestroyImageView(m_device, m_depthImageView, nullptr);
		vmaDestroyImage(m_allocator, m_depthImage, m_depthImageAllocation);

		// Recreate depth image
		createDepthImage();
	}
}

uint32_t NtshEngn::GraphicsModule::createSampler(const NtshEngn::ImageSampler& sampler) {
	const std::unordered_map<NtshEngn::ImageSamplerFilter, VkFilter> filterMap{ { NtshEngn::ImageSamplerFilter::Linear, VK_FILTER_LINEAR },
	{ NtshEngn::ImageSamplerFilter::Nearest, VK_FILTER_NEAREST },
	{ NtshEngn::ImageSamplerFilter::Unknown, VK_FILTER_LINEAR }
	};
	const std::unordered_map<NtshEngn::ImageSamplerFilter, VkSamplerMipmapMode> mipmapFilterMap{ { NtshEngn::ImageSamplerFilter::Linear, VK_SAMPLER_MIPMAP_MODE_LINEAR },
	{ NtshEngn::ImageSamplerFilter::Nearest, VK_SAMPLER_MIPMAP_MODE_NEAREST },
	{ NtshEngn::ImageSamplerFilter::Unknown, VK_SAMPLER_MIPMAP_MODE_LINEAR }
	};
	const std::unordered_map<NtshEngn::ImageSamplerAddressMode, VkSamplerAddressMode> addressModeMap{ { NtshEngn::ImageSamplerAddressMode::Repeat, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ NtshEngn::ImageSamplerAddressMode::MirroredRepeat, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT },
	{ NtshEngn::ImageSamplerAddressMode::ClampToEdge, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ NtshEngn::ImageSamplerAddressMode::ClampToBorder, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER },
	{ NtshEngn::ImageSamplerAddressMode::Unknown, VK_SAMPLER_ADDRESS_MODE_REPEAT }
	};
	const std::unordered_map<NtshEngn::ImageSamplerBorderColor, VkBorderColor> borderColorMap{ { NtshEngn::ImageSamplerBorderColor::FloatTransparentBlack, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
	{ NtshEngn::ImageSamplerBorderColor::IntTransparentBlack, VK_BORDER_COLOR_INT_TRANSPARENT_BLACK },
	{ NtshEngn::ImageSamplerBorderColor::FloatOpaqueBlack, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
	{ NtshEngn::ImageSamplerBorderColor::IntOpaqueBlack, VK_BORDER_COLOR_INT_OPAQUE_BLACK },
	{ NtshEngn::ImageSamplerBorderColor::FloatOpaqueWhite, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
	{ NtshEngn::ImageSamplerBorderColor::IntOpaqueWhite, VK_BORDER_COLOR_INT_OPAQUE_WHITE },
	{ NtshEngn::ImageSamplerBorderColor::Unknown, VK_BORDER_COLOR_INT_OPAQUE_BLACK },
	};

	VkSampler newSampler;

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = filterMap.at(sampler.magFilter);
	samplerCreateInfo.minFilter = filterMap.at(sampler.minFilter);
	samplerCreateInfo.mipmapMode = mipmapFilterMap.at(sampler.mipmapFilter);
	samplerCreateInfo.addressModeU = addressModeMap.at(sampler.addressModeU);
	samplerCreateInfo.addressModeV = addressModeMap.at(sampler.addressModeV);
	samplerCreateInfo.addressModeW = addressModeMap.at(sampler.addressModeW);
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = sampler.anisotropyLevel > 0.0f ? VK_TRUE : VK_FALSE;
	samplerCreateInfo.maxAnisotropy = sampler.anisotropyLevel;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = borderColorMap.at(sampler.borderColor);
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &newSampler));

	m_textureSamplers.push_back(newSampler);

	return static_cast<uint32_t>(m_textureSamplers.size() - 1);
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