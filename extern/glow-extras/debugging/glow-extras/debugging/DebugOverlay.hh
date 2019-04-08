#pragma once

#ifdef GLOW_EXTRAS_HAS_IMGUI

#include <imgui/imgui.h>

namespace glow
{
namespace debugging
{
class DebugOverlay
{
private:
    /// Whether the DebugOverlay was initialized
    static bool mInitialized;
    /// Whether the top level nanogui window is visible
    static bool mVisible;

    static ImGuiTextBuffer mImguiBuffer;
    static ImGuiTextFilter mImguiFilter;
    static ImVector<int> mImguiLineOffsets;
    static bool mScrollToBottom;

private:
    DebugOverlay() = delete;

public:
    /// Prints a line of text to the OpenGL error log
    /// Do not call this function manually
    static void PrintErrorLine(const char* content);

public:
    /// Overrides the default GLOW OpenGL debug callback
    static void Init();

    /// Toggle visibility of the overlay
    /// Note that OpenGL errors always make the overlay visible
    static void ToggleVisibility();

    /// ImGui call
    static void OnGui();

    /// Clear the error log
    static void ClearErrorLog();
};
}
}

#endif
