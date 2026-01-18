cmake_minimum_required(VERSION 3.28)
project(realTimeFrameDisplay)

set(CMAKE_CXX_STANDARD 17)

#add_subdirectory(shared)

include(FetchContent)
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(glfw)
find_package(common REQUIRED)

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
        cpp/FrameGeneratorTwo.cpp
        include/FrameGeneratorTwo.h
        include/computes/VulkanYuvToRgba.h
        cpp/computes/VulkanYuvToRgba.cpp
        include/computes/VulkanFilterR8Image.h
        cpp/computes/VulkanFilterR8Image.cpp
        cpp/computes/TemporalHisotryTwoImg.cpp
        include/computes/TemporalHistoryTwoImg.h
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
        ole32
        uuid
        common::common
        common::common2
)

function(copyDLL target src)
    add_custom_command(TARGET ${target}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${src}>
            $<TARGET_FILE_DIR:${target}>
    )
endfunction()
copyDLL(realTimeFrameDisplay common::common)
copyDLL(realTimeFrameDisplay common::common2)