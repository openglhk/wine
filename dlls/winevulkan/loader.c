/* Wine Vulkan ICD implementation
 *
 * Copyright 2017 Roderick Colenbrander
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winuser.h"
#include "initguid.h"
#include "devguid.h"
#include "setupapi.h"

#include "vulkan_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

/* For now default to 4 as it felt like a reasonable version feature wise to support.
 * Version 5 adds more extensive version checks. Something to tackle later.
 */
#define WINE_VULKAN_ICD_VERSION 4

DEFINE_DEVPROPKEY(DEVPROPKEY_GPU_LUID, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 2);
DEFINE_DEVPROPKEY(WINE_DEVPROPKEY_GPU_VULKAN_UUID, 0x233a9ef3, 0xafc4, 0x4abd, 0xb5, 0x64, 0xc3, 0x2f, 0x21, 0xf1, 0x53, 0x5c, 2);

static HINSTANCE hinstance;

static void *wine_vk_get_global_proc_addr(const char *name);

#define wine_vk_find_struct(s, t) wine_vk_find_struct_((void *)s, VK_STRUCTURE_TYPE_##t)
static void *wine_vk_find_struct_(void *s, VkStructureType t)
{
    VkBaseOutStructure *header;

    for (header = s; header; header = header->pNext)
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

VkResult WINAPI wine_vkEnumerateInstanceLayerProperties(uint32_t *count, VkLayerProperties *properties)
{
    TRACE("%p, %p\n", count, properties);

    *count = 0;
    return VK_SUCCESS;
}

static const struct vulkan_func vk_global_dispatch_table[] =
{
    /* These functions must call wine_vk_init_once() before accessing vk_funcs. */
    {"vkCreateInstance", &wine_vkCreateInstance},
    {"vkEnumerateInstanceExtensionProperties", &wine_vkEnumerateInstanceExtensionProperties},
    {"vkEnumerateInstanceLayerProperties", &wine_vkEnumerateInstanceLayerProperties},
    {"vkEnumerateInstanceVersion", &wine_vkEnumerateInstanceVersion},
    {"vkGetInstanceProcAddr", &wine_vkGetInstanceProcAddr},
};

static void *wine_vk_get_global_proc_addr(const char *name)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(vk_global_dispatch_table); i++)
    {
        if (strcmp(name, vk_global_dispatch_table[i].name) == 0)
        {
            TRACE("Found name=%s in global table\n", debugstr_a(name));
            return vk_global_dispatch_table[i].func;
        }
    }
    return NULL;
}

PFN_vkVoidFunction WINAPI wine_vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    void *func;

    TRACE("%p, %s\n", instance, debugstr_a(name));

    if (!name)
        return NULL;

    /* vkGetInstanceProcAddr can load most Vulkan functions when an instance is passed in, however
     * for a NULL instance it can only load global functions.
     */
    func = wine_vk_get_global_proc_addr(name);
    if (func)
    {
        return func;
    }
    if (!instance)
    {
        WARN("Global function %s not found.\n", debugstr_a(name));
        return NULL;
    }

    func = wine_vk_get_instance_proc_addr(name);
    if (func) return func;

    func = wine_vk_get_phys_dev_proc_addr(name);
    if (func) return func;

    /* vkGetInstanceProcAddr also loads any children of instance, so device functions as well. */
    func = wine_vk_get_device_proc_addr(name);
    if (func) return func;

    WARN("Unsupported device or instance function: %s.\n", debugstr_a(name));
    return NULL;
}

PFN_vkVoidFunction WINAPI wine_vkGetDeviceProcAddr(VkDevice device, const char *name)
{
    void *func;
    TRACE("%p, %s\n", device, debugstr_a(name));

    /* The spec leaves return value undefined for a NULL device, let's just return NULL. */
    if (!device || !name)
        return NULL;

    /* Per the spec, we are only supposed to return device functions as in functions
     * for which the first parameter is vkDevice or a child of vkDevice like a
     * vkCommandBuffer or vkQueue.
     * Loader takes care of filtering of extensions which are enabled or not.
     */
    func = wine_vk_get_device_proc_addr(name);
    if (func)
        return func;

    /* vkGetDeviceProcAddr was intended for loading device and subdevice functions.
     * idTech 6 titles such as Doom and Wolfenstein II, however use it also for
     * loading of instance functions. This is undefined behavior as the specification
     * disallows using any of the returned function pointers outside of device /
     * subdevice objects. The games don't actually use the function pointers and if they
     * did, they would crash as VkInstance / VkPhysicalDevice parameters need unwrapping.
     * Khronos clarified behavior in the Vulkan spec and expects drivers to get updated,
     * however it would require both driver and game fixes.
     * https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/2323
     * https://github.com/KhronosGroup/Vulkan-Docs/issues/655
     */
    if (device->quirks & WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR
            && ((func = wine_vk_get_instance_proc_addr(name))
             || (func = wine_vk_get_phys_dev_proc_addr(name))))
    {
        WARN("Returning instance function %s.\n", debugstr_a(name));
        return func;
    }

    WARN("Unsupported device function: %s.\n", debugstr_a(name));
    return NULL;
}

void * WINAPI wine_vk_icdGetPhysicalDeviceProcAddr(VkInstance instance, const char *name)
{
    TRACE("%p, %s\n", instance, debugstr_a(name));

    return wine_vk_get_phys_dev_proc_addr(name);
}

void * WINAPI wine_vk_icdGetInstanceProcAddr(VkInstance instance, const char *name)
{
    TRACE("%p, %s\n", instance, debugstr_a(name));

    /* Initial version of the Vulkan ICD spec required vkGetInstanceProcAddr to be
     * exported. vk_icdGetInstanceProcAddr was added later to separate ICD calls from
     * Vulkan API. One of them in our case should forward to the other, so just forward
     * to the older vkGetInstanceProcAddr.
     */
    return wine_vkGetInstanceProcAddr(instance, name);
}

VkResult WINAPI wine_vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *supported_version)
{
    uint32_t req_version;

    TRACE("%p\n", supported_version);

    /* The spec is not clear how to handle this. Mesa drivers don't check, but it
     * is probably best to not explode. VK_INCOMPLETE seems to be the closest value.
     */
    if (!supported_version)
        return VK_INCOMPLETE;

    req_version = *supported_version;
    *supported_version = min(req_version, WINE_VULKAN_ICD_VERSION);
    TRACE("Loader requested ICD version %u, returning %u\n", req_version, *supported_version);

    return VK_SUCCESS;
}

static HANDLE get_display_device_init_mutex(void)
{
    static const WCHAR init_mutexW[] = {'d','i','s','p','l','a','y','_','d','e','v','i','c','e','_','i','n','i','t',0};
    HANDLE mutex = CreateMutexW(NULL, FALSE, init_mutexW);

    WaitForSingleObject(mutex, INFINITE);
    return mutex;
}

static void release_display_device_init_mutex(HANDLE mutex)
{
    ReleaseMutex(mutex);
    CloseHandle(mutex);
}

/* Wait until graphics driver is loaded by explorer */
static void wait_graphics_driver_ready(void)
{
    static BOOL ready = FALSE;

    if (!ready)
    {
        SendMessageW(GetDesktopWindow(), WM_NULL, 0, 0);
        ready = TRUE;
    }
}

static void fill_luid_property(VkPhysicalDeviceProperties2 *properties2)
{
    static const WCHAR pci[] = {'P','C','I',0};
    VkPhysicalDeviceIDProperties *id;
    SP_DEVINFO_DATA device_data;
    DWORD type, device_idx = 0;
    HDEVINFO devinfo;
    HANDLE mutex;
    GUID uuid;
    LUID luid;

    if (!(id = wine_vk_find_struct(properties2, PHYSICAL_DEVICE_ID_PROPERTIES)))
        return;

    wait_graphics_driver_ready();
    mutex = get_display_device_init_mutex();
    devinfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, pci, NULL, 0);
    device_data.cbSize = sizeof(device_data);
    while (SetupDiEnumDeviceInfo(devinfo, device_idx++, &device_data))
    {
        if (!SetupDiGetDevicePropertyW(devinfo, &device_data, &WINE_DEVPROPKEY_GPU_VULKAN_UUID,
                &type, (BYTE *)&uuid, sizeof(uuid), NULL, 0))
            continue;

        if (!IsEqualGUID(&uuid, id->deviceUUID))
            continue;

        if (SetupDiGetDevicePropertyW(devinfo, &device_data, &DEVPROPKEY_GPU_LUID, &type,
                (BYTE *)&luid, sizeof(luid), NULL, 0))
        {
            memcpy(&id->deviceLUID, &luid, sizeof(id->deviceLUID));
            id->deviceLUIDValid = VK_TRUE;
            id->deviceNodeMask = 1;
            break;
        }
    }
    SetupDiDestroyDeviceInfoList(devinfo);
    release_display_device_init_mutex(mutex);

    TRACE("deviceName:%s deviceLUIDValid:%d LUID:%08x:%08x deviceNodeMask:%#x.\n",
            properties2->properties.deviceName, id->deviceLUIDValid, luid.HighPart, luid.LowPart,
            id->deviceNodeMask);
}

void WINAPI wine_vkGetPhysicalDeviceProperties2(VkPhysicalDevice phys_dev,
        VkPhysicalDeviceProperties2 *properties2)
{
    TRACE("%p, %p\n", phys_dev, properties2);

    thunk_vkGetPhysicalDeviceProperties2(phys_dev, properties2);
    fill_luid_property(properties2);
}

void WINAPI wine_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice phys_dev,
        VkPhysicalDeviceProperties2 *properties2)
{
    TRACE("%p, %p\n", phys_dev, properties2);

    thunk_vkGetPhysicalDeviceProperties2KHR(phys_dev, properties2);
    fill_luid_property(properties2);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved)
{
    TRACE("%p, %u, %p\n", hinst, reason, reserved);

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            hinstance = hinst;
            DisableThreadLibraryCalls(hinst);
            break;
    }
    return TRUE;
}

static const WCHAR winevulkan_json_resW[] = {'w','i','n','e','v','u','l','k','a','n','_','j','s','o','n',0};
static const WCHAR winevulkan_json_pathW[] = {'\\','w','i','n','e','v','u','l','k','a','n','.','j','s','o','n',0};
static const WCHAR vulkan_driversW[] = {'S','o','f','t','w','a','r','e','\\','K','h','r','o','n','o','s','\\',
                                        'V','u','l','k','a','n','\\','D','r','i','v','e','r','s',0};

HRESULT WINAPI DllRegisterServer(void)
{
    WCHAR json_path[MAX_PATH];
    HRSRC rsrc;
    const char *data;
    DWORD datalen, written, zero = 0;
    HANDLE file;
    HKEY key;

    /* Create the JSON manifest and registry key to register this ICD with the official Vulkan loader. */
    TRACE("\n");
    rsrc = FindResourceW(hinstance, winevulkan_json_resW, (const WCHAR *)RT_RCDATA);
    data = LockResource(LoadResource(hinstance, rsrc));
    datalen = SizeofResource(hinstance, rsrc);

    GetSystemDirectoryW(json_path, ARRAY_SIZE(json_path));
    lstrcatW(json_path, winevulkan_json_pathW);
    file = CreateFileW(json_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        ERR("Unable to create JSON manifest.\n");
        return E_UNEXPECTED;
    }
    WriteFile(file, data, datalen, &written, NULL);
    CloseHandle(file);

    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, vulkan_driversW, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
    {
        RegSetValueExW(key, json_path, 0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));
        RegCloseKey(key);
    }
    return S_OK;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    WCHAR json_path[MAX_PATH];
    HKEY key;

    /* Remove the JSON manifest and registry key */
    TRACE("\n");
    GetSystemDirectoryW(json_path, ARRAY_SIZE(json_path));
    lstrcatW(json_path, winevulkan_json_pathW);
    DeleteFileW(json_path);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, vulkan_driversW, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS)
    {
        RegDeleteValueW(key, json_path);
        RegCloseKey(key);
    }

    return S_OK;
}
