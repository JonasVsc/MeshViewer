#include "Application.h"

#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <map>

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
		create_surface();
		setup_debug_messenger();
		pick_physical_device();
		create_logical_device();
		create_swapchain();
		create_image_views();
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

	void Application::create_surface()
	{
		VkSurfaceKHR surface{ nullptr };

		if (!SDL_Vulkan_CreateSurface(m_window, *m_instance, nullptr, &surface))
		{
			throw std::runtime_error("failed to create window surface");
		}

		m_surface = vk::raii::SurfaceKHR(m_instance, surface);

	}

	void Application::pick_physical_device()
	{
		auto physical_devices = m_instance.enumeratePhysicalDevices();
		const auto physical_devices_it = std::ranges::find_if(physical_devices,
			[&](const auto& physical_device)
			{
				return is_device_suitable(physical_device);
			});
		if (physical_devices_it == physical_devices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		m_physical_device = *physical_devices_it;
	}

	void Application::create_logical_device()
	{
		auto queue_family_properties = m_physical_device.getQueueFamilyProperties();

		uint32_t queue_index = ~0;
		for (uint32_t i = 0; i < queue_family_properties.size(); i++)
		{
			if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && m_physical_device.getSurfaceSupportKHR(i, *m_surface))
			{
				queue_index = i;
				break;
			}
		}

		if (queue_index == ~0)
		{
			throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
		}

		float queue_priority = 0.5f;
		vk::DeviceQueueCreateInfo device_queue_ci
		{
			.queueFamilyIndex = queue_index,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};

		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> feature_chain
		{
			{},
			{ .dynamicRendering = true },
			{ .extendedDynamicState = true }
		};

		vk::DeviceCreateInfo device_ci
		{
			.pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &device_queue_ci,
			.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
			.ppEnabledExtensionNames = required_device_extensions.data()
		};

		m_device = vk::raii::Device(m_physical_device, device_ci);
	
		m_graphics_queue = vk::raii::Queue(m_device, queue_index, 0);
	}

	void Application::create_swapchain()
	{
		auto surface_capabilities    = m_physical_device.getSurfaceCapabilitiesKHR(*m_surface);
		auto available_formats       = m_physical_device.getSurfaceFormatsKHR(*m_surface);
		auto available_present_modes = m_physical_device.getSurfacePresentModesKHR(*m_surface);
	
		m_swapchain_extent = choose_swap_extent(surface_capabilities);
		m_swapchain_surface_format = choose_swap_surface_format(available_formats);

		uint32_t min_image_count = choose_swap_min_image_count(surface_capabilities);


		vk::SwapchainCreateInfoKHR swapchain_ci
		{
			.surface = *m_surface,
			.minImageCount = min_image_count,
			.imageFormat = m_swapchain_surface_format.format,
			.imageColorSpace = m_swapchain_surface_format.colorSpace,
			.imageExtent = m_swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
			.imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = surface_capabilities.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = choose_swap_present_mode(available_present_modes),
			.clipped = true,
			.oldSwapchain = nullptr
		};

		m_swapchain = vk::raii::SwapchainKHR(m_device, swapchain_ci);
		m_swapchain_images = m_swapchain.getImages();
	}

	void Application::create_image_views()
	{
		assert(m_swapchain_image_views.empty());

		vk::ImageViewCreateInfo image_view_ci
		{
			.viewType = vk::ImageViewType::e2D,
			.format = m_swapchain_surface_format.format,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};

		for (auto& image : m_swapchain_images)
		{
			image_view_ci.image = image;
			m_swapchain_image_views.emplace_back(m_device, image_view_ci);
		}
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

	bool Application::is_device_suitable(const vk::raii::PhysicalDevice& physical_device)
	{
		// check if the physical device supports the vulkan 1.3 API version
		bool supports_vulkan1_3 = physical_device.getProperties().apiVersion >= vk::ApiVersion13;

		// check if any of the queue families support graphics operations
		auto queue_families = physical_device.getQueueFamilyProperties();
		bool support_graphics = std::ranges::any_of(queue_families,
			[](const auto& queue_family_property)
			{
				return !!(queue_family_property.queueFlags & vk::QueueFlagBits::eGraphics);
			});

		// check if all required physical device extensions are available
		auto available_device_extensions = physical_device.enumerateDeviceExtensionProperties();
		bool support_all_required_extensions = std::ranges::all_of(required_device_extensions,
			[&available_device_extensions](const auto& required_device_extension)
			{
				return std::ranges::any_of(available_device_extensions, [&required_device_extension](const auto& available_device_extension)
				{
					return std::strcmp(available_device_extension.extensionName, required_device_extension) == 0;
				});
			});

		// check if the physical device supports the required features (dynamic rendering and extended dynamic state)
		auto features = physical_device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool support_required_features = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
										 features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supports_vulkan1_3 && support_graphics && support_all_required_extensions && support_required_features;
	}

	vk::SurfaceFormatKHR Application::choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> available_formats)
	{
		assert(!available_formats.empty());

		const auto format_it = std::ranges::find_if(available_formats,
			[](const auto& format)
			{
				return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});

		return format_it != available_formats.end() ? *format_it : available_formats[0];
	}

	vk::PresentModeKHR Application::choose_swap_present_mode(std::span<const vk::PresentModeKHR> available_present_modes)
	{
		assert(std::ranges::any_of(available_present_modes, [](const auto present_mode) { return present_mode == vk::PresentModeKHR::eFifo; }));

		return std::ranges::any_of(available_present_modes,
			[](const auto present_mode)
			{
				return present_mode == vk::PresentModeKHR::eMailbox;
			}) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D Application::choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		int width, height;
		SDL_GetWindowSizeInPixels(m_window, &width, &height);

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	uint32_t Application::choose_swap_min_image_count(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		auto min_image_count = std::max(3u, capabilities.minImageCount);
		if ((0 < capabilities.maxImageCount) && capabilities.maxImageCount < min_image_count)
		{
			min_image_count = capabilities.maxImageCount;
		}

		return min_image_count;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}
		return vk::False;
	}


} // namespace mv