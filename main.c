/*
 * Vulkan experiment for 32-bit integer compression on the GPU using compute shaders.
 *
 * Copyright (C) 2017 Sascha Willems (Vulkan Example - Minimal headless compute example)
 * Copyright (C) 2020 Lauri Räsänen
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "assert.h"
#include "common.h"
#include <time.h>

void vk_init( struct vk_app *vk_app )
{
    // Create instance
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "vk_comp",
        .apiVersion = VK_MAKE_VERSION( 1, 0, 2 ),
    };
    VkInstanceCreateInfo instance_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                           .pApplicationInfo = &app_info };
    uint32_t layer_count = 1;
    const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
#if DEBUG
    // Check if layers are available
    uint32_t instance_layer_count;
    vkEnumerateInstanceLayerProperties( &instance_layer_count, NULL );
    VkLayerProperties *instance_layers = malloc( instance_layer_count * sizeof( VkLayerProperties ) );
    vkEnumerateInstanceLayerProperties( &instance_layer_count, instance_layers );

    bool layers_available = true;
    printf( "Layers:\n" );
    for ( uint32_t i = 0; i < layer_count; i++ )
    {
        bool layer_available = false;
        for ( uint32_t j = 0; j < instance_layer_count; j++ )
        {
            if ( strcmp( instance_layers[j].layerName, validation_layers[i] ) == 0 )
            {
                printf( "  %s\n", validation_layers[i] );
                layer_available = true;
                break;
            }
        }
        if ( !layer_available )
        {
            printf( "  MISSING %s\n", validation_layers[i] );
            layers_available = false;
            break;
        }
    }

    free( instance_layers );

    if ( layers_available )
    {
        instance_info.ppEnabledLayerNames = validation_layers;
        const char *validation_ext = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
        instance_info.enabledLayerCount = layer_count;
        instance_info.enabledExtensionCount = 1;
        instance_info.ppEnabledExtensionNames = &validation_ext;
    }
#endif
    vk_check( vkCreateInstance( &instance_info, g_pAllocator, &vk_app->instance ), "Failed to create instance" );

#if DEBUG
    if ( layers_available )
    {
        VkDebugReportCallbackCreateInfoEXT debug_report_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
            .pfnCallback = (PFN_vkDebugReportCallbackEXT)debug_message_callback,
        };
        PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
            vkGetInstanceProcAddr( vk_app->instance, "vkCreateDebugReportCallbackEXT" );
        assert( vkCreateDebugReportCallbackEXT );
        vk_check( vkCreateDebugReportCallbackEXT(
                      vk_app->instance, &debug_report_info, g_pAllocator, &vk_app->debug_report_callback ),
                  "Failed to create debug report callback" );
    }
#endif

    // Get physical device
    uint32_t count;
    vk_check( vkEnumeratePhysicalDevices( vk_app->instance, &count, NULL ), "Failed to enumerate physical devices" );
    assert( count > 0 );
    VkPhysicalDevice *physical_devices = malloc( count * sizeof( VkPhysicalDevice ) );
    vk_check( vkEnumeratePhysicalDevices( vk_app->instance, &count, physical_devices ),
              "Failed to enumerate physical devices" );
    vk_app->physical_device = physical_devices[0];
    free( physical_devices );

    // Get device properties
    vkGetPhysicalDeviceProperties( vk_app->physical_device, &vk_app->physical_device_properties );
    vkGetPhysicalDeviceMemoryProperties( vk_app->physical_device, &vk_app->physical_device_memory_properties );

    // Check compute queue
    vkGetPhysicalDeviceQueueFamilyProperties( vk_app->physical_device, &count, NULL );
    assert( count > 0 );
    vkGetPhysicalDeviceQueueFamilyProperties( vk_app->physical_device, &count, &vk_app->queue_family_properties );
    uint32_t queue_family_index = 0;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = ( float[] ){ 1.0f },
    };
    for ( uint32_t i = 0; i < vk_app->queue_family_properties.queueCount; i++ )
    {
        if ( vk_app->queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT )
        {
            queue_family_index = i;
            queue_info.queueFamilyIndex = i;
            break;
        }
    }

    // Create logical device
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    vk_check( vkCreateDevice( vk_app->physical_device, &device_info, g_pAllocator, &vk_app->device ),
              "Failed to create device" );

    // Get compute queue
    vkGetDeviceQueue( vk_app->device, queue_family_index, 0, &vk_app->queue );

    // Create command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    vk_check( vkCreateCommandPool( vk_app->device, &pool_info, g_pAllocator, &vk_app->command_pool ),
              "Failed to create command pool" );
}

void vk_shutdown( struct vk_app *vk_app )
{
    vkDestroyPipelineLayout( vk_app->device, vk_app->pipeline_layout, g_pAllocator );
    vkDestroyDescriptorSetLayout( vk_app->device, vk_app->descriptor_set_layout, g_pAllocator );
    vkDestroyDescriptorPool( vk_app->device, vk_app->descriptor_pool, g_pAllocator );
    vkDestroyPipeline( vk_app->device, vk_app->pipeline, g_pAllocator );
    vkDestroyPipelineCache( vk_app->device, vk_app->pipeline_cache, g_pAllocator );
    vkDestroyFence( vk_app->device, vk_app->fence, g_pAllocator );
    vkDestroyCommandPool( vk_app->device, vk_app->command_pool, g_pAllocator );
    vkDestroyShaderModule( vk_app->device, vk_app->shader_module, g_pAllocator );
    vkDestroyDevice( vk_app->device, g_pAllocator );
#if DEBUG
    if ( vk_app->debug_report_callback )
    {
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallback =
            vkGetInstanceProcAddr( vk_app->instance, "vkDestroyDebugReportCallbackEXT" );
        assert( vkDestroyDebugReportCallback );
        vkDestroyDebugReportCallback( vk_app->instance, vk_app->debug_report_callback, g_pAllocator );
    }
#endif
    vkDestroyInstance( vk_app->instance, g_pAllocator );
}

void process( struct vk_app *vk_app, bool compress, uint32_t *src, uint32_t *dst, uint32_t size )
{
    VkBuffer device_buffer, host_buffer;
    VkDeviceMemory device_memory, host_memory;
    uint32_t buffer_size = size * sizeof( uint32_t );

    // Copy input data to VRAM using a staging buffer
    {
        create_buffer( vk_app,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                       &host_buffer,
                       &host_memory,
                       buffer_size,
                       src );

        // Flush
        void *mapped;
        vk_check( vkMapMemory( vk_app->device, host_memory, 0, VK_WHOLE_SIZE, 0, &mapped ), "Failed to map memory" );
        VkMappedMemoryRange mapped_range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = host_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        vkFlushMappedMemoryRanges( vk_app->device, 1, &mapped_range );
        vkUnmapMemory( vk_app->device, host_memory );

        create_buffer( vk_app,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &device_buffer,
                       &device_memory,
                       buffer_size,
                       NULL );

        // Copy to staging buffer
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = vk_app->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };
        VkCommandBuffer copy_cmd;
        vk_check( vkAllocateCommandBuffers( vk_app->device, &alloc_info, &copy_cmd ),
                  "Failed to allocate command buffers" );
        VkCommandBufferBeginInfo cmd_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        vk_check( vkBeginCommandBuffer( copy_cmd, &cmd_buffer_info ), "Failed to begin command buffer" );

        VkBufferCopy copy_region = {
            .size = buffer_size,
        };
        vkCmdCopyBuffer( copy_cmd, host_buffer, device_buffer, 1, &copy_region );
        vk_check( vkEndCommandBuffer( copy_cmd ), "Failed to end command buffer" );

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &copy_cmd,
        };
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,

        };
        VkFence fence;
        vk_check( vkCreateFence( vk_app->device, &fence_info, g_pAllocator, &fence ), "Failed to create fence" );

        // Submit
        vk_check( vkQueueSubmit( vk_app->queue, 1, &submit_info, fence ), "Failed to submit queue" );
        vk_check( vkWaitForFences( vk_app->device, 1, &fence, VK_TRUE, UINT64_MAX ), "Failed to wait for fences" );

        vkDestroyFence( vk_app->device, fence, g_pAllocator );
        vkFreeCommandBuffers( vk_app->device, vk_app->command_pool, 1, &copy_cmd );
    }

    // Prepare compute pipeline
    {
        VkDescriptorPoolSize pool_size = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
            .maxSets = 1,
        };
        vk_check( vkCreateDescriptorPool( vk_app->device, &pool_info, g_pAllocator, &vk_app->descriptor_pool ),
                  "Failed to create descriptor pool" );

        VkDescriptorSetLayoutBinding layout_binding = {
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .binding = 0,
            .descriptorCount = 1,
        };
        VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings = &layout_binding,
            .bindingCount = 1,
        };
        vk_check( vkCreateDescriptorSetLayout(
                      vk_app->device, &descriptor_layout_info, g_pAllocator, &vk_app->descriptor_set_layout ),
                  "Failed to create descriptor set layout" );

        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &vk_app->descriptor_set_layout,
        };
        vk_check(
            vkCreatePipelineLayout( vk_app->device, &pipeline_layout_info, g_pAllocator, &vk_app->pipeline_layout ),
            "Failed to create pipeline layout" );

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk_app->descriptor_pool,
            .pSetLayouts = &vk_app->descriptor_set_layout,
            .descriptorSetCount = 1,
        };
        vk_check( vkAllocateDescriptorSets( vk_app->device, &alloc_info, &vk_app->descriptor_set ),
                  "Failed to allocate descriptor sets" );

        VkDescriptorBufferInfo buffer_descriptor = {
            .buffer = device_buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        VkWriteDescriptorSet write_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_app->descriptor_set,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .dstBinding = 0,
            .pBufferInfo = &buffer_descriptor,
            .descriptorCount = 1,
        };
        vkUpdateDescriptorSets( vk_app->device, 1, &write_descriptor_set, 0, NULL );

        VkPipelineCacheCreateInfo cache_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        };
        vk_check( vkCreatePipelineCache( vk_app->device, &cache_info, g_pAllocator, &vk_app->pipeline_cache ),
                  "Failed to create pipeline cache" );

        // Create pipeline
        VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .layout = vk_app->pipeline_layout,
            .flags = 0,
        };

        // Pass SSBO size via specialization constant
        struct SpecializationData specialization_data = { .BUFFER_ELEMENT_COUNT = size };
        VkSpecializationMapEntry specialization_map_entry = {
            .constantID = 0,
            .offset = 0,
            .size = sizeof( uint32_t ),
        };
        VkSpecializationInfo specialization_info = {
            .mapEntryCount = 1,
            .pMapEntries = &specialization_map_entry,
            .dataSize = sizeof( struct SpecializationData ),
            .pData = &specialization_data,
        };

        const char *shader_path = compress ? "../shaders/compress.comp.spv" : "../shaders/uncompress.comp.spv";

        VkPipelineShaderStageCreateInfo shader_stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = load_shader( shader_path, vk_app->device ),
            .pName = "main",
            .pSpecializationInfo = &specialization_info,
        };
        assert( shader_stage.module != VK_NULL_HANDLE );
        vk_app->shader_module = shader_stage.module;
        pipeline_info.stage = shader_stage;
        vk_check( vkCreateComputePipelines(
                      vk_app->device, vk_app->pipeline_cache, 1, &pipeline_info, g_pAllocator, &vk_app->pipeline ),
                  "Failed to create compute pipeline" );

        // Create a command buffer for compute operations
        VkCommandBufferAllocateInfo cmd_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vk_app->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        vk_check( vkAllocateCommandBuffers( vk_app->device, &cmd_buffer_info, &vk_app->command_buffer ),
                  "Failed to allocate command buffer" );

        // Fence for compute CB sync
        VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        vk_check( vkCreateFence( vk_app->device, &fence_create_info, g_pAllocator, &vk_app->fence ),
                  "Failed to create fence" );
    }

    // Create command buffer to submit work
    {
        VkCommandBufferBeginInfo cmd_buffer_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vk_check( vkBeginCommandBuffer( vk_app->command_buffer, &cmd_buffer_info ), "Failed to begin command buffer" );

        // Barrier to ensure that input buffer transfer is finished before compute shader reads from it
        VkBufferMemoryBarrier buffer_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .buffer = device_buffer,
            .size = VK_WHOLE_SIZE,
            .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        };

        vkCmdPipelineBarrier( vk_app->command_buffer,
                              VK_PIPELINE_STAGE_HOST_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0,
                              0,
                              NULL,
                              1,
                              &buffer_barrier,
                              0,
                              NULL );

        vkCmdBindPipeline( vk_app->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_app->pipeline );
        vkCmdBindDescriptorSets( vk_app->command_buffer,
                                 VK_PIPELINE_BIND_POINT_COMPUTE,
                                 vk_app->pipeline_layout,
                                 0,
                                 1,
                                 &vk_app->descriptor_set,
                                 0,
                                 0 );

        vkCmdDispatch( vk_app->command_buffer, buffer_size, 1, 1 );

        // Barrier to ensure that shader writes are finished before buffer is read back from GPU
        buffer_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        buffer_barrier.buffer = device_buffer;
        buffer_barrier.size = VK_WHOLE_SIZE;
        buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier( vk_app->command_buffer,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0,
                              0,
                              NULL,
                              1,
                              &buffer_barrier,
                              0,
                              NULL );

        // Read back to host visible buffer
        VkBufferCopy copy_region = {
            .size = buffer_size,
        };
        vkCmdCopyBuffer( vk_app->command_buffer, device_buffer, host_buffer, 1, &copy_region );

        // Barrier to ensure that buffer copy is finished before host reading from it
        buffer_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        buffer_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        buffer_barrier.buffer = host_buffer;
        buffer_barrier.size = VK_WHOLE_SIZE;
        buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier( vk_app->command_buffer,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_HOST_BIT,
                              0,
                              0,
                              NULL,
                              1,
                              &buffer_barrier,
                              0,
                              NULL );

        vk_check( vkEndCommandBuffer( vk_app->command_buffer ), "Failed to end command buffer" );

        // Submit compute work
        vkResetFences( vk_app->device, 1, &vk_app->fence );
        VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo compute_submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &wait_stage_mask,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk_app->command_buffer,
        };
        vk_check( vkQueueSubmit( vk_app->queue, 1, &compute_submit_info, vk_app->fence ), "Failed to submit queue" );
        vk_check( vkWaitForFences( vk_app->device, 1, &vk_app->fence, VK_TRUE, UINT64_MAX ),
                  "Failed to wait for fence" );

        // Make device writes visible to the host
        void *mapped;
        vk_check( vkMapMemory( vk_app->device, host_memory, 0, VK_WHOLE_SIZE, 0, &mapped ), "Failed to map memory" );
        VkMappedMemoryRange mapped_range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = host_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        vkInvalidateMappedMemoryRanges( vk_app->device, 1, &mapped_range );

        // Copy to output
        memcpy( dst, mapped, buffer_size );
        vkUnmapMemory( vk_app->device, host_memory );
    }

    vkQueueWaitIdle( vk_app->queue );

    vkDestroyBuffer( vk_app->device, device_buffer, g_pAllocator );
    vkFreeMemory( vk_app->device, device_memory, g_pAllocator );
    vkDestroyBuffer( vk_app->device, host_buffer, g_pAllocator );
    vkFreeMemory( vk_app->device, host_memory, g_pAllocator );
}

int main( int argc, char **argv )
{
    struct vk_app vk_app;
    vk_init( &vk_app );
    printf( "device: %s\n\n", vk_app.physical_device_properties.deviceName );

    uint32_t array_size = 100;

    uint32_t *src = malloc( sizeof( uint32_t ) * array_size );
    uint32_t *dst = malloc( sizeof( uint32_t ) * array_size );

    srand( time( NULL ) );
    printf( "src:\n" );
    for ( uint32_t i = 0; i < array_size; i++ )
    {
        src[i] = rand();
        printf( "%d ", src[i] );
    }

    process( &vk_app, true, src, dst, array_size );

    printf( "\n\ncompressed:\n" );
    for ( uint32_t i = 0; i < array_size; i++ )
    {
        printf( "%d ", dst[i] );
    }

    process( &vk_app, false, dst, src, array_size );

    printf( "\n\nuncompressed:\n" );
    for ( uint32_t i = 0; i < array_size; i++ )
    {
        printf( "%d ", src[i] );
    }
    printf( "\n" );

    free( src );
    free( dst );

    vk_shutdown( &vk_app );

    return 0;
}