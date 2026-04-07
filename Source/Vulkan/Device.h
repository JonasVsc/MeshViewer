#pragma once
#include <vulkan/vulkan.h>

namespace mv::vk
{
	class Context;

	class Device
	{
	public:

		Device(Context& context);

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

		Device(Device&&) = delete;
		Device& operator=(Device&&) = delete;

		~Device();

		VkPhysicalDevice physical() const { return m_physical; }
		VkDevice logical() const { return m_logical; }

		uint32_t graphics_queue_index() const { return m_graphics_queue_index; }
		uint32_t present_queue_index() const { return m_present_queue_index; }

		VkQueue graphics_queue() const { return m_graphics_queue; }
		VkQueue present_queue() const { return m_present_queue; }

	private:

		void select_physical_device();

		void find_queue_family_indices();

		void create_logical_device();

		Context& m_context;
		
		VkPhysicalDevice m_physical{ VK_NULL_HANDLE };
		VkDevice m_logical{ VK_NULL_HANDLE };

		uint32_t m_graphics_queue_index{};
		uint32_t m_present_queue_index{};

		VkQueue m_graphics_queue{ VK_NULL_HANDLE };
		VkQueue m_present_queue{ VK_NULL_HANDLE };

	}; // class Device

} // namespace mv::vk