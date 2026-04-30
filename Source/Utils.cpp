#include "Utils.h"
#include "Viewer.h"

#include <fstream>

namespace mv
{
	std::vector<char> read_file(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("failed to open file!");

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();

		return buffer;
	}

	bool is_device_suitable(const vk::raii::PhysicalDevice& physical_device)
	{
		// check vk version
		bool supports_vulkan1_3 = physical_device.getProperties().apiVersion >= vk::ApiVersion13;

		// check if has graphics queue
		auto queue_families = physical_device.getQueueFamilyProperties();
		const bool support_graphics = std::ranges::any_of(queue_families,
			[](const auto& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// check required device extensions
		auto available_device_extensions = physical_device.enumerateDeviceExtensionProperties();
		const bool support_all_required_extensions = std::ranges::all_of(Viewer::required_device_extensions,
			[&available_device_extensions](const auto& required_ext) {
				return std::ranges::any_of(available_device_extensions, [&required_ext](const auto& available_ext) {
					return std::strcmp(available_ext.extensionName, required_ext) == 0; });
			});

		// check required features
		auto features = physical_device.template getFeatures2<vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

		bool support_required_features = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
			features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
			features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supports_vulkan1_3 && support_graphics && support_all_required_extensions && support_required_features;
	}

	vk::SurfaceFormatKHR choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats)
	{
		assert(!available_formats.empty());

		const auto format_it = std::ranges::find_if(available_formats,
			[](const auto& format)
			{
				return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});

		return format_it != available_formats.end() ? *format_it : available_formats[0];
	}

	vk::PresentModeKHR choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes)
	{
		assert(std::ranges::any_of(available_present_modes, [](const auto present_mode) { return present_mode == vk::PresentModeKHR::eFifo; }));

		return std::ranges::any_of(available_present_modes,
			[](const auto present_mode)
			{
				return present_mode == vk::PresentModeKHR::eMailbox;
			}) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D choose_swap_extent(SDL_Window* window, const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		int width, height;
		SDL_GetWindowSizeInPixels(window, &width, &height);

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	uint32_t choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		auto min_image_count = std::max(3u, capabilities.minImageCount);
		if ((0 < capabilities.maxImageCount) && capabilities.maxImageCount < min_image_count)
		{
			min_image_count = capabilities.maxImageCount;
		}

		return min_image_count;
	}

	void transition_image_layout(vk::raii::CommandBuffer& cmd, vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier =
		{
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				   .aspectMask = image_aspect_flags,
				   .baseMipLevel = 0,
				   .levelCount = 1,
				   .baseArrayLayer = 0,
				   .layerCount = 1
			}
		};
		vk::DependencyInfo dependency_info
		{
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};

		cmd.pipelineBarrier2(dependency_info);
	}

	uint32_t find_memory_type(vk::raii::PhysicalDevice& physical_device, uint32_t type_filter, vk::MemoryPropertyFlags properties)
	{
		vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();

		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type");
	}

	vk::Format find_supported_format(vk::raii::PhysicalDevice& physical_device, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlagBits features)
	{
		for (const auto format : candidates)
		{
			vk::FormatProperties props = physical_device.getFormatProperties(format);

			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format");
	}

	vk::Format find_depth_format(vk::raii::PhysicalDevice& physical_device)
	{
		return find_supported_format(
			physical_device,
			{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

} // namespace mv