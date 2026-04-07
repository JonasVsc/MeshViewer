#pragma once
#include "Check.h"
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(res) \
	FATAL_CHECK((res) == VK_SUCCESS, "Vulkan error: {}", string_VkResult(res))