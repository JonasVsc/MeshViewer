#include "Context.h"
#include "Assert.h"
#include "Common.h"

#include <SDL3/SDL_vulkan.h>

#include <vector>



namespace mv::vk
{
	Context::Context(SDL_Window* window)
		: m_window{ window }
	{
		FATAL_CHECK(window, "Failed to initialize vulkan context, window reference is nullptr");

		init_instance();

		FATAL_CHECK(SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface), "Failed to create surface {}", SDL_GetError());
	}

	Context::~Context()
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	void Context::init_instance()
	{
		VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Olivia Software",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "Olivia Software",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_3
		};

		VkInstanceCreateInfo instance_ci = {
		   .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		   .pApplicationInfo = &appInfo
		};

		Uint32 SDL_extension_count{};
		const char* const* SDL_vulkan_extensions = SDL_Vulkan_GetInstanceExtensions(&SDL_extension_count);
	
		instance_ci.enabledExtensionCount = SDL_extension_count;
		instance_ci.ppEnabledExtensionNames = SDL_vulkan_extensions;

#ifdef MV_DEBUG

		uint32_t layer_count{};
		vkEnumerateInstanceLayerProperties(&layer_count, NULL);
		std::vector<VkLayerProperties> available_layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		bool validation_supported{ false };
		for (auto& layer : available_layers)
		{
			if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
			{
				validation_supported = true;
				break;
			}
		}

		const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };

		instance_ci.enabledLayerCount = validation_supported ? SDL_arraysize(validation_layers) : 0;
		instance_ci.ppEnabledLayerNames = validation_layers;

#endif // MV_DEBUG

		VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &m_instance));
	}

} // namespace mv::vk