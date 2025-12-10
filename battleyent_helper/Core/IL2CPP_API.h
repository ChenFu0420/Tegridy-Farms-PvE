#pragma once
#include <Windows.h>
#include <cstdint>

// =================================================================
// IL2CPP Global API Declarations
// Single source of truth for all IL2CPP types and function pointers
// =================================================================

// ========================================
// IL2CPP Forward Declarations
// ========================================
struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppMethod;
struct Il2CppImage;
struct Il2CppObject;
struct Il2CppField;
struct Il2CppType;
struct Il2CppArray;

typedef void (*Il2CppMethodPointer)();

// Unity Vector3 Struct
struct Vector3 {
    float x, y, z;
};

// Simple C# Array Header Structure (IL2CPP)
struct Il2CppArraySize {
    void* klass;
    void* monitor;
    void* bounds;
    int32_t max_length;
};

// ========================================
// IL2CPP Function Pointer Type Definitions
// ========================================

// Domain & Threading
typedef Il2CppDomain* (*il2cpp_get_root_domain_prot)();
typedef Il2CppThread* (*il2cpp_thread_attach_prot)(Il2CppDomain*);
typedef const Il2CppAssembly** (*il2cpp_domain_get_assemblies_prot)(Il2CppDomain*, size_t*);

// Assembly & Image
typedef const Il2CppImage* (*il2cpp_assembly_get_image_prot)(const Il2CppAssembly*);
typedef const char* (*il2cpp_image_get_name_prot)(const Il2CppImage*);
typedef Il2CppImage* (*il2cpp_image_loaded_prot)(const char*);

// Class & Type
typedef Il2CppClass* (*il2cpp_class_from_name_prot)(const Il2CppImage*, const char*, const char*);
typedef Il2CppClass* (*il2cpp_object_get_class_prot)(void*);
typedef void* (*il2cpp_class_get_type_prot)(Il2CppClass*);
typedef void* (*il2cpp_type_get_object_prot)(void*);
typedef Il2CppClass* (*il2cpp_class_get_parent_prot)(Il2CppClass*);

// Method
typedef const Il2CppMethod* (*il2cpp_class_get_methods_prot)(Il2CppClass*, void**);
typedef const char* (*il2cpp_method_get_name_prot)(const Il2CppMethod*);
typedef uint32_t(*il2cpp_method_get_param_count_prot)(const Il2CppMethod*);
typedef uint32_t(*il2cpp_method_get_flags_prot)(const Il2CppMethod*, uint32_t*);

// Field
typedef void* (*il2cpp_class_get_fields_prot)(Il2CppClass*, void**);
typedef const char* (*il2cpp_field_get_name_prot)(void*);
typedef void (*il2cpp_field_get_value_prot)(void*, void*, void*);

// Runtime
typedef void* (*il2cpp_runtime_invoke_prot)(const Il2CppMethod*, void*, void**, void**);
typedef void* (*il2cpp_object_unbox_prot)(void*);
typedef Il2CppObject* (*il2cpp_string_new_prot)(const char*);

// ========================================
// Global IL2CPP Function Pointers
// Declared as extern - defined in dllmain.cpp
// ========================================

// Domain & Threading
extern il2cpp_get_root_domain_prot il2cpp_get_root_domain;
extern il2cpp_thread_attach_prot il2cpp_thread_attach;
extern il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies;

// Assembly & Image
extern il2cpp_assembly_get_image_prot il2cpp_assembly_get_image;
extern il2cpp_image_get_name_prot il2cpp_image_get_name;

// Class & Type
extern il2cpp_class_from_name_prot il2cpp_class_from_name;
extern il2cpp_object_get_class_prot il2cpp_object_get_class;
extern il2cpp_class_get_type_prot il2cpp_class_get_type;
extern il2cpp_type_get_object_prot il2cpp_type_get_object;
extern il2cpp_class_get_parent_prot il2cpp_class_get_parent;

// Method
extern il2cpp_class_get_methods_prot il2cpp_class_get_methods;
extern il2cpp_method_get_name_prot il2cpp_method_get_name;
extern il2cpp_method_get_param_count_prot il2cpp_method_get_param_count;
extern il2cpp_method_get_flags_prot il2cpp_method_get_flags;

// Field
extern il2cpp_class_get_fields_prot il2cpp_class_get_fields;
extern il2cpp_field_get_name_prot il2cpp_field_get_name;
extern il2cpp_field_get_value_prot il2cpp_field_get_value;

// Runtime
extern il2cpp_runtime_invoke_prot il2cpp_runtime_invoke;
extern il2cpp_object_unbox_prot il2cpp_object_unbox;
extern il2cpp_string_new_prot il2cpp_string_new;

// ========================================
// IL2CPP Helper Functions
// Declared as extern - defined in IL2CPP_API.cpp
// ========================================

// Load IL2CPP image by name
extern Il2CppImage* il2cpp_image_loaded(const char* image_name);

// Find single IL2CPP method by name
extern const Il2CppMethod* FindMethod(Il2CppClass* klass, const char* methodName);

// Find method recursively (in class hierarchy)
extern const Il2CppMethod* FindMethodRecursive(Il2CppClass* klass, const char* name);

// Find field recursively (in class hierarchy)
extern void* FindFieldRecursive(Il2CppClass* klass, const char* name);

// Patch helper functions
extern void PatchBoolGetter(void* methodPtr, bool returnValue, unsigned char* savedBytes);
extern void RestoreBoolGetter(void* methodPtr, unsigned char* savedBytes);
extern void PatchFloatGetter(void* methodPtr, float returnValue, unsigned char* savedBytes);
extern void RestoreFloatGetter(void* methodPtr, unsigned char* savedBytes);

// System Type helper
extern void* GetSystemType(Il2CppClass* klass);

// GameWorld helper
extern void* GetGameWorldInstance();