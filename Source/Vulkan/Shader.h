#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace mv::vk
{
	class Device;

	class Shader
	{
	public:

		Shader(Device& device, const std::string& vertex_path, const std::string& fragment_path);

		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;

		~Shader();

		const std::vector<VkPipelineShaderStageCreateInfo>& stages() const { return m_stages; }

		VkShaderModule module() const { return m_module; }

	private:

		VkShaderModule create_shader_module(const std::string& code);

		Device& m_device;
		VkShaderModule m_module{ VK_NULL_HANDLE };
		std::vector<VkPipelineShaderStageCreateInfo> m_stages;

	}; // class Shader

} // namespace mv::vk
