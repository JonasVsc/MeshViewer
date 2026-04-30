#include "Viewer.h"
#include "glTFLoader.h"
#include "Utils.h"

#include <SDL3/SDL_vulkan.h>


#include <stdexcept>#
#include <iostream>

namespace mv
{
	

	void Viewer::init_window()
	{
		if(!SDL_Init(SDL_INIT_VIDEO))
			throw std::runtime_error("failed to init sdl");

		m_window = SDL_CreateWindow("Viewer", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		if (!m_window)
			throw std::runtime_error("failed to create window");
	}

	void Viewer::init_vulkan()
	{
		create_instance();
		create_surface();
		select_physical_device();
		create_logical_device();
		create_swapchain();
		create_sync_objects();
		create_descriptor_set_layout();
		create_graphics_pipeline();
		create_staging_buffer();
		load_model();
	}

	void Viewer::cleanup_swapchain()
	{
		m_swapchain_views.clear();
		m_swapchain = nullptr;
	}

	void Viewer::cleanup()
	{
		m_device.waitIdle();

		cleanup_swapchain();

		if(m_window)
			SDL_DestroyWindow(m_window);

		SDL_Quit();
	}

	void Viewer::run()
	{
		init_window();
		init_vulkan();
		
		while (!window_closed())
		{
			poll_events();
			draw_frame();
		}

		cleanup();
	}

	void Viewer::poll_events()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			{
				m_window_closed = true;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
			{
				m_window_minimized = true;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED)
			{
				m_window_minimized = false;
			}
		}
	}

	void Viewer::create_instance()
	{
		// application info
		constexpr vk::ApplicationInfo app_info
		{
			.pApplicationName = "MeshViewer",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "No Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = vk::ApiVersion13
		};

		uint32_t instance_extension_count{  };
		auto instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);

		// create instance
		vk::InstanceCreateInfo instance_ci
		{
			.pApplicationInfo = &app_info,
			.enabledExtensionCount = instance_extension_count,
			.ppEnabledExtensionNames = instance_extensions
		};

		m_instance = vk::raii::Instance(m_context, instance_ci);
	}

	void Viewer::create_surface()
	{
		VkSurfaceKHR surface{ nullptr };

		if (!SDL_Vulkan_CreateSurface(m_window, *m_instance, nullptr, &surface))
			throw std::runtime_error("failed to create vulkan surface");

		m_surface = vk::raii::SurfaceKHR(m_instance, surface);
	}

	void Viewer::select_physical_device()
	{
		auto physical_devices = m_instance.enumeratePhysicalDevices();
		for (auto& physical_device : physical_devices)
		{
			if (is_device_suitable(physical_device))
			{
				m_physical_device = physical_device;
				break;
			}

		}

		if(!*m_physical_device)
			throw std::runtime_error("failed to find a suitable GPU");
	}

	void Viewer::create_logical_device()
	{
		auto queue_family_properties = m_physical_device.getQueueFamilyProperties();

		uint32_t graphics_queue_index = ~0;
		for (uint32_t i = 0; i < queue_family_properties.size(); i++)
		{
			if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && m_physical_device.getSurfaceSupportKHR(i, *m_surface))
			{
				graphics_queue_index = i;
				break;
			}
		}

		if (graphics_queue_index == ~0)
		{
			throw std::runtime_error("failed to find a queue for graphics and present");
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
			.queueFamilyIndex = graphics_queue_index,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};

		vk::DeviceCreateInfo device_ci
		{
			.pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &device_queue_ci,
			.enabledExtensionCount = static_cast<uint32_t>(Viewer::required_device_extensions.size()),
			.ppEnabledExtensionNames = Viewer::required_device_extensions.data()
		};

		m_device = vk::raii::Device(m_physical_device, device_ci);
		m_graphics_queue = vk::raii::Queue(m_device, graphics_queue_index, 0);
		m_graphics_queue_family_index = graphics_queue_index;
	}

	void Viewer::create_swapchain()
	{
		auto surface_capabilities = m_physical_device.getSurfaceCapabilitiesKHR(*m_surface);
		auto available_formats = m_physical_device.getSurfaceFormatsKHR(*m_surface);
		auto available_present_modes = m_physical_device.getSurfacePresentModesKHR(*m_surface);

		m_swapchain_extent = choose_swap_extent(m_window, surface_capabilities);
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

		assert(m_swapchain_views.empty());

		vk::ImageViewCreateInfo image_view_ci
		{
			.viewType = vk::ImageViewType::e2D,
			.format = m_swapchain_surface_format.format,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};

		for (auto& image : m_swapchain_images)
		{
			image_view_ci.image = image;
			m_swapchain_views.emplace_back(m_device, image_view_ci);
		}

		vk::Format depth_format = find_depth_format(m_physical_device);

		create_image(
			m_swapchain_extent.width, m_swapchain_extent.height, depth_format,
			vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			m_depth_image, m_depth_image_memory
		);

		m_depth_image_view = create_image_view(m_depth_image, depth_format, vk::ImageAspectFlagBits::eDepth);
	}

	void Viewer::create_sync_objects()
	{
		vk::CommandPoolCreateInfo cmd_pool_ci
		{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = m_graphics_queue_family_index
		};

		m_command_pool = vk::raii::CommandPool(m_device, cmd_pool_ci);
		
		vk::CommandBufferAllocateInfo alloc_info
		{
			.commandPool = m_command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT
		};

		m_command_buffers = vk::raii::CommandBuffers(m_device, alloc_info);

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

	void Viewer::create_descriptor_set_layout()
	{
		std::array bindings
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
		};

		vk::DescriptorSetLayoutCreateInfo layout_ci{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };
		m_descriptor_set_layout = vk::raii::DescriptorSetLayout(m_device, layout_ci);
	}

	void Viewer::create_graphics_pipeline()
	{
		auto shader_module = create_shader_module(read_file("Shaders/default.spv"));

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

		vk::Format depth_format = find_depth_format(m_physical_device);

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
			{.colorAttachmentCount = 1, .pColorAttachmentFormats = &m_swapchain_surface_format.format, .depthAttachmentFormat = depth_format }
		};

		m_graphics_pipeline = vk::raii::Pipeline(m_device, nullptr, pipeline_chain_ci.get<vk::GraphicsPipelineCreateInfo>());
	}

	void Viewer::create_staging_buffer()
	{
		create_buffer(
			MB(256),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
			m_staging_buffer, m_staging_buffer_memory
		);
	}

	void Viewer::recreate_swapchain()
	{
		if (m_window_minimized)
			return;

		int width, height;
		SDL_GetWindowSizeInPixels(m_window, &width, &height);

		if (width == 0 || height == 0)
			return;

		m_device.waitIdle();

		cleanup_swapchain();

		create_swapchain();
	}

	void Viewer::record_command_buffer(uint32_t image_index)
	{
		auto& command_buffer = m_command_buffers[m_frame_index];

		command_buffer.begin({});

		transition_image_layout(
			m_command_buffers[m_frame_index],
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
			m_command_buffers[m_frame_index],
			m_depth_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth
		);

		vk::ClearValue clear_color = vk::ClearColorValue(0.0f, 0.0f, 0.1f, 1.0f);
		vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo color_attachment_info
		{
			.imageView = m_swapchain_views[image_index],
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
			.renderArea = {.offset = {0,0}, .extent = m_swapchain_extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = &depth_attachment_info
		};

		command_buffer.beginRendering(rendering_info);



		command_buffer.endRendering();

		transition_image_layout(
			m_command_buffers[m_frame_index],
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

	void Viewer::draw_frame()
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

		m_device.resetFences(*m_in_flight_fences[m_frame_index]);

		m_command_buffers[m_frame_index].reset();
		record_command_buffer(image_index);

		vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
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

		m_graphics_queue.submit(submit_info, *m_in_flight_fences[m_frame_index]);

		const vk::PresentInfoKHR present_info
		{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*m_render_finished_semaphores[image_index],
			.swapchainCount = 1,
			.pSwapchains = &*m_swapchain,
			.pImageIndices = &image_index
		};

		result = m_graphics_queue.presentKHR(present_info);
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || m_window_resized)
		{
			m_window_resized = false;
			recreate_swapchain();
		}
		else
		{
			assert(result == vk::Result::eSuccess);
		}

		m_frame_index = (m_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vk::raii::ShaderModule Viewer::create_shader_module(const std::vector<char>& code)
	{
		vk::ShaderModuleCreateInfo shader_module_ci
		{
			.codeSize = code.size() * sizeof(char),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())
		};

		return vk::raii::ShaderModule{ m_device, shader_module_ci };
	}

	void Viewer::create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory)
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
			.memoryTypeIndex = find_memory_type(m_physical_device, mem_requirements.memoryTypeBits, properties)
		};

		memory = vk::raii::DeviceMemory(m_device, mem_alloc_info);
		buffer.bindMemory(*memory, 0);
	}

	void Viewer::create_image(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& image_memory)
	{
		vk::ImageCreateInfo image_ci
		{
			.imageType = vk::ImageType::e2D,
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
			.memoryTypeIndex = find_memory_type(m_physical_device, mem_requirements.memoryTypeBits, properties)
		};

		image_memory = vk::raii::DeviceMemory(m_device, alloc_info);
		image.bindMemory(image_memory, 0);
	}

	vk::raii::ImageView Viewer::create_image_view(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspect_flags)
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

	vk::raii::CommandBuffer Viewer::begin_single_time_commands()
	{
		vk::CommandBufferAllocateInfo cmd_alloc_info{ .commandPool = m_command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1, };
		vk::raii::CommandBuffer command_buffer = std::move(m_device.allocateCommandBuffers(cmd_alloc_info).front());

		vk::CommandBufferBeginInfo begin_info{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		command_buffer.begin(begin_info);

		return command_buffer;
	}

	void Viewer::end_single_time_commands(vk::raii::CommandBuffer& command_buffer)
	{
		command_buffer.end();

		vk::SubmitInfo submit_info{ .commandBufferCount = 1, .pCommandBuffers = &*command_buffer };
		m_graphics_queue.submit(submit_info, nullptr);
		m_graphics_queue.waitIdle();
	}

	vk::VertexInputBindingDescription Vertex::get_binding_description()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	std::array<vk::VertexInputAttributeDescription, 3> Vertex::get_attribute_descriptions()
	{
		return { {
			{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,    .offset = offsetof(Vertex, pos) },
			{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color) },
			{.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(Vertex, texCoord) },
		} };
	}

} // namespace mv
