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

		Application() = default;

		void run();

	private:

		void init_window();
		void init_vulkan();
		void main_loop();
		void cleanup();

		void setup_debug_messenger();
		void create_instance();

		std::vector<const char*> get_required_instance_extensions();

		bool m_running{ false };

		SDL_Window* m_window{ nullptr };

		vk::raii::Context m_context;
		vk::raii::Instance m_instance{ nullptr };
		vk::raii::DebugUtilsMessengerEXT debug_messenger{ nullptr };

	}; // class Application

} // namespace mv