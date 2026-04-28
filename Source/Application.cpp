#include "Application.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h> 
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <map>

namespace mv
{
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

	namespace
	{
		static std::vector<char> read_file(const std::string& filename)
		{
			std::ifstream file(filename, std::ios::ate | std::ios::binary);

			if (!file.is_open())
				throw std::runtime_error("failed to open file!");

			std::vector<char> buffer(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			file.close();
		
			return buffer;
		}

	} // namespace

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

		m_window = SDL_CreateWindow("MeshViewer", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
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
		create_descriptor_set_layout();
		create_graphics_pipeline();
		create_command_pool();
		create_depth_resources();
		create_texture_image();
		create_texture_image_view();
		create_texture_sampler();
		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffers();
		create_descriptor_pool();
		create_descriptor_sets();
		create_command_buffers();
		create_sync_objects();
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
				if (event.type == SDL_EVENT_WINDOW_RESIZED)
				{
					std::cerr << "Window resized\n";
					m_framebuffer_resized = true;
				}
				if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
				{
					std::cerr << "Window minimized\n";
					m_framebuffer_minimized = true;
				}
				if (event.type == SDL_EVENT_WINDOW_RESTORED)
				{
					std::cerr << "Window restored\n";
					m_framebuffer_minimized = false;
				}
			}

			draw_frame();
		}

