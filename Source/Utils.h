#pragma once
#include <SDL3/SDL.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

namespace mv
{
	std::vector<char> read_file(const std::string& filename);

	bool is_device_suitable(const vk::raii::PhysicalDevice& physical_device);

	vk::SurfaceFormatKHR choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats);

	vk::PresentModeKHR choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes);

	vk::Extent2D choose_swap_extent(SDL_Window* window, const vk::SurfaceCapabilitiesKHR& capabilities);

	uint32_t choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities);

	void transition_image_layout(vk::raii::CommandBuffer& cmd, vk::Image image, 
		vk::ImageLayout old_layout, vk::ImageLayout new_layout, 
		vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, 
		vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, 
		vk::ImageAspectFlags image_aspect_flags);

	uint32_t find_memory_type(vk::raii::PhysicalDevice& physical_device, uint32_t type_filter, vk::MemoryPropertyFlags properties);

	vk::Format find_supported_format(vk::raii::PhysicalDevice& physical_device, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlagBits features);

	vk::Format find_depth_format(vk::raii::PhysicalDevice& physical_device);

} // namespace mv