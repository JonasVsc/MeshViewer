#include "Device.h"
#include "Context.h"
#include "Common.h"

#include <vector>
#include <array>

namespace mv::vk
{
	Device::Device(Context& context)
		: m_context{ context }
	{
		select_physical_device();

		find_queue_family_indices();

		create_logical_device();
	}

	Device::~Device()
	{
		vkDestroyDevice(m_logical, nullptr);
	}

	void Device::select_physical_device()
	{
		uint32_t physical_device_count{};
		VK_CHECK(vkEnumeratePhysicalDevices(m_context.instance(), &physical_device_count, NULL));
		FATAL_CHECK(physical_device_count > 0, "Failed, no suitable physical device");

		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		VK_CHECK(vkEnumeratePhysicalDevices(m_context.instance(), &physical_device_count, physical_devices.data()));

		for (auto physical_device : physical_devices)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_device, &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				m_physical = physical_device;
				break;
			}
		}

		if (!m_physical) 
		{ 
			m_physical = physical_devices[0]; 
		}
	}

	void Device::find_queue_family_indices()
	{
		uint32_t queue_family_count{};
		vkGetPhysicalDeviceQueueFamilyProperties(m_physical, &queue_family_count, NULL);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physical, &queue_family_count, queue_families.data());

		uint32_t q_index{};
		uint32_t graphics_queue_index{ UINT32_MAX };
		uint32_t present_queue_index{ UINT32_MAX };

		for (auto& queue_family : queue_families)
		{
			VkBool32 present_support = VK_FALSE;
			VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physical, q_index, m_context.surface(), &present_support));

			if (present_support)
				present_queue_index = q_index;

			if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				graphics_queue_index = q_index;

			if (graphics_queue_index != -1 && present_queue_index != -1)
				break;

			q_index++;
		}

		FATAL_CHECK((graphics_queue_index != UINT32_MAX || present_queue_index != UINT32_MAX), "Failed to find suitable gpu queue families");
		FATAL_CHECK((graphics_queue_index == present_queue_index), "Failed, exclusive sharing mode not supported");

		m_graphics_queue_index = graphics_queue_index;
		m_present_queue_index = present_queue_index;
	}

	void Device::create_logical_device()
	{

		float queue_priority = 1.0f;
		VkDeviceQueueCreateInfo queue_ci
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = static_cast<uint32_t>(m_graphics_queue_index),
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};

		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
			.dynamicRendering = VK_TRUE,
		};

		std::array<const char*, 2> enabled_extensions
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		};

		VkDeviceCreateInfo device_ci
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &dynamicRenderingFeature,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue_ci,
			.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
			.ppEnabledExtensionNames = enabled_extensions.data()
		};

		VK_CHECK(vkCreateDevice(m_physical, &device_ci, NULL, &m_logical));

		vkGetDeviceQueue(m_logical, m_graphics_queue_index, 0, &m_graphics_queue);
		vkGetDeviceQueue(m_logical, m_present_queue_index, 0, &m_present_queue);
	}

} // namespace mv::vk