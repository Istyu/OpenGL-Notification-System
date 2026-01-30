#include "WinWindow.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

static void APIENTRY OpenGLDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    std::cerr << "[OpenGL] " << message << std::endl;
}

WinWindow::WinWindow(int width, int height, const std::string& title)
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwMakeContextCurrent(m_Window);

    //gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return;
    }

    // DEBUG INIT
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OpenGLDebugCallback, nullptr);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;

    glfwGetFramebufferSize(m_Window, &width, &height);
    glViewport(0, 0, width, height);
}

WinWindow::~WinWindow()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void WinWindow::OnUpdate()
{
    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

bool WinWindow::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}
