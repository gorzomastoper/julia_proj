#pragma once

#include <array>
#include <cstddef>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"

// NOTE(DH): UBO
struct uniform_buffer_object {
	alignas(16) mat4 model;
	alignas(16) mat4 view;
	alignas(16) mat4 proj;
};

struct vk_vertex {
	v3 position;
	v3 color;
	v2 tex_coord;
public:
	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription result = {};
		result.binding = 0;
		result.stride = sizeof(vk_vertex);
		result.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return result;
	}

	static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions() {
		std::array<VkVertexInputAttributeDescription, 3> result = {};
		result[0].binding = 0;
		result[0].location = 0;
		result[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		result[0].offset = offsetof(vk_vertex, position);

		result[1].binding = 0;
		result[1].location = 1;
		result[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		result[1].offset = offsetof(vk_vertex, color);

		result[2].binding = 0;
		result[2].location = 2;
		result[2].format = VK_FORMAT_R32G32_SFLOAT;
		result[2].offset = offsetof(vk_vertex, tex_coord);
		return result;
	}

	bool operator==(const vk_vertex& other) const {
		return position == other.position && color == other.color && tex_coord == other.tex_coord;
	}
};

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

struct vk_image_and_memory {
	VkImage			image;
	VkDeviceMemory 	image_memory;
};

template<typename T>
struct buffer {
	u32 size;
	VkBuffer buffer;
	VkDeviceMemory memory;
	u32 get_el_size() { return sizeof(T); }
	u32 get_el_count() { return size / get_el_size(); };
};

template<typename T>
struct uniform_buffer {
	u32 			size;
	VkBuffer 		buffer;
	VkDeviceMemory 	memory;
	void* 			mapped_memory;
	u32 get_el_size() { return sizeof(T); }
	u32 get_el_count() { return size / get_el_size(); };
};

struct texture_buffer {
	u32 				size;
	u32					mip_levels;
	vk_image_and_memory	vk_data;
	VkImageView			image_view;
	VkSampler			image_sampler;
	u32 get_el_size() { return 4; }
	u32 get_el_count() { return size / get_el_size(); };
};

template<typename T>
struct model {
	texture_buffer 		texture;
	buffer<vk_vertex> 	vertices;
	buffer<T> 			indices;
};

struct vk_context {
	u32 				u_elapsed_time;
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

	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout 	pipeline_layout;

	buffer<vk_vertex>	vtex_buffer;
	buffer<u16>			idex_buffer;

	uniform_buffer<uniform_buffer_object> uni_buffer[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet		uni_descriptor_sets			[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorPool	uni_descriptor_pool;

	texture_buffer		simple_texture;
	texture_buffer		depth_texture;
	model<u32>			viking_room;

	u32 				window_width, window_height;

public:
static func make(HWND) 								-> vk_context;
	func cleanup() 									-> void;
	func resize(u32 new_width, u32 new_height) 		-> void;
	func draw_frame(u32 dt, u32 width, u32 height) 	-> void;
};

func vk_pick_physical_device	(VkInstance) 		-> VkPhysicalDevice;

func vk_create_logical_device	(VkPhysicalDevice) 	-> VkDevice;

func vk_cleanup_swap_chain		(vk_context* ctx)	-> void;

func vk_create_texture_from_image(VkDevice device)	-> texture_buffer;

func vk_find_graphics_family(VkPhysicalDevice physical_device, VkQueueFlags flags) -> u32;

func vk_create_graphics_queue(vk_context ctx) -> VkQueue;

func vk_create_surface(vk_context ctx, HWND hwnd) -> VkSurfaceKHR;

func vk_choose_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) -> VkSurfaceFormatKHR;

func vk_choose_swap_chain_present_mode(VkPhysicalDevice, VkSurfaceKHR) -> VkPresentModeKHR;

func vk_choose_swap_extent(VkPhysicalDevice, VkSurfaceKHR, u32, u32) -> VkExtent2D;

func vk_create_swap_chain(VkPhysicalDevice, VkDevice, VkSurfaceKHR, u32 width, u32 height) -> VkSwapchainKHR;

func vk_create_images_for_swap_chain(vk_context *ctx) -> u32;

func vk_create_image_view(VkDevice device, VkImage image, u32, VkFormat format, VkImageViewType view_type, VkImageAspectFlags aspect_mask) -> VkImageView;

func vk_create_shader_module(vk_context ctx, u32* code, u32 size) -> VkShaderModule;

func vk_create_render_pass(vk_context ctx) -> VkRenderPass;

func vk_allocate_frame_buffer(vk_context ctx, VkRenderPass pass, VkImageView* attachments, u32 attachments_count, u32 layer_count) -> VkFramebuffer;

func vk_create_command_pool(VkPhysicalDevice physical_device, VkDevice device) -> VkCommandPool;

func vk_create_command_buffer(vk_context ctx, VkCommandPool command_pool) -> VkCommandBuffer;

func vk_record_comand_buffer(vk_context ctx, VkCommandBuffer command_buffer, u32 image_idx) -> void;

func vk_create_semaphore(VkDevice device) -> VkSemaphore;

func vk_create_fence(VkDevice device, VkFenceCreateFlags flags) -> VkFence;

func vk_create_buffer(VkDevice device, u32 count, u32 size_of_one_elem, VkBufferUsageFlags usage_flags, VkSharingMode sharing_mode) -> VkBuffer;

func vk_find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags flags) -> u32;

func vk_allocate_buffer_memory(VkPhysicalDevice physical_device, VkDevice device, VkMemoryPropertyFlags properties, VkBuffer buffer) -> VkDeviceMemory;

func vk_copy_buffer(VkDevice device, VkQueue queue, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, VkCommandPool pool) -> void;

func create_vertex_buffer(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool pool, VkDeviceSize size, void* vertex_data, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags properties) -> buffer<vk_vertex>;

template<typename T>
func create_index_buffer(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool pool, VkDeviceSize size, void* index_data, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags properties) -> buffer<T>;

func vk_create_descriptor_set_layout(VkDevice device, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags) -> VkDescriptorSetLayout;

func vk_create_pipeline_layout(VkDevice device, VkDescriptorSetLayout desc_set_layout) -> VkPipelineLayout;

func vk_create_uniform_buffer(VkPhysicalDevice physical_device, VkDevice device) -> uniform_buffer<uniform_buffer_object>;

func update_uniform_buffer(uniform_buffer<uniform_buffer_object> *buffers, u32 width, u32 height, u32 image_idx, u32 time_elapsed) -> void;

func vk_create_descriptor_pool(VkDevice device, u32 frames_in_flight, VkDescriptorType type) -> VkDescriptorPool;

func vk_create_descriptor_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout) -> VkDescriptorSet;

func vk_recreate_swap_chain(vk_context* ctx, u32 new_width, u32 new_height) -> void;

func vk_cleanup_swap_chain(vk_context* ctx) -> void;