#include "vk_backend.h"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <libloaderapi.h>
#include <stdexcept>
#include <stdlib.h>
#include <winuser.h>

// NOTE(DH): Include VULKAN API stuff
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

func vk_create_instance() -> VkInstance {
	VkInstance result;
	VkApplicationInfo app_info = {};
	app_info.pApplicationName = "Vulkan Based App";
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "Does not exist";
	app_info.apiVersion = VK_API_VERSION_1_4;

	const char *extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
		"VK_EXT_debug_utils",
		"VK_KHR_get_physical_device_properties2",
	};

	const char *validations[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	u32 avail_ext_count = 0;
	char* layer_name;
	vkEnumerateInstanceExtensionProperties(nullptr, &avail_ext_count, nullptr);
	VkExtensionProperties properties[avail_ext_count];
	vkEnumerateInstanceExtensionProperties(nullptr, &avail_ext_count, properties);
	for(u32 i = 0; i < avail_ext_count; ++i) {
		printf("%s\n", properties[i].extensionName);
	}

	VkInstanceCreateInfo instance_create_info = {};
	instance_create_info.pApplicationInfo = &app_info;
	instance_create_info.enabledExtensionCount = _countof(extensions);
	instance_create_info.enabledLayerCount = _countof(validations);
	instance_create_info.ppEnabledLayerNames = validations;
	instance_create_info.ppEnabledExtensionNames = extensions;

	VkResult _result = vkCreateInstance(&instance_create_info, nullptr, &result);

	//vkCreateWin32SurfaceKHR(result, &app_info, nullptr, surface);

	return result;
}

func vk_context::cleanup() -> void {
	vk_cleanup_swap_chain(this);
	vkDestroyInstance(instance, nullptr);
}

func vk_pick_physical_device(VkInstance instance) -> VkPhysicalDevice {
	VkPhysicalDevice result = VK_NULL_HANDLE;

	u32 device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

	if(device_count == 0)
		throw std::runtime_error("failed to find GPUS with VULKAN support!");

	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(instance, &device_count, devices);

	for(u32 i = 0; i < device_count; ++i) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[i], &props);
		if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			result = devices[i];
		}
	}
	return result;
}

func vk_find_graphics_family(VkPhysicalDevice physical_device, VkQueueFlags flags) -> u32 {
	u32 result = 0;

	u32 graphics_family = 0;
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties properties[queue_family_count];
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, properties);
	for(u32 i = 0 ; i < queue_family_count; ++i) {
		if(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_family = i;
	}

	return result;
}

func vk_create_logical_device(VkPhysicalDevice physical_device) -> VkDevice {
	VkDevice result;

	// NOTE(DH): Find family indices
	u32 graphics_family = vk_find_graphics_family(physical_device, VK_QUEUE_GRAPHICS_BIT);

	// NOTE(DH): We can have a multiple queues and can assign
	// to them priority values, that can modify scheduling
	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = graphics_family;
	queue_create_info.queueCount = 1;
	f32 queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;

	u32 property_count = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &property_count, nullptr);
	VkExtensionProperties ext_properties[property_count];
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &property_count, ext_properties);
	for(u32 i = 0; i < property_count; ++i) {
		printf("%s\n", ext_properties[i].extensionName);
	}
	const char* ext_names[] = {
		"VK_KHR_swapchain"
	};

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;
	create_info.enabledExtensionCount = _countof(ext_names);
	create_info.ppEnabledExtensionNames = ext_names;

	VkResult err = vkCreateDevice(physical_device, &create_info, nullptr, &result);

	return result;
}

func vk_create_graphics_queue(vk_context ctx) -> VkQueue {
	VkQueue result;
	
	u32 graphics_family = 0;
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties properties[queue_family_count];
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, properties);
	for(u32 i = 0 ; i < queue_family_count; ++i) {
		if(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_family = i;
	}

	vkGetDeviceQueue(ctx.device, graphics_family, 0, &result);

	return result;
}

func vk_create_surface(vk_context ctx, HWND hwnd) -> VkSurfaceKHR {
	VkSurfaceKHR result;
	VkWin32SurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info.hwnd = hwnd;
	create_info.hinstance = GetModuleHandle(nullptr);

	VkResult err = vkCreateWin32SurfaceKHR(ctx.instance, &create_info, nullptr, &result);

	return result;
}

func vk_choose_surface_format(vk_context ctx) -> VkSurfaceFormatKHR {
	u32 format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, nullptr);
	VkSurfaceFormatKHR	formats[format_count];
	if(format_count > 0) {
		vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, formats);
	}

	for(u32 i = 0; i < format_count; ++i) {
		if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}

	return formats[0];
}