		m_device.waitIdle();
	}

	void Application::draw_frame()
	{
		auto fence_result = m_device.waitForFences(*m_in_flight_fences[m_frame_index], vk::True, UINT64_MAX);
		if (fence_result != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence");
		}

		auto [result, image_index] = m_swapchain.acquireNextImage(UINT64_MAX, *m_present_complete_semaphores[m_frame_index], nullptr);
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			recreate_swapchain();
			return;
		}
		if ((result == vk::Result::eTimeout) || (result == vk::Result::eNotReady))
		{
			throw std::runtime_error("failed to acquire swap chain image");
		}

		update_uniform_buffer(m_frame_index);

		m_device.resetFences(*m_in_flight_fences[m_frame_index]);

		m_command_buffers[m_frame_index].reset();
		record_command_buffer(image_index);

		vk::PipelineStageFlags wait_destination_stage_mask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
		const vk::SubmitInfo submit_info
		{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*m_present_complete_semaphores[m_frame_index],
			.pWaitDstStageMask = &wait_destination_stage_mask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*m_command_buffers[m_frame_index],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*m_render_finished_semaphores[image_index]
		};

		m_queue.submit(submit_info, *m_in_flight_fences[m_frame_index]);

		const vk::PresentInfoKHR present_info
		{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*m_render_finished_semaphores[image_index],
			.swapchainCount = 1,
			.pSwapchains = &*m_swapchain,
			.pImageIndices = &image_index
		};

		result = m_queue.presentKHR(present_info);
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || m_framebuffer_resized)
		{
			m_framebuffer_resized = false;
			recreate_swapchain();
		}
		else
		{
			assert(result == vk::Result::eSuccess);
		}

		m_frame_index = (m_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Application::cleanup()
	{
		cleanup_swapchain();

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

		// query for Vulkan 1.3 features
		vk::StructureChain<vk::PhysicalDeviceFeatures2,
						   vk::PhysicalDeviceVulkan11Features,
						   vk::PhysicalDeviceVulkan13Features,
						   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		feature_chain
		{
				{.features = {.samplerAnisotropy = true} },                  // vk::PhysicalDeviceFeatures2
				{.shaderDrawParameters = true},                              // vk::PhysicalDeviceVulkan11Features
				{.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
				{.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};

		float queue_priority = 0.5f;
		vk::DeviceQueueCreateInfo device_queue_ci
		{
			.queueFamilyIndex = queue_index,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
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

		m_queue = vk::raii::Queue(m_device, queue_index, 0);
		m_queue_index = queue_index;
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

	void Application::create_descriptor_set_layout()
	{
		std::array bindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
		};

		vk::DescriptorSetLayoutCreateInfo layout_ci{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };
		m_descriptor_set_layout = vk::raii::DescriptorSetLayout(m_device, layout_ci);
	}

	void Application::create_graphics_pipeline()
	{
		vk::raii::ShaderModule shader_module = create_shader_module(read_file("Shaders/tutorial.spv"));
	
		// shader stages
		vk::PipelineShaderStageCreateInfo vert_shader_ci{ .stage = vk::ShaderStageFlagBits::eVertex, .module = shader_module, .pName = "vertMain" };
		vk::PipelineShaderStageCreateInfo frag_shader_ci{ .stage = vk::ShaderStageFlagBits::eFragment, .module = shader_module, .pName = "fragMain" };
		vk::PipelineShaderStageCreateInfo pipeline_shader_stages[] = { vert_shader_ci, frag_shader_ci };
	
		// vertex input
		auto binding_description = Vertex::get_binding_description();
		auto attribute_descriptions = Vertex::get_attribute_descriptions();

		vk::PipelineVertexInputStateCreateInfo pipeline_vertex_Input
		{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &binding_description,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
			.pVertexAttributeDescriptions = attribute_descriptions.data()
		};

		// input assembly
		vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly{ .topology = vk::PrimitiveTopology::eTriangleList };

		// viewports and scissors
		vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(m_swapchain_extent.width), static_cast<float>(m_swapchain_extent.height), 0.0f, 1.0f };
		vk::Rect2D scissor{ vk::Offset2D{ 0, 0 }, m_swapchain_extent };
		vk::PipelineViewportStateCreateInfo pipeline_viewport{ .viewportCount = 1, .scissorCount = 1 };

		// dynamic states
		std::vector<vk::DynamicState> dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state{ .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()), .pDynamicStates = dynamic_states.data() };

		// rasterizer
		vk::PipelineRasterizationStateCreateInfo pipeline_rasterizer
		{
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};

		// multisampling
		vk::PipelineMultisampleStateCreateInfo pipeline_multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False };
	
		// depth stencil
		vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil
		{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False,
		};

		// color blending
		vk::PipelineColorBlendAttachmentState color_blend_attachment
		{
			.blendEnable = vk::True,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp = vk::BlendOp::eAdd,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA 
		};

		vk::PipelineColorBlendStateCreateInfo pipeline_color_blending{ .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &color_blend_attachment };

		// pipeline layout
		vk::PipelineLayoutCreateInfo layout_ci{ .setLayoutCount = 1, .pSetLayouts = &*m_descriptor_set_layout };
		m_pipeline_layout = vk::raii::PipelineLayout(m_device, layout_ci);

		vk::Format depth_format = find_depth_format();

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_chain_ci
		{
			{
				.stageCount = 2,
				.pStages = pipeline_shader_stages,
				.pVertexInputState = &pipeline_vertex_Input,
				.pInputAssemblyState = &pipeline_input_assembly,
				.pViewportState = &pipeline_viewport,
				.pRasterizationState = &pipeline_rasterizer,
				.pMultisampleState = &pipeline_multisampling,
				.pDepthStencilState = &pipeline_depth_stencil,
				.pColorBlendState = &pipeline_color_blending,
				.pDynamicState = &pipeline_dynamic_state,
				.layout = m_pipeline_layout,
				.renderPass = nullptr,
			},
			{ .colorAttachmentCount = 1, .pColorAttachmentFormats = &m_swapchain_surface_format.format, .depthAttachmentFormat = depth_format }
		};

		m_graphics_pipeline = vk::raii::Pipeline(m_device, nullptr, pipeline_chain_ci.get<vk::GraphicsPipelineCreateInfo>());
	}

	void Application::create_command_pool()
	{
		vk::CommandPoolCreateInfo cmd_pool_ci
		{ 
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = m_queue_index 
		};

		m_command_pool = vk::raii::CommandPool(m_device, cmd_pool_ci);
	}

	void Application::create_depth_resources()
	{
		vk::Format depth_format = find_depth_format();

		create_image(
			m_swapchain_extent.width, m_swapchain_extent.height, depth_format,
			vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			m_depth_image, m_depth_image_memory
		);

		m_depth_image_view = create_image_view(m_depth_image, depth_format, vk::ImageAspectFlagBits::eDepth);
	}

	void Application::create_texture_image()
	{
		int tex_width, tex_height, tex_channels;
		stbi_uc* pixels = stbi_load("Textures/texture.jpg", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
		vk::DeviceSize image_size = tex_width * tex_height * 4;

		if (!pixels)
		{
			throw std::runtime_error("failed to load texture image");
		}

		vk::raii::Buffer staging_buffer{ nullptr };
		vk::raii::DeviceMemory staging_memory{ nullptr };

		create_buffer(
			image_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer, staging_memory
		);

		void* data = staging_memory.mapMemory(0, image_size);
		memcpy(data, pixels, image_size);
		staging_memory.unmapMemory();

		stbi_image_free(pixels);

		create_image(
			tex_width, tex_height,
			vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			m_texture_image, m_texture_memory
		);

		transition_image_layout(m_texture_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		copy_buffer_to_image(staging_buffer, m_texture_image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));
		transition_image_layout(m_texture_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	}

	void Application::create_texture_image_view()
	{
		m_texture_image_view = create_image_view(m_texture_image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
	}

	void Application::create_texture_sampler()
	{
		vk::PhysicalDeviceProperties properties = m_physical_device.getProperties();

		vk::SamplerCreateInfo sampler_ci
		{
			.magFilter = vk::Filter::eLinear, .minFilter = vk::Filter::eLinear, .mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat, .addressModeV = vk::SamplerAddressMode::eRepeat, .addressModeW = vk::SamplerAddressMode::eRepeat,
			.anisotropyEnable = vk::True, .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False, .compareOp = vk::CompareOp::eAlways,
			.borderColor = vk::BorderColor::eIntOpaqueBlack,
			.unnormalizedCoordinates = vk::False
		};

		m_texture_sampler = vk::raii::Sampler(m_device, sampler_ci);
	}

	void Application::create_vertex_buffer()
	{
		vk::DeviceSize buffer_size = sizeof(m_vertices[0]) * m_vertices.size();

		vk::raii::Buffer staging_buffer{ nullptr };
		vk::raii::DeviceMemory staging_memory{ nullptr };

		create_buffer(buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer, staging_memory
		);

		void* data = staging_memory.mapMemory(0, buffer_size);
		memcpy(data, m_vertices.data(), buffer_size);
		staging_memory.unmapMemory();

		create_buffer(buffer_size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			m_vertex_buffer, m_vertex_memory
		);

		copy_buffer(staging_buffer, m_vertex_buffer, buffer_size);
	}

	void Application::create_index_buffer()
	{
		vk::DeviceSize buffer_size = sizeof(m_indices[0]) * m_indices.size();

		vk::raii::Buffer staging_buffer{ nullptr };
		vk::raii::DeviceMemory staging_memory{ nullptr };

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			staging_buffer, staging_memory
		);

		void* data = staging_memory.mapMemory(0, buffer_size);
		memcpy(data, m_indices.data(), buffer_size);
		staging_memory.unmapMemory();

		create_buffer(
			buffer_size,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			m_index_buffer, m_index_memory
		);

		copy_buffer(staging_buffer, m_index_buffer, buffer_size);
	}

	void Application::create_uniform_buffers()
	{
		vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vk::raii::Buffer buffer{ nullptr };
			vk::raii::DeviceMemory memory{ nullptr };

			create_buffer(
				buffer_size,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				buffer,
				memory
			);

			m_uniform_buffers.emplace_back(std::move(buffer));
			m_uniform_buffers_memory.emplace_back(std::move(memory));
			m_uniform_buffers_mapped.emplace_back(m_uniform_buffers_memory[i].mapMemory(0, buffer_size));
		}
	}

	void Application::create_descriptor_pool()
	{
		
		std::array pool_size = {
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
		};

		vk::DescriptorPoolCreateInfo pool_ci{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, .maxSets = MAX_FRAMES_IN_FLIGHT, 
			.poolSizeCount = static_cast<uint32_t>(pool_size.size()), .pPoolSizes = pool_size.data() };
	
		m_descriptor_pool = vk::raii::DescriptorPool(m_device, pool_ci);
	}

	void Application::create_descriptor_sets()
	{
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptor_set_layout);
		vk::DescriptorSetAllocateInfo set_alloc_info{ .descriptorPool = m_descriptor_pool, .descriptorSetCount = static_cast<uint32_t>(layouts.size()), .pSetLayouts = layouts.data() };
		m_descriptor_sets = m_device.allocateDescriptorSets(set_alloc_info);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
		{
			vk::DescriptorBufferInfo buffer_info{ .buffer = m_uniform_buffers[i], .offset = 0, .range = sizeof(UniformBufferObject) };
			vk::DescriptorImageInfo image_info{ .sampler = m_texture_sampler, .imageView = m_texture_image_view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };
			std::array descriptor_write = {
				vk::WriteDescriptorSet{ .dstSet = m_descriptor_sets[i], .dstBinding = 0, .dstArrayElement = 0,
					.descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &buffer_info },
				vk::WriteDescriptorSet{ .dstSet = m_descriptor_sets[i], .dstBinding = 1, .dstArrayElement = 0,
					.descriptorCount = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .pImageInfo = &image_info }
			};

			m_device.updateDescriptorSets(descriptor_write, {});
		}
	}

	void Application::create_command_buffers()
	{
		vk::CommandBufferAllocateInfo alloc_info
		{
			.commandPool = m_command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT
		};

		m_command_buffers = vk::raii::CommandBuffers(m_device, alloc_info);
	}

	void Application::create_sync_objects()
	{
		assert(m_present_complete_semaphores.empty() && m_render_finished_semaphores.empty() && m_in_flight_fences.empty());

		for (size_t i = 0; i < m_swapchain_images.size(); i++)
		{
			m_render_finished_semaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_present_complete_semaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
			m_in_flight_fences.emplace_back(m_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
		}
	}

	void Application::recreate_swapchain()
	{
		if (m_framebuffer_minimized)
			return;

		int width, height;
		SDL_GetWindowSizeInPixels(m_window, &width, &height);

		m_device.waitIdle();

		cleanup_swapchain();

		create_swapchain();
		create_image_views();
		create_depth_resources();
	}

	void Application::cleanup_swapchain()
	{
		m_swapchain_image_views.clear();
		m_swapchain = nullptr;
	}

	void Application::update_uniform_buffer(uint32_t current_image)
	{
		static auto start_time = std::chrono::high_resolution_clock::now();
		auto current_time = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(m_swapchain_extent.width) / static_cast<float>(m_swapchain_extent.height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;
	
		memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
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
		auto features = physical_device.template getFeatures2<vk::PhysicalDeviceFeatures2, 
															  vk::PhysicalDeviceVulkan11Features, 
															  vk::PhysicalDeviceVulkan13Features, 
															  vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

		bool support_required_features = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
										 features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
										 features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
										 features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
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

	vk::raii::ShaderModule Application::create_shader_module(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo shader_module_ci
		{
			.codeSize = code.size() * sizeof(char),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())
		};

		vk::raii::ShaderModule shader_module{ m_device, shader_module_ci };
		return shader_module;
	}

	void Application::record_command_buffer(uint32_t image_index)
	{
		auto& command_buffer = m_command_buffers[m_frame_index];

		command_buffer.begin({});

		transition_image_layout(
			m_swapchain_images[image_index],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor
		);

		transition_image_layout(
			m_depth_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth
		);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo color_attachment_info
		{
			.imageView = m_swapchain_image_views[image_index],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clear_color
		};

		vk::RenderingAttachmentInfo depth_attachment_info
		{
			.imageView = m_depth_image_view,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clear_depth
		};

		vk::RenderingInfo rendering_info
		{
			.renderArea = { .offset = {0,0}, .extent = m_swapchain_extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = &depth_attachment_info
		};

		command_buffer.beginRendering(rendering_info);

		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphics_pipeline);

		command_buffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<uint32_t>(m_swapchain_extent.width), static_cast<uint32_t>(m_swapchain_extent.height), 0.0f, 1.0f));
		command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_swapchain_extent));

		command_buffer.bindVertexBuffers(0, *m_vertex_buffer, { 0 });
		command_buffer.bindIndexBuffer(*m_index_buffer, 0, vk::IndexType::eUint16);

		command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, *m_descriptor_sets[m_frame_index], nullptr);

		command_buffer.drawIndexed(m_indices.size(), 1, 0, 0, 0);

		command_buffer.endRendering();

		transition_image_layout(
			m_swapchain_images[image_index],
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,             
			{},                                                     
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,     
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor
		);

		command_buffer.end();

	}

	void Application::transition_image_layout(vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier = 
		{
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				   .aspectMask = image_aspect_flags,
				   .baseMipLevel = 0,
				   .levelCount = 1,
				   .baseArrayLayer = 0,
				   .layerCount = 1
			} 
		};
		vk::DependencyInfo dependency_info
		{
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier 
		};

		m_command_buffers[m_frame_index].pipelineBarrier2(dependency_info);
	}

	void Application::transition_image_layout(const vk::raii::Image& image, vk::ImageLayout old_layout, vk::ImageLayout new_layout)
	{
		auto cmd = begin_single_time_commands();

		vk::ImageMemoryBarrier barrier
		{
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.image = image,
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};

		vk::PipelineStageFlags source_stage;
		vk::PipelineStageFlags destination_stage;

		if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
			destination_stage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			source_stage = vk::PipelineStageFlagBits::eTransfer;
			destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition");
		}

		cmd.pipelineBarrier(source_stage, destination_stage, {}, {}, nullptr, barrier);
		end_single_time_commands(cmd);
	}

	uint32_t Application::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties)
	{
		vk::PhysicalDeviceMemoryProperties mem_properties = m_physical_device.getMemoryProperties();

		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type");
	}

	vk::Format Application::find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlagBits features)
	{
		for(const auto format : candidates)
		{
			vk::FormatProperties props = m_physical_device.getFormatProperties(format);

			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) 
			{
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) 
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format");
	}

	vk::Format Application::find_depth_format()
	{
		return find_supported_format(
			{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool Application::has_stencil_component(vk::Format format)
	{
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void Application::create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory)
	{
		vk::BufferCreateInfo buffer_ci
		{
			.size = size,
			.usage = usage,
		};

		buffer = vk::raii::Buffer(m_device, buffer_ci);

		auto mem_requirements = buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo mem_alloc_info
		{
			.allocationSize = mem_requirements.size,
			.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties)
		};

		memory = vk::raii::DeviceMemory(m_device, mem_alloc_info);
		buffer.bindMemory(*memory, 0);
	}

	void Application::copy_buffer(vk::raii::Buffer& src_buffer, vk::raii::Buffer& dst_buffer, vk::DeviceSize size)
	{
		auto cmd = begin_single_time_commands();
		cmd.copyBuffer(*src_buffer, *dst_buffer, vk::BufferCopy(0, 0, size));
		end_single_time_commands(cmd);
	}

	void Application::copy_buffer_to_image(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
	{
		auto cmd = begin_single_time_commands();

		vk::BufferImageCopy region
		{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
			.imageOffset = { 0, 0, 0 },
			.imageExtent = { width, height, 1 }
		};

		cmd.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });

		end_single_time_commands(cmd);
	}

	void Application::create_image(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& image_memory)
	{
		vk::ImageCreateInfo image_ci
		{
			.imageType =  vk::ImageType::e2D,
			.format = format,
			.extent = { width, height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = tiling, 
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive
		};
		image = vk::raii::Image(m_device, image_ci);

		vk::MemoryRequirements mem_requirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo alloc_info
		{
			.allocationSize = mem_requirements.size,
			.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties)
		};

		image_memory = vk::raii::DeviceMemory(m_device, alloc_info);
		image.bindMemory(image_memory, 0);
	}

	vk::raii::ImageView Application::create_image_view(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspect_flags)
	{
		vk::ImageViewCreateInfo view_ci
		{
			.image = image, 
			.viewType = vk::ImageViewType::e2D, 
			.format = format, 
			.subresourceRange = { aspect_flags, 0, 1, 0, 1 }
		};

		return vk::raii::ImageView(m_device, view_ci);
	}

	vk::raii::CommandBuffer Application::begin_single_time_commands()
	{
		vk::CommandBufferAllocateInfo cmd_alloc_info{ .commandPool = m_command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1, };
		vk::raii::CommandBuffer command_buffer = std::move(m_device.allocateCommandBuffers(cmd_alloc_info).front());
		
		vk::CommandBufferBeginInfo begin_info{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		command_buffer.begin(begin_info);

		return command_buffer;
	}

	void Application::end_single_time_commands(vk::raii::CommandBuffer& command_buffer)
	{
		command_buffer.end();

		vk::SubmitInfo submit_info{ .commandBufferCount = 1, .pCommandBuffers = &*command_buffer };
		m_queue.submit(submit_info, nullptr);
		m_queue.waitIdle();
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}
		return vk::False;
	}


	vk::VertexInputBindingDescription Vertex::get_binding_description()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	std::array<vk::VertexInputAttributeDescription, 3> Vertex::get_attribute_descriptions()
	{
		return {{
			{ .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,    .offset = offsetof(Vertex, position) },
			{ .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color) },
			{ .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(Vertex, tex_coord) },
		}};
	}

} // namespace mv