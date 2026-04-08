#include "Pipeline.h"
#include "Device.h"
#include "Shader.h"
#include "Common.h"

#include <stdexcept>
#include <array>

namespace mv::vk
{
	Pipeline::Pipeline(Device& device, VkPipelineLayout layout, VkPipeline pipeline)
		: m_device{ device }
		, m_layout{ layout }
		, m_handle{ pipeline }
	{
		FATAL_CHECK(pipeline, "Failed, pipeline null handle");
		FATAL_CHECK(layout, "Failed, pipeline layout null handle");
	}

	Pipeline::Pipeline(Pipeline&& other) noexcept
		: m_device{ other.m_device }
		, m_layout{ other.m_layout }
		, m_handle { other.m_handle }
	{
		other.m_layout = nullptr;
		other.m_handle = nullptr;
	}

	Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
	{
		if (this != &other)
		{
			vkDestroyPipelineLayout(m_device.logical(), m_layout, nullptr);
			vkDestroyPipeline(m_device.logical(), m_handle, nullptr);

			m_layout = other.m_layout;
			m_handle = other.m_handle;

			other.m_layout = nullptr;
			other.m_handle = nullptr;
		}

		return *this;
	}

	Pipeline::~Pipeline()
	{
		vkDestroyPipelineLayout(m_device.logical(), m_layout, nullptr);
		vkDestroyPipeline(m_device.logical(), m_handle, nullptr);
	}

	void Pipeline::bind(VkCommandBuffer cmd, VkPipelineBindPoint bind_point) const
	{
		vkCmdBindPipeline(cmd, bind_point, m_handle);
	}

	PipelineBuilder& PipelineBuilder::with_shader(Shader& shader)
	{
		m_shader = &shader;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::with_descriptor_layouts(std::vector<VkDescriptorSetLayout> layouts)
	{
		m_descriptor_layouts = std::move(layouts);
		return *this;
	}

	PipelineBuilder& PipelineBuilder::with_vertex_input(std::vector<VkVertexInputBindingDescription> bindings, std::vector<VkVertexInputAttributeDescription> attributes)
	{
		m_vertex_bindings = std::move(bindings);
		m_vertex_attributes = std::move(attributes);
		return *this;
	}

	PipelineBuilder& PipelineBuilder::with_rasterization(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode)
	{
		m_polygon_mode = polygon_mode;
		m_cull_mode = cull_mode;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::with_color_blending(bool enable_blending)
	{
		m_blending_enabled = enable_blending;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::with_depth_test(bool enable)
	{
		m_depth_testing = enable;
		return *this;
	}

	Pipeline PipelineBuilder::build(Device& device, VkFormat format)
	{
		FATAL_CHECK(m_shader, "Failed, Shader must be set before building");

		// Vertex input state (no vertex input for now)
		VkPipelineVertexInputStateCreateInfo vertex_input_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		};

		if (m_vertex_bindings.has_value())
		{
			vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_bindings.value().size());
			vertex_input_info.pVertexBindingDescriptions = m_vertex_bindings.value().data();
		}

		if (m_vertex_attributes.has_value())
		{
			vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_attributes.value().size());
			vertex_input_info.pVertexAttributeDescriptions = m_vertex_attributes.value().data();
		}

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineViewportStateCreateInfo viewport_state
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		};

		VkPipelineRasterizationStateCreateInfo rasterization
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = m_polygon_mode,
			.cullMode = m_cull_mode,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f
		};
		
		VkPipelineMultisampleStateCreateInfo multisample
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};

		VkPipelineDepthStencilStateCreateInfo depth_stencil
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = m_depth_testing ? VK_TRUE : VK_FALSE,
			.depthWriteEnable = m_depth_testing ? VK_TRUE : VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_LESS
		};

		VkPipelineColorBlendAttachmentState blend_attachment
		{
			.blendEnable = m_blending_enabled ? VK_TRUE : VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
							  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};


		VkPipelineColorBlendStateCreateInfo color_blend
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &blend_attachment,
		};

		std::array<VkDynamicState, 2> dynamic_states
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};


		VkPipelineDynamicStateCreateInfo dynamic_state
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
			.pDynamicStates = dynamic_states.data()
		};
		
		VkPipelineRenderingCreateInfo rendering_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &format
		};

		VkPipelineLayoutCreateInfo layout_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(m_descriptor_layouts.size()),
			.pSetLayouts = m_descriptor_layouts.empty() ? nullptr : m_descriptor_layouts.data()
		};

		VkPipelineLayout pipeline_layout{};
		VK_CHECK(vkCreatePipelineLayout(device.logical(), &layout_info, nullptr, &pipeline_layout));

		// Create graphics pipeline
		VkGraphicsPipelineCreateInfo pipeline_info
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &rendering_info,
			.stageCount = static_cast<uint32_t>(m_shader->stages().size()),
			.pStages = m_shader->stages().data(),
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_state_info,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization,
			.pMultisampleState = &multisample,
			.pDepthStencilState = &depth_stencil,
			.pColorBlendState = &color_blend,
			.pDynamicState = &dynamic_state,
			.layout = pipeline_layout
		};

		VkPipeline pipeline_handle{};
		VK_CHECK(vkCreateGraphicsPipelines(device.logical(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_handle));

		return Pipeline(device, pipeline_layout, pipeline_handle);

	}

	void PipelineBuilder::reset()
	{
		m_shader = nullptr;
		m_descriptor_layouts.clear();
		m_vertex_bindings.reset();
		m_vertex_attributes.reset();
		m_polygon_mode = VK_POLYGON_MODE_FILL;
		m_cull_mode = VK_CULL_MODE_BACK_BIT;
		m_blending_enabled = false;
		m_depth_testing = true;
	}

} // namespace mv::vk