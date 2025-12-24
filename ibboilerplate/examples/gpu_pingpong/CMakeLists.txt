set(proj_name proj_ibboilerplate)

cmake_minimum_required(VERSION 3.21.0)
project(${proj_name} LANGUAGES CXX HIP)


set(GPU_RUNTIME "HIP" CACHE STRING "Switches between HIP and CUDA")
set(GPU_RUNTIMES "HIP" "CUDA")
set_property(CACHE GPU_RUNTIME PROPERTY STRINGS ${GPU_RUNTIMES})


if(NOT "${GPU_RUNTIME}" IN_LIST GPU_RUNTIMES)
    set(ERROR_MESSAGE
        "GPU_RUNTIME is set to \"${GPU_RUNTIME}\". \nGPU_RUNTIME must be either HIP or CUDA."
    )
    message(FATAL_ERROR ${ERROR_MESSAGE})
endif()


enable_language(${GPU_RUNTIME})
set(CAMKE_${GPU_RUNTIME}_STANDARD 23)
set(CMAKE_${GPU_RUNTIME}_EXTENSIONS OFF)
set(CMAKE_${GPU_RUNTIME}_STANDARD_REQUIRED ON)
# set(CMAKE_HIP_FLAGS "--save-temps")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



set(ROCM_ROOT
    "/opt/rocm"
    CACHE PATH
    "Root directory of the ROCm installations"
)

list(APPEND CMAKE_PREFIX_PATH "${ROCM_ROOT}")


# Other libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBVERBS REQUIRED IMPORTED_TARGET libibverbs)
pkg_check_modules(RDMACM REQUIRED IMPORTED_TARGET librdmacm)

# 1. Create the library from the source files
add_executable(gpu_pingpong_server 
    gpu_pingpong_server.cpp
)
add_executable(gpu_pingpong_client 
    gpu_pingpong_client.cpp
)

set(include_dirs ${CMAKE_CURRENT_SOURCE_DIR}/kernels)
target_include_directories(gpu_pingpong_server PRIVATE ${include_dirs})
target_include_directories(gpu_pingpong_client PRIVATE ${include_dirs})
set_source_files_properties(kernels/gpu_pingpong_server.cpp PROPERTIES LANGUAGE ${GPU_RUNTIME})
set_source_files_properties(kernels/gpu_pingpong_client.cpp PROPERTIES LANGUAGE ${GPU_RUNTIME})

target_link_libraries(gpu_pingpong_server  
    PRIVATE
        ibboilerplate
        amdhip64
        PkgConfig::IBVERBS
        PkgConfig::RDMACM
)
target_link_libraries(gpu_pingpong_client   
    PRIVATE
        ibboilerplate
        amdhip64
        PkgConfig::IBVERBS
        PkgConfig::RDMACM
)