#include "vulkan/vulkan.h"
#include "vulkan/vk_layer.h"

#include <string.h>

#include <mutex>
#include <unordered_map>

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

// single global lock, for simplicity
std::mutex gLock;
typedef std::lock_guard<std::mutex> scoped_lock;

static PFN_vkGetInstanceProcAddr gGIPA = VK_NULL_HANDLE;
static PFN_vkGetDeviceProcAddr gGDPA = VK_NULL_HANDLE;

static VkDevice* gLastDevice = VK_NULL_HANDLE;

// use the loader's dispatch table pointer as a key for dispatch map lookups
template<typename DispatchableType>
void* GetKey(DispatchableType inst)
{
    return (void*)inst;
}

struct CommandBufferStats
{
    uint32_t drawCount = 0, instanceCount = 0, vertCount = 0;
};

std::unordered_map<VkCommandBuffer, CommandBufferStats> gCommandBufferStats;

struct InstanceFunctions
{
    PFN_vkGetInstanceProcAddr jnrGetInstanceProcAddr;
};

struct DeviceFunctions
{
    PFN_vkGetInstanceProcAddr jnrGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr jnrGetDeviceProcAddr;
    
    PFN_vkBeginCommandBuffer jnrBeginCommandBuffer;
    PFN_vkEndCommandBuffer jnrEndCommandBuffer;

    PFN_vkCmdDraw jnrCmdDraw;
    PFN_vkCmdDrawIndexed jnrCmdDrawIndexed;
};

// layer book-keeping information, to store dispatch tables by key
std::unordered_map<void*, InstanceFunctions> gInstanceDispatch;
std::unordered_map<void*, DeviceFunctions> gDeviceDispatch;

#define GETPROCADDR(func) if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&JnrLayer_##func;

/* Actual implementations */
VK_LAYER_EXPORT VkResult VKAPI_CALL JnrLayer_CreateInstance(
    const VkInstanceCreateInfo * pCreateInfo,
    const VkAllocationCallbacks * pAllocator,
    VkInstance * pInstance)
{
    VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*)pCreateInfo->pNext;

    // step through the chain of pNext until we get to the link info
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerInstanceCreateInfo*)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == NULL)
    {
        // No loader instance create info
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;

    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");

    VkResult ret = createFunc(pCreateInfo, pAllocator, pInstance);

    InstanceFunctions instanceFunctions{};
    {
        instanceFunctions.jnrGetInstanceProcAddr = gpa;
    }

    {
        scoped_lock l(gLock);
        void* key = GetKey(pInstance);
        printf("Instance with key = %p initialized\n", key);
        gInstanceDispatch[key] = instanceFunctions;
        gGIPA = gpa;
    }

    return ret;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL JnrLayer_CreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*)pCreateInfo->pNext;

    // step through the chain of pNext until we get to the link info
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerDeviceCreateInfo*)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == NULL)
    {
        // No loader instance create info
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    // move chain on for next layer
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

    VkResult ret = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);

    DeviceFunctions deviceFunctions{};
    {
        deviceFunctions.jnrGetDeviceProcAddr = gdpa;
        deviceFunctions.jnrGetInstanceProcAddr = gipa;

        deviceFunctions.jnrBeginCommandBuffer = (PFN_vkBeginCommandBuffer)gdpa(*pDevice, "vkBeginCommandBuffer");
        deviceFunctions.jnrEndCommandBuffer = (PFN_vkEndCommandBuffer)gdpa(*pDevice, "vkEndCommandBuffer");
        deviceFunctions.jnrCmdDraw = (PFN_vkCmdDraw)gdpa(*pDevice, "vkCmdDraw");
        deviceFunctions.jnrCmdDrawIndexed = (PFN_vkCmdDrawIndexed)gdpa(*pDevice, "vkCmdDrawIndexed");
    }

    {
        scoped_lock l(gLock);
        gDeviceDispatch[GetKey(pDevice)] = deviceFunctions;
        gGDPA = gdpa;
        gLastDevice = pDevice;
    }

    return ret;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL JnrLayer_BeginCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    scoped_lock l(gLock);
    gCommandBufferStats[commandBuffer] = CommandBufferStats();

    return gDeviceDispatch[GetKey(gLastDevice)].jnrBeginCommandBuffer(commandBuffer, pBeginInfo);
}

VK_LAYER_EXPORT void VKAPI_CALL JnrLayer_CmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
    scoped_lock l(gLock);

    gCommandBufferStats[commandBuffer].drawCount++;
    gCommandBufferStats[commandBuffer].instanceCount += instanceCount;
    gCommandBufferStats[commandBuffer].vertCount += instanceCount * indexCount;

    gDeviceDispatch[GetKey(gLastDevice)].jnrCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL JnrLayer_CmdDraw(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
    scoped_lock l(gLock);

    gCommandBufferStats[commandBuffer].drawCount++;
    gCommandBufferStats[commandBuffer].instanceCount += instanceCount;
    gCommandBufferStats[commandBuffer].vertCount += vertexCount * instanceCount;

    gDeviceDispatch[GetKey(gLastDevice)].jnrCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstVertex);
}


VK_LAYER_EXPORT VkResult VKAPI_CALL JnrLayer_EndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
    scoped_lock l(gLock);

    auto& s = gCommandBufferStats[commandBuffer];
    printf("Command buffer %p ended with %u draws, %u instances and %u vertices\n", commandBuffer, s.drawCount, s.instanceCount, s.vertCount);

    return gDeviceDispatch[GetKey(gLastDevice)].jnrEndCommandBuffer(commandBuffer);
}

/* Get pointer functions */
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL JnrLayer_GetDeviceProcAddr(VkDevice device, const char* pName)
{
    GETPROCADDR(BeginCommandBuffer);
    GETPROCADDR(EndCommandBuffer);
    GETPROCADDR(CmdDraw);
    GETPROCADDR(CmdDrawIndexed);

    if (auto it = gDeviceDispatch.find(GetKey(device)); it != gDeviceDispatch.end())
    {
        return it->second.jnrGetDeviceProcAddr(device, pName);
    }
    return gGDPA(device, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL JnrLayer_GetInstanceProcAddr(VkInstance instance, const char* pName)
{
    
    // instance chain functions we intercept
    GETPROCADDR(GetInstanceProcAddr);
    GETPROCADDR(CreateInstance);
    GETPROCADDR(CreateDevice);

    // device chain functions we intercept
    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(BeginCommandBuffer);
    GETPROCADDR(CmdDraw);
    GETPROCADDR(CmdDrawIndexed);
    GETPROCADDR(EndCommandBuffer);

    // return gInstanceDispatch[GetKey(instance)].jnrGetInstanceProcAddr(instance, pName);
    if (auto it = gInstanceDispatch.find(GetKey(instance)); it != gInstanceDispatch.end())
    {
        return it->second.jnrGetInstanceProcAddr(instance, pName);
    }
    return gGIPA(instance, pName);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL JnrLayer_NegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct)
{
    if (pVersionStruct->loaderLayerInterfaceVersion >= 0)
    {
        pVersionStruct->sType = LAYER_NEGOTIATE_INTERFACE_STRUCT;
        pVersionStruct->pNext = nullptr;
        pVersionStruct->pfnGetInstanceProcAddr = JnrLayer_GetInstanceProcAddr;
        pVersionStruct->pfnGetDeviceProcAddr = JnrLayer_GetDeviceProcAddr;
        pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

        return VK_SUCCESS;
    }
    return VK_ERROR_DEVICE_LOST;
}
