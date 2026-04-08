#include "Shader.h"
#include "Device.h"
#include "Common.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace mv::vk
{
	Shader::Shader(Device& device, const std::string& vertex_path, const std::string& fragment_path)
		: m_device{ device }
	{
		// Read shader files
		std::ifstream shader_file(vertex_path, std::ios::binary);
		if (!shader_file.is_open())
		{
			throw std::runtime_error("Failed to open vertex shader: " + vertex_path);
		}
		std::stringstream shader_stream;
		shader_stream << shader_file.rdbuf();
		std::string shader_code = shader_stream.str();
		shader_file.close();

		m_module = create_shader_module(shader_code);

		VkPipelineShaderStageCreateInfo vertex_stage_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = m_module,
			.pName = "vertexMain"
		};

		VkPipelineShaderStageCreateInfo fragment_stage_info
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = m_module,
			.pName = "fragmentMain"
		};

		m_stages.push_back(vertex_stage_info);
		m_stages.push_back(fragment_stage_info);
	}

	Shader::~Shader()
	{
		if (m_module)
		{
			vkDestroyShaderModule(m_device.logical(), m_module, nullptr);
		}
	}

	VkShaderModule Shader::create_shader_module(const std::string& code)
	{
		FATAL_CHECK(code.empty(), "Failed: shader ({}) bytecode is empty", code);
		FATAL_CHECK(((code.size() % sizeof(uint32_t)) != 0), "Failed: shader ({}) bytecode size must be a multiple of 4 bytes", code);

		std::vector<uint32_t> spirv_words(code.size() / sizeof(uint32_t));
		std::memcpy(spirv_words.data(), code.data(), code.size());

		VkShaderModuleCreateInfo create_info
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = code.size(),
			.pCode = spirv_words.data()
		};

		VkShaderModule shader_module;
		VK_CHECK(vkCreateShaderModule(m_device.logical(), &create_info, nullptr, &shader_module));

		return shader_module;
	}

} // namespace mv::vk
