#include "Swapchain.h"
#include "Context.h"
#include "Device.h"
#include "Common.h"


namespace mv::vk
{
	Swapchain::Swapchain(Context& context, Device& device)
		: m_context{ context }
		, m_device{ device }
	{
		create_swapchain();
		create_image_views();
	}

	Swapchain::~Swapchain()
	{
		for (auto view : m_views)
		{
			vkDestroyImageView(m_device.logical(), view, nullptr);
		}

		vkDestroySwapchainKHR(m_device.logical(), m_swapchain, nullptr);
	}

	void Swapchain::create_swapchain()
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.physical(), m_context.surface(), &capabilities));

		uint32_t min_image_count = (capabilities.minImageCount > max_frames_in_flight)
			? capabilities.minImageCount
			: max_frames_in_flight;

		uint32_t surface_format_count{};
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physical(), m_context.surface(), &surface_format_count, nullptr);

		std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.physical(), m_context.surface(), &surface_format_count, surface_formats.data());


		for (auto& format : surface_formats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			{
				m_format = format;
				break;
			}
		}

		if (m_format.format != VK_FORMAT_B8G8R8A8_SRGB)
		{
			m_format = surface_formats[0];
		}

		VkSwapchainCreateInfoKHR swapchain_ci
		{
			 .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_context.surface(),
			.minImageCount = min_image_count,
			.imageFormat = m_format.format,
			.imageColorSpace = m_format.colorSpace,
			.imageExtent = capabilities.currentExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
			.oldSwapchain = VK_NULL_HANDLE
		};

		m_extent = capabilities.currentExtent;

		VK_CHECK(vkCreateSwapchainKHR(m_device.logical(), &swapchain_ci, nullptr, &m_swapchain));
	}

	void Swapchain::create_image_views()
	{
		uint32_t image_count{};
		VK_CHECK(vkGetSwapchainImagesKHR(m_device.logical(), m_swapchain, &image_count, nullptr));

		m_images.resize(image_count);
		m_views.resize(image_count);

		VK_CHECK(vkGetSwapchainImagesKHR(m_device.logical(), m_swapchain, &image_count, m_images.data()));

		uint32_t image_index{};
		for (auto image : m_images)
		{
			VkImageViewCreateInfo view_ci = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_format.format,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			VK_CHECK(vkCreateImageView(m_device.logical(), &view_ci, nullptr, &m_views[image_index]));
			image_index++;
		}

	}

} // namespace mv::vk