func vk_choose_swap_chain_present_mode(vk_context ctx) -> VkPresentModeKHR {
	u32 present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, nullptr);
	VkPresentModeKHR present_modes[present_mode_count];
	if(present_mode_count > 0) {
		vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, present_modes);
	}

	for(u32 i = 0; i < present_mode_count; ++i) {
		if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return present_modes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

func vk_choose_swap_extent(vk_context ctx, u32 width, u32 height) {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &capabilities);

	if(capabilities.currentExtent.width != UINT_MAX) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actual_extent = { width,	height };
		return actual_extent;
	}
}

func vk_create_swap_chain(vk_context ctx, u32 width, u32 height) -> VkSwapchainKHR {
	VkSwapchainKHR result = {};

	VkSurfaceFormatKHR 	surface_format 	= vk_choose_surface_format(ctx);
	VkPresentModeKHR	present_mode 	= vk_choose_swap_chain_present_mode(ctx);
	VkExtent2D			extent			= vk_choose_swap_extent(ctx, width, height);

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &capabilities);
	u32 image_count	= capabilities.minImageCount + 1;

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType 			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.oldSwapchain 	= VK_NULL_HANDLE;
	create_info.clipped 		= VK_TRUE;
	create_info.minImageCount	= image_count;
	create_info.imageFormat		= surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.surface			= ctx.surface;
	create_info.imageExtent		= extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // In the future it will be VK_IMAGE_USAGE_TRANSFER_DST_BIT!
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = nullptr;
	create_info.preTransform	= capabilities.currentTransform;
	create_info.compositeAlpha	= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode		= present_mode;
	create_info.clipped			= VK_TRUE;

	if(vkCreateSwapchainKHR(ctx.device, &create_info, nullptr, &result) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain! DH");
	}

	return result;
}

func vk_create_images_for_swap_chain(vk_context *ctx) -> u32 {
	u32 result = 0;
	vkGetSwapchainImagesKHR(ctx->device, ctx->swap_chain, &result, nullptr);
	if(result > _countof(ctx->swap_chain_images)) { throw std::runtime_error("Image count is greater than we can handle for now! DH");}
	vkGetSwapchainImagesKHR(ctx->device, ctx->swap_chain, &result, ctx->swap_chain_images);
	return result;
}

func vk_create_image_view(vk_context ctx, VkImage image, VkImageViewType view_type, VkImageAspectFlags aspect_mask) -> VkImageView {
	VkImageView result = {};
	VkImageViewCreateInfo create_info = {};
	create_info.sType 		= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image 		= image;
	create_info.viewType 	= view_type;
	create_info.format 		= vk_choose_surface_format(ctx).format;
	create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.subresourceRange.aspectMask = aspect_mask;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;
	VkResult error = vkCreateImageView(ctx.device, &create_info, nullptr, &result);
	if(error) {
		// TODO(): Catch if not success
	}
	return result;
}

func vk_load_file(const char* file_path, const char* file_name) -> std::ifstream {
	char buffer[1024];
	if(file_name != nullptr)
		snprintf(buffer, _countof(buffer), "%s\\%s", file_path, file_name);
	else
		snprintf(buffer, _countof(buffer), "%s\\", file_path);

	std::ifstream file(buffer, std::ios::ate | std::ios::binary);
	if(!file.is_open()) {
		throw std::runtime_error("Failed to open file!");
	}
	return file;
}

func vk_create_shader_module(vk_context ctx, u32* code, u32 size) -> VkShaderModule {
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = code;
	VkShaderModule result = {};
	vkCreateShaderModule(ctx.device, &create_info, nullptr, &result);
	return result;
}

func vk_create_render_pass(vk_context ctx) -> VkRenderPass {
	VkRenderPass result = {};

	VkAttachmentDescription color_attachment = {};
	color_attachment.format = ctx.swap_chain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0; // NOTE(DH): In shader it will appear in layout(location = 0) ...
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	vkCreateRenderPass(ctx.device, &render_pass_info, nullptr, &result);

	return result;
}

func vk_allocate_frame_buffer(vk_context ctx, VkRenderPass pass, VkImageView attachment, u32 layer_count) -> VkFramebuffer {
	VkFramebuffer result = {};

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = pass;
	framebuffer_info.attachmentCount = 1;
	framebuffer_info.pAttachments = &attachment;
	framebuffer_info.width = ctx.swap_chain_extent.width;
	framebuffer_info.height = ctx.swap_chain_extent.height;
	framebuffer_info.layers = layer_count;

	vkCreateFramebuffer(ctx.device, &framebuffer_info, nullptr, &result);

	return result;
}

func vk_create_command_pool(VkPhysicalDevice physical_device, VkDevice device) -> VkCommandPool {
	VkCommandPool result = {};

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = vk_find_graphics_family(physical_device, VK_QUEUE_GRAPHICS_BIT);

	vkCreateCommandPool(device, &pool_info, nullptr, &result);

	return result;
}

func vk_create_command_buffer(vk_context ctx, VkCommandPool command_pool) -> VkCommandBuffer {
	VkCommandBuffer result = {};
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	vkAllocateCommandBuffers(ctx.device, &alloc_info, &result);
	return result;
}

func vk_record_comand_buffer(vk_context ctx, VkCommandBuffer command_buffer, u32 image_idx) {
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = ctx.only_pass;
	render_pass_info.framebuffer = ctx.swap_chain_framebuffers[image_idx];
	render_pass_info.renderArea.offset = {.x = 0, .y = 0};
	render_pass_info.renderArea.extent = ctx.swap_chain_extent;
	VkClearValue clear_color = {.color = {0.0, 0.0f, 0.0f, 1.0f}};
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graph_pipeline);
	vkCmdSetViewport(command_buffer, 0, 1, &ctx.viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &ctx.scissor);
	vkCmdDraw(command_buffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(command_buffer);
	vkEndCommandBuffer(command_buffer);
}

func vk_create_semaphore(VkDevice device) -> VkSemaphore {
	VkSemaphore result = {};
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(device, &info, nullptr, &result);
	return result;
}

func vk_create_fence(VkDevice device, VkFenceCreateFlags flags) -> VkFence {
	VkFence result = {};
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.flags = flags;
	vkCreateFence(device, &info, nullptr, &result);
	return result;
}

func vk_context::make(HWND hwnd) -> vk_context {
	vk_context result = {};

	RECT rect;
	GetWindowRect(hwnd, &rect);
	result.window_width = rect.right - rect.left;
	result.window_height = rect.bottom - rect.top;

	result.current_frame 			= 0;
	result.instance 				= vk_create_instance				();
	result.physical_device 			= vk_pick_physical_device			(result.instance);
	result.device					= vk_create_logical_device			(result.physical_device);
	result.queue					= vk_create_graphics_queue			(result);
	result.surface					= vk_create_surface					(result, hwnd);
	result.swap_chain				= vk_create_swap_chain				(result, result.window_width, result.window_height);
	result.swap_chain_image_count	= vk_create_images_for_swap_chain	(&result);
	result.swap_chain_extent		= vk_choose_swap_extent				(result, result.window_width, result.window_height);
	result.swap_chain_image_format	= vk_choose_surface_format			(result).format;

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		result.swap_chain_image_views[i] = vk_create_image_view(result, result.swap_chain_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	// NOTE(DH): Pipeline configuration and creation is down there!
	std::ifstream vert_shader = vk_load_file("..\\data", "vert.spv");
	u32 vert_shader_file_size = vert_shader.tellg();
	char vert_shader_code[vert_shader_file_size];
	vert_shader.seekg(0);
	vert_shader.read(vert_shader_code, vert_shader_file_size);
	vert_shader.close();

	std::ifstream frag_shader = vk_load_file("..\\data", "frag.spv");
	u32 frag_shader_file_size = frag_shader.tellg();
	char frag_shader_code[frag_shader_file_size];
	frag_shader.seekg(0);
	frag_shader.read(frag_shader_code, frag_shader_file_size);
	frag_shader.close();

	let vert_shader_module = vk_create_shader_module(result, (u32*)vert_shader_code, vert_shader_file_size);
	let frag_shader_module = vk_create_shader_module(result, (u32*)frag_shader_code, frag_shader_file_size);

	VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
	frag_shader_stage_info.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage 	= VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.module 	= frag_shader_module;
	frag_shader_stage_info.pName 	= "main";

	VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
	vert_shader_stage_info.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage 	= VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_stage_info.module 	= vert_shader_module;
	vert_shader_stage_info.pName 	= "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = {frag_shader_stage_info, vert_shader_stage_info};

	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = _countof(dynamic_states);
	dynamic_state.pDynamicStates = dynamic_states;

	VkPipelineVertexInputStateCreateInfo vertex_state_info = {};
	vertex_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_state_info.vertexBindingDescriptionCount = 0;
	vertex_state_info.pVertexBindingDescriptions = nullptr;
	vertex_state_info.vertexAttributeDescriptionCount = 0;
	vertex_state_info.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	result.viewport = {};
	result.viewport.x = 0.0f;
	result.viewport.y = 0.0f;
	result.viewport.width = result.window_width;
	result.viewport.height = result.window_height;
	result.viewport.minDepth = 0.0f;
	result.viewport.maxDepth = 1.0f;

	result.scissor = {};
	result.scissor.offset = {.x = 0, .y = 0};
	result.scissor.extent = result.swap_chain_extent;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &result.viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &result.scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;

	// NOTE(DH): Pay attention please!
	VkPipelineLayout pipeline_layout = {};
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 0;
	vkCreatePipelineLayout(result.device, &pipeline_layout_info, nullptr, &pipeline_layout);

	result.only_pass = vk_create_render_pass(result);

	VkGraphicsPipelineCreateInfo graph_pipeline_info = {};
	graph_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graph_pipeline_info.stageCount = _countof(shader_stages);
	graph_pipeline_info.pStages = shader_stages;
	graph_pipeline_info.pVertexInputState = &vertex_state_info;
	graph_pipeline_info.pInputAssemblyState = &input_assembly;
	graph_pipeline_info.pViewportState = &viewport_state;
	graph_pipeline_info.pRasterizationState = &rasterizer;
	graph_pipeline_info.pMultisampleState = &multisampling;
	graph_pipeline_info.pDepthStencilState = nullptr;
	graph_pipeline_info.pColorBlendState = &color_blending;
	graph_pipeline_info.pDynamicState = &dynamic_state;
	graph_pipeline_info.layout = pipeline_layout;
	graph_pipeline_info.renderPass = result.only_pass;
	graph_pipeline_info.subpass = 0;
	graph_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	graph_pipeline_info.basePipelineIndex = -1;
	vkCreateGraphicsPipelines(result.device, VK_NULL_HANDLE, 1, &graph_pipeline_info, nullptr, &result.graph_pipeline);

	result.command_pool = vk_create_command_pool(result.physical_device, result.device);
	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		result.swap_chain_framebuffers	[i] = vk_allocate_frame_buffer	(result, result.only_pass, result.swap_chain_image_views[i], 1);
		result.command_buffers			[i] = vk_create_command_buffer	(result, result.command_pool);
		result.image_avail_semaphores	[i] = vk_create_semaphore		(result.device);
		result.render_finish_semaphores	[i] = vk_create_semaphore		(result.device);
		result.in_flight_fences			[i] = vk_create_fence			(result.device, VK_FENCE_CREATE_SIGNALED_BIT);
	}

	result.present_queue = vk_create_graphics_queue(result);

	return result;
}

func vk_recreate_swap_chain(vk_context* ctx, u32 new_width, u32 new_height) -> void {
	vkDeviceWaitIdle(ctx->device);

	vk_cleanup_swap_chain(ctx);

	ctx->swap_chain = vk_create_swap_chain(*ctx, new_width, new_height);
	ctx->swap_chain_image_count	= vk_create_images_for_swap_chain	(ctx);

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		ctx->swap_chain_image_views[i] = vk_create_image_view(*ctx, ctx->swap_chain_images[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
		ctx->swap_chain_framebuffers[i] = vk_allocate_frame_buffer(*ctx, ctx->only_pass, ctx->swap_chain_image_views[i], 1);
	}
}

func vk_cleanup_swap_chain(vk_context* ctx) -> void {
	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroyFramebuffer(ctx->device, ctx->swap_chain_framebuffers[i], nullptr);
	}

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroyImageView(ctx->device, ctx->swap_chain_image_views[i], nullptr);
	}

	vkDestroySwapchainKHR(ctx->device, ctx->swap_chain, nullptr);
}

func vk_context::draw_frame(u32 dt, u32 width, u32 height) -> void {
	vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	u32 image_idx = 0;
	VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_avail_semaphores[current_frame], VK_NULL_HANDLE, &image_idx);

	// if(result == VK_ERROR_OUT_OF_DATE_KHR) {
	// 	vk_recreate_swap_chain(this, width, height);
	// 	return;
	// }

	vkResetFences(device, 1, &in_flight_fences[current_frame]);

	vkResetCommandBuffer(command_buffers[current_frame], 0);
	vk_record_comand_buffer(*this, command_buffers[current_frame], image_idx);

	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = _countof(wait_stages);
	submit_info.pWaitSemaphores = &image_avail_semaphores[current_frame];
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffers[current_frame];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &render_finish_semaphores[current_frame];
	vkQueueSubmit(present_queue, 1, &submit_info, in_flight_fences[current_frame]);

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finish_semaphores[current_frame];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swap_chain;
	present_info.pImageIndices = &image_idx;
	present_info.pResults = nullptr;
	result = vkQueuePresentKHR(present_queue, &present_info);

	// if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
	// 	vk_recreate_swap_chain(this, width, height);
	// }

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

func vk_context::resize(u32 width, u32 height) -> void {
	window_width = width;
	window_height = height;
	swap_chain_extent = vk_choose_swap_extent (*this, width, height);
	
	viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = window_width;
	viewport.height = window_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor = {};
	scissor.offset = {.x = 0, .y = 0};
	scissor.extent = swap_chain_extent;
	vk_recreate_swap_chain(this, width, height);
}