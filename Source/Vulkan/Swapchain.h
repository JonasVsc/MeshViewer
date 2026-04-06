#pragma once
#include <vulkan/vulkan.h>

namespace mv::vk
{
	class Swapchain
	{
	public:

		Swapchain();
		~Swapchain();

	private:

		VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };

	};


} // namespace mv::vk