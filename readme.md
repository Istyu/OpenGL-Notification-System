# OpenGL Glow Text & Splash Notification System

This project is an **animated notification (splash) system** built on OpenGL 4.6 and specifically designed for games or realtime graphics applications.

The goal of the system is to implement a **Modern Warfare 2 style UI notification solution** that supports:
- glow text
- icon + title + description layout
- multiple animation behaviors (splash / killstreak)
- time-based animations and sound effects

---

## Main features

- 🎯 Centered main text
- 🖼 Show icon above text
- 📝 Show description below text
- ✨ Glow + blur effect using framebuffer
- 📐 Dynamic scaling (`scale`)
- 🎞 Multi-phase animation (In / Hold / Out)
- 🔊 Play sound on notification
- 🔄 Notification queue support

---

## Libraries/dependencies used

- GLFW – window management, input
- GLAD – OpenGL loader
- GLM – math utility functions
- FreeType 2.9 – font rendering
- stb_image – texture loading
- miniaudio – sound playback

---

## Technical basics

- **OpenGL version:** 4.6
- **Rendering:** Immediate-mode style quad rendering
- **Text rendering:** FreeType based glyph textures
- **Glow:** Offscreen FBO + blur shader

---

## Basic concept

The splash is built on three main elements:

1. **Text (title)** – central element, with animated position
2. **Icon** – appears above the text
3. **Description** – appears below the text

During a splash animation:
- the elements initially **start from one point**
- during the In phase they **slide apart to their final position**
- during the Out phase they scale and fade

During the killstreak animation:
- the elements initially **start from the left edge of the screen**
- during the In phase **the X axis approaches zero, while alpha increases from 0 to 1**
- during the Out phase they move towards the right edge of the screen and fade out

---

## Animation logic

The animation is based on `alpha` `scale` and `x`slide values:

- `alpha` → transition (0.0 → 1.0)
- `scale` → scaling (In / Out phase)
- `x` → slide (-640.0 → 0 → 640.0)

---

## Runtime requirements

The program loads shaders, textures, fonts and sound files from **relative paths**, so when running it is **required** that the following folders and files are available in addition to the executable file (`.exe`):
```
OpenGLglowtext/
└── x64/
    └── Debug/
        ├── assets/
        │   ├── bank-gothic-medium-bt.ttf
        │   ├── Carbon-Bold.ttf
        │   ├── compass_objpoint_satallite.png
        │   ├── Conduit-ITC-Std-Font.otf
        │   ├── crosshair_red.png
        │   ├── mp_killstrk_radar.wav
        │   ├── mp_last_stand.wav
        │   ├── MS Reference Sans Serif Bold.ttf
        │   ├── Old_R.ttf
        │   ├── ui_computer_text_blip1x.wav
        │   ├── ui_computer_text_delete1.wav
        │   └── VCR_OSD_MONO_1.001.ttf
        ├── shaders/
        │   ├── blur.frag
        │   ├── screen.vert
        │   ├── text.frag
        │   └── text.vert
        └── freetype.dll
```
---

## Media

![](https://github.com/Istyu/OpenGL-Notification-System/media/MW2_Notification_System.gif)
![](https://github.com/Istyu/OpenGL-Notification-System/media/killstreak.png)
![](https://github.com/Istyu/OpenGL-Notification-System/media/splash.png)

---

## Clone this repo

```bash
git clone --recurse-submodules https://github.com/Istyu/OpenGL-Notification-System.git
git submodule update --init --recursive
```
