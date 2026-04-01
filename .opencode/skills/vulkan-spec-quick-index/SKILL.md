---
name: vulkan-spec-quick-index
description: Use the Vulkan specification as a fast reference by topic, lookup keyword, and official navigation path instead of re-reading the full spec linearly.
compatibility: opencode
metadata:
  audience: contributors
  source: official Vulkan specification structure
---

# Vulkan Spec Quick Index

Load this skill when the task needs fast Vulkan specification navigation, not a beginner tutorial.

## Goal

Turn the Vulkan specification into a practical lookup index so you can jump to the right topic, terms, and official reference area quickly.

This skill is optimized for:

- finding the right spec area before implementing or debugging Vulkan code
- mapping an engine problem to the most likely Vulkan chapter family
- recalling high-value search terms for official docs, registry pages, and extension names

## Working rules

- Treat this skill as a navigation aid, not as a substitute for reading the exact valid usage rules.
- When correctness matters, follow this skill to the right topic, then confirm details in the official Vulkan spec or API reference.
- Prefer exact Vulkan object, structure, enum, flag, command, and extension names when searching.
- For extension-driven features, check both the core feature area and the extension page.
- If a feature might have moved into core Vulkan versions, verify promotion/deprecation status in the registry.

---

## How to use this index

1. Start from the problem you are solving.
2. Match it to one of the topic families below.
3. Search using the suggested lookup keywords.
4. Confirm exact rules, synchronization requirements, limits, and valid usage in the official spec.

### Fast routing by problem

| If you are dealing with... | Go to... |
|---|---|
| device creation, queues, features, limits | Core setup and device model |
| buffers, images, memory heaps, layout transitions | Resources and memory |
| descriptor sets, bindings, update-after-bind | Descriptors and resource binding |
| shaders, pipeline creation, fixed-function state | Pipelines and shaders |
| render pass compatibility, attachments, subpasses | Render pass and framebuffer |
| `vkCmdBeginRendering*`, no render pass objects | Dynamic rendering |
| barriers, semaphores, fences, stage/access masks | Synchronization |
| command pools, command buffers, submission flow | Commands and execution |
| swapchain, surface, present modes, acquire/present | WSI and presentation |
| validation messages, debug labels, object naming | Validation and debugging |
| acceleration structures, RT pipelines, ray queries | Ray tracing |
| feature gating, vendor functionality, promotions | Extensions and versions |

---

## Topic family index

## 1. Core setup and device model

Use this area for instance creation, physical device selection, logical device creation, queues, features, limits, and format/property queries.

### Lookup keywords

- `VkInstance`
- `VkPhysicalDevice`
- `VkDevice`
- `VkQueue`
- `VkPhysicalDeviceFeatures2`
- `VkPhysicalDeviceProperties2`
- queue family
- device limits
- format properties

### Common entry points

- instance and device creation
- physical device enumeration
- queue family selection
- feature/extension enablement

---

## 2. Resources and memory

Use this area for buffers, images, image layouts, memory requirements, binding memory, mapping memory, and memory visibility rules.

### Lookup keywords

- `VkBuffer`
- `VkImage`
- `VkImageView`
- `VkDeviceMemory`
- `VkMemoryRequirements`
- `vkAllocateMemory`
- `vkBindBufferMemory`
- `vkBindImageMemory`
- image layout
- memory heap
- memory type

### Common entry points

- buffer/image creation
- image layout transitions
- host-visible vs device-local memory
- sparse or dedicated allocation paths

### Notes

- If the question is about visibility, ordering, or hazards, jump to Synchronization as well.
- If the question involves allocation strategy rather than spec legality, pair this with VMA-specific docs separately.

---

## 3. Descriptors and resource binding

Use this area for descriptor set layouts, descriptor pools, descriptor sets, pipeline layouts, push constants, and descriptor indexing features.

### Lookup keywords

- `VkDescriptorSetLayout`
- `VkDescriptorPool`
- `VkDescriptorSet`
- `VkPipelineLayout`
- push constants
- descriptor binding
- descriptor indexing
- update after bind
- bindless

### Common entry points

- resource binding model
- descriptor write/update rules
- set compatibility
- push constant ranges

---

## 4. Pipelines and shaders

Use this area for graphics pipelines, compute pipelines, shader stages, specialization constants, fixed-function state, and pipeline cache/library behavior.

### Lookup keywords

- `VkGraphicsPipelineCreateInfo`
- `VkComputePipelineCreateInfo`
- `VkPipelineShaderStageCreateInfo`
- specialization constants
- rasterization state
- color blend state
- depth stencil state
- dynamic state
- pipeline cache

### Common entry points

- graphics pipeline creation
- compute dispatch setup
- dynamic state requirements
- shader interface compatibility

---

## 5. Render pass and framebuffer

Use this area for attachments, subpasses, dependencies, framebuffer compatibility, load/store ops, and legacy render pass flow.

### Lookup keywords

- `VkRenderPass`
- `VkFramebuffer`
- `VkAttachmentDescription`
- `VkSubpassDescription`
- `VkSubpassDependency`
- loadOp
- storeOp
- attachment layout
- subpass

### Common entry points

- render pass compatibility
- multisample resolve behavior
- input attachments
- subpass dependency rules

---

## 6. Dynamic rendering

Use this area when the code path uses rendering commands without pre-created render pass/framebuffer objects.

### Lookup keywords

- `vkCmdBeginRendering`
- `vkCmdEndRendering`
- `VkRenderingInfo`
- `VkPipelineRenderingCreateInfo`
- dynamic rendering
- color attachment formats
- depth attachment format

### Common entry points

- replacing render passes with dynamic rendering
- pipeline compatibility under dynamic rendering
- mixed legacy and dynamic rendering migration questions

---

## 7. Synchronization

Use this area for barriers, memory dependencies, execution order, semaphores, fences, events, queue ownership transfer, and synchronization2.

### Lookup keywords

- `vkCmdPipelineBarrier`
- `vkCmdPipelineBarrier2`
- `VkMemoryBarrier`
- `VkBufferMemoryBarrier`
- `VkImageMemoryBarrier`
- `VkDependencyInfo`
- `VkSemaphore`
- `VkFence`
- stage mask
- access mask
- queue ownership transfer
- synchronization2

### Common entry points

- image transition barriers
- transfer-to-shader or color-to-present hazards
- acquire/release ownership
- CPU/GPU coordination

### Notes

- Most hard Vulkan bugs end up here.
- If the issue mentions hazards, visibility, ordering, races, or undefined results, inspect this topic first.

---

## 8. Commands and execution

Use this area for command pools, command buffers, recording rules, primary/secondary command buffers, submission, and queue execution behavior.

### Lookup keywords

- `VkCommandPool`
- `VkCommandBuffer`
- `VkCommandBufferBeginInfo`
- primary command buffer
- secondary command buffer
- `vkQueueSubmit`
- `vkQueueSubmit2`
- submission
- inheritance

### Common entry points

- command buffer lifecycle
- reset/reuse rules
- secondary command buffer inheritance
- submission batching

---

## 9. WSI and presentation

Use this area for surfaces, swapchains, presentation flow, present modes, surface capabilities, and platform integration.

### Lookup keywords

- `VK_KHR_surface`
- `VK_KHR_swapchain`
- `VkSurfaceKHR`
- `VkSwapchainKHR`
- `vkAcquireNextImageKHR`
- `vkQueuePresentKHR`
- present mode
- surface format
- surface capabilities

### Common entry points

- swapchain creation and recreation
- acquire/present synchronization
- window resize handling
- platform-specific presentation constraints

---

## 10. Validation and debugging

Use this area for validation layers, debug utils, object naming, labels, and diagnostics flow.

### Lookup keywords

- validation layers
- `VK_LAYER_KHRONOS_validation`
- `VK_EXT_debug_utils`
- debug messenger
- object name
- debug label
- valid usage
- VUID

### Common entry points

- understanding validation output
- naming objects for tooling
- correlating VUIDs with spec text

### Notes

- When you have a VUID, search the exact VUID string first.
- Validation usually points to the right chapter family even when the root cause is elsewhere.

---

## 11. Ray tracing

Use this area for acceleration structures, RT pipelines, shader binding tables, and ray queries.

### Lookup keywords

- `VK_KHR_acceleration_structure`
- `VK_KHR_ray_tracing_pipeline`
- `VK_KHR_ray_query`
- acceleration structure
- shader binding table
- SBT
- ray generation
- closest hit
- miss shader

### Common entry points

- acceleration structure build/update
- RT pipeline creation
- SBT layout requirements
- ray query usage in non-RT pipelines

---

## 12. Extensions and versions

Use this area for KHR, EXT, vendor extensions, provisional functionality, feature promotion, and version-to-core migration.

### Lookup keywords

- extension promotion
- Vulkan 1.1
- Vulkan 1.2
- Vulkan 1.3
- KHR
- EXT
- provisional
- feature struct
- extension dependency

### Common entry points

- whether a feature is core or extension-based
- required enable chains
- promoted or deprecated behavior

---

## High-value search patterns

Use these patterns directly in official docs or repo searches:

- exact API name: `vkCmdPipelineBarrier2`
- exact struct name: `VkPipelineRenderingCreateInfo`
- exact extension name: `VK_KHR_dynamic_rendering`
- exact VUID: `VUID-VkWriteDescriptorSet-descriptorType-00319`
- concept + object: `image layout VkImage`
- concept + extension: `descriptor indexing VK_EXT_descriptor_indexing`

### Search order

1. Exact VUID, command, struct, or extension name
2. Topic family from this skill
3. Official Vulkan spec chapter
4. Vulkan Guide or Vulkan-Docs appendices for context

---

## Common problem mapping

| Problem | First place to check | Also check |
|---|---|---|
| black frame after present | WSI and presentation | Synchronization, Render pass or Dynamic rendering |
| validation error on descriptor update | Descriptors and resource binding | Pipelines and shaders |
| random texture corruption | Synchronization | Resources and memory |
| render pass compatibility error | Render pass and framebuffer | Pipelines and shaders |
| layout transition confusion | Synchronization | Resources and memory |
| `vkCmdBeginRendering` mismatch | Dynamic rendering | Pipelines and shaders |
| feature not available on device | Core setup and device model | Extensions and versions |
| RT pipeline creation failure | Ray tracing | Core setup and device model |

---

## Official reference entry points

Start from these authoritative sources:

- Vulkan registry home: `https://registry.khronos.org/vulkan/`
- Latest Vulkan spec: `https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html`
- Vulkan 1.3 HTML index: `https://registry.khronos.org/vulkan/specs/1.3/html/index.html`
- Vulkan-Docs repository: `https://github.com/KhronosGroup/Vulkan-Docs`
- Vulkan Guide: `https://github.com/KhronosGroup/Vulkan-Guide`
- Vulkan Validation Layers: `https://github.com/KhronosGroup/Vulkan-ValidationLayers`

Useful topic-specific starting points:

- synchronization examples: `https://github.com/KhronosGroup/Vulkan-Docs/wiki/synchronization-examples`
- dynamic rendering proposal/history: `https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_KHR_dynamic_rendering.adoc`
- Vulkan memory model appendix: `https://github.com/KhronosGroup/Vulkan-Docs/blob/main/appendices/VK_KHR_vulkan_memory_model.adoc`
- swapchain maintenance appendix: `https://github.com/KhronosGroup/Vulkan-Docs/blob/main/appendices/VK_EXT_swapchain_maintenance1.adoc`
- ray tracing guide chapter: `https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/extensions/ray_tracing.adoc`

---

## Scope note

This skill was built as a general quick index from the official Vulkan spec structure and Khronos reference material.

It is not a line-by-line summary of a specific local PDF export.

If you later attach a concrete `vkspec.pdf`, this skill can be refined with PDF-specific page anchors or a condensed chapter map that matches that exact edition.

---

## Quick checklist before relying on an answer

Before finishing a Vulkan-related task, verify all of the following:

1. You searched using the exact Vulkan symbol or extension name.
2. You checked the correct topic family from this index.
3. You confirmed the exact valid usage or synchronization rule in the official spec.
4. You verified whether the behavior is core, extension, or version-promoted.
5. You checked validation output or VUIDs when available.
