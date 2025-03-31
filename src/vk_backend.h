#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"

struct vk_context {
	VkInstance 			instance;
	VkPhysicalDevice 	physical_device;
	VkDevice 			device;
	VkQueue				queue;
	VkQueue				present_queue;
	VkSurfaceKHR		surface;
};
func vk_init() 						-> vk_context;

func vk_cleanup(vk_context *ctx)	-> void;
func vk_pick_physical_device(vk_context ctx) -> VkPhysicalDevice;
func vk_create_logical_device(vk_context ctx) -> VkDevice;