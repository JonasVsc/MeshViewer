#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <vector>

namespace mv
{
	class Application
	{
	public:

		static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
		static constexpr bool enable_validation_layers = false;
#else
		static constexpr bool enable_validation_layers = true;
#endif

		std::vector<const char*> validation_layers
		{
			"VK_LAYER_KHRONOS_validation"
		};

		std::vector<const char*> required_device_extensions
		{
			vk::KHRSwapchainExtensionName
		};

		Application() = default;

		void run();

	private:

		void init_window();
		void init_vulkan();
		void main_loop();
		void draw_frame();
		void cleanup();

		void setup_debug_messenger();
		void create_instance();
		void create_surface();
		void pick_physical_device();
		void create_logical_device();
		void create_swapchain();
		void create_image_views();
		void create_graphics_pipeline();
		void create_command_pool();
		void create_command_buffers();
		void create_sync_objects();
		void recreate_swapchain();
		void cleanup_swapchain();

		std::vector<const char*> get_required_instance_extensions();
		bool is_device_suitable(const vk::raii::PhysicalDevice& physical_device);
		vk::SurfaceFormatKHR choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats);
		vk::PresentModeKHR choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes);
		vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities);
		uint32_t choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] vk::raii::ShaderModule create_shader_module(const std::vector<char>& code) const;
		void record_command_buffer(uint32_t image_index);
		void transition_image_layout(uint32_t image_index, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask);

		bool m_running{ false };
		bool m_framebuffer_resized{ false };
		bool m_framebuffer_minimized{ false };

		SDL_Window* m_window{ nullptr };

		vk::raii::Context m_context;
		vk::raii::Instance m_instance{ nullptr };
		vk::raii::SurfaceKHR m_surface{ nullptr };
		vk::raii::DebugUtilsMessengerEXT debug_messenger{ nullptr };
		vk::raii::PhysicalDevice m_physical_device{ nullptr };
		vk::raii::Device m_device{ nullptr };
		vk::raii::Queue m_queue{ nullptr };
		uint32_t m_queue_index{ 0 };

		vk::raii::SwapchainKHR m_swapchain{ nullptr };
		std::vector<vk::Image> m_swapchain_images;
		std::vector<vk::raii::ImageView> m_swapchain_image_views;
		vk::Extent2D m_swapchain_extent{};
		vk::SurfaceFormatKHR m_swapchain_surface_format{};

		vk::raii::PipelineLayout m_pipeline_layout{ nullptr };
		vk::raii::Pipeline m_graphics_pipeline{ nullptr };

		vk::raii::CommandPool m_command_pool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_command_buffers;
		std::vector<vk::raii::Semaphore> m_present_complete_semaphores;
		std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
		std::vector<vk::raii::Fence> m_in_flight_fences;
		uint32_t m_frame_index{ 0 };

	}; // class Application

} // namespace mv