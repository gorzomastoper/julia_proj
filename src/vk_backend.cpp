#include "vk_backend.h"
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <libloaderapi.h>
#include <stdexcept>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <winuser.h>

// NOTE(DH): Include VULKAN API stuff
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

// NOTE(DH): Include math stuff from GLM. TODO(DH): Write my own functions to study
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_transform.hpp>

// NOTE(DH): Include STB image for just expirementing with some stuff
#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

// NOTE(DH): Include Tiny Obj Loader for fast iterations
#define TINYOBJLOADER_IMPLEMENTATION
#include "deps/tiny_obj_loader.h"

inline func v3_to_glm(v3 vec) -> glm::vec3 {
	return glm::vec3(vec.x, vec.y, vec.z);
}

inline func v2_to_glm(v2 vec) -> glm::vec2 {
	return glm::vec2(vec.x, vec.y);
}

func vk_create_texture_sampler(VkPhysicalDevice physical_device, VkDevice device) -> VkSampler {
	VkSampler result = {};
	VkSamplerCreateInfo create_info = {};
	create_info.sType 				= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	create_info.magFilter 			= VK_FILTER_LINEAR;
	create_info.minFilter 			= VK_FILTER_LINEAR;
	create_info.addressModeU 		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	create_info.addressModeV 		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	create_info.addressModeW 		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	create_info.anisotropyEnable 	= VK_TRUE;

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(physical_device, &props);
	create_info.maxAnisotropy 			= props.limits.maxSamplerAnisotropy;
	create_info.borderColor 			= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	create_info.unnormalizedCoordinates = VK_FALSE;
	create_info.compareEnable 			= VK_FALSE;
	create_info.compareOp 				= VK_COMPARE_OP_ALWAYS;
	create_info.mipmapMode 				= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	create_info.mipLodBias 				= 0.0f;
	create_info.minLod 					= 0.0f;
	create_info.maxLod 					= VK_LOD_CLAMP_NONE;
	vkCreateSampler(device, &create_info, nullptr, &result);
	return result;
}

func vk_create_image(VkPhysicalDevice physical_device, VkDevice device, u32 width, u32 height, u32 mip_levels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags property_flags) -> vk_image_and_memory {
	vk_image_and_memory result = {};

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mip_levels;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage_flags;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0;
	vkCreateImage(device, &image_info, nullptr, &result.image);

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(device, result.image, &mem_req);
	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type(physical_device, mem_req.memoryTypeBits, property_flags);
	vkAllocateMemory(device, &alloc_info, nullptr, &result.image_memory);
	
	vkBindImageMemory(device, result.image, result.image_memory, 0);

	return result;
}

func vk_begin_single_time_commands(VkDevice device, VkCommandPool command_pool) -> VkCommandBuffer {
	VkCommandBuffer result;
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;
	vkAllocateCommandBuffers(device, &alloc_info, &result);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(result, &begin_info);

	return result;
}

func vk_end_single_time_commands(VkDevice device, VkCommandBuffer buffer, VkQueue queue, VkCommandPool pool) -> void {
	vkEndCommandBuffer(buffer);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &buffer;
	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, pool, 1, &buffer);
}

func vk_has_stencil_component(VkFormat format) -> bool {
	bool result = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	return result;
}

func vk_transition_image_layout(VkDevice device, VkImage image, u32 mip_levels, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, VkQueue queue, VkCommandPool command_pool) -> void {
	VkCommandBuffer cmd_buffer = vk_begin_single_time_commands(device, command_pool);
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags source_stage_flags;
	VkPipelineStageFlags dest_stage_flags;

	if(new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if(vk_has_stencil_component(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	} else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		source_stage_flags 	= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dest_stage_flags 	= VK_PIPELINE_STAGE_TRANSFER_BIT;

	} else if(old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage_flags 	= VK_PIPELINE_STAGE_TRANSFER_BIT;
		dest_stage_flags 	= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	} else if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {

		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		source_stage_flags 	= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dest_stage_flags 	= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	} else {
		throw std::runtime_error("Nah, unsupported layout transition man!");
	}

	vkCmdPipelineBarrier(cmd_buffer, source_stage_flags, dest_stage_flags, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	vk_end_single_time_commands(device, cmd_buffer, queue, command_pool);
}

func vk_find_supported_format(VkPhysicalDevice physical_device, VkFormat *candidates, u32 count, VkImageTiling tiling, VkFormatFeatureFlags features_flags) -> VkFormat {
	VkFormat result = {};
	for(u32 i = 0 ; i < count; ++i) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, candidates[i], &props);
		if(tiling == VK_IMAGE_TILING_LINEAR && (props.optimalTilingFeatures & features_flags) == features_flags) {
			return candidates[i];
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features_flags) == features_flags) {
			return candidates[i];
		}
	}

	throw std::runtime_error("Nah, failed to find supported format, sry :()");
	return result;
}

func vk_find_depth_format(VkPhysicalDevice physical_device) -> VkFormat {
	VkFormat fmts[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	VkFormat result = vk_find_supported_format(
		physical_device, 
		fmts, 
		_countof(fmts), 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	return result;
}

func vk_copy_buffer_to_image(VkDevice device, VkQueue queue, VkCommandPool command_pool, VkBuffer buffer, VkImage image, u32 width, u32 height) -> void {
	VkCommandBuffer cmd_buffer = vk_begin_single_time_commands(device, command_pool);
	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {.x = 0, .y = 0, .z = 0};
	region.imageExtent = {.width = width, .height = height, .depth = 1};
	vkCmdCopyBufferToImage(cmd_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vk_end_single_time_commands(device, cmd_buffer, queue, command_pool);
}

func vk_create_depth_resources(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool command_pool, VkExtent2D extent) -> texture_buffer {
	texture_buffer result = {};
	VkFormat depth_format 	= vk_find_depth_format(physical_device);
	result.vk_data 			= vk_create_image(physical_device, device, extent.width, extent.height, 1, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	result.image_view 		= vk_create_image_view(device, result.vk_data.image, 1, depth_format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

	vk_transition_image_layout(device, result.vk_data.image, 1, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, queue, command_pool);
	
	return result;
}

func vk_generate_mipmaps(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool command_pool, VkImage image, VkFormat image_format, u32 width, u32 height, u32 mip_levels) -> void {
	
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(physical_device, image_format, &props);
	if(!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("Nah, tex image format does not support linear blitting :(");
	}
	
	VkCommandBuffer cmd_buffer = vk_begin_single_time_commands(device, command_pool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	i32 mip_width = width;
	i32 mip_height = height;

	for(u32 i = 1; i < mip_levels; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(
			cmd_buffer, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 
			0, nullptr, 
			0, nullptr, 
			1, &barrier
		);

		VkImageBlit blit = {};
		blit.srcOffsets[0] 					= {0, 0, 0};
		blit.srcOffsets[1] 					= {mip_width, mip_height, 1};
		blit.srcSubresource.aspectMask 		= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel 		= i - 1;
		blit.srcSubresource.baseArrayLayer 	= 0;
		blit.srcSubresource.layerCount 		= 1;
		blit.dstOffsets[0] 					= {0, 0, 0};
		blit.dstOffsets[1] 					= {mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1};
		blit.dstSubresource.aspectMask 		= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel 		= i;
		blit.dstSubresource.baseArrayLayer 	= 0;
		blit.dstSubresource.layerCount 		= 1;
		vkCmdBlitImage(cmd_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		barrier.oldLayout 		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout 		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask 	= VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask 	= VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(
			cmd_buffer, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 
			0, nullptr, 
			0, nullptr, 
			1, &barrier
		);

		if(mip_width > 1) mip_width /= 2;
		if(mip_height > 1) mip_height /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(
		cmd_buffer, 
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 
		0, nullptr, 
		0, nullptr, 
		1, &barrier
	);

	vk_end_single_time_commands(device, cmd_buffer, queue, command_pool);
}

func vk_create_texture_from_image_stb(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool command_pool, const char* texture_path) -> texture_buffer {
	texture_buffer result = {};

	i32 tex_width, tex_height, tex_channels;
	stbi_uc* pixels = stbi_load(texture_path, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
	VkDeviceSize image_size = tex_width * tex_height * 4;
	if(pixels == nullptr) throw std::runtime_error("Nah, there are no image man :()");
	
	result.mip_levels = floor(log2(fmax(tex_width, tex_height))) + 1;

	VkBuffer staiging_buffer;
	VkDeviceMemory staiging_memory;
	staiging_buffer = vk_create_buffer(device, 1, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
	staiging_memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staiging_buffer);
	void* data;
	vkMapMemory(device, staiging_memory, 0, image_size, 0, &data);
	memcpy(data, pixels, image_size);
	vkUnmapMemory(device, staiging_memory);
	stbi_image_free(pixels);

	result.vk_data = vk_create_image(physical_device, device, tex_width, tex_height, result.mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vk_transition_image_layout(device, result.vk_data.image, result.mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, queue, command_pool);
	vk_copy_buffer_to_image(device, queue, command_pool, staiging_buffer, result.vk_data.image, tex_width, tex_height);
	//vk_transition_image_layout(device, result.vk_data.image, result.mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, queue, command_pool);

	vkDestroyBuffer(device, staiging_buffer, nullptr);
	vkFreeMemory(device, staiging_memory, nullptr);

	result.image_view 		= vk_create_image_view(device, result.vk_data.image, result.mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
	result.image_sampler 	= vk_create_texture_sampler(physical_device, device);

	vk_generate_mipmaps(physical_device, device, queue, command_pool, result.vk_data.image, VK_FORMAT_R8G8B8A8_SRGB, tex_width, tex_height, result.mip_levels);

	return result;
}

namespace std {
	template<>
	struct hash<vk_vertex> {
		size_t operator()(vk_vertex const& vertex) const {
			return (
				((hash<glm::vec3>()(v3_to_glm(vertex.position)) ^ 
				(hash<glm::vec3>()(v3_to_glm(vertex.color)) << 1)) >> 1) ^
				(hash<glm::vec2>()(v2_to_glm(vertex.tex_coord)) << 1)
			);
		}
	};
}

func load_model(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool pool, const char* model_path, const char* texture_path) -> model<u32> {
	model<u32> result = {};
	tinyobj::attrib_t attrib;
	std::string warn, err;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::vector<vk_vertex> vertices;
	std::vector<u32> indices;

	if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path)) {
		throw std::runtime_error("Nah, you cant do that! :(");
	}

	std::unordered_map<vk_vertex, u32> unique_vertices = {};
	for(u32 i = 0; i < shapes.size(); ++i) {
		for(u32 indice = 0; indice < shapes[i].mesh.indices.size(); ++indice) {
			vk_vertex vertex = {
				.position = {
					.x = attrib.vertices[3 * shapes[i].mesh.indices[indice].vertex_index + 0],
					.y = attrib.vertices[3 * shapes[i].mesh.indices[indice].vertex_index + 1],
					.z = attrib.vertices[3 * shapes[i].mesh.indices[indice].vertex_index + 2]
				},
				.color = {1.0f, 1.0f, 1.0f},
				.tex_coord = {
					.x = attrib.texcoords[2 * shapes[i].mesh.indices[indice].texcoord_index + 0],
					.y = 1.0f - attrib.texcoords[2 * shapes[i].mesh.indices[indice].texcoord_index + 1],
				},
			};

			if(unique_vertices.count(vertex) == 0) {
				unique_vertices[vertex] = vertices.size();
				vertices.push_back(vertex);
			}

			indices.push_back(unique_vertices[vertex]);
		}
	}

	result.vertices = create_vertex_buffer(
		physical_device, 
		device,
		queue,
		pool,
		vertices.size() * sizeof(vk_vertex), 
		vertices.data(),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		VK_SHARING_MODE_EXCLUSIVE
	);

	result.indices = create_index_buffer<u32>(
		physical_device, 
		device, 
		queue, 
		pool, 
		indices.size() * sizeof(u32), 
		indices.data(), 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		VK_SHARING_MODE_EXCLUSIVE
	);

	result.texture = vk_create_texture_from_image_stb(physical_device, device, queue, pool, texture_path);

	return result;
}

func my_mat4_to_glm_mat4(glm::mat4 mtx) -> mat4 {
	mat4 mat = Identity();
	mat.Value1_1 = mtx[0][0];
	mat.Value1_2 = mtx[0][1];
	mat.Value1_3 = mtx[0][2];
	mat.Value1_4 = mtx[0][3];
	mat.Value2_1 = mtx[1][0];
	mat.Value2_2 = mtx[1][1];
	mat.Value2_3 = mtx[1][2];
	mat.Value2_4 = mtx[1][3];
	mat.Value3_1 = mtx[2][0];
	mat.Value3_2 = mtx[2][1];
	mat.Value3_3 = mtx[2][2];
	mat.Value3_4 = mtx[2][3];
	mat.Value4_1 = mtx[3][0];
	mat.Value4_2 = mtx[3][1];
	mat.Value4_3 = mtx[3][2];
	mat.Value4_4 = mtx[3][3];
	return mat;
}

func vk_create_instance() -> VkInstance {
	VkInstance result;
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
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
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
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
		if(properties[i].queueFlags & flags) graphics_family = i;
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

	VkPhysicalDeviceFeatures supported_features = {};
	vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;
	create_info.enabledExtensionCount = _countof(ext_names);
	create_info.ppEnabledExtensionNames = ext_names;
	create_info.pEnabledFeatures = &supported_features;

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

func vk_choose_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) -> VkSurfaceFormatKHR {
	u32 format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
	VkSurfaceFormatKHR	formats[format_count];
	if(format_count > 0) {
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats);
	}

	for(u32 i = 0; i < format_count; ++i) {
		if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return formats[i];
		}
	}

	return formats[0];
}

func vk_choose_swap_chain_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface) -> VkPresentModeKHR {
	u32 present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
	VkPresentModeKHR present_modes[present_mode_count];
	if(present_mode_count > 0) {
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);
	}

	for(u32 i = 0; i < present_mode_count; ++i) {
		if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return present_modes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

func vk_choose_swap_extent(VkPhysicalDevice physical_device, VkSurfaceKHR surface, u32 width, u32 height) -> VkExtent2D {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

	if(capabilities.currentExtent.width != UINT_MAX) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actual_extent = { width,	height };
		return actual_extent;
	}
}

func vk_create_swap_chain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, u32 width, u32 height) -> VkSwapchainKHR {
	VkSwapchainKHR result = {};

	VkSurfaceFormatKHR 	surface_format 	= vk_choose_surface_format(physical_device, surface);
	VkPresentModeKHR	present_mode 	= vk_choose_swap_chain_present_mode(physical_device, surface);
	VkExtent2D			extent			= vk_choose_swap_extent(physical_device, surface, width, height);

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
	u32 image_count	= capabilities.minImageCount + 1;

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType 			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.oldSwapchain 	= VK_NULL_HANDLE;
	create_info.clipped 		= VK_TRUE;
	create_info.minImageCount	= image_count;
	create_info.imageFormat		= surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.surface			= surface;
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

	if(vkCreateSwapchainKHR(device, &create_info, nullptr, &result) != VK_SUCCESS) {
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

func vk_create_image_view(VkDevice device, VkImage image, u32 mip_levels, VkFormat format, VkImageViewType view_type, VkImageAspectFlags aspect_mask) -> VkImageView {
	VkImageView result = {};
	VkImageViewCreateInfo create_info = {};
	create_info.sType 		= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image 		= image;
	create_info.viewType 	= view_type;
	create_info.format 		= format;
	create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.subresourceRange.aspectMask = aspect_mask;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = mip_levels;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;
	VkResult error = vkCreateImageView(device, &create_info, nullptr, &result);
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

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = vk_find_depth_format(ctx.physical_device);
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1; // NOTE(DH): In shader it will appear in layout(location = 1) ...
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkSubpassDependency dependency 	= {};
	dependency.srcSubpass 			= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass 			= 0;
	dependency.srcStageMask 		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask 		= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask 		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask 		= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attach_descs[] = {color_attachment, depth_attachment};
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = _countof(attach_descs);
	render_pass_info.pAttachments = attach_descs;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	vkCreateRenderPass(ctx.device, &render_pass_info, nullptr, &result);

	return result;
}

func vk_allocate_frame_buffer(vk_context ctx, VkRenderPass pass, VkImageView* attachments, u32 attachments_count, u32 layer_count) -> VkFramebuffer {
	VkFramebuffer result = {};

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = pass;
	framebuffer_info.attachmentCount = attachments_count;
	framebuffer_info.pAttachments = attachments;
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

func vk_record_comand_buffer(vk_context ctx, VkCommandBuffer command_buffer, u32 image_idx) -> void {
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
	VkClearValue clear_values[] = {{.color = {0.0, 0.0f, 0.0f, 1.0f}}, {.depthStencil = {1.0f, 0}}};
	render_pass_info.clearValueCount = _countof(clear_values);
	render_pass_info.pClearValues = clear_values;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graph_pipeline);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &ctx.viking_room.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(command_buffer, ctx.viking_room.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdSetViewport(command_buffer, 0, 1, &ctx.viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &ctx.scissor);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout, 0, 1, &ctx.uni_descriptor_sets[image_idx], 0, nullptr);
	vkCmdDrawIndexed(command_buffer, ctx.viking_room.indices.get_el_count(), 1, 0, 0, 1);
	// vkCmdDrawIndexed(command_buffer, ctx.idex_buffer.get_el_count(), 1, 0, 0, 1);
	// vkCmdDraw(command_buffer, 3, 1, 0, 0);
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

func vk_create_buffer(VkDevice device, u32 count, u32 size_of_one_elem, VkBufferUsageFlags usage_flags, VkSharingMode sharing_mode) -> VkBuffer {
	VkBuffer result = {};
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = count * size_of_one_elem;
	info.usage = usage_flags;
	info.sharingMode = sharing_mode;
	if(vkCreateBuffer(device, &info, nullptr, &result) != VK_SUCCESS) {
		throw std::runtime_error("Nah, isnt work!");
	}
	return result;
}

func vk_find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags flags) -> u32 {
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
	for(u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
		if((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & flags) == flags) {
			return i;
		}
	}

	throw std::runtime_error("Suitable memory is not found :()");
}

func vk_allocate_buffer_memory(VkPhysicalDevice physical_device, VkDevice device, VkMemoryPropertyFlags properties, VkBuffer buffer) -> VkDeviceMemory {
	VkDeviceMemory result = {};
	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(device, buffer, &mem_req);
	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = mem_req.size;
	info.memoryTypeIndex = vk_find_memory_type(physical_device, mem_req.memoryTypeBits, properties);
	if(vkAllocateMemory(device, &info, nullptr, &result) != VK_SUCCESS) {
		throw std::runtime_error("Unfortunetly we cannot allocate buffer :()");
	}
	vkBindBufferMemory(device, buffer, result, 0);
	return result;
}

func vk_copy_buffer(VkDevice device, VkQueue queue, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, VkCommandPool pool) -> void {
	VkCommandBuffer cmd_buffer = vk_begin_single_time_commands(device, pool);

	VkBufferCopy copy_region = {};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = size;
	vkCmdCopyBuffer(cmd_buffer, src_buffer, dst_buffer, 1, &copy_region);

	vk_end_single_time_commands(device, cmd_buffer, queue, pool);
}

func create_vertex_buffer(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool pool, VkDeviceSize size, void* vertex_data, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags properties) -> buffer<vk_vertex> {
	buffer<vk_vertex> result = {};
	result.size = size;

	VkBuffer staiging_buffer;
	VkDeviceMemory staiging_memory;
	staiging_buffer = vk_create_buffer(device, 1, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
	staiging_memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staiging_buffer);
	
	void* data;
	vkMapMemory(device, staiging_memory, 0, size, 0, &data);
	memcpy(data, vertex_data, size);
	vkUnmapMemory(device, staiging_memory);

	result.buffer = vk_create_buffer(device, 1, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
	result.memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, result.buffer);

	vk_copy_buffer(device, queue, staiging_buffer, result.buffer, size, pool);
	vkDestroyBuffer(device, staiging_buffer, nullptr);
	vkFreeMemory(device, staiging_memory, nullptr);

	return result;
}

template<typename T>
func create_index_buffer(VkPhysicalDevice physical_device, VkDevice device, VkQueue queue, VkCommandPool pool, VkDeviceSize size, void* index_data, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags properties) -> buffer<T> {
	buffer<T> result = {};
	result.size = size;

	VkBuffer staiging_buffer;
	VkDeviceMemory staiging_memory;
	staiging_buffer = vk_create_buffer(device, 1, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
	staiging_memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staiging_buffer);
	
	void* data;
	vkMapMemory(device, staiging_memory, 0, size, 0, &data);
	memcpy(data, index_data, size);
	vkUnmapMemory(device, staiging_memory);

	result.buffer = vk_create_buffer(device, 1, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
	result.memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, result.buffer);

	vk_copy_buffer(device, queue, staiging_buffer, result.buffer, size, pool);
	vkDestroyBuffer(device, staiging_buffer, nullptr);
	vkFreeMemory(device, staiging_memory, nullptr);

	return result;
}

func vk_create_descriptor_set_layout(VkDevice device, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags) -> VkDescriptorSetLayout {
	VkDescriptorSetLayout result = {};
	VkDescriptorSetLayoutBinding ubo_layout_binding = {};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = descriptor_type;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = stage_flags;
	ubo_layout_binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding sampler_layout_binding = {};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.pImmutableSamplers = nullptr;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindings[] = {ubo_layout_binding, sampler_layout_binding};
	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = _countof(bindings);
	layout_info.pBindings = bindings;
	vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &result);
	return result;
}

func vk_create_pipeline_layout(VkDevice device, VkDescriptorSetLayout desc_set_layout) -> VkPipelineLayout {
	VkPipelineLayout result = {};
	VkPipelineLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = 1;
	info.pSetLayouts = &desc_set_layout;
	vkCreatePipelineLayout(device, &info, nullptr, &result);
	return result;
}

func vk_create_uniform_buffer(VkPhysicalDevice physical_device, VkDevice device) -> uniform_buffer<uniform_buffer_object> {
	uniform_buffer<uniform_buffer_object> result = {};

	result.buffer = vk_create_buffer(device, 1, result.get_el_size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
	result.memory = vk_allocate_buffer_memory(physical_device, device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, result.buffer);
	vkMapMemory(device, result.memory, 0, result.get_el_size(), 0, &result.mapped_memory);
	return result;
}

func update_uniform_buffer(uniform_buffer<uniform_buffer_object> *buffers, u32 width, u32 height, u32 image_idx, u32 time_elapsed) -> void {
	uniform_buffer_object ubo = {};
	printf("time_elapsed: %u\n", time_elapsed);
	ubo.model = my_mat4_to_glm_mat4(glm::rotate(glm::mat4(1.0f), glm::radians((f32)((f32)time_elapsed * 360.0f * 1.0 / 10000.0f)), glm::vec3(0.0, 0.0, 1.0f)));
	//ubo.model = TransposeMatrix(translation_matrix(V3(1,1,sin((f32)time_elapsed * 10 * 1.0f / 10000.0f))));
	ubo.view = my_mat4_to_glm_mat4(glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0), glm::vec3(0.0, 0.0, 1.0f)));
	ubo.proj = my_mat4_to_glm_mat4(glm::perspective(glm::radians(45.0f), width / (f32)height, 0.1f, 10.0f));
	ubo.proj.Value2_2 *= -1;
	memcpy(buffers[image_idx].mapped_memory, &ubo, sizeof(ubo));
}

func vk_create_descriptor_pool(VkDevice device, u32 frames_in_flight, VkDescriptorType type) -> VkDescriptorPool {
	VkDescriptorPool result = {};
	VkDescriptorPoolSize pool_sizes[2];
	pool_sizes[0].descriptorCount = frames_in_flight;
	pool_sizes[0].type = type;
	pool_sizes[1].descriptorCount = frames_in_flight;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = _countof(pool_sizes);
	info.pPoolSizes = pool_sizes;
	info.maxSets = frames_in_flight;
	vkCreateDescriptorPool(device, &info, nullptr, &result);
	return result;
}

func vk_create_descriptor_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout) -> VkDescriptorSet {
	VkDescriptorSet result = {};
	VkDescriptorSetAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.descriptorPool = pool;
	info.descriptorSetCount = 1;
	info.pSetLayouts = &layout;
	vkAllocateDescriptorSets(device, &info, &result);
	return result;
}

func vk_context::make(HWND hwnd) -> vk_context {
	vk_context result = {};

	RECT rect;
	GetWindowRect(hwnd, &rect);
	result.window_width = rect.right - rect.left;
	result.window_height = rect.bottom - rect.top;

	result.current_frame 			= 0;
	result.u_elapsed_time			= 0;
	result.instance 				= vk_create_instance				();
	result.physical_device 			= vk_pick_physical_device			(result.instance);
	result.device					= vk_create_logical_device			(result.physical_device);
	result.queue					= vk_create_graphics_queue			(result);
	result.surface					= vk_create_surface					(result, hwnd);
	result.swap_chain				= vk_create_swap_chain				(result.physical_device, result.device, result.surface, result.window_width, result.window_height);
	result.swap_chain_image_count	= vk_create_images_for_swap_chain	(&result);
	result.swap_chain_extent		= vk_choose_swap_extent				(result.physical_device, result.surface, result.window_width, result.window_height);
	result.swap_chain_image_format	= vk_choose_surface_format			(result.physical_device, result.surface).format;

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		result.swap_chain_image_views[i] = vk_create_image_view(result.device, result.swap_chain_images[i], 1, vk_choose_surface_format(result.physical_device, result.surface).format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
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

	let binding_desc = vk_vertex::get_binding_description();
	let attrib_desc = vk_vertex::get_attribute_descriptions();
	VkPipelineVertexInputStateCreateInfo vertex_state_info = {};
	vertex_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_state_info.vertexBindingDescriptionCount = 1;
	vertex_state_info.pVertexBindingDescriptions = &binding_desc;
	vertex_state_info.vertexAttributeDescriptionCount = attrib_desc.size();
	vertex_state_info.pVertexAttributeDescriptions = attrib_desc.data();

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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	depth_stencil.front = {};
	depth_stencil.back = {};

	// NOTE(DH): Pay attention please!
	result.descriptor_set_layout = vk_create_descriptor_set_layout(result.device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	result.pipeline_layout = vk_create_pipeline_layout(result.device, result.descriptor_set_layout);
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
	graph_pipeline_info.pDepthStencilState = &depth_stencil;
	graph_pipeline_info.pColorBlendState = &color_blending;
	graph_pipeline_info.pDynamicState = &dynamic_state;
	graph_pipeline_info.layout = result.pipeline_layout;
	graph_pipeline_info.renderPass = result.only_pass;
	graph_pipeline_info.subpass = 0;
	graph_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	graph_pipeline_info.basePipelineIndex = -1;
	vkCreateGraphicsPipelines(result.device, VK_NULL_HANDLE, 1, &graph_pipeline_info, nullptr, &result.graph_pipeline);

	result.command_pool = vk_create_command_pool(result.physical_device, result.device);

	result.depth_texture = vk_create_depth_resources(result.physical_device, result.device, result.queue, result.command_pool, result.swap_chain_extent);

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkImageView attachments[] 			= {result.swap_chain_image_views[i], result.depth_texture.image_view};
		result.swap_chain_framebuffers	[i] = vk_allocate_frame_buffer	(result, result.only_pass, attachments, _countof(attachments), 1);
		result.command_buffers			[i] = vk_create_command_buffer	(result, result.command_pool);
		result.image_avail_semaphores	[i] = vk_create_semaphore		(result.device);
		result.render_finish_semaphores	[i] = vk_create_semaphore		(result.device);
		result.in_flight_fences			[i] = vk_create_fence			(result.device, VK_FENCE_CREATE_SIGNALED_BIT);
	}

	vk_vertex vertices[] = {
		{.position = {-0.5, -0.5, 0.5}, .color = {1.0, 0.0, 0.0}, .tex_coord = {1.0f, 0.0f}},
		{.position = {0.5, -0.5, 0.5}, .color = {0.0, 1.0, 0.0}, .tex_coord = {0.0f, 0.0f}},
		{.position = {0.5, 0.5, 0.5}, .color = {0.0, 0.0, 1.0}, .tex_coord = {0.0f, 1.0f}},
		{.position = {-0.5, 0.5, 0.5}, .color = {1.0, 1.0, 1.0}, .tex_coord = {1.0f, 1.0f}},

		{.position = {-0.5, -0.5, -0.3}, .color = {1.0, 0.0, 0.0}, .tex_coord = {1.0f, 0.0f}},
		{.position = {0.5, -0.5, -0.3}, .color = {0.0, 1.0, 0.0}, .tex_coord = {0.0f, 0.0f}},
		{.position = {0.5, 0.5, -0.3}, .color = {0.0, 0.0, 1.0}, .tex_coord = {0.0f, 1.0f}},
		{.position = {-0.5, 0.5, -0.3}, .color = {1.0, 1.0, 1.0}, .tex_coord = {1.0f, 1.0f}},

		// {.position = {-0.5, -0.5, -0.5}, .color = {1.0, 0.0, 0.0}, .tex_coord = {1.0f, 0.0f}},
		// {.position = {0.5, -0.5, -0.5}, .color = {0.0, 1.0, 0.0}, .tex_coord = {0.0f, 0.0f}},
		// {.position = {0.5, 0.5, -0.5}, .color = {0.0, 0.0, 1.0}, .tex_coord = {0.0f, 1.0f}},
		// {.position = {-0.5, 0.5, -0.5}, .color = {1.0, 1.0, 1.0}, .tex_coord = {1.0f, 1.0f}},
	};
	result.vtex_buffer = create_vertex_buffer(
			result.physical_device, 
			result.device,
			result.queue,
			result.command_pool,
			_countof(vertices) * sizeof(vk_vertex), 
			vertices, 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
			VK_SHARING_MODE_EXCLUSIVE
		);
	
	// u16 indices[] = {0, 1, 2, 2, 3, 0, 4, 6, 5, 4, 7, 6, 3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 0, 4, 1, 1, 4, 5, 2, 1, 5, 5, 6, 2};
	u16 indices[] = {4, 5, 6, 6, 7, 4, 0, 1, 2, 2, 3, 0};
	result.idex_buffer = create_index_buffer<u16>(
			result.physical_device, 
			result.device, 
			result.queue, 
			result.command_pool, 
			_countof(indices) * sizeof(u16), 
			indices, 
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
			VK_SHARING_MODE_EXCLUSIVE
		);
	
	result.uni_descriptor_pool = vk_create_descriptor_pool(result.device, MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	result.simple_texture = vk_create_texture_from_image_stb(result.physical_device, result.device, result.queue, result.command_pool, "..\\data\\textures\\texture.jpg");
	// result.simple_texture = vk_create_texture_from_image_stb(result.physical_device, result.device, result.queue, result.command_pool, "..\\data\\textures\\viking_room.png");
	result.viking_room = load_model(result.physical_device, result.device, result.queue, result.command_pool, "..\\data\\models\\viking_room.obj", "..\\data\\textures\\viking_room.png");

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		result.uni_buffer[i] = vk_create_uniform_buffer(result.physical_device, result.device);
		result.uni_descriptor_sets[i] = vk_create_descriptor_set(result.device, result.uni_descriptor_pool, result.descriptor_set_layout);
		VkDescriptorBufferInfo buffer_info = {};
		buffer_info.buffer = result.uni_buffer[i].buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(uniform_buffer_object);

		// VkDescriptorImageInfo image_info = {};
		// image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// image_info.imageView = result.simple_texture.image_view;
		// image_info.sampler = result.simple_texture.image_sampler;
		VkDescriptorImageInfo image_info = {};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = result.viking_room.texture.image_view;
		image_info.sampler = result.viking_room.texture.image_sampler;

		VkWriteDescriptorSet write_descs[] = {{}, {}};
		write_descs[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descs[0].dstSet = result.uni_descriptor_sets[i];
		write_descs[0].dstBinding = 0;
		write_descs[0].dstArrayElement = 0;
		write_descs[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write_descs[0].descriptorCount = 1;
		write_descs[0].pBufferInfo = &buffer_info;
		write_descs[0].pImageInfo = nullptr;
		write_descs[0].pTexelBufferView = nullptr;

		write_descs[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descs[1].dstSet = result.uni_descriptor_sets[i];
		write_descs[1].dstBinding = 1;
		write_descs[1].dstArrayElement = 0;
		write_descs[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descs[1].descriptorCount = 1;
		write_descs[1].pBufferInfo = nullptr;
		write_descs[1].pImageInfo = &image_info;
		write_descs[1].pTexelBufferView = nullptr;
		vkUpdateDescriptorSets(result.device, _countof(write_descs), write_descs, 0, nullptr);
	}

	result.present_queue = vk_create_graphics_queue(result);

	return result;
}

func vk_recreate_swap_chain(vk_context* ctx, u32 new_width, u32 new_height) -> void {
	vkDeviceWaitIdle(ctx->device);

	vk_cleanup_swap_chain(ctx);

	ctx->swap_chain = vk_create_swap_chain(ctx->physical_device, ctx->device, ctx->surface, new_width, new_height);
	ctx->swap_chain_image_count	= vk_create_images_for_swap_chain	(ctx);
	ctx->depth_texture = vk_create_depth_resources(ctx->physical_device, ctx->device, ctx->queue, ctx->command_pool, ctx->swap_chain_extent);

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		ctx->swap_chain_image_views	[i] = vk_create_image_view(ctx->device, ctx->swap_chain_images[i], 1, vk_choose_surface_format(ctx->physical_device, ctx->surface).format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
		VkImageView attachments		[] 	= {ctx->swap_chain_image_views[i], ctx->depth_texture.image_view};
		ctx->swap_chain_framebuffers[i] = vk_allocate_frame_buffer(*ctx, ctx->only_pass, attachments, _countof(attachments), 1);
	}
}

func vk_cleanup_swap_chain(vk_context* ctx) -> void {
	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroyFramebuffer(ctx->device, ctx->swap_chain_framebuffers[i], nullptr);
	}

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroyImageView(ctx->device, ctx->swap_chain_image_views[i], nullptr);
	}

	vkDestroyImageView(ctx->device, ctx->depth_texture.image_view, nullptr);
	vkDestroyImage(ctx->device, ctx->depth_texture.vk_data.image, nullptr);
	vkFreeMemory(ctx->device, ctx->depth_texture.vk_data.image_memory, nullptr);

	vkDestroySwapchainKHR(ctx->device, ctx->swap_chain, nullptr);
}

func vk_context::draw_frame(u32 dt, u32 width, u32 height) -> void {
	u_elapsed_time = (u_elapsed_time + dt) % 10000;
	
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

	update_uniform_buffer(uni_buffer, width, height, current_frame, u_elapsed_time);

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
	swap_chain_extent = vk_choose_swap_extent (physical_device, surface, width, height);

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