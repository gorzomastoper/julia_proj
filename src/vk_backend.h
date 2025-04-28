#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"

template<typename T>
struct pipeline_info {
	u32 shader_stages_count;
	VkPipelineShaderStageCreateInfo shader_stage_infos[5];
	VkPipelineVertexInputStateCreateInfo vertex_state_info;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
	VkPipelineViewportStateCreateInfo viewport_state_info;
	VkPipelineRasterizationStateCreateInfo rasterizer_state_info;
	VkPipelineMultisampleStateCreateInfo multisampling_state_info;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info;
	VkPipelineColorBlendStateCreateInfo color_blending_state_info;
};

#define MAX_FRAMES_IN_FLIGHT 3

struct vk_context {
	VkInstance 			instance;
	VkPhysicalDevice 	physical_device;
	VkDevice 			device;
	VkQueue				queue;
	VkQueue				present_queue;
	VkSurfaceKHR		surface;
	VkSwapchainKHR		swap_chain;
	u32 				swap_chain_image_count;
	u32 				current_frame;
	VkImage				swap_chain_images			[MAX_FRAMES_IN_FLIGHT];// NOTE(DH): I choose max 5 images for now!
	VkImageView			swap_chain_image_views		[MAX_FRAMES_IN_FLIGHT];
	VkFramebuffer		swap_chain_framebuffers		[MAX_FRAMES_IN_FLIGHT];
	VkCommandBuffer		command_buffers				[MAX_FRAMES_IN_FLIGHT];
	VkCommandPool		command_pool;
	VkSemaphore			image_avail_semaphores		[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore			render_finish_semaphores	[MAX_FRAMES_IN_FLIGHT];
	VkFence				in_flight_fences			[MAX_FRAMES_IN_FLIGHT];
	VkFormat			swap_chain_image_format;
	VkExtent2D			swap_chain_extent;
	VkViewport			viewport;
	VkRect2D			scissor;
	VkPipeline			graph_pipeline;
	VkRenderPass		only_pass;
	u32 				window_width, window_height;

public:
static func make(HWND) 								-> vk_context;
	func cleanup() 									-> void;
	func resize(u32 new_width, u32 new_height) 		-> void;
	func draw_frame(u32 dt, u32 width, u32 height) 	-> void;
};
func vk_pick_physical_device(VkInstance) 		-> VkPhysicalDevice;
func vk_create_logical_device(VkPhysicalDevice) -> VkDevice;
func vk_cleanup_swap_chain(vk_context* ctx) 	-> void;