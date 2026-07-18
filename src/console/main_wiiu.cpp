/*
    Copyright 2019-2025 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#ifdef __WIIU__

#include <coreinit/foreground.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2r/draw.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <whb/sdcard.h>
#include <whb/gfx.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "console_ui.h"
#include "../defines.h"
#include "../settings.h"

#define MAX_DRAWS 1024
extern const uint8_t shader_wiiu_gsh[];

ScreenLayout gpLayout;
GX2Texture *gpTexture;
int tvWidth, tvHeight;
int bufOffset;
bool firstScreen;

VPADStatus vpad;
VPADTouchData touch;
bool scanned;

WHBGfxShaderGroup group;
GX2RBuffer posBuffer, texBuffer, colBuffer;
GX2Sampler samplers[2];

uint32_t ConsoleUI::defaultKeys[] {
    VPAD_BUTTON_A, VPAD_BUTTON_B, VPAD_BUTTON_MINUS, VPAD_BUTTON_PLUS,
    VPAD_BUTTON_RIGHT | VPAD_STICK_R_EMULATION_RIGHT | VPAD_STICK_L_EMULATION_RIGHT,
    VPAD_BUTTON_LEFT | VPAD_STICK_R_EMULATION_LEFT | VPAD_STICK_L_EMULATION_LEFT,
    VPAD_BUTTON_UP | VPAD_STICK_R_EMULATION_UP | VPAD_STICK_L_EMULATION_UP,
    VPAD_BUTTON_DOWN | VPAD_STICK_R_EMULATION_DOWN | VPAD_STICK_L_EMULATION_DOWN,
    VPAD_BUTTON_ZR, VPAD_BUTTON_ZL, VPAD_BUTTON_X, VPAD_BUTTON_Y,
    VPAD_BUTTON_L | VPAD_BUTTON_R
};

const char *ConsoleUI::keyNames[] {
    "Sync", "Home", "Minus", "Plus", "R", "L", "ZR", "ZL",
    "Down", "Up", "Right", "Left", "Y", "X", "B", "A",
    "TV", "R Stick", "L Stick", "", "", "", "",
    "RS Down", "RS Up", "RS Right", "RS Left",
    "LS Down", "LS Up", "LS Right", "LS Left"
};

void ConsoleUI::startFrame(uint32_t color) {
    // Convert the clear color to floats
    float r = float(color & 0xFF) / 0xFF;
    float g = float((color >> 8) & 0xFF) / 0xFF;
    float b = float((color >> 16) & 0xFF) / 0xFF;
    float a = float(color >> 24) / 0xFF;

    // Clear the TV and gamepad screens
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(r, g, b, a);
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(r, g, b, a);

}

void ConsoleUI::endFrame() {
    // Finish and display a frame
    WHBGfxFinishRenderTV();
    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();

    // Free the gamepad screen texture if it was overridden
    if (gpTexture) {
        destroyTexture(gpTexture);
        gpTexture = nullptr;
    }

    // Reset the frame status
    bufOffset = 0;
    firstScreen = true;
    scanned = false;
}

void *ConsoleUI::createTexture(uint32_t *data, int width, int height) {
    // Create a new texture with the given dimensions
    GX2Texture *texture = new GX2Texture;
    memset(texture, 0, sizeof(GX2Texture));
    texture->surface.width = width;
    texture->surface.height = height;
    texture->surface.depth = 1;
    texture->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    texture->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture->viewNumSlices = 1;
    texture->compMap = 0x03020100;
    GX2CalcSurfaceSizeAndAlignment(&texture->surface);
    GX2InitTextureRegs(texture);
    texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);

    // Copy data to the texture
    uint32_t *dst = (uint32_t*)texture->surface.image;
    for (int y = 0; y < height; y++) {
        memcpy(dst, &data[y * width], width * sizeof(uint32_t));
        dst += texture->surface.pitch;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);
    return texture;
}

void ConsoleUI::destroyTexture(void *texture) {
    // Clean up a texture
    MEMFreeToDefaultHeap(((GX2Texture*)texture)->surface.image);
    delete (GX2Texture*)texture;
}

void ConsoleUI::drawTexture(void *texture, float tx, float ty, float tw, float th,
    float x, float y, float w, float h, bool filter, int rotation, uint32_t color) {
    // Convert position values to floats for the TV
    float x1 = (x / (tvWidth / 2) - 1.0f);
    float y1 = -(y / (tvHeight / 2) - 1.0f);
    float x2 = ((x + w) / (tvWidth / 2) - 1.0f);
    float y2 = -((y + h) / (tvHeight / 2) - 1.0f);

    // Copy positions for the gamepad, but ensure lines are at least one pixel thick
    float x1a = x1, y1a = y1, x2a = x2;
    float y2a = (y1 - y2 < 1.0f / 240) ? (y1 - 1.0f / 240) : y2;

    // Detect and override gamepad screen layout positions
    if (running && tw >= 240) {
        if (firstScreen) {
            x1a = (float(gpLayout.topX) / 427 - 1.0f);
            y1a = -(float(gpLayout.topY) / 240 - 1.0f);
            x2a = (float(gpLayout.topX + gpLayout.topWidth) / 427 - 1.0f);
            y2a = -(float(gpLayout.topY + gpLayout.topHeight) / 240 - 1.0f);
            firstScreen = false;
        }
        else {
            x1a = (float(gpLayout.botX) / 427 - 1.0f);
            y1a = -(float(gpLayout.botY) / 240 - 1.0f);
            x2a = (float(gpLayout.botX + gpLayout.botWidth) / 427 - 1.0f);
            y2a = -(float(gpLayout.botY + gpLayout.botHeight) / 240 - 1.0f);
        }
    }

    // Convert texture coordinates to floats
    float s1 = tx / ((GX2Texture*)texture)->surface.width;
    float t1 = ty / ((GX2Texture*)texture)->surface.height;
    float s2 = (tx + tw) / ((GX2Texture*)texture)->surface.width;
    float t2 = (ty + th) / ((GX2Texture*)texture)->surface.height;

    // Convert the surface color to floats
    float r = float((color >> 0) & 0xFF) / 0xFF;
    float g = float((color >> 8) & 0xFF) / 0xFF;
    float b = float((color >> 16) & 0xFF) / 0xFF;
    float a = float((color >> 24) & 0xFF) / 0xFF;

    // Define arrays for position and color values
    float posCoords[] = { x1, y1, x2, y1, x2, y2, x1, y2, x1a, y1a, x2a, y1a, x2a, y2a, x1a, y2a };
    float vtxColors[] = { r, g, b, a, r, g, b, a, r, g, b, a, r, g, b, a };

    // Define texture coordinates based on rotation
    float texCoords[] = {
        s1, t1, s2, t1, s2, t2, s1, t2, // None
        s1, t2, s1, t1, s2, t1, s2, t2, // Clockwise
        s2, t1, s2, t2, s1, t2, s1, t1, // Counter-clockwise
    };

    // Upload positions to the buffer
    uint8_t *buffer = (uint8_t*)GX2RLockBufferEx(&posBuffer, GX2R_RESOURCE_BIND_NONE);
    memcpy(&buffer[bufOffset * 2], posCoords, posBuffer.elemSize * 8);
    GX2RUnlockBufferEx(&posBuffer, GX2R_RESOURCE_BIND_NONE);

    // Upload texture coordinates to the buffer
    buffer = (uint8_t*)GX2RLockBufferEx(&texBuffer, GX2R_RESOURCE_BIND_NONE);
    memcpy(&buffer[bufOffset], &texCoords[rotation * 8], texBuffer.elemSize * 4);
    GX2RUnlockBufferEx(&texBuffer, GX2R_RESOURCE_BIND_NONE);

    // Upload vertex colors to the buffer
    buffer = (uint8_t*)GX2RLockBufferEx(&colBuffer, GX2R_RESOURCE_BIND_NONE);
    memcpy(&buffer[bufOffset * 2], vtxColors, colBuffer.elemSize * 4);
    GX2RUnlockBufferEx(&colBuffer, GX2R_RESOURCE_BIND_NONE);

    // Draw a texture on the TV
    WHBGfxBeginRenderTV();
    GX2RSetAttributeBuffer(&posBuffer, 0, posBuffer.elemSize, bufOffset * 2);
    GX2RSetAttributeBuffer(&texBuffer, 1, texBuffer.elemSize, bufOffset);
    GX2RSetAttributeBuffer(&colBuffer, 2, colBuffer.elemSize, bufOffset * 2);
    GX2SetPixelTexture((GX2Texture*)texture, group.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[filter], group.pixelShader->samplerVars[0].location);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    // Override the gamepad screen texture with the other screen in single screen mode
    GX2Texture *tempTexture = nullptr;
    if (running && tw >= 240 && !(ConsoleUI::gbaMode && ScreenLayout::gbaCrop) && ScreenLayout::screenArrangement == 3) {
        bool shift = (Settings::highRes3D || Settings::screenFilter == 1);
        uint32_t *data = &ConsoleUI::framebuffer[(256 * 192 * (ScreenLayout::screenSizing < 2)) << (shift * 2)];
        tempTexture = gpTexture = (GX2Texture*)createTexture(data, 256 << shift, 192 << shift);
    }

    // Draw a texture on the gamepad
    WHBGfxBeginRenderDRC();
    GX2RSetAttributeBuffer(&posBuffer, 0, posBuffer.elemSize, bufOffset * 2 + 8 * sizeof(float));
    GX2RSetAttributeBuffer(&texBuffer, 1, texBuffer.elemSize, bufOffset);
    GX2RSetAttributeBuffer(&colBuffer, 2, colBuffer.elemSize, bufOffset * 2);
    GX2SetPixelTexture(tempTexture ? tempTexture : (GX2Texture*)texture, group.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samplers[filter], group.pixelShader->samplerVars[0].location);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    // Adjust the buffer offset if there's still room
    if (bufOffset < 8 * sizeof(float) * MAX_DRAWS)
        bufOffset += 8 * sizeof(float);
}

uint32_t ConsoleUI::getInputHeld() {
    // Scan for input once per frame
    if (!scanned) {
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch, &vpad.tpNormal);
        scanned = true;
    }

    // Return a mask of mappable keys
    return vpad.hold & 0x7FFFFFFF;
}

MenuTouch ConsoleUI::getInputTouch() {
    // Scan for input once per frame and read touch data
    if (!scanned) {
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch, &vpad.tpNormal);
        scanned = true;
    }
    return MenuTouch(touch.touched, touch.x, touch.y);
}

void outputAudio(void *data, uint8_t *buffer, int length) {
    // Refill the audio buffer as a callback
    ConsoleUI::fillAudioBuffer((uint32_t*)buffer, length / sizeof(uint32_t), 32768);
}

struct AutobootConfig {
    bool foundConfig = false;
    bool foundRom = false;
    bool fallbackToBrowser = true;
    bool showFailure = true;
    bool logEnabled = true;
    std::string configPath;
    std::string romPath;
};

static std::string autobootLogPath;

static std::string trimAutobootLine(std::string line) {
    // Remove comments and surrounding whitespace.
    size_t comment = line.find('#');
    if (comment != std::string::npos)
        line = line.substr(0, comment);

    size_t start = 0;
    while (start < line.size() && std::isspace((unsigned char)line[start]))
        start++;

    size_t end = line.size();
    while (end > start && std::isspace((unsigned char)line[end - 1]))
        end--;

    return line.substr(start, end - start);
}

static std::string lowerAutobootToken(std::string value) {
    for (char &c : value)
        c = std::tolower((unsigned char)c);
    return value;
}

static void writeAutobootLog(const std::string &message, bool enabled);

static bool parseAutobootBool(const std::string &value, bool defaultValue) {
    std::string normalized = lowerAutobootToken(trimAutobootLine(value));
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
        return true;
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
        return false;
    return defaultValue;
}

static bool parseAutobootInt(const std::string &value, int minValue, int maxValue, int &result) {
    std::string trimmed = trimAutobootLine(value);
    if (trimmed.empty())
        return false;

    char *end = nullptr;
    long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || *end != '\0')
        return false;

    if (parsed < minValue)
        parsed = minValue;
    else if (parsed > maxValue)
        parsed = maxValue;

    result = (int)parsed;
    return true;
}

static bool applyAutobootLayoutOption(const std::string &name, const std::string &value, bool logEnabled) {
    int parsed = 0;
    int *target = nullptr;
    int minValue = 0;
    int maxValue = 0;
    std::string settingName;

    if (name == "screenposition" || name == "screen_position") {
        target = &ScreenLayout::screenPosition;
        maxValue = 4;
        settingName = "screenPosition";
    }
    else if (name == "screenrotation" || name == "screen_rotation") {
        target = &ScreenLayout::screenRotation;
        maxValue = 2;
        settingName = "screenRotation";
    }
    else if (name == "screenarrangement" || name == "screen_arrangement") {
        target = &ScreenLayout::screenArrangement;
        maxValue = 3;
        settingName = "screenArrangement";
    }
    else if (name == "screensizing" || name == "screen_sizing") {
        target = &ScreenLayout::screenSizing;
        maxValue = 2;
        settingName = "screenSizing";
    }
    else if (name == "screengap" || name == "screen_gap") {
        target = &ScreenLayout::screenGap;
        maxValue = 3;
        settingName = "screenGap";
    }
    else if (name == "aspectratio" || name == "aspect_ratio") {
        target = &ScreenLayout::aspectRatio;
        maxValue = 3;
        settingName = "aspectRatio";
    }
    else if (name == "integerscale" || name == "integer_scale") {
        target = &ScreenLayout::integerScale;
        maxValue = 1;
        settingName = "integerScale";
    }
    else {
        return false;
    }

    if (!parseAutobootInt(value, minValue, maxValue, parsed)) {
        writeAutobootLog("Ignored invalid layout setting " + settingName + "=" + value, logEnabled);
        return true;
    }

    *target = parsed;
    writeAutobootLog("Applied layout setting " + settingName + "=" + std::to_string(parsed), logEnabled);
    return true;
}

static bool applyAutobootRuntimeOption(const std::string &name, const std::string &value, bool logEnabled) {
    int parsed = 0;
    int *target = nullptr;
    int maxValue = 0;
    std::string settingName;

    if (name == "frameskip" || name == "frame_skip") {
        target = &Settings::frameskip;
        maxValue = 5;
        settingName = "frameskip";
    }
    else if (name == "screenfilter" || name == "screen_filter") {
        target = &Settings::screenFilter;
        maxValue = 2;
        settingName = "screenFilter";
    }
    else if (name == "threaded2d" || name == "threaded_2d") {
        target = &Settings::threaded2D;
        maxValue = 1;
        settingName = "threaded2D";
    }
    else if (name == "threaded3d" || name == "threaded_3d") {
        target = &Settings::threaded3D;
        maxValue = 2;
        settingName = "threaded3D";
    }
    else if (name == "highres3d" || name == "high_res_3d") {
        target = &Settings::highRes3D;
        maxValue = 1;
        settingName = "highRes3D";
    }
    else if (name == "fpslimiter" || name == "fps_limiter") {
        target = &Settings::fpsLimiter;
        maxValue = 1;
        settingName = "fpsLimiter";
    }
    else if (name == "emulateaudio" || name == "emulate_audio") {
        target = &Settings::emulateAudio;
        maxValue = 1;
        settingName = "emulateAudio";
    }
    else if (name == "audio16bit" || name == "audio_16_bit") {
        target = &Settings::audio16Bit;
        maxValue = 1;
        settingName = "audio16Bit";
    }
    else {
        return false;
    }

    if (!parseAutobootInt(value, 0, maxValue, parsed)) {
        writeAutobootLog("Ignored invalid runtime setting " + settingName + "=" + value, logEnabled);
        return true;
    }

    *target = parsed;
    writeAutobootLog("Applied runtime setting " + settingName + "=" + std::to_string(parsed), logEnabled);
    return true;
}

static void initializeAutobootLog(const std::string &base) {
    mkdir((base + "/uinjectforge").c_str() MKDIR_ARGS);
    mkdir((base + "/uinjectforge/noods").c_str() MKDIR_ARGS);
    autobootLogPath = base + "/uinjectforge/noods/autoboot.log";

    std::ofstream output(autobootLogPath, std::ios::out | std::ios::trunc);
    if (output.is_open())
        output << "NooDS Wii U autoboot startup\n";
}

static void writeAutobootLog(const std::string &message, bool enabled = true) {
    if (!enabled || autobootLogPath.empty())
        return;

    std::ofstream output(autobootLogPath, std::ios::out | std::ios::app);
    if (output.is_open())
        output << message << "\n";
}

static bool readAutobootConfig(const std::string &configPath, AutobootConfig &config) {
    std::ifstream input(configPath);
    if (!input.is_open())
        return false;

    config.foundConfig = true;
    config.configPath = configPath;
    writeAutobootLog("Opened config: " + configPath, config.logEnabled);

    std::string line;
    while (std::getline(input, line)) {
        std::string candidate = trimAutobootLine(line);
        if (candidate.empty())
            continue;

        size_t split = candidate.find('=');
        if (split == std::string::npos) {
            if (!config.foundRom) {
                config.romPath = candidate;
                config.foundRom = true;
            }
            continue;
        }

        std::string name = lowerAutobootToken(trimAutobootLine(candidate.substr(0, split)));
        std::string value = trimAutobootLine(candidate.substr(split + 1));
        if ((name == "rom" || name == "path" || name == "game") && !value.empty()) {
            config.romPath = value;
            config.foundRom = true;
        }
        else if (name == "fallback" || name == "fallback_on_fail") {
            std::string normalized = lowerAutobootToken(value);
            config.fallbackToBrowser = normalized.empty() || normalized == "filebrowser" ||
                normalized == "browser" || normalized == "1" || normalized == "true" ||
                normalized == "yes" || normalized == "on";
        }
        else if (name == "show_error" || name == "failure_screen") {
            config.showFailure = parseAutobootBool(value, config.showFailure);
        }
        else if (name == "log" || name == "logging") {
            config.logEnabled = parseAutobootBool(value, config.logEnabled);
        }
        else {
            if (!applyAutobootLayoutOption(name, value, config.logEnabled))
                applyAutobootRuntimeOption(name, value, config.logEnabled);
        }
    }

    if (config.foundRom)
        writeAutobootLog("Configured ROM: " + config.romPath, config.logEnabled);
    else
        writeAutobootLog("Config had no ROM path: " + configPath, config.logEnabled);

    return true;
}

static void showAutobootFailure(const AutobootConfig &config, int result) {
    while (true) {
        ConsoleUI::startFrame(0xFF18111F);
        ConsoleUI::drawRectangle(56, 52, tvWidth - 112, tvHeight - 104, 0xFF241B30);
        ConsoleUI::drawRectangle(56, 52, tvWidth - 112, 4, 0xFF7A3FD1);
        ConsoleUI::drawString("NooDS autoboot could not load the game", 84, 84, 34, 0xFFFFFFFF);
        ConsoleUI::drawString("Config: " + config.configPath, 84, 142, 24, 0xFFD8D3E3);
        ConsoleUI::drawString("ROM: " + config.romPath, 84, 182, 24, 0xFFD8D3E3);
        ConsoleUI::drawString("Result: " + std::to_string(result), 84, 222, 24, 0xFFD8D3E3);
        ConsoleUI::drawString(
            config.fallbackToBrowser ? "A: open file browser" : "A: return to Wii U Menu",
            84, tvHeight - 128, 28, 0xFFFFFFFF);
        ConsoleUI::drawString("HOME: exit through Wii U Menu", 84, tvHeight - 88, 24, 0xFFC8BED8);
        ConsoleUI::endFrame();

        uint32_t pressed = ConsoleUI::getInputPress();
        if (pressed & ConsoleUI::defaultKeys[INPUT_A])
            return;

        SDL_Delay(16);
    }
}

int main() {
    // Initialize various things
    ProcUIInit(OSSavesDone_ReadyToRelease);
    WHBGfxInit();
    VPADInit();
    WHBMountSdCard();
    gpLayout.update(854, 480, false);

    // Get the current TV render dimensions
    switch (GX2GetSystemTVScanMode()) {
    case GX2_TV_SCAN_MODE_480I:
    case GX2_TV_SCAN_MODE_480P:
        tvWidth = 854;
        tvHeight = 480;
        break;

    case GX2_TV_SCAN_MODE_1080I:
    case GX2_TV_SCAN_MODE_1080P:
        tvWidth = 1920;
        tvHeight = 1080;
        break;

    default:
        tvWidth = 1280;
        tvHeight = 720;
        break;
    }

    // Initialize the shader
    WHBGfxLoadGFDShaderGroup(&group, 0, shader_wiiu_gsh);
    WHBGfxInitShaderAttribute(&group, "position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&group, "tex_coords", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&group, "vtx_color", 2, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&group);

    // Initialize the position buffer
    posBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    posBuffer.elemSize = 2 * sizeof(float);
    posBuffer.elemCount = 8 * MAX_DRAWS;
    GX2RCreateBuffer(&posBuffer);

    // Initialize the texture coordinate buffer
    texBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    texBuffer.elemSize = 2 * sizeof(float);
    texBuffer.elemCount = 4 * MAX_DRAWS;
    GX2RCreateBuffer(&texBuffer);

    // Initialize the vertex color buffer
    colBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
    colBuffer.elemSize = 4 * sizeof(float);
    colBuffer.elemCount = 4 * MAX_DRAWS;
    GX2RCreateBuffer(&colBuffer);

    // Initialize samplers for each texture filter mode
    GX2InitSampler(&samplers[0], GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSampler(&samplers[1], GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // Configure shading and blending for the TV
    WHBGfxBeginRenderTV();
    GX2SetFetchShader(&group.fetchShader);
    GX2SetVertexShader(group.vertexShader);
    GX2SetPixelShader(group.pixelShader);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_NEVER);
    GX2SetAlphaTest(TRUE, GX2_COMPARE_FUNC_GREATER, 0.0f);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
        GX2_BLEND_COMBINE_MODE_ADD, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);

    // Configure shading and blending for the gamepad
    WHBGfxBeginRenderDRC();
    GX2SetFetchShader(&group.fetchShader);
    GX2SetVertexShader(group.vertexShader);
    GX2SetPixelShader(group.pixelShader);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_NEVER);
    GX2SetAlphaTest(TRUE, GX2_COMPARE_FUNC_GREATER, 0.0f);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
        GX2_BLEND_COMBINE_MODE_ADD, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);

    // Initialize audio output
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec AudioSettings, ObtainedSettings;
    AudioSettings.freq = 32768;
    AudioSettings.format = AUDIO_S16MSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &outputAudio;
    AudioSettings.userdata = nullptr;
    SDL_AudioDeviceID id = SDL_OpenAudioDevice(nullptr, 0, &AudioSettings, &ObtainedSettings, 0);
    SDL_PauseAudioDevice(id, 0);

    // Initialize the UI and open the configured autoboot ROM if available.
    std::string base = WHBGetSdCardMountPath();
    initializeAutobootLog(base);
    writeAutobootLog("SD base: " + base);

    ConsoleUI::initialize(tvWidth, tvHeight, base, base + "/wiiu/apps/noods/");
    AutobootConfig autobootConfig;
    std::vector<std::string> autobootPaths = {
        "fs:/vol/content/autoboot.txt",
        "fs:/vol/content/noods/autoboot.txt",
        base + "/wiiu/apps/noods/autoboot.txt",
        base + "/noods/autoboot.txt",
        base + "/uinjectforge/noods/autoboot.txt"
    };

    for (const std::string &path : autobootPaths) {
        readAutobootConfig(path, autobootConfig);
        if (autobootConfig.foundRom)
            break;
    }

    int autobootResult = -1;
    bool loadedAutoboot = false;
    if (autobootConfig.foundRom) {
        writeAutobootLog("Attempting ROM load: " + autobootConfig.romPath, autobootConfig.logEnabled);
        autobootResult = ConsoleUI::setPath(autobootConfig.romPath);
        loadedAutoboot = autobootResult == 2;
        writeAutobootLog("ROM load result: " + std::to_string(autobootResult), autobootConfig.logEnabled);
    }
    else {
        writeAutobootLog(autobootConfig.foundConfig ? "No autoboot ROM configured." : "No autoboot config found.");
    }

    if (!loadedAutoboot) {
        if (autobootConfig.foundConfig && autobootConfig.showFailure)
            showAutobootFailure(autobootConfig, autobootResult);

        if (!autobootConfig.foundConfig || autobootConfig.fallbackToBrowser)
            ConsoleUI::fileBrowser();
        else {
            writeAutobootLog("Exiting after autoboot failure because fallback is disabled.", autobootConfig.logEnabled);
            SYSLaunchMenu();
            ProcUIShutdown();
            return 0;
        }
    }

    // Run the emulator until it exits
    ConsoleUI::mainLoop(nullptr, &gpLayout);
    SYSLaunchMenu();

    // Respond to system messages appropriately to allow exiting
    while (true) {
        switch (ProcUIProcessMessages(true)) {
        case PROCUI_STATUS_EXITING:
            ProcUIShutdown();
            return 0;

        case PROCUI_STATUS_RELEASE_FOREGROUND:
            ProcUIDrawDoneRelease();
            break;
        }
    }
}

#endif // __WIIU__
