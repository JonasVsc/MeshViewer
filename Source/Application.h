#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <vector>

namespace mv
{
	class Application
	{
	public:

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
		void cleanup();

		void setup_debug_messenger();
		void create_instance();
		void create_surface();
		void pick_physical_device();
		void create_logical_device();
		void create_swapchain();
		void create_image_views();

		std::vector<const char*> get_required_instance_extensions();
		bool is_device_suitable(const vk::raii::PhysicalDevice& physical_device);
		vk::SurfaceFormatKHR choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats);
		vk::PresentModeKHR choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes);
		vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities);
		uint32_t choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities);

		bool m_running{ false };

		SDL_Window* m_window{ nullptr };

		vk::raii::Context m_context;
		vk::raii::Instance m_instance{ nullptr };
		vk::raii::SurfaceKHR m_surface{ nullptr };
		vk::raii::DebugUtilsMessengerEXT debug_messenger{ nullptr };
		vk::raii::PhysicalDevice m_physical_device{ nullptr };
		vk::raii::Device m_device{ nullptr };
		vk::raii::Queue m_graphics_queue{ nullptr };
		
		vk::raii::SwapchainKHR m_swapchain{ nullptr };
		std::vector<vk::Image> m_swapchain_images;
		std::vector<vk::raii::ImageView> m_swapchain_image_views;
		vk::Extent2D m_swapchain_extent{};
		vk::SurfaceFormatKHR m_swapchain_surface_format{};

	}; // class Application

} // namespace mv