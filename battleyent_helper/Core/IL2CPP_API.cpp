#include "IL2CPP_API.h"
#include <cstring>
#include <Windows.h>


// IL2CPP Global Function Pointer Definitions
// Actual storage allocation - initialized to nullptr - for now we dont do shit with it, but we keep it here


// Domain & Threading
il2cpp_get_root_domain_prot il2cpp_get_root_domain = nullptr;
il2cpp_thread_attach_prot il2cpp_thread_attach = nullptr;
il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies = nullptr;

// Assembly & Image
il2cpp_assembly_get_image_prot il2cpp_assembly_get_image = nullptr;
il2cpp_image_get_name_prot il2cpp_image_get_name = nullptr;

// Class & Type
il2cpp_class_from_name_prot il2cpp_class_from_name = nullptr;
il2cpp_object_get_class_prot il2cpp_object_get_class = nullptr;
il2cpp_class_get_type_prot il2cpp_class_get_type = nullptr;
il2cpp_type_get_object_prot il2cpp_type_get_object = nullptr;
il2cpp_class_get_parent_prot il2cpp_class_get_parent = nullptr;

// Method
il2cpp_class_get_methods_prot il2cpp_class_get_methods = nullptr;
il2cpp_method_get_name_prot il2cpp_method_get_name = nullptr;
il2cpp_method_get_param_count_prot il2cpp_method_get_param_count = nullptr;
il2cpp_method_get_flags_prot il2cpp_method_get_flags = nullptr;

// Field
il2cpp_class_get_fields_prot il2cpp_class_get_fields = nullptr;
il2cpp_field_get_name_prot il2cpp_field_get_name = nullptr;
il2cpp_field_get_value_prot il2cpp_field_get_value = nullptr;

// Runtime
il2cpp_runtime_invoke_prot il2cpp_runtime_invoke = nullptr;
il2cpp_object_unbox_prot il2cpp_object_unbox = nullptr;
il2cpp_string_new_prot il2cpp_string_new = nullptr;


// ----- IL2CPP Helper Function Implementations ------


/**
 * @brief Load IL2CPP image by name
 */
Il2CppImage* il2cpp_image_loaded(const char* image_name)
{
    Il2CppDomain* domain = il2cpp_get_root_domain();
    if (!domain) return nullptr;

    size_t size = 0;
    const Il2CppAssembly* const* asmbl = il2cpp_domain_get_assemblies(domain, &size);
    if (!asmbl) return nullptr;

    for (size_t i = 0; i < size; i++)
    {
        const Il2CppImage* img = il2cpp_assembly_get_image(asmbl[i]);
        if (!img) continue;

        const char* nm = il2cpp_image_get_name(img);
        if (!nm) continue;

        if (_stricmp(nm, image_name) == 0)
            return (Il2CppImage*)img;
    }

    return nullptr;
}

/**
 * @brief Find single IL2CPP method by name
 */
const Il2CppMethod* FindMethod(Il2CppClass* klass, const char* methodName)
{
    if (!klass || !methodName) return nullptr;

    void* iter = nullptr;
    const Il2CppMethod* method;

    while ((method = il2cpp_class_get_methods(klass, &iter)))
    {
        const char* name = il2cpp_method_get_name(method);
        if (name && strcmp(name, methodName) == 0)
        {
            return method;
        }
    }

    return nullptr;
}

/**
 * @brief Find method recursively in class hierarchy
 */
const Il2CppMethod* FindMethodRecursive(Il2CppClass* klass, const char* name)
{
    if (!klass) return nullptr;
    Il2CppClass* current = klass;
    while (current)
    {
        void* iter = nullptr;
        while (const Il2CppMethod* m = il2cpp_class_get_methods(current, &iter))
        {
            if (strcmp(il2cpp_method_get_name(m), name) == 0)
            {
                return m;
            }
        }
        current = il2cpp_class_get_parent(current);
    }
    return nullptr;
}

/**
 * @brief Find field recursively in class hierarchy
 */
void* FindFieldRecursive(Il2CppClass* klass, const char* name)
{
    if (!klass) return nullptr;
    Il2CppClass* current = klass;
    while (current)
    {
        void* iter = nullptr;
        while (void* f = il2cpp_class_get_fields(current, &iter))
        {
            const char* fname = il2cpp_field_get_name(f);
            if (fname && strcmp(fname, name) == 0)
            {
                return f;
            }
        }
        current = il2cpp_class_get_parent(current);
    }
    return nullptr;
}

/**
 * @brief Patch bool getter to return specific value
 */
void PatchBoolGetter(void* methodPtr, bool returnValue, unsigned char* savedBytes)
{
    if (!methodPtr) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    // Save original bytes if needed
    if (savedBytes)
    {
        memcpy(savedBytes, methodPtr, 16);
    }

    uint8_t* code = (uint8_t*)methodPtr;
    code[0] = 0xB0;                 // mov al, imm8
    code[1] = returnValue ? 1 : 0;
    code[2] = 0xC3;                 // ret

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Restore bool getter original bytes
 */
void RestoreBoolGetter(void* methodPtr, unsigned char* savedBytes)
{
    if (!methodPtr || !savedBytes) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    memcpy(methodPtr, savedBytes, 16);

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Patch float getter to return specific value
 */
void PatchFloatGetter(void* methodPtr, float returnValue, unsigned char* savedBytes)
{
    if (!methodPtr) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 32, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    // Save original bytes if needed
    if (savedBytes)
    {
        memcpy(savedBytes, methodPtr, 16);
    }

    static float forced_value = 0.0f;
    forced_value = returnValue;

    uint8_t stub[32];
    memset(stub, 0x90, sizeof(stub));
    stub[0] = 0x48;
    stub[1] = 0xB8;
    uint64_t addr = (uint64_t)&forced_value;
    memcpy(&stub[2], &addr, sizeof(addr));
    int idx = 2 + (int)sizeof(addr);
    stub[idx++] = 0xF3;
    stub[idx++] = 0x0F;
    stub[idx++] = 0x10;
    stub[idx++] = 0x00;
    stub[idx++] = 0xC3;

    memcpy(methodPtr, stub, idx);

    VirtualProtect(methodPtr, 32, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 32);
}

/**
 * @brief Restore float getter original bytes
 */
void RestoreFloatGetter(void* methodPtr, unsigned char* savedBytes)
{
    if (!methodPtr || !savedBytes) return;

    DWORD oldProtect;
    if (!VirtualProtect(methodPtr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    memcpy(methodPtr, savedBytes, 16);

    VirtualProtect(methodPtr, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), methodPtr, 16);
}

/**
 * @brief Get C# System.Type object from class
 */
void* GetSystemType(Il2CppClass* klass)
{
    if (!klass) return nullptr;
    void* type = il2cpp_class_get_type(klass);
    return il2cpp_type_get_object(type);
}

/**
 * @brief Get current GameWorld instance via FindObjectOfType
 */
void* GetGameWorldInstance()
{
    Il2CppImage* coreModule = il2cpp_image_loaded("UnityEngine.CoreModule.dll");
    if (!coreModule) return nullptr;

    Il2CppClass* unityObjectClass = il2cpp_class_from_name(coreModule, "UnityEngine", "Object");

    const Il2CppMethod* findMethod = nullptr;
    void* iter = nullptr;
    while (const Il2CppMethod* m = il2cpp_class_get_methods(unityObjectClass, &iter))
    {
        if (strcmp(il2cpp_method_get_name(m), "FindObjectOfType") == 0 &&
            il2cpp_method_get_param_count(m) == 1)
        {
            findMethod = m;
            break;
        }
    }
    if (!findMethod) return nullptr;

    Il2CppImage* csharpImage = il2cpp_image_loaded("Assembly-CSharp.dll");
    Il2CppClass* gameWorldClass = il2cpp_class_from_name(csharpImage, "EFT", "GameWorld");
    void* gameWorldType = GetSystemType(gameWorldClass);

    void* args[] = { gameWorldType };
    void* result = il2cpp_runtime_invoke(findMethod, nullptr, args, nullptr);

    return result;
}
