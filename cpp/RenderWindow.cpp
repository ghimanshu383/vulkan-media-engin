//
// Created by ghima on 08-01-2026.
//
#include "RenderWindow.h"
#include "Util.h"

namespace fd {
    RenderWindow::RenderWindow() {
        init();
    }

    void RenderWindow::init() {
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize glfw ");
            std::exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Render Window", nullptr, nullptr);
        if (m_window == nullptr) {
            LOG_ERROR("Unable to create the window");
            std::exit(EXIT_FAILURE);
        }
        m_graphics = new VulkanGraphics(m_window);
    }

    void RenderWindow::render() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
            m_graphics->render();
        }
        delete m_graphics;
    }
}