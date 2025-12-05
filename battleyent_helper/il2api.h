#pragma once
#include <vector>
#include <string>

// Forward declarations
struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppMethod;
struct Il2CppImage;

// ========================================
// IL2CPP Function Pointer Types
// ========================================

using il2cpp_get_root_domain_prot = Il2CppDomain * (*)();
using il2cpp_thread_attach_prot = Il2CppThread * (*)(Il2CppDomain*);
using il2cpp_domain_get_assemblies_prot = const Il2CppAssembly** (*)(Il2CppDomain*, size_t*);
using il2cpp_class_from_name_prot = Il2CppClass * (*)(const Il2CppImage*, const char*, const char*);
using il2cpp_class_get_methods_prot = const Il2CppMethod* (*)(Il2CppClass*, void**);
using il2cpp_method_get_name_prot = const char* (*)(const Il2CppMethod*);
using il2cpp_assembly_get_image_prot = const Il2CppImage* (*)(const Il2CppAssembly*);
using il2cpp_image_get_name_prot = const char* (*)(const Il2CppImage*);
using il2cpp_runtime_invoke_prot = void* (*)(const Il2CppMethod*, void*, void**, void**);

// ========================================
// External IL2CPP Function Pointers
// (Defined in dllmain.cpp, used everywhere)
// ========================================

extern il2cpp_get_root_domain_prot il2cpp_get_root_domain;
extern il2cpp_thread_attach_prot il2cpp_thread_attach;
extern il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies;
extern il2cpp_class_from_name_prot il2cpp_class_from_name;
extern il2cpp_class_get_methods_prot il2cpp_class_get_methods;
extern il2cpp_method_get_name_prot il2cpp_method_get_name;
extern il2cpp_assembly_get_image_prot il2cpp_assembly_get_image;
extern il2cpp_image_get_name_prot il2cpp_image_get_name;
extern il2cpp_runtime_invoke_prot il2cpp_runtime_invoke;

// ========================================
// Helper Functions
// ========================================

extern void find_methods_by_names(Il2CppClass* klass, const std::vector<std::string>& names);
extern std::vector<const Il2CppMethod*> found_methods;
extern std::vector<const Il2CppMethod*> error_screen_methods;