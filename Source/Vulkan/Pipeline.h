#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

namespace mv::vk
{
	class Device;
	class Shader;
	class DescriptorSetLayout;

	class Pipeline
	{
	public:

		Pipeline(Device& device, VkPipelineLayout layout, VkPipeline pipeline);

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		Pipeline(Pipeline&& other) noexcept;
		Pipeline& operator=(Pipeline&& other) noexcept;

		~Pipeline();

		void bind(VkCommandBuffer cmd, VkPipelineBindPoint bind_point) const;

		VkPipeline handle() const { return m_handle; }
		VkPipelineLayout layout() const { return m_layout; }

	private:

		Device& m_device;

		VkPipelineLayout m_layout{ VK_NULL_HANDLE };
		VkPipeline m_handle{ VK_NULL_HANDLE };

	}; // class Pipeline

	class PipelineBuilder
	{
	public:

		PipelineBuilder& with_shader(Shader& shader);

		PipelineBuilder& with_descriptor_layouts(std::vector<VkDescriptorSetLayout> layouts);

		PipelineBuilder& with_vertex_input(std::vector<VkVertexInputBindingDescription> bindings, std::vector<VkVertexInputAttributeDescription> attributes);

		PipelineBuilder& with_rasterization(VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL, VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT);

		PipelineBuilder& with_color_blending(bool enable_blending = false);

		PipelineBuilder& with_depth_test(bool enable = true);

		Pipeline build(Device& device, VkFormat format);

		void reset();

	private:

		Shader* m_shader{ nullptr };
		std::vector<VkDescriptorSetLayout> m_descriptor_layouts;
		std::optional<std::vector<VkVertexInputBindingDescription>> m_vertex_bindings;
		std::optional<std::vector<VkVertexInputAttributeDescription>> m_vertex_attributes;

		VkPolygonMode m_polygon_mode{ VK_POLYGON_MODE_FILL };
		VkCullModeFlags m_cull_mode{ VK_CULL_MODE_BACK_BIT };
		bool m_blending_enabled{ false };
		bool m_depth_testing{ true };

	}; // class PipelineBuilder

} // namespace mv::vk