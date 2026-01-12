cmake_minimum_required(VERSION 3.28)
project(realTimeFrameDisplay)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.0
        GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(spdlog)

add_executable(realTimeFrameDisplay main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/
        cpp/FrameGenerator.cpp
        include/FrameGenerator.h
        cpp/VulkanGraphics.cpp
        include/VulkanGraphics.h
        include/Util.h
        include/RenderWindow.h
        cpp/RenderWindow.cpp
        cpp/FrameHandler.cpp
        include/FrameHandler.h

)

target_include_directories(realTimeFrameDisplay PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        D:\\VulkanSDK\\1.3.283.0\\Include
        ${CMAKE_SOURCE_DIR}/externals/glm
)


target_link_libraries(realTimeFrameDisplay PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/externals/avutil.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/externals/avcodec.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/externals/avformat.lib
        spdlog::spdlog
        glfw
        D:\\VulkanSDK\\1.3.283.0\\Lib\\vulkan-1.lib
)