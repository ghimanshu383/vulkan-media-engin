//
// Created by ghima on 08-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_RENDERWINDOW_H
#define REALTIMEFRAMEDISPLAY_RENDERWINDOW_H

#include <glfw/glfw3.h>
#include "VulkanGraphics.h"

namespace fd {
    class RenderWindow {
        GLFWwindow *m_window = nullptr;
        VulkanGraphics *m_graphics = nullptr;

        void init();

    public:
        RenderWindow();

        void render();
    };
}
#endif //REALTIMEFRAMEDISPLAY_RENDERWINDOW_H
