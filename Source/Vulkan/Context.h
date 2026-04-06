#pragma once
#include <vulkan/vulkan.h>

struct SDL_Window;

namespace mv::vk
{
	class Context
	{
	public:

		Context(SDL_Window* window);
		~Context();

		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		Context(Context&&) = delete;
		Context& operator=(Context&&) = delete;
		
		VkInstance instance() const { return m_instance; }
		VkSurfaceKHR surface() const { return m_surface; }

	private:

		void init_instance();

		SDL_Window* m_window{ nullptr };
		VkInstance m_instance{ VK_NULL_HANDLE };
		VkSurfaceKHR m_surface{ VK_NULL_HANDLE };

	}; // class Context

} // namespace mv::vk