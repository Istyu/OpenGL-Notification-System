#pragma once
#include <string>

struct GLFWwindow;

class WinWindow
{
public:
    WinWindow(int width, int height, const std::string& title);
    ~WinWindow();

    void OnUpdate();
    bool ShouldClose() const;

    GLFWwindow* GetNativeWindow() const { return m_Window; }

private:
    GLFWwindow* m_Window;
};
