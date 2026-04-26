#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <stb_image.h> 
#include <SDL3/SDL.h>
#include <vector>
#include <array>

namespace mv
{
	struct Vertex
	{
		glm::vec2 position;
		glm::vec3 color;
		glm::vec2 tex_coord;

		static vk::VertexInputBindingDescription get_binding_description();
		static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_descriptions();
	};

	struct UniformBufferObject 
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

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
		void create_descriptor_set_layout();
		void create_graphics_pipeline();
		void create_command_pool();
		void create_texture_image();
		void create_texture_image_view();
		void create_texture_sampler();
		void create_vertex_buffer();
		void create_index_buffer();
		void create_uniform_buffers();
		void create_descriptor_pool();
		void create_descriptor_sets();
		void create_command_buffers();
		void create_sync_objects();
		void recreate_swapchain();
		void cleanup_swapchain();
		void update_uniform_buffer(uint32_t current_image);

		std::vector<const char*> get_required_instance_extensions();
		bool is_device_suitable(const vk::raii::PhysicalDevice& physical_device);
		vk::SurfaceFormatKHR choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats);
		vk::PresentModeKHR choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes);
		vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities);
		uint32_t choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities);
		[[nodiscard]] vk::raii::ShaderModule create_shader_module(const std::vector<char>& code) const;
		void record_command_buffer(uint32_t image_index);
		void transition_image_layout(uint32_t image_index, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask);
		void transition_image_layout(const vk::raii::Image& image, vk::ImageLayout old_layout, vk::ImageLayout new_layout);
		uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties);
		void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags propoerties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory);
		void copy_buffer(vk::raii::Buffer& src_buffer, vk::raii::Buffer& dst_buffer, vk::DeviceSize size);
		void copy_buffer_to_image(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);
		void create_image(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& image_memory);
		vk::raii::ImageView create_image_view(vk::raii::Image& image, vk::Format format);
		vk::raii::CommandBuffer begin_single_time_commands();
		void end_single_time_commands(vk::raii::CommandBuffer& command_buffer);


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

		vk::raii::DescriptorSetLayout m_descriptor_set_layout{ nullptr };
		vk::raii::DescriptorPool m_descriptor_pool{ nullptr };
		std::vector<vk::raii::DescriptorSet> m_descriptor_sets;

		vk::raii::PipelineLayout m_pipeline_layout{ nullptr };
		vk::raii::Pipeline m_graphics_pipeline{ nullptr };

		vk::raii::CommandPool m_command_pool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_command_buffers;
		std::vector<vk::raii::Semaphore> m_present_complete_semaphores;
		std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
		std::vector<vk::raii::Fence> m_in_flight_fences;
		uint32_t m_frame_index{ 0 };


		const std::vector<Vertex> m_vertices = {
			{ {-0.5f,-0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 0.5f,-0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
		};

		const std::vector<uint16_t> m_indices = {
			0, 1, 2, 2, 3, 0
		};

		vk::raii::Buffer m_vertex_buffer{ nullptr };
		vk::raii::Buffer m_index_buffer{ nullptr };
		vk::raii::DeviceMemory m_vertex_memory{ nullptr };
		vk::raii::DeviceMemory m_index_memory{ nullptr };
		vk::raii::Image m_texture_image{ nullptr };
		vk::raii::DeviceMemory m_texture_memory{ nullptr };
		vk::raii::ImageView m_texture_image_view{ nullptr };
		vk::raii::Sampler m_texture_sampler{ nullptr };
		
		std::vector<vk::raii::Buffer> m_uniform_buffers;
		std::vector<vk::raii::DeviceMemory> m_uniform_buffers_memory;
		std::vector<void*> m_uniform_buffers_mapped;


	}; // class Application

} // namespace mv