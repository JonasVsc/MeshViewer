#pragma once
#include "Vulkan/Swapchain.h"

namespace mv
{
	namespace vk
	{
		class Context;
		class Device;
	}

	class Renderer
	{
	public:

		Renderer(vk::Context& context, vk::Device& device);

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		~Renderer();

		bool begin_frame();
		void begin_rendering();

		void end_frame();

	private:

		void setup();

		struct Frame
		{
			VkCommandPool command_pool;
			VkCommandBuffer command_buffer;
			VkSemaphore image_available_semaphore;
			VkSemaphore render_finished_semaphore;
			VkFence in_flight_fence;
		};

		vk::Context& m_context;
		vk::Device& m_device;

		vk::Swapchain m_swapchain{ m_context, m_device };

		std::vector<Frame> m_frames{};

		uint32_t m_current_frame{};
		uint32_t m_current_image{};

	}; // class Renderer

} // namespace mv