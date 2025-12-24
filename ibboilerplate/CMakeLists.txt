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


add_library(ibboilerplate
    src/rdma_base.cpp
)

set(include_dirs "./include")

target_include_directories(ibboilerplate PUBLIC ${include_dirs})
target_link_libraries(ibboilerplate PRIVATE  
        PkgConfig::IBVERBS
        PkgConfig::RDMACM)
set_source_files_properties(src/rdma_base.c PROPERTIES LANGUAGE ${GPU_RUNTIME})

add_subdirectory(examples)