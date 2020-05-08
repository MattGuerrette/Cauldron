project (Cauldron_Common)

if(WIN32)
add_compile_options(/MP)
endif()

set(base_src
    "base/Benchmark.cpp"
    "base/Benchmark.h"
    "base/ImGuiHelper.cpp"
    "base/ImGuiHelper.h"
    "base/ShaderCompiler.cpp"
    "base/ShaderCompiler.h"
)


if(WIN32)
set(GLTF_src
    "GLTF/GltfStructures.h"
    "GLTF/GltfCommon.cpp"
    "GLTF/GltfCommon.h"
    "GLTF/GltfPbrMaterial.cpp"
    "GLTF/GltfPbrMaterial.h"
    "GLTF/GltfHelpers.cpp"
    "GLTF/GltfHelpers.h"
)
endif()

if(WIN32)
file(GLOB_RECURSE Misc_src
    "Misc/*.cpp"
    "Misc/*.h"
)
else()
set(Misc_src
    "Misc/Cache.cpp"
    "Misc/Cache.h")
endif()

if(WIN32)
add_library (${PROJECT_NAME} STATIC ${base_src} ${GLTF_src} ${Misc_src} )
else()
add_library (${PROJECT_NAME} STATIC ${base_src} ${Misc_src} )
endif()
target_link_libraries (${PROJECT_NAME} NJSON)
target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
source_group("base"         FILES ${base_src})
source_group("GLTF"         FILES ${GLTF_src})
source_group("Misc"         FILES ${Misc_src})
endif()
