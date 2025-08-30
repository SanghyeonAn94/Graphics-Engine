# CMakeLists.txt - Modern CMake 3.28+ with C++23 Modules
cmake_minimum_required(VERSION 3.28)

# 프로젝트 정의
project(GraphicsEngine
    VERSION 1.0.0
    DESCRIPTION "Next-generation graphics engine with C++23 modules"
    LANGUAGES CXX
)

# C++23 표준 설정
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 모듈 지원 활성화
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API ON)
    set(CMAKE_CXX_MODULES ON)
endif()

# 컴파일러별 설정
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "17.0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules-ts")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.37")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /experimental:module")
    endif()
endif()

# 빌드 타입별 플래그 설정
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(ENGINE_DEBUG=1)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Wall -Wextra -Wpedantic")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /DNDEBUG")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    endif()
endif()

# 엔진 핵심 라이브러리 정의
add_library(EngineCore)

# 모듈 파일들 추가
if(CMAKE_CXX_MODULES)
    target_sources(EngineCore
        PUBLIC
            FILE_SET CXX_MODULES FILES
                Engine/Core/Modules/Core.cppm
                Engine/Core/Modules/TypeRegistry.cppm
    )
else()
    # 모듈을 지원하지 않는 경우를 위한 fallback
    message(WARNING "C++ modules not supported, using header-only implementation")
    
    # 헤더로 변환된 파일들 (필요시 생성)
    target_sources(EngineCore
        PUBLIC
            Engine/Core/Core.hpp
            Engine/Core/TypeRegistry.hpp
    )
endif()

# 포함 디렉토리 설정
target_include_directories(EngineCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 컴파일러별 추가 설정
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(EngineCore PRIVATE
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(EngineCore PRIVATE
        -Wno-padded
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(EngineCore PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
    )
endif()

# 테스트 실행 파일
add_executable(EngineTests)

target_sources(EngineTests
    PRIVATE
        Tests/ModuleTests.cpp
)

target_link_libraries(EngineTests 
    PRIVATE 
        EngineCore
)

# 예제 실행 파일
add_executable(BasicExample)

target_sources(BasicExample
    PRIVATE
        Examples/BasicExample.cpp
)

target_link_libraries(BasicExample 
    PRIVATE 
        EngineCore
)

# 패키지 찾기 (향후 확장을 위해)
# find_package(Vulkan QUIET)
# find_package(glfw3 QUIET)
# find_package(glm QUIET)

# if(Vulkan_FOUND)
#     target_link_libraries(EngineCore PUBLIC Vulkan::Vulkan)
#     add_compile_definitions(ENGINE_VULKAN_SUPPORT=1)
# endif()

# 설치 설정
install(TARGETS EngineCore EngineTests BasicExample
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 모듈 파일들도 설치
if(CMAKE_CXX_MODULES)
    install(FILES
        Engine/Core/Modules/Core.cppm
        Engine/Core/Modules/TypeRegistry.cppm
        DESTINATION include/Engine/Core/Modules
    )
endif()

# 개발용 유틸리티 타겟들
add_custom_target(format
    COMMAND find Engine Tests Examples -name "*.cpp" -o -name "*.hpp" -o -name "*.cppm" | xargs clang-format -i
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting source code"
)

add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMENT "Clean build directory"
)

# 정보 출력
message(STATUS "GraphicsEngine Configuration:")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Modules: ${CMAKE_CXX_MODULES}")
if(CMAKE_CXX_MODULES)
    message(STATUS "  Module API: ${CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API}")
endif()