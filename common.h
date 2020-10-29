#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

struct vk_app
{
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    VkQueueFamilyProperties queue_family_properties;
    VkDevice device;
    VkShaderModule shader_module;
    VkQueue queue;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkPipelineCache pipeline_cache;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;
    VkFence fence;
    VkDebugReportCallbackEXT debug_report_callback;
};

struct SpecializationData
{
    uint32_t BUFFER_ELEMENT_COUNT;
};

VkAllocationCallbacks *g_pAllocator = NULL;

inline void fail( char *err_msg )
{
    printf( "%s\n", err_msg );
    *(int *)( 0 ) = 0;
}

inline void vk_check( VkResult result, char *err_msg )
{
    if ( result != VK_SUCCESS )
    {
        fail( err_msg );
    }
}

inline void create_buffer( struct vk_app *vk_app,
                           VkBufferUsageFlags buffer_usage_flags,
                           VkMemoryPropertyFlags memory_property_flags,
                           VkBuffer *buffer,
                           VkDeviceMemory *memory,
                           VkDeviceSize size,
                           void *data )
{
    // Create buffer
    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = buffer_usage_flags,
        .size = size,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vk_check( vkCreateBuffer( vk_app->device, &buffer_create_info, g_pAllocator, buffer ), "Failed to create buffer" );

    // Create memory
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    };
    vkGetBufferMemoryRequirements( vk_app->device, *buffer, &memory_requirements );
    alloc_info.allocationSize = memory_requirements.size;

    // Find memory type
    bool type_found = false;
    for ( uint32_t i = 0; i < vk_app->physical_device_memory_properties.memoryTypeCount; i++ )
    {
        if ( ( memory_requirements.memoryTypeBits & 1 ) == 1 )
        {
            if ( ( vk_app->physical_device_memory_properties.memoryTypes[i].propertyFlags & memory_property_flags ) ==
                 memory_property_flags )
            {
                alloc_info.memoryTypeIndex = i;
                type_found = true;
            }
        }
        memory_requirements.memoryTypeBits >>= 1;
    }
    if ( !type_found ) fail( "Failed to find memory type" );
    vk_check( vkAllocateMemory( vk_app->device, &alloc_info, g_pAllocator, memory ), "Failed to allocate memory" );

    // Copy data if any
    if ( data != NULL )
    {
        void *mapped;
        vk_check( vkMapMemory( vk_app->device, *memory, 0, size, 0, &mapped ), "Failed to map memory" );
        memcpy( mapped, data, size );
        vkUnmapMemory( vk_app->device, *memory );
    }

    vk_check( vkBindBufferMemory( vk_app->device, *buffer, *memory, 0 ), "Failed to bind memory" );
}

inline VkShaderModule load_shader( const char *path, VkDevice device )
{
    FILE *fp = fopen( path, "rb" );
    if ( fp != NULL )
    {
        fseek( fp, 0L, SEEK_END );
        size_t size = ftell( fp );
        rewind( fp );

        char *shader_code = malloc( size );
        fread( shader_code, size, 1, fp );
        fclose( fp );

        VkShaderModule shader_module;
        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = size,
            .pCode = (uint32_t *)shader_code,
        };
        vk_check( vkCreateShaderModule( device, &create_info, g_pAllocator, &shader_module ),
                  "Failed to create shader module" );

        free( shader_code );
        return shader_module;
    }

    printf( "Could not open shader file %s\n", path );
    return VK_NULL_HANDLE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message_callback( VkDebugReportFlagsEXT flags,
                                                              VkDebugReportObjectTypeEXT object_type,
                                                              uint64_t object,
                                                              size_t location,
                                                              int32_t message_code,
                                                              const char *layer_prefix,
                                                              const char *message,
                                                              void *user_data )
{
    printf( "%s - %s\n", layer_prefix, message );
    return VK_FALSE;
}