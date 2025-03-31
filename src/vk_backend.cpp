#include "vk_backend.h"
#include <libloaderapi.h>
#include <stdexcept>
#include <stdlib.h>

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

	VkSurfaceKHR surface;
	char *extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
		"VK_EXT_debug_utils",
		"VK_KHR_get_physical_device_properties2",
	};

	VkInstanceCreateInfo instance_create_info = {};
	instance_create_info.pApplicationInfo = &app_info;
	instance_create_info.enabledExtensionCount = _countof(extensions);
	instance_create_info.enabledLayerCount = 0;
	instance_create_info.ppEnabledExtensionNames = extensions;

	VkResult _result = vkCreateInstance(&instance_create_info, nullptr, &result);

	//vkCreateWin32SurfaceKHR(result, &app_info, nullptr, surface);

	return result;
}

func vk_cleanup(vk_context *ctx) -> void {
	vkDestroyInstance(ctx->instance, nullptr);
}

func vk_pick_physical_device(vk_context ctx) -> VkPhysicalDevice {
	VkPhysicalDevice result = VK_NULL_HANDLE;

	u32 device_count = 0;
	vkEnumeratePhysicalDevices(ctx.instance, &device_count, nullptr);

	if(device_count == 0)
		throw std::runtime_error("failed to find GPUS with VULKAN support!");

	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(ctx.instance, &device_count, devices);

	for(u32 i = 0; i < device_count; ++i) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[i], &props);
		if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			result = devices[i];
		}
	}
	return result;
}

func vk_create_logical_device(vk_context ctx) -> VkDevice {
	VkDevice result;

	// NOTE(DH): Find family indices
	u32 graphics_family = 0;
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties properties[queue_family_count];
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, properties);
	for(u32 i = 0 ; i < queue_family_count; ++i) {
		if(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_family = i;
	}

	// NOTE(DH): We can have a multiple queues and can assign
	// to them priority values, that can modify scheduling
	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = graphics_family;
	queue_create_info.queueCount = 1;
	f32 queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;
	create_info.enabledExtensionCount = 0;

	VkResult err = vkCreateDevice(ctx.physical_device, &create_info, nullptr, &result);

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

func vk_init(HWND hwnd) -> vk_context {
	vk_context result = {};

	result.instance 		= vk_create_instance();
	result.physical_device 	= vk_pick_physical_device(result);
	result.device			= vk_create_logical_device(result);
	result.queue			= vk_create_graphics_queue(result);
	result.surface			= vk_create_surface(result, hwnd);

	return result;
}