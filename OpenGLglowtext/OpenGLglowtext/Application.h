#pragma once
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <deque>
#include <map>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <random>

enum class TextAlignX
{
    Left,
    Center,
    Right
};
enum class TextAlignY
{
    Bottom,
    Center,
    Top
};

struct Character
{
    GLuint TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    GLuint Advance;
};

struct PulseTextFX
{
    std::string text;
    float birthTime;
    float letterDelay = 0.08f;
    float pulseSpeed = 6.0f;
    float holdTime = 2.0f;
    float decayDuration = 1.0f;
    int lastPlayedIndex = -1;
    bool playedDecaySound = false;
    bool active = false;
};

enum class SplashPhase
{
    In,
    Hold,
    Out,
    Finished
};

struct SplashAnim {
    double duration = 0.15;
    double startTime = 0.0;
    double holdTime    = 2.0;
    bool active = false;
    SplashPhase phase = SplashPhase::In;
    void Start()
    {
        active = true;
        phase = SplashPhase::In;
        startTime = glfwGetTime();
    }
    bool WantsToFinish() const
    {
        return phase == SplashPhase::Hold &&
            (glfwGetTime() - startTime) >= holdTime;
    }
    double EaseOut(double t)
    {return 1.0 - pow(1.0 - t, 3.0);}
    double GetValue(double from, double to) {
        double now = glfwGetTime();
        double elapsed = now - startTime;
        switch (phase)
        {
        case SplashPhase::In:
        {
            double t = std::min(elapsed / duration, to);
            if (t >= to)
            {
                phase = SplashPhase::Hold;
                startTime = now;
            }
            return from + EaseOut(t) * (to - from);
        }
        case SplashPhase::Hold:
        {
            if (elapsed >= holdTime)
            {
                    phase = SplashPhase::Out;
                    startTime = now;
            }
            return to;
        }
        case SplashPhase::Out:
        {
            double t = std::min(elapsed / duration, to);
            if (t >= to)
            {
                phase = SplashPhase::Finished;
                active = false;
            }
            return to + EaseOut(t) * (from - to);
        }
        default:
            return 0.0;
        }
    }

    void Update() {
        if (!active) return;
        double elapsed = glfwGetTime() - startTime;
        if (elapsed >= (phase == SplashPhase::Hold ? holdTime : duration)) {
            if (phase == SplashPhase::In) {
                phase = SplashPhase::Hold;
                startTime = glfwGetTime();
            } else if (phase == SplashPhase::Hold) {
                phase = SplashPhase::Out;
                startTime = glfwGetTime();
            } else if (phase == SplashPhase::Out) {
                phase = SplashPhase::Finished;
                active = false;
            }
        }
    }

    double GetProgress() {
        double elapsed = glfwGetTime() - startTime;
        return std::min(elapsed / duration, 1.0);
    }

    double GetAlphaMW2() {
        double t = GetProgress();
        if (phase == SplashPhase::In)   return std::min(t * 2.0, 1.0);
        if (phase == SplashPhase::Hold) return 1.0;
        if (phase == SplashPhase::Out)  return 1.0 - t;
        return 0.0;
    }

    double GetSlideMW2(double offset) {
        double t = EaseOut(GetProgress());
        if (phase == SplashPhase::In)   return -offset + (t * offset);
        if (phase == SplashPhase::Hold) return 0.0;
        if (phase == SplashPhase::Out)  return t * offset;
        return 0.0;
    }
    bool IsFinished() const
    {return phase == SplashPhase::Finished;}
    bool IsEnding() const
    {return phase == SplashPhase::Out;}
};

struct PulseFX
{
    bool active = false;
    double birthTime = 0.0;
    int letterTimeMs = 100;
    int decayStartMs = 2000;
    int decayDurationMs = 1000;
};

struct NotifyData
{
    std::string text;
    std::string description;
    GLuint      icon;
    glm::vec3   color;
    std::string type;
};

enum class NotifyState
{
    None,
    Splash,
    Pulse
};

class Application
{
public:
    Application();
    ~Application();

    void Run();

private:
    bool m_Running = true;

    SplashAnim m_Splash;
    NotifyState m_NotifyState = NotifyState::None;

    Shader* s_BlurShader;

    GLuint m_FBO;
    GLuint m_FBOTexture;
    int m_Width, m_Height;

    GLuint m_QuadVAO, m_QuadVBO;

    std::string m_SplashText;
    std::string m_SplashDesc;
    GLuint      m_SplashIcon;
    glm::vec3   m_SplashColor = glm::vec3(1.0f);
    std::string m_SplashType;

    std::map<std::string, GLuint> m_Textures;
    bool m_DoingNotify = false;
    std::deque<NotifyData> m_NotifyQueue;

    void InitFBO(int width, int height);
    void InitScreenQuad();
    void RenderScreenQuad();
    void SplashNotify(const std::string& text, const std::string& desc, GLuint icon, float textScale, double scale, double alpha, double x, const glm::vec3& color);
    void glowPulse(const std::string& text, float textScale);
    void StartNotify(const NotifyData& data);
    void NotifyMessage(const NotifyData& data);
    void RenderPulseText(PulseTextFX& fx, float baseX, float baseY);
    //void textPulse(const std::string& text, float x, float y, float textScale, TextAlignX alignX, TextAlignY alignY, float alpha);
    void StartPulseText(PulseTextFX& fx, const std::string& text);
    char GetStableRandomChar(int index, int seed);
    void DrawPulseTextLayers(PulseTextFX& fx, float baseX, float baseY, bool isFboPass, bool endFX);
    void RenderColoredText(const std::string& text, float x, float y, float scale, float alpha);
    float GetTextWidth(const std::string& text, float scale);
    void RenderIcon(GLuint textureID, float x, float y, float w, float h, float alpha);
    void playNotifySound(const char* pFilePath);
};