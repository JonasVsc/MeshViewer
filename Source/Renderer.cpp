#include "Renderer.h"
#include "Vulkan/Context.h"
#include "Vulkan/Device.h"
#include "Vulkan/Common.h"


namespace mv
{
	static inline bool cmd_transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
	{
		bool is_depth = (old_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		VkImageAspectFlags flags = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageMemoryBarrier barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				.aspectMask = flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VkPipelineStageFlags src_stage{};
		VkPipelineStageFlags dst_stage{};

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = 0;
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			return false;
		}

		vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

		return true;
	}

	Renderer::Renderer(vk::Context& context, vk::Device& device)
		: m_context{ context }
		, m_device{ device }
	{
		setup();
	}

	Renderer::~Renderer()
	{
		vkDeviceWaitIdle(m_device.logical());

		for (auto& frame : m_frames)
		{
			vkDestroyCommandPool(m_device.logical(), frame.command_pool, nullptr);
			vkDestroySemaphore(m_device.logical(), frame.image_available_semaphore, nullptr);
			vkDestroySemaphore(m_device.logical(), frame.render_finished_semaphore, nullptr);
			vkDestroyFence(m_device.logical(), frame.in_flight_fence, nullptr);
		}
	}

	bool Renderer::begin_frame()
	{
		auto& frame = m_frames[m_current_frame];

		vkWaitForFences(m_device.logical(), 1, &frame.in_flight_fence, VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(m_device.logical(), m_swapchain.handle(), UINT64_MAX, frame.image_available_semaphore, nullptr, &m_current_image);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// RESIZE
			// RETURN
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			return false;
		}

		VK_CHECK(vkResetFences(m_device.logical(), 1, &frame.in_flight_fence));

		VkCommandBufferBeginInfo cmd_begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		VK_CHECK(vkBeginCommandBuffer(frame.command_buffer, &cmd_begin));


		bool res = cmd_transition_image_layout(frame.command_buffer, m_swapchain.image(m_current_image), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		return res;
	}

	void Renderer::begin_rendering()
	{
		VkRenderingAttachmentInfo colorAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = m_swapchain.view(m_current_image),
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {{0.1f, 0.1f, 0.1f, 0.0f}}
		};

		VkRect2D renderArea
		{
			.offset = { 0 },
			.extent = m_swapchain.extent()
		};

		VkRenderingInfo renderInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = renderArea,
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
		};

		vkCmdBeginRendering(m_frames[m_current_frame].command_buffer, &renderInfo);
	}

	void Renderer::end_frame()
	{
		auto& frame = m_frames[m_current_frame];

		vkCmdEndRendering(frame.command_buffer);

		cmd_transition_image_layout(frame.command_buffer, m_swapchain.image(m_current_image), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		
		VK_CHECK(vkEndCommandBuffer(frame.command_buffer));

		VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submit_info
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame.image_available_semaphore,
			.pWaitDstStageMask = stages,
			.commandBufferCount = 1,
			.pCommandBuffers = &frame.command_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &frame.render_finished_semaphore,
		};

		VK_CHECK(vkQueueSubmit(m_device.graphics_queue(), 1, &submit_info, frame.in_flight_fence));

		VkSwapchainKHR swapchains[] = { m_swapchain.handle() };

		VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &frame.render_finished_semaphore,
			.swapchainCount = 1,
			.pSwapchains = swapchains,
			.pImageIndices = &m_current_image
		};

		VkResult res = vkQueuePresentKHR(m_device.present_queue(), &present_info);

		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
		{
			// resize();
		}
		else if (res != VK_SUCCESS)
		{
			FATAL_CHECK(false, "Failed end frame");
		}

		m_current_frame = (m_current_frame + 1) % vk::Swapchain::max_frames_in_flight;
	}

	void Renderer::setup()
	{
		VkCommandPoolCreateInfo pool_ci
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = m_device.graphics_queue_index(),
		};

		VkFenceCreateInfo fence_ci
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		VkSemaphoreCreateInfo semaphore_ci
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};
		
		m_frames.resize(vk::Swapchain::max_frames_in_flight);
		
		for (uint32_t i = 0; i < vk::Swapchain::max_frames_in_flight; ++i)
		{
			auto& frame = m_frames[i];

			VK_CHECK(vkCreateCommandPool(m_device.logical(), &pool_ci, nullptr, &frame.command_pool));
			
			VkCommandBufferAllocateInfo cmd_alloc_info
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = frame.command_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			VK_CHECK(vkAllocateCommandBuffers(m_device.logical(), &cmd_alloc_info, &frame.command_buffer));
			VK_CHECK(vkCreateSemaphore(m_device.logical(), &semaphore_ci, nullptr, &frame.image_available_semaphore));
			VK_CHECK(vkCreateSemaphore(m_device.logical(), &semaphore_ci, nullptr, &frame.render_finished_semaphore));
			VK_CHECK(vkCreateFence(m_device.logical(), &fence_ci, nullptr, &frame.in_flight_fence));
		}
	}

} // namespace mv