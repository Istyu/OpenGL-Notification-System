#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "Application.h"
#include "WinWindow.h"
#include "Shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

ma_engine engine;

static std::map<char, Character> s_Characters;
static GLuint s_VAO, s_VBO;
static Shader* s_TextShader;
static Shader* s_BlurShader;
static WinWindow* s_Window;
static glm::mat4 s_Projection;

std::string curFontType;

struct DecayEntry
{
    int index;
    float startTime;
};

std::vector<DecayEntry> decayOrder;


std::string GetExecutableDirectory()
{
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetExecutableDirectory, &hModule);

    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, MAX_PATH);

    std::string fullPath(path);
    return fullPath.substr(0, fullPath.find_last_of("\\/"));
}
std::string g_AssetRoot = GetExecutableDirectory() + "/assets";

std::string AssetPath(const std::string& relative)
{
    return g_AssetRoot + "/" + relative;
}

static glm::vec2 MeasureText(const std::string& text, float scale)
{
    float width = 0.0f;
    float maxHeight = 0.0f;

    for (char c : text)
    {
        const Character& ch = s_Characters[c];
        width += (ch.Advance >> 6) * scale;
        maxHeight = std::max(maxHeight, ch.Size.y * scale);
    }
    return { width, maxHeight };
}

static void RenderText(Shader* shader, const std::string& text, float x, float y, float scale, TextAlignX alignX, TextAlignY alignY, float padding = 0.0f)
{
    glm::vec2 size = MeasureText(text, scale);

    // X alignment
    if (alignX == TextAlignX::Center)
        x -= size.x * 0.5f;
    else if (alignX == TextAlignX::Right)
        x -= size.x;

    // Y alignment
    if (alignY == TextAlignY::Center)
        y -= size.y * 0.5f;
    else if (alignY == TextAlignY::Top)
        y -= size.y;

    shader->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(s_VAO);

    for (char c : text)
    {
        Character ch = s_Characters[c];

        if (ch.TextureID == 0) // space / empty glyph
        {
            x += ch.Advance * scale;
            continue;
        }
        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Adding padding to geometry
        float p = padding * scale;
        float x_pad = xpos - p;
        float y_pad = ypos - p;
        float w_pad = w + p * 2.0f;
        float h_pad = h + p * 2.0f;

        // Correcting UV coordinates for padding
        float u_pad = (ch.Size.x > 0) ? padding / (float)ch.Size.x : 0.0f;
        float v_pad = (ch.Size.y > 0) ? padding / (float)ch.Size.y : 0.0f;
        float vertices[6][4] = {
            { x_pad,         y_pad + h_pad,   -u_pad,         -v_pad },
            { x_pad,         y_pad,           -u_pad,          1.0f + v_pad },
            { x_pad + w_pad, y_pad,            1.0f + u_pad,   1.0f + v_pad },

            { x_pad,         y_pad + h_pad,   -u_pad,         -v_pad },
            { x_pad + w_pad, y_pad,            1.0f + u_pad,   1.0f + v_pad },
            { x_pad + w_pad, y_pad + h_pad,    1.0f + u_pad,  -v_pad }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glNamedBufferSubData(s_VBO, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Application::InitFBO(int width, int height) {
    m_Width = width;
    m_Height = height;

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glGenTextures(1, &m_FBOTexture);
    glBindTexture(GL_TEXTURE_2D, m_FBOTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attaching a texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_FBOTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Return to normal screen
}

void Application::InitScreenQuad() {
    float quadVertices[] = { 
        // Positions   // TexCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
    };
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void Application::RenderScreenQuad() {
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void LoadFont(FT_Library ft, const std::string& path, int pixelSize)
{
    FT_Face face;

    FT_New_Face(ft, path.c_str(), 0, &face);
    FT_Set_Pixel_Sizes(face, 0, pixelSize); // face, pixel_width, pixel_height

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 32; c < 128; c++)
    {
        FT_Load_Char(face, c, FT_LOAD_RENDER);
        int w = face->glyph->bitmap.width;
        int h = face->glyph->bitmap.rows;

        if (w == 0 || h == 0)
        {
        	// space or empty glyph
            s_Characters[c] = {
                0,
                { 0, 0 },
                { face->glyph->bitmap_left, face->glyph->bitmap_top },
                (GLuint)(face->glyph->advance.x >> 6)
            };
            continue;
        }

        GLuint tex;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 1, GL_R8,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows);

        glTextureSubImage2D(
            tex, 0, 0, 0,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);

        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // Set the "border" (the part outside the texture) to be completely transparent
        float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glTextureParameterfv(tex, GL_TEXTURE_BORDER_COLOR, borderColor);

        s_Characters[c] = {
            tex,
            { face->glyph->bitmap.width, face->glyph->bitmap.rows },
            { face->glyph->bitmap_left, face->glyph->bitmap_top },
            (GLuint)face->glyph->advance.x
        };
    }
    FT_Done_Face(face);
}

void SetFont(const std::string name)
{
    if( curFontType == name )
        return;
    curFontType = name;
    FT_Library ft;
    FT_Init_FreeType(&ft);

    if( name == "bold" )
    {
        LoadFont(ft, AssetPath("MS Reference Sans Serif Bold.ttf"), 48);
    }
    else if( name == "extrabig" )
    {
        LoadFont(ft, AssetPath("bank-gothic-medium-bt.ttf"), 48);
    }
    else if( name == "objective" )
    {
        LoadFont(ft, AssetPath("Carbon-Bold.ttf"), 48);
    }
    else if( name == "default" )
    {
        LoadFont(ft, AssetPath("Conduit-ITC-Std-Font.otf"), 48);
    }
    FT_Done_FreeType(ft);

    //std::cout << "Font type changed" << std::endl; // DEV
}

float Application::GetTextWidth(const std::string& text, float scale)
{
    float width = 0.0f;
    for (size_t i = 0; i < text.length(); ++i)
    {
    	// Skip the color codes when measuring
        if (text[i] == '^' && i + 1 < text.length()) {
            i++; 
            continue;
        }
        const Character& ch = s_Characters[text[i]];
        width += (ch.Advance >> 6) * scale;
    }
    return width;
}

void Application::RenderColoredText(const std::string& text, float x, float y, float scale, float alpha)
{
    glm::vec3 currentColor(1.0f);
    s_TextShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(s_VAO);

    for (size_t i = 0; i < text.length(); ++i)
    {
    	// Color codes
        if (text[i] == '^' && i + 1 < text.length())
        {
            char code = text[++i];
            if(code == '1') currentColor = {1.0f, 0.2f, 0.2f}; // Red
            else if(code == '2') currentColor = {0.2f, 1.0f, 0.2f}; // Green
            else if(code == '3') currentColor = {1.0f, 1.0f, 0.0f}; // Yellow
            else if(code == '4') currentColor = {0.0f, 0.0f, 1.0f}; // Blue
            else if(code == '5') currentColor = {0.0f, 1.0f, 1.0f}; // Cyan
            else if(code == '6') currentColor = {0.8f, 0.2f, 0.5f}; // pink/Magenta
            else if(code == '7') currentColor = {1.0f, 1.0f, 1.0f}; // White
            else if(code == '0') currentColor = {0.0f, 0.0f, 0.0f}; // Black
            continue;
        }
        const Character& ch = s_Characters[text[i]];
        if (ch.TextureID == 0) // space / empty glyph
        {
            x += ch.Advance * scale;
            continue;
        }
        s_TextShader->SetVec3("u_Color", currentColor * alpha);

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glNamedBufferSubData(s_VBO, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Application::Application()
{
    int width = 1280;
    int height = 720;
    s_Window = new WinWindow(width, height, "Text Glow");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    InitFBO(width, height);
    InitScreenQuad();
    s_BlurShader = new Shader("shaders/screen.vert", "shaders/blur.frag");
    s_TextShader = new Shader("shaders/text.vert", "shaders/text.frag");

    s_Projection = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f);
    s_TextShader->Bind();
    s_TextShader->SetMat4("u_Projection", s_Projection);

    glCreateVertexArrays(1, &s_VAO);
    glCreateBuffers(1, &s_VBO);
    glNamedBufferData(s_VBO, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    glVertexArrayVertexBuffer(s_VAO, 0, s_VBO, 0, sizeof(float) * 4);
    glEnableVertexArrayAttrib(s_VAO, 0);
    glVertexArrayAttribFormat(s_VAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(s_VAO, 0, 0);

    ma_engine_init(NULL, &engine);

    // ===== FreeType =====
    SetFont("objective");
}

Application::~Application()
{
    ma_engine_uninit(&engine);
    delete s_TextShader;
    delete s_BlurShader;
    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_FBOTexture);
    glDeleteVertexArrays(1, &m_QuadVAO);
    glDeleteBuffers(1, &m_QuadVBO);
    delete s_Window;
}

void Application::StartPulseText(PulseTextFX& fx, const std::string& text)
{
    fx.text = text;
    fx.birthTime = (float)glfwGetTime();
    fx.letterDelay = 0.08f;
    fx.holdTime = 2.0f;
    fx.decayDuration = 1.0f;
    fx.pulseSpeed = 6.0f;
    fx.active = true;

    float decayStepTime = 0.12f; // 0.12f = MW2 ~100–150ms
    int   decayBatchMin = 2;
    int   decayBatchMax = 3;

    decayOrder.clear();

    std::vector<int> indices(fx.text.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng((uint32_t)(fx.birthTime * 1000));
    std::shuffle(indices.begin(), indices.end(), rng);

    float t = 0.0f;
    size_t i = 0;

    while (i < indices.size())
    {
        int batch = rng() % (decayBatchMax - decayBatchMin + 1) + decayBatchMin;
    
        for (int b = 0; b < batch && i < indices.size(); b++, i++)
        {
            decayOrder.push_back({
                indices[i],
                t + ((rng() % 30) / 1000.0f) // 0–30ms jitter
            });
        }
        t += decayStepTime;
    }
}

char Application::GetStableRandomChar(int index, int seed)
{
    unsigned int s = seed ^ (index * 1664525u);
    s = (s ^ (s >> 16)) * 2246822519u;
    return 'A' + (s % 26);
}

void Application::DrawPulseTextLayers(PulseTextFX& fx, float baseX, float baseY, bool isFboPass, bool endFX)
{
    float elapsed = (float)glfwGetTime() - fx.birthTime;
    float textScale = 1.0f;
    glm::vec2 totalSize = MeasureText(fx.text, textScale);
    float correctedBaseY = baseY - (totalSize.y * 0.5f);
    float x = baseX;
    float flickerSpeed = 40.0f;
    int currentLeadIndex = (int)(elapsed / fx.letterDelay);
    float decayStart = (fx.text.size() * fx.letterDelay) + fx.holdTime;

    if (currentLeadIndex > fx.lastPlayedIndex && currentLeadIndex < (int)fx.text.size())
    {
    	// Don't make a sound when using a space.
        if (fx.text[currentLeadIndex] != ' ')
        {
            ma_engine_play_sound(&engine, AssetPath("ui_computer_text_blip1x.wav").c_str(), NULL);
        }

        // Updating that this index has already been "handled"
        fx.lastPlayedIndex = currentLeadIndex;
    }
    if (elapsed >= decayStart && !fx.playedDecaySound)
    {
        ma_engine_play_sound(&engine, AssetPath("ui_computer_text_delete1.wav").c_str(), NULL);
        fx.playedDecaySound = true; // Mark it as done, it won't play again in this cycle
    }

    s_TextShader->Bind();

    for (size_t i = 0; i < fx.text.size(); i++)
    {
        float appearTime = i * fx.letterDelay;
        if (elapsed < appearTime) break;

        //float localT = elapsed - appearTime;

        char drawChar;
        float alpha = 1.0f;
        bool decaying = elapsed > decayStart;

        if (!decaying) {
            if (i == currentLeadIndex) {
                alpha = std::min((elapsed - appearTime) / fx.letterDelay, 1.0f);
                drawChar = GetStableRandomChar((int)i, (int)(elapsed * flickerSpeed) + (int)i);
            }
            else
            {
                drawChar = fx.text[i];
            }
        }
        else
        {
            float totalDecayTime = elapsed - decayStart;
            float progress = totalDecayTime / fx.decayDuration;
            bool isRemoved = false;
            bool isDecayingNow = false;
            float localT;
            int flickerSeed = (int)(elapsed * flickerSpeed) + (int)i + 789;

            for (const auto& d : decayOrder)
            {
                if (d.index == (int)i)
                {
                    localT = totalDecayTime - d.startTime;
            
                    if (localT >= fx.decayDuration * 0.3f)
                        isRemoved = true;
                    else if (localT > 0.0f)
                        isDecayingNow = true;
                    break;
                }
            }

            if (isRemoved)
            {
                if (s_Characters[fx.text[i]].TextureID == 0) // space / empty glyph
                {
                    x += s_Characters[fx.text[i]].Advance * textScale;
                    continue;
                }
                x += (s_Characters[fx.text[i]].Advance >> 6) * textScale;
                continue;
            }

            if (isDecayingNow)
            {
                float fade = 1.0f - (localT / (fx.decayDuration * 0.3f));
                alpha = fade;
                drawChar = GetStableRandomChar((int)i, flickerSeed);
            }
            else
            {
                drawChar = fx.text[i];
                alpha = 1.0f;
            }
        }

        //float pulse = sin(localT * fx.pulseSpeed) * 5.0f;

        s_TextShader->SetVec3("u_Color", glm::vec3(1.0f) * alpha);

        std::string s(1, drawChar);
        RenderText(s_TextShader, s, x, correctedBaseY /*+ pulse*/, textScale, TextAlignX::Left, TextAlignY::Bottom);

        if (s_Characters[fx.text[i]].TextureID == 0) // space / empty glyph
        {
            x += s_Characters[fx.text[i]].Advance * textScale;
            continue;
        }

        x += (s_Characters[fx.text[i]].Advance >> 6) * textScale;
    }
    if( endFX )
    {
        if (elapsed > decayStart + fx.decayDuration)
            fx.active = false;
    }
}

void Application::RenderPulseText(PulseTextFX& fx, float baseX, float baseY)
{
    if (!fx.active) return;
    SetFont("objective");

    // 1. DRAWING ON FBO (FOR GLOW)
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
    glClear(GL_COLOR_BUFFER_BIT); // Only delete once at the beginning of the FBO!

    // First, draw all visible characters on the FBO
    DrawPulseTextLayers(fx, baseX, baseY, true, false);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. SCREEN CLEARANCE AND GLOW DRAWING
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Additive blending for shine
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    s_BlurShader->Bind();
    s_BlurShader->SetVec3("u_GlowColor", { 0.25f, 0.75f, 0.25f });
    s_BlurShader->SetFloat("u_BlurRadius", 6.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_FBOTexture);
    RenderScreenQuad();

    // 3. DRAWING SHARP TEXT
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawPulseTextLayers(fx, baseX, baseY, false, true);
}

GLuint LoadTexture(const char* path)
{
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);

    if (data)
    {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void Application::RenderIcon(GLuint textureID, float x, float y, float w, float h, float alpha)
{
    s_TextShader->Bind();
    s_TextShader->SetInt("u_IsIcon", 1); // Turn on icon mode
    s_TextShader->SetVec3("u_Color", glm::vec3(1.0f) * (float)alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    float vertices[6][4] = {
        { x,     y + h,   0.0f, 1.0f },
        { x,     y,       0.0f, 0.0f },
        { x + w, y,       1.0f, 0.0f },

        { x,     y + h,   0.0f, 1.0f },
        { x + w, y,       1.0f, 0.0f },
        { x + w, y + h,   1.0f, 1.0f }
    };

    glBindVertexArray(s_VAO); // Use existing VAO
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    s_TextShader->SetInt("u_IsIcon", 0); // Reset to text mode
}

void Application::StartNotify(const NotifyData& data)
{
    m_DoingNotify = true;
    m_SplashText  = data.text;
    m_SplashDesc = data.description;
    m_SplashIcon = data.icon;
    m_SplashColor = data.color;
    m_SplashType = data.type;

    m_Splash.Start();
    m_NotifyState = NotifyState::Splash;
}

void Application::NotifyMessage(const NotifyData& data)
{
    if (!m_DoingNotify || m_Splash.IsFinished())
    {
        StartNotify(data);
        return;
    }
    m_NotifyQueue.push_back(data);
}

int soundIter = 0;
void Application::playNotifySound(const char* pFilePath)
{
    if( soundIter == 2 && m_Splash.phase == SplashPhase::In )
    {
        ma_engine_play_sound(&engine, pFilePath, NULL);
    }
    soundIter++;
}

void Application::SplashNotify(const std::string& text, const std::string& desc, GLuint icon, float textScale, double scale, double alpha, double x, const glm::vec3& color)
{
    float centerX = m_Width * 0.5f;
    float centerY = m_Height * 0.5f;
    float yOffset;
    float xOffset = 0.0f;
    float descScale = 0.0f;
    float startYOffset = 260.0f;
    float endYOffset = 180.0f;
    float t = glm::clamp((float)alpha, 0.0f, 1.0f);
    float descY = 0.0f;
    float iconY = 0.0f;
    glm::vec2 mainSize = MeasureText(text, textScale);
    float textCenterY = centerY + endYOffset;
    float iconCenterY = textCenterY + mainSize.y * 3.3f;
    float descCenterY = textCenterY - mainSize.y * 2.5f;

    float startY = centerY + startYOffset;

    float iconDrawY = 0.0f;
    float iconSize = 0.0f;
    float iconX = 0.0f;

    float textY = 0.0f;

    if (icon != 0)
        iconSize = 140.0f * textScale * (float)scale;

    if( m_SplashType == "killstreak" )
    {
        SetFont("extrabig");
        yOffset = 180.0f;
        float spacing = mainSize.y * 2.7f;
        descY = (centerY + yOffset) - spacing;
        xOffset = (float)x;
        textY = centerY + yOffset;
        iconY = (centerY + yOffset) + 20.0f;
        iconDrawY = iconY;
        descScale = 0.375f * (float)scale;
        playNotifySound( AssetPath("mp_killstrk_radar.wav").c_str() );
    }
    else if( m_SplashType == "splash" )
    {
        SetFont("bold");
        descY = glm::mix(startY, descCenterY, t);
        yOffset = glm::mix(startYOffset, endYOffset, t);
        textY = glm::mix(startY, textCenterY, t);
        iconY = glm::mix(startY, iconCenterY, t);
        iconDrawY = iconY - iconSize * 0.5f;
        textScale = textScale * (float)scale;
        descScale = 0.375f * (float)scale;
        playNotifySound( AssetPath("mp_last_stand.wav").c_str() );
    }

    if (icon != 0)
        iconX = (centerX + xOffset) - (iconSize * 0.5f);

    // -- 1. Rendering the "Mask" into FBO --
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    s_TextShader->Bind();
    s_TextShader->SetVec3("u_Color", glm::vec3(1.0f));
    RenderText(s_TextShader, text, centerX + xOffset, textY, (float)textScale, TextAlignX::Center, TextAlignY::Center, 0.0f);

    // -- 2. Rendering Glow to the screen (from the FBO texture) --
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    s_BlurShader->Bind();
    s_BlurShader->SetVec2("u_Resolution", { (float)m_Width, (float)m_Height });

    glm::vec3 glowColor = color * (float)alpha;
    s_BlurShader->SetVec3("u_GlowColor", glowColor);
    s_BlurShader->SetFloat("u_BlurRadius", 5.0f * (float)alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_FBOTexture);
    s_BlurShader->SetInt("u_ScreenTexture", 0);

    RenderScreenQuad();

    // -- 3. Drawing the sharp text on top of the glow --
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_TextShader->Bind();
    s_TextShader->SetVec3("u_Color", glm::vec3(1.0f) * (float)alpha);
    RenderText(s_TextShader, text, centerX + xOffset, textY, (float)textScale, TextAlignX::Center, TextAlignY::Center, 0.0f);


    if (icon != 0)
    {RenderIcon(icon, iconX + 7.0f, iconDrawY, iconSize, iconSize, (float)alpha);}


    SetFont("default");
    float totalWidth = GetTextWidth(desc, descScale);
    float startX = (centerX + xOffset) - (totalWidth * 0.5f);
    RenderColoredText(desc, startX, descY, descScale, (float)alpha);
}

void Application::glowPulse(const std::string& text, float textScale)
{
    SetFont("extrabig");

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    s_TextShader->Bind();
    s_TextShader->SetVec3("u_Color", {1.0f, 1.0f, 1.0f});
    RenderText(s_TextShader, text, 400, 360, textScale, TextAlignX::Center, TextAlignY::Center, 0.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    s_BlurShader->Bind();
    s_BlurShader->SetVec2("u_Resolution", { (float)m_Width, (float)m_Height });
    s_BlurShader->SetVec3("u_GlowColor", { 0.25f, 0.75f, 0.25f });
    float pulse = 4.0f + sin((float)glfwGetTime() * 3.0f) * 1.5f;
    s_BlurShader->SetFloat("u_BlurRadius", pulse);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_FBOTexture);
    s_BlurShader->SetInt("u_ScreenTexture", 0);

    RenderScreenQuad();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_TextShader->Bind();
    s_TextShader->SetVec3("u_Color", { 1.0f, 1.0f, 1.0f });
    RenderText(s_TextShader, text, 400, 360, textScale, TextAlignX::Center, TextAlignY::Center, 0.0f);
}
PulseTextFX g_PulseTextFX;

void Application::Run()
{
    std::filesystem::path p = g_AssetRoot;
    bool filepathExists = std::filesystem::is_directory(p.parent_path());
    if( !filepathExists )
    {
        MessageBoxA(
            nullptr,
            "The assets folder is missing!\n\n"
            "Check if the 'assets' directory exists next to the exe.",
            "Error",
            MB_ICONERROR | MB_OK
        );
        return;
    }
    const std::string& text = "Eliminate enemy players.";
    float textScale = 1.0f;
    bool pulseActive = false;

    m_Textures["uav_icon"] = LoadTexture( AssetPath("compass_objpoint_satallite.png").c_str() );
    m_Textures["splash_icon"] = LoadTexture( AssetPath("crosshair_red.png").c_str() );

    // Set the resolution for the shaders (only needed once)
    s_BlurShader->Bind();
    s_BlurShader->SetVec2("u_Resolution", { (float)m_Width, (float)m_Height });

    while (m_Running && !s_Window->ShouldClose())
    {
        if( !m_Splash.active )
            soundIter = 0;

        static bool enterWasDown = false;
        bool enterIsDown = glfwGetKey(s_Window->GetNativeWindow(), GLFW_KEY_ENTER) == GLFW_PRESS;
        if (enterIsDown && !enterWasDown)
        {
            if( g_PulseTextFX.active )
                std::cout << "Wait until pulsefxtext finishes" << std::endl;
            else if( pulseActive )
                std::cout << "Wait until pulsetext finishes" << std::endl;
            else
            {
                NotifyMessage({
                    "First Blood!",
                    "You got the first kill. (^3+100^7)",
                    m_Textures["splash_icon"],
                    {0.75f, 0.25f, 0.25f}, 
                    "splash"
                });
            }
        }
        enterWasDown = enterIsDown;

        static bool dWasDown = false;
        bool dIsDown = glfwGetKey(s_Window->GetNativeWindow(), GLFW_KEY_D) == GLFW_PRESS;
        if (dIsDown && !dWasDown)
        {
            if( g_PulseTextFX.active )
                std::cout << "Wait until pulsefxtext finishes" << std::endl;
            else if( pulseActive )
                std::cout << "Wait until pulsetext finishes" << std::endl;
            else
            {
                NotifyMessage({
                    "3 Kill Streak!",
                    "Press 6 for UAV.",
                    m_Textures["uav_icon"],
                    {0.25f, 0.75f, 0.25f}, 
                    "killstreak"
                });
            }
        }
        dWasDown = dIsDown;

        static bool aWasDown = false;
        bool aIsDown = glfwGetKey(s_Window->GetNativeWindow(), GLFW_KEY_A) == GLFW_PRESS;

        if (aIsDown && !aWasDown)
        {
            if( g_PulseTextFX.active )
                std::cout << "Wait until pulsefxtext finishes" << std::endl;
            else if( m_Splash.active )
                std::cout << "Wait for notifies to finish" << std::endl;
            else
            {
                if( !pulseActive )
                {
                    m_NotifyState = NotifyState::Pulse;
                    pulseActive = true;
                }
                else if( pulseActive )
                {
                    m_NotifyState = NotifyState::None;
                    pulseActive = false;
                }
            }
        }
        aWasDown = aIsDown;

        static bool sWasDown = false;
        bool sIsDown = glfwGetKey(s_Window->GetNativeWindow(), GLFW_KEY_S) == GLFW_PRESS;

        if (sIsDown && !sWasDown)
        {
            if( m_Splash.active )
                std::cout << "Wait for notifies to finish" << std::endl;
            else if( pulseActive )
                std::cout << "Wait until pulsetext finishes" << std::endl;
            else
            {
                StartPulseText(g_PulseTextFX, text);
            }
        }
        sWasDown = sIsDown;

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if( g_PulseTextFX.active )
        {
            RenderPulseText(g_PulseTextFX, 360.0f, 540.0f);
        }
        else
        {
            g_PulseTextFX.lastPlayedIndex = -1;
            g_PulseTextFX.playedDecaySound = false;
        }

        double alpha, x, scale = 1.0;

        switch (m_NotifyState)
        {
            case NotifyState::Splash:
            {
                if (m_SplashType == "splash" && !m_NotifyQueue.empty())
                {
                    if (m_NotifyQueue.front().type == "killstreak" && m_Splash.phase == SplashPhase::In)
                    {
                        const NotifyData& next = m_NotifyQueue.front();
                        m_SplashText  = next.text;
                        m_SplashDesc = next.description;
                        m_SplashIcon = next.icon;
                        m_SplashColor = next.color;
                        m_Splash.phase = SplashPhase::Out;
                        m_Splash.startTime = glfwGetTime();
                        break;
                    }
                }

                if (!m_Splash.active)
                {
                    m_DoingNotify = false;
                
                    if (!m_NotifyQueue.empty())
                    {
                        NotifyData next = m_NotifyQueue.front();
                        m_NotifyQueue.pop_front();
                        StartNotify(next);
                    }
                    else
                    {
                        m_NotifyState = NotifyState::None;
                    }
                    break;
                }

                if( m_Splash.WantsToFinish() )
                {
                    if( !m_NotifyQueue.empty() )
                    {
                        const NotifyData& next = m_NotifyQueue.front();
                        if( next.type == m_SplashType )
                        {
                            if( m_SplashType != "killstreak" )
                            {
                                soundIter = 0;
                                m_Splash.active = false;
                                NotifyData dataToStart = m_NotifyQueue.front();
                                m_NotifyQueue.pop_front();
                                StartNotify(dataToStart);
                                break;
                            }
                        }
                    }
                }

                if( m_SplashType == "killstreak" )
                {
                    m_Splash.Update();
                    alpha = m_Splash.GetAlphaMW2();
                    x     = m_Splash.GetSlideMW2(640.0);
                    textScale = 0.6f;
                }
                else if( m_SplashType == "splash" )
                {
                    scale = m_Splash.GetValue(10.0, 1.0);
                    alpha = m_Splash.GetValue(0.0, 1.0);
                    x     = 0.0;
                    textScale = 0.5f;
                }
                SplashNotify(m_SplashText, m_SplashDesc, m_SplashIcon, textScale, scale, alpha, x, m_SplashColor);
                break;
            }
            case NotifyState::Pulse:
                glowPulse(text, 1.0f);
                break;
            default:
                break;
        }
        s_Window->OnUpdate();
    }
}