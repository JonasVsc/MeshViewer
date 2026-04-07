#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace mv::vk
{
	class Context;
	class Device;

	class Swapchain
	{
	public:

		static constexpr uint32_t max_frames_in_flight{ 2u };

		Swapchain(Context& context, Device& device);
		~Swapchain();

		VkSwapchainKHR handle() const { return m_swapchain; }
		VkImage image(uint32_t i) const { return m_images[i]; }
		VkImageView view(uint32_t i) const { return m_views[i]; }
		VkExtent2D extent() const { return m_extent; }

	private:

		void create_swapchain();
		void create_image_views();

		Context& m_context;
		Device& m_device;

		VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };

		std::vector<VkImage> m_images{};
		std::vector<VkImageView> m_views{};
		VkSurfaceFormatKHR m_format{};
		VkExtent2D m_extent{};

	};


} // namespace mv::vk