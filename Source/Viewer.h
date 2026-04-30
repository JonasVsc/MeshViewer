#pragma once
#include <SDL3/SDL.h>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <array>

#define KB(x) x * 1000
#define MB(x) KB(x) * 1000

namespace mv
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static vk::VertexInputBindingDescription get_binding_description();
		static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_descriptions();
	};

	class Viewer
	{
	public:

		static constexpr size_t MEMORY_BLOCK{ MB(256) };

		static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

		static constexpr std::array<const char*, 1> required_device_extensions
		{
			vk::KHRSwapchainExtensionName
		};

		void run();

	private:

		void init_window();
		void init_vulkan();

		void cleanup_swapchain();
		void cleanup();
		
		bool window_closed() const { return m_window_closed; }
		void poll_events();

		void create_instance();
		void create_surface();
		void select_physical_device();
		void create_logical_device();
		void create_swapchain();
		void create_sync_objects();
		void create_descriptor_set_layout();
		void create_graphics_pipeline();
		void create_staging_buffer();

		void recreate_swapchain();
		void record_command_buffer(uint32_t image_index);
		void draw_frame();

		vk::raii::ShaderModule create_shader_module(const std::vector<char>& code);
		void                   create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory);
		void                   create_image(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& image_memory);
		vk::raii::ImageView    create_image_view(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspect_flags);

		vk::raii::CommandBuffer begin_single_time_commands();
		void                    end_single_time_commands(vk::raii::CommandBuffer& command_buffer);

	private:

		// window

		SDL_Window* m_window{ nullptr };
		bool m_window_closed{ false };
		bool m_window_resized{ false };
		bool m_window_minimized{ false };

		// vulkan core

		vk::raii::Context        m_context;
		vk::raii::Instance       m_instance{ nullptr };
		vk::raii::SurfaceKHR     m_surface{ nullptr };
		vk::raii::PhysicalDevice m_physical_device{ nullptr };
		vk::raii::Device         m_device{ nullptr };
		vk::raii::Queue          m_graphics_queue{ nullptr };
		uint32_t                 m_graphics_queue_family_index = ~0;

		// vulkan swapchain

		vk::raii::SwapchainKHR           m_swapchain{ nullptr };
		vk::Extent2D                     m_swapchain_extent;
		vk::SurfaceFormatKHR             m_swapchain_surface_format;
		std::vector<vk::Image>           m_swapchain_images;
		std::vector<vk::raii::ImageView> m_swapchain_views;
		vk::raii::Image                  m_depth_image{ nullptr };
		vk::raii::DeviceMemory           m_depth_image_memory{ nullptr };
		vk::raii::ImageView              m_depth_image_view{ nullptr };

		// vulkan per frame

		vk::raii::CommandPool                m_command_pool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_command_buffers;
		std::vector<vk::raii::Semaphore>     m_present_complete_semaphores;
		std::vector<vk::raii::Semaphore>     m_render_finished_semaphores;
		std::vector<vk::raii::Fence>         m_in_flight_fences;
		uint32_t                             m_frame_index{ 0 };

		// vulkan resources

		vk::raii::DescriptorSetLayout m_descriptor_set_layout{ nullptr };
		vk::raii::PipelineLayout      m_pipeline_layout{ nullptr };
		vk::raii::Pipeline            m_graphics_pipeline{ nullptr };

		// loader resources

		vk::raii::Buffer m_staging_buffer{ nullptr };
		vk::raii::DeviceMemory m_staging_buffer_memory{ nullptr };
		vk::DeviceSize m_staging_offset{ 0 };

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

	}; // class Viewer

} // namespace mv