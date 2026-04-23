#include "Application.h"

#include <SDL3/SDL_vulkan.h>
#include <iostream>

namespace mv
{
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

	void Application::run()
	{
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

	void Application::init_window()
	{
		if(!SDL_Init(SDL_INIT_VIDEO))
			throw std::runtime_error("failed to init sdl");

		m_window = SDL_CreateWindow("MeshViewer", 800, 600, SDL_WINDOW_VULKAN);
		if (!m_window)
			throw std::runtime_error("failed to create window");

		m_running = true;
	}

	void Application::init_vulkan()
	{
		create_instance();
		setup_debug_messenger();
	}

	void Application::main_loop()
	{
		while (m_running)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
				{
					m_running = false;
				}
			}
		}
	}

	void Application::cleanup()
	{
		SDL_DestroyWindow(m_window);
		SDL_Quit();
	}

	void Application::setup_debug_messenger()
	{
		if (!enable_validation_layers)
			return;

		vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
															 vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | 
															 vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
															 vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

		vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
															 vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | 
															 vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
															 vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding);
		
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT
		{ 
			.messageSeverity = severity_flags,
			.messageType = message_type_flags,
			.pfnUserCallback = &debug_callback 
		};
		debug_messenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	void Application::create_instance()
	{
		constexpr vk::ApplicationInfo app_info
		{ 
			.pApplicationName = "MeshViewer",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "No Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = vk::ApiVersion14 
		};

		std::vector<const char*> required_layers;
		if (enable_validation_layers)
		{
			required_layers.assign(validation_layers.begin(), validation_layers.end());
		}

		auto layer_properties = m_context.enumerateInstanceLayerProperties();
		auto unsupported_layer_it = 
			std::ranges::find_if(required_layers, 
				[&layer_properties](auto const& required_layer) 
				{
					return std::ranges::none_of(layer_properties,
					[required_layer](auto const& layerProperty) 
					{ 
						return strcmp(layerProperty.layerName, required_layer) == 0; 
					});
				});

		if (unsupported_layer_it != required_layers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupported_layer_it));
		}

		auto required_extensions = get_required_instance_extensions();

		auto extension_properties = m_context.enumerateInstanceExtensionProperties();
		auto unsupported_property_it = 
			std::ranges::find_if(required_extensions,
				[&extension_properties](auto const& required_extension)
				{
					return std::ranges::none_of(extension_properties,
					[required_extension](auto const& extension_property)
					{
						return strcmp(extension_property.extensionName, required_extension) == 0;
					});
				});

		if (unsupported_property_it != required_extensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupported_property_it));
		}

		vk::InstanceCreateInfo instance_ci
		{
			.pApplicationInfo = &app_info,
			.enabledLayerCount = static_cast<uint32_t>(required_layers.size()),
			.ppEnabledLayerNames = required_layers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
			.ppEnabledExtensionNames = required_extensions.data(),
		};

		m_instance = vk::raii::Instance(m_context, instance_ci);
	}

	std::vector<const char*> Application::get_required_instance_extensions()
	{
		uint32_t SDL_ExtensionCount;
		auto instance_extensions = SDL_Vulkan_GetInstanceExtensions(&SDL_ExtensionCount);

		std::vector<const char*> extensions(instance_extensions, instance_extensions + SDL_ExtensionCount);

		if (enable_validation_layers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
	{
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		return vk::False;
	}


} // namespace mv