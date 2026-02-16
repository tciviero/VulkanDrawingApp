/* Copyright (c) 2025-2026, Sascha Willems
 * SPDX-License-Identifier: MIT
 */
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "slang-com-ptr.h"
#include "slang.h"
#include <ktx.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

constexpr uint32_t maxFramesInFlight{ 2 };
uint32_t imageIndex{ 0 };
uint32_t frameIndex{ 0 };
VkInstance instance{ VK_NULL_HANDLE };
VkDevice device{ VK_NULL_HANDLE };
VkQueue queue{ VK_NULL_HANDLE };
VkSurfaceKHR surface{ VK_NULL_HANDLE };
bool updateSwapchain{ false };
VkSwapchainKHR swapchain{ VK_NULL_HANDLE };
VkCommandPool commandPool{ VK_NULL_HANDLE };
VkPipeline pipeline{ VK_NULL_HANDLE };
VkPipeline displayPipeline{ VK_NULL_HANDLE };
VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
VkImage depthImage;
VkImage textImage;
VkSampler textSampler;
VmaAllocator allocator{ VK_NULL_HANDLE };
VmaAllocation depthImageAllocation;
VmaAllocation textImageAllocation;
VkImageView depthImageView;
VkImageView textImageView;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers;
std::array<VkFence, maxFramesInFlight> fences;
std::array<VkSemaphore, maxFramesInFlight> presentSemaphores;
std::vector<VkSemaphore> renderSemaphores;

struct MouseData {
	float x;
	float y;
	float lastX;
	float lastY;
	bool erase;
	alignas(16) glm::vec3 paintColor;

} mouseData{};

VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
VkDescriptorSetLayout descriptorSetLayoutTex{ VK_NULL_HANDLE };
VkDescriptorSet descriptorSetTex{ VK_NULL_HANDLE };
Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
glm::ivec2 windowSize{};

struct Vertex {
	glm::vec2 pos;
	glm::vec2 uv;
};

static inline void chk(VkResult result) {
	if (result != VK_SUCCESS) [[unlikely]] {
		std::cerr << "Vulkan call returned an error (" << result << ")\n";
		exit(result);
	}
}

static inline void chkSwapchain(VkResult result) {
	if (result < VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			updateSwapchain = true;
			return;
		}
		std::cerr << "Vulkan call returned an error (" << result << ")\n";
		exit(result);
	}
}

static inline void chk(bool result) {
	if (!result) [[unlikely]] {
		std::cerr << "Call returned an error\n";
		exit(result);
	}
}

int main(int argc, char *argv[]) {
	chk(SDL_Init(SDL_INIT_VIDEO));
	chk(SDL_Vulkan_LoadLibrary(NULL));
	volkInitialize();
	VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "How to Vulkan",
		.apiVersion = VK_API_VERSION_1_3 };
	uint32_t instanceExtensionsCount{ 0 };
	char const *const *instanceExtensions{
		SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount)
	};
	VkInstanceCreateInfo instanceCI{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = instanceExtensionsCount,
		.ppEnabledExtensionNames = instanceExtensions,
	};

	chk(vkCreateInstance(&instanceCI, nullptr, &instance));
	volkLoadInstance(instance);

	uint32_t deviceCount{ 0 };
	chk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
	std::vector<VkPhysicalDevice> devices(deviceCount);
	chk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));
	uint32_t deviceIndex{ 0 };
	if (argc > 1) {
		deviceIndex = std::stoi(argv[1]);
		assert(deviceIndex < deviceCount);
	}
	VkPhysicalDeviceProperties2 deviceProperties{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
	};
	vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
	std::cout << "Selected device: " << deviceProperties.properties.deviceName << "\n";
	std::cout << "Max push constant size : " << deviceProperties.properties.limits.maxPushConstantsSize << "\n";
	uint32_t queueFamilyCount{ 0 };
	vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex],
			&queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(
			devices[deviceIndex], &queueFamilyCount, queueFamilies.data());
	uint32_t queueFamily{ 0 };
	for (size_t i = 0; i < queueFamilies.size(); i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queueFamily = i;
			break;
		}
	}
	chk(SDL_Vulkan_GetPresentationSupport(instance, devices[deviceIndex],
			queueFamily));
	const float qfpriorities{ 1.0f };
	VkDeviceQueueCreateInfo queueCI{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queueFamily,
		.queueCount = 1,
		.pQueuePriorities = &qfpriorities
	};
	VkPhysicalDeviceVulkan12Features enabledVk12Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.descriptorIndexing = true,
		.shaderSampledImageArrayNonUniformIndexing = true,
		.descriptorBindingVariableDescriptorCount = true,
		.runtimeDescriptorArray = true,
		.bufferDeviceAddress = true
	};
	VkPhysicalDeviceVulkan13Features enabledVk13Features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &enabledVk12Features,
		.synchronization2 = true,
		.dynamicRendering = true
	};
	const std::vector<const char *> deviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	const VkPhysicalDeviceFeatures enabledVk10Features{ .samplerAnisotropy =
																VK_TRUE };
	VkDeviceCreateInfo deviceCI{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &enabledVk13Features,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCI,
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &enabledVk10Features
	};
	chk(vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &device));
	vkGetDeviceQueue(device, queueFamily, 0, &queue);
	VmaVulkanFunctions vkFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkCreateImage = vkCreateImage };
	VmaAllocatorCreateInfo allocatorCI{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = devices[deviceIndex],
		.device = device,
		.pVulkanFunctions = &vkFunctions,
		.instance = instance
	};
	chk(vmaCreateAllocator(&allocatorCI, &allocator));
	SDL_Window *window = SDL_CreateWindow(
			"How to Vulkan", 1920, 1080u, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
	assert(window);
	chk(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));
	chk(SDL_GetWindowSize(window, &windowSize.x, &windowSize.y));
	VkSurfaceCapabilitiesKHR surfaceCaps{};
	chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[deviceIndex], surface,
			&surfaceCaps));

	int width, height;
	SDL_GetWindowSizeInPixels(window, &width, &height);
	VkExtent2D swapchainExtent;
	if (surfaceCaps.currentExtent.width != 0xFFFFFFFF) {
		swapchainExtent = surfaceCaps.currentExtent;
	} else {
		swapchainExtent.width = std::clamp((uint32_t)width,
				surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
		swapchainExtent.height = std::clamp((uint32_t)height,
				surfaceCaps.minImageExtent.height,
				surfaceCaps.maxImageExtent.height);
	}
	const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
	VkSwapchainCreateInfoKHR swapchainCI{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = surfaceCaps.minImageCount,
		.imageFormat = imageFormat,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};
	chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));
	uint32_t imageCount{ 0 };
	chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
	swapchainImages.resize(imageCount);
	chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

	swapchainImageViews.resize(imageCount);
	for (auto i = 0; i < imageCount; i++) {
		VkImageViewCreateInfo viewCI{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = imageFormat,
			.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1 }
		};
		chk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
	}

	VkSemaphoreCreateInfo semaphoreCI{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT };
	for (auto i = 0; i < maxFramesInFlight; i++) {
		chk(vkCreateFence(device, &fenceCI, nullptr, &fences[i]));
		chk(vkCreateSemaphore(device, &semaphoreCI, nullptr,
				&presentSemaphores[i]));
	}
	renderSemaphores.resize(swapchainImages.size());
	for (auto &semaphore : renderSemaphores) {
		chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
	}
	VkCommandPoolCreateInfo commandPoolCI{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamily
	};
	chk(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool));
	VkCommandBufferAllocateInfo cbAllocCI{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.commandBufferCount = maxFramesInFlight
	};
	chk(vkAllocateCommandBuffers(device, &cbAllocCI, commandBuffers.data()));
	VkDescriptorSetLayoutBinding descLayoutBindingTex{
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};
	VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &descLayoutBindingTex
	};
	chk(vkCreateDescriptorSetLayout(device, &descLayoutTexCI, nullptr,
			&descriptorSetLayoutTex));
	VkDescriptorPoolSize poolSize{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
	};
	VkDescriptorPoolCreateInfo descPoolCI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &poolSize
	};
	chk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));
	VkDescriptorSetAllocateInfo texDescSetAlloc{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptorSetLayoutTex
	};
	chk(vkAllocateDescriptorSets(device, &texDescSetAlloc, &descriptorSetTex));

	VkImageCreateInfo canvasCI{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_SRGB,
		.extent = { 1920, 1080, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT
	};
	VmaAllocationCreateInfo allocCI{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	chk(vmaCreateImage(allocator, &canvasCI, &allocCI, &textImage, &textImageAllocation, nullptr));
	VkImageViewCreateInfo textViewCI{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = textImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_SRGB,
		.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1 }
	};
	chk(vkCreateImageView(device, &textViewCI, nullptr, &textImageView));
	VkSamplerCreateInfo samplerCI{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 8.0f,
		.maxLod = 0.0,
	};
	chk(vkCreateSampler(device, &samplerCI, nullptr, &textSampler));
	VkDescriptorImageInfo descImageInfo{
		.sampler = textSampler,
		.imageView = textImageView,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet writeDescSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptorSetTex,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &descImageInfo,
	};
	vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
	VkBuffer imgSrcBuffer{};
	VmaAllocation imgSrcAllocation{};
	uint32_t textSize = 1920 * 1080 * 4;
	VkBufferCreateInfo imgSrcBufferCI{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (uint32_t)textSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	VmaAllocationCreateInfo imgSrcAllocCI{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	chk(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, nullptr));
	void *imgSrcBufferPtr{ nullptr };
	chk(vmaMapMemory(allocator, imgSrcAllocation, &imgSrcBufferPtr));
	memset(imgSrcBufferPtr, 255, textSize);

	VkFenceCreateInfo fenceOneTimeCI{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};
	VkFence fenceOneTime{};
	chk(vkCreateFence(device, &fenceOneTimeCI, nullptr, &fenceOneTime));
	VkCommandBuffer cbOneTime{};
	VkCommandBufferAllocateInfo cbOneTimeAI{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.commandBufferCount = 1
	};
	chk(vkAllocateCommandBuffers(device, &cbOneTimeAI, &cbOneTime));

	VkCommandBufferBeginInfo cbOneTimeBI{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	chk(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
	VkImageMemoryBarrier2 barrierTexImage{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = textImage,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
	};
	VkDependencyInfo barrierTexInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrierTexImage
	};
	vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
	VkBufferImageCopy textBufferIC{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { 1920, 1080, 1 },
	};
	vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textBufferIC);
	VkImageMemoryBarrier2 barrierTexRead{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		.image = textImage,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
	};
	barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
	vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
	chk(vkEndCommandBuffer(cbOneTime));
	VkSubmitInfo oneTimeSI{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cbOneTime
	};
	chk(vkQueueSubmit(queue, 1, &oneTimeSI, fenceOneTime));
	chk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
	slang::createGlobalSession(slangGlobalSession.writeRef());
	auto slangTargets{ std::to_array<slang::TargetDesc>(
			{ { .format{ SLANG_SPIRV },
					.profile{ slangGlobalSession->findProfile("spirv_1_4") } } }) };
	auto slangOptions{ std::to_array<slang::CompilerOptionEntry>(
			{ { slang::CompilerOptionName::EmitSpirvDirectly,
					{ slang::CompilerOptionValueKind::Int, 1 } } }) };
	slang::SessionDesc slangSessionDesc{
		.targets{ slangTargets.data() },
		.targetCount{ SlangInt(slangTargets.size()) },
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.compilerOptionEntries{ slangOptions.data() },
		.compilerOptionEntryCount{ uint32_t(slangOptions.size()) }
	};
	Slang::ComPtr<slang::ISession> slangSession;
	slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());
	Slang::ComPtr<slang::IModule> slangModule{ slangSession->loadModuleFromSource(
			"triangle", "assets/shader.slang", nullptr, nullptr) };
	Slang::ComPtr<ISlangBlob> spirv;
	slangModule->getTargetCode(0, spirv.writeRef());
	VkShaderModuleCreateInfo shaderModuleCI{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = spirv->getBufferSize(),
		.pCode = (uint32_t *)spirv->getBufferPointer()
	};
	VkShaderModule shaderModule{};
	chk(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));

	VkPushConstantRange pushConstantRange{ .stageFlags =
												   VK_SHADER_STAGE_FRAGMENT_BIT,
		.size = sizeof(MouseData) };
	VkPipelineLayoutCreateInfo pipelineLayoutCI{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayoutTex,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	chk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr,
			&pipelineLayout));
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = shaderModule,
				.pName = "vertexMain" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = shaderModule,
				.pName = "fragmentPaint" },
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates.data()
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};
	VkPipelineRasterizationStateCreateInfo rasterizationState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.0f
	};
	VkPipelineMultisampleStateCreateInfo multisampleState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineColorBlendAttachmentState blendAttachment{ .colorWriteMask = 0xF };
	VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment
	};
	VkPipelineRenderingCreateInfo renderingCI{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &imageFormat,
	};
	VkGraphicsPipelineCreateInfo pipelineCI{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingCI,
		.stageCount = 2,
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = pipelineLayout
	};
	chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr,
			&pipeline));

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages2{
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = shaderModule,
				.pName = "vertexMain" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = shaderModule,
				.pName = "fragmentDisplay" },
	};
	pipelineCI.pStages = shaderStages2.data();
	chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr,
			&displayPipeline));
	SDL_SetWindowMouseGrab(window, true);
	bool quit{ false };
	bool clear{ false };
	mouseData.lastX = 0.0f;
	mouseData.lastY = 0.0f;
	while (!quit) {
		chk(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
		chk(vkResetFences(device, 1, &fences[frameIndex]));
		chkSwapchain(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
				presentSemaphores[frameIndex],
				VK_NULL_HANDLE, &imageIndex));
		auto cb = commandBuffers[frameIndex];

		bool moved = false;
		mouseData.erase = false;
		for (SDL_Event event; SDL_PollEvent(&event);) {
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				if (event.button.button == SDL_BUTTON_LEFT) {
					moved = true;
					mouseData.erase = false;
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					moved = true;
					mouseData.erase = true;
				}
			}

			if (event.type == SDL_EVENT_MOUSE_MOTION) {
				if (event.motion.state & SDL_BUTTON_LMASK) {
					moved = true;
					mouseData.erase = false;
				} else if (event.motion.state & SDL_BUTTON_RMASK) {
					moved = true;
					mouseData.erase = true;
				}
			}
			if (event.type == SDL_EVENT_QUIT) {
				quit = true;
			}
			clear = false;
			if (event.type == SDL_EVENT_KEY_UP) {
				switch (event.key.key) {
					case SDLK_SPACE:
						clear = true;
						break;
					case SDLK_1:
						mouseData.paintColor = { 1.0f, 0.0f, 0.0f };
						break;
					case SDLK_2:
						mouseData.paintColor = { 1.0f, 1.0f, 0.0f };
						break;
					case SDLK_3:
						mouseData.paintColor = { 0.0f, 1.0f, 0.0f };
						break;
					case SDLK_4:
						mouseData.paintColor = { 0.0f, 1.0f, 1.0f };
						break;
					case SDLK_5:
						mouseData.paintColor = { 0.0f, 0.0f, 1.0f };
						break;
					case SDLK_6:
						mouseData.paintColor = { 1.0f, 0.0f, 1.0f };
						break;
					case SDLK_7:
						mouseData.paintColor = { 1.0f, 0.5f, 0.0f };
						break;
					case SDLK_8:
						mouseData.paintColor = { 0.75f, 0.75f, 0.25f };
						break;
					case SDLK_9:
						mouseData.paintColor = { 0.0f, 0.0f, 0.0f };
						break;
					case SDLK_0:
						mouseData.paintColor = { 0.5f, 0.5f, 0.5f };
						break;
					default:
						break;
				}
			}
		}

		chk(vkResetCommandBuffer(cb, 0));
		VkCommandBufferBeginInfo cbBI{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		chk(vkBeginCommandBuffer(cb, &cbBI));
		float mx, my;
		SDL_GetMouseState(&mx, &my);
		mouseData.x = mx / (float)windowSize.x;
		mouseData.y = my / (float)windowSize.y;

		if (moved || clear) {
			VkAttachmentLoadOp attachment_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
			if (clear) {
				attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
			}
			VkImageMemoryBarrier2 outputBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				.image = textImage,
				.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1 },
			};
			VkDependencyInfo barrierDependencyInfo{
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &outputBarrier
			};
			vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);
			VkRenderingAttachmentInfo colorAttachmentInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = textImageView,
				.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				.loadOp = attachment_load_op,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue{ .color{ 1.0f, 1.0f, 1.0f, 1.0f } }
			};
			VkRenderingInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea{ .extent{ .width = static_cast<uint32_t>(windowSize.x),
						.height = static_cast<uint32_t>(windowSize.y) } },
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachmentInfo,
			};
			vkCmdBeginRendering(cb, &renderingInfo);
			if (moved) {
				VkViewport vp{ .width = static_cast<float>(windowSize.x),
					.height = static_cast<float>(windowSize.y),
					.minDepth = 0.0f,
					.maxDepth = 1.0f };
				vkCmdSetViewport(cb, 0, 1, &vp);
				VkRect2D scissor{ .extent{ .width = static_cast<uint32_t>(windowSize.x),
						.height = static_cast<uint32_t>(windowSize.y) } };
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdSetScissor(cb, 0, 1, &scissor);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
						0, 1, &descriptorSetTex, 0, nullptr);
				vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						sizeof(MouseData),
						&mouseData);
				vkCmdDraw(cb, 6, 1, 0, 0);
				vkCmdEndRendering(cb);
			}
			VkImageMemoryBarrier2 readBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.image = textImage,
				.subresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1 }
			};

			VkDependencyInfo depRead{
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &readBarrier
			};

			vkCmdPipelineBarrier2(cb, &depRead);
		}
		VkImageMemoryBarrier2 swapchainToAttachBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = swapchainImages[imageIndex],
			.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1 }
		};
		VkDependencyInfo barrierPresentDependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &swapchainToAttachBarrier,
		};
		vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);
		VkRenderingAttachmentInfo colorAttachmentInfo2{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = swapchainImageViews[imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue{ .color{ 0.0f, 0.0f, 0.0f, 1.0f } }
		};
		VkRenderingInfo renderingInfo2{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea{ .extent{ .width = static_cast<uint32_t>(windowSize.x),
					.height = static_cast<uint32_t>(windowSize.y) } },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo2,
		};
		vkCmdBeginRendering(cb, &renderingInfo2);
		VkViewport vp2{ .width = static_cast<float>(windowSize.x),
			.height = static_cast<float>(windowSize.y),
			.minDepth = 0.0f,
			.maxDepth = 1.0f };
		vkCmdSetViewport(cb, 0, 1, &vp2);
		VkRect2D scissor2{ .extent{ .width = static_cast<uint32_t>(windowSize.x),
				.height = static_cast<uint32_t>(windowSize.y) } };
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, displayPipeline);
		vkCmdSetScissor(cb, 0, 1, &scissor2);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, 1, &descriptorSetTex, 0, nullptr);
		vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(MouseData),
				&mouseData);
		vkCmdDraw(cb, 6, 1, 0, 0);
		vkCmdEndRendering(cb);

		VkImageMemoryBarrier2 presentBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = swapchainImages[imageIndex],
			.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		};
		VkDependencyInfo presentDepInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &presentBarrier };
		vkCmdPipelineBarrier2(cb, &presentDepInfo);

		chk(vkEndCommandBuffer(cb));
		VkPipelineStageFlags waitStages =
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &presentSemaphores[frameIndex],
			.pWaitDstStageMask = &waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &cb,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &renderSemaphores[imageIndex],
		};
		chk(vkQueueSubmit(queue, 1, &submitInfo, fences[frameIndex]));
		frameIndex = (frameIndex + 1) % maxFramesInFlight;
		VkPresentInfoKHR presentInfo{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores =
					&renderSemaphores[imageIndex],
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &imageIndex };
		chkSwapchain(vkQueuePresentKHR(queue, &presentInfo));
		mouseData.lastX = mouseData.x;
		mouseData.lastY = mouseData.y;
	}
	vkDeviceWaitIdle(device);
	for (int i = 0; i < maxFramesInFlight; i++) {
		vkDestroyFence(device, fences[i], nullptr);
		vkDestroySemaphore(device, presentSemaphores[i], nullptr);
	}
	for (auto s : renderSemaphores) {
		vkDestroySemaphore(device, s, nullptr);
	}
	for (auto v : swapchainImageViews) {
		vkDestroyImageView(device, v, nullptr);
	}

	vkDestroySampler(device, textSampler, nullptr);
	vkDestroyImageView(device, textImageView, nullptr);
	vmaDestroyImage(allocator, textImage, textImageAllocation);

	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipeline(device, displayPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTex, nullptr);
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);

	vmaDestroyAllocator(allocator);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);

	return 0;
}