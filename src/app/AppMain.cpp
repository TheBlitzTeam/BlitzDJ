#include "App.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

extern const unsigned char blitzdjFont[];
extern const std::size_t blitzdjFontSize;
extern const unsigned char blitzdjIcon[];
extern const std::size_t blitzdjIconSize;

namespace
{
    void glfwError(int code, const char* description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    }

    void setWindowIcon(GLFWwindow* window)
    {
        int width = 0;
        int height = 0;
        int channels = 0;

        unsigned char* pixels = stbi_load_from_memory(
            blitzdjIcon,
            static_cast<int>(blitzdjIconSize),
            &width,
            &height,
            &channels,
            4
        );

        if (!pixels)
        {
            return;
        }

        GLFWimage image;
        image.width = width;
        image.height = height;
        image.pixels = pixels;

        glfwSetWindowIcon(window, 1, &image);
        stbi_image_free(pixels);
    }

    void applyStyle(float scale)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark(&style);

        style.WindowRounding = 0.0f;
        style.ChildRounding = 16.0f;
        style.FrameRounding = 12.0f;
        style.PopupRounding = 12.0f;
        style.GrabRounding = 12.0f;
        style.ScrollbarRounding = 12.0f;
        style.FramePadding = ImVec2(14.0f, 9.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.957f, 0.957f, 0.965f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.604f, 0.612f, 0.659f, 1.0f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.059f, 0.075f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_FrameBg] = ImVec4(0.122f, 0.133f, 0.169f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.163f, 0.205f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.185f, 0.23f, 1.0f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.12f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.2f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1, 1, 1, 0.28f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(1.0f, 0.761f, 0.102f, 0.35f);
        colors[ImGuiCol_NavHighlight] = ImVec4(1.0f, 0.761f, 0.102f, 0.6f);
        colors[ImGuiCol_Border] = ImVec4(0.173f, 0.188f, 0.231f, 1.0f);

        style.ScaleAllSizes(scale);
    }

    blitzdj::app::Fonts loadFonts(float pixelScale, float uiScale)
    {
        ImGuiIO& io = ImGui::GetIO();

        static const ImWchar ranges[] = {
            0x0020, 0x00FF,
            0x0100, 0x017F,
            0x2010, 0x2027,
            0x2030, 0x205E,
            0
        };

        auto load = [&](float size)
        {
            ImFontConfig config;
            config.FontDataOwnedByAtlas = false;
            config.OversampleH = 2;
            config.OversampleV = 2;

            return io.Fonts->AddFontFromMemoryTTF(
                const_cast<unsigned char*>(blitzdjFont),
                static_cast<int>(blitzdjFontSize),
                size * uiScale * pixelScale,
                &config,
                ranges
            );
        };

        blitzdj::app::Fonts fonts;
        fonts.regular = load(16.0f);
        fonts.small = load(13.5f);
        fonts.title = load(18.0f);
        fonts.headline = load(25.0f);
        fonts.display = load(42.0f);

        io.FontDefault = fonts.regular;
        io.FontGlobalScale = 1.0f / pixelScale;
        return fonts;
    }
}

int main()
{
    glfwSetErrorCallback(glfwError);

    if (!glfwInit())
    {
        return 1;
    }

#ifdef __APPLE__
    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    int width = 480;
    int height = 900;

    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor())
    {
        int x = 0;
        int y = 0;
        int areaWidth = 0;
        int areaHeight = 0;
        glfwGetMonitorWorkarea(monitor, &x, &y, &areaWidth, &areaHeight);

        if (areaHeight > 0)
        {
            height = std::min(height, static_cast<int>(static_cast<float>(areaHeight) * 0.9f));
        }
    }

    GLFWwindow* window = glfwCreateWindow(width, height, "BlitzDJ", nullptr, nullptr);

    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwSetWindowSizeLimits(window, 360, 560, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    setWindowIcon(window);

    float contentScaleX = 1.0f;
    float contentScaleY = 1.0f;
    glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);

#ifdef __APPLE__
    const float pixelScale = std::max(1.0f, contentScaleX);
    const float uiScale = 1.0f;
#else
    const float pixelScale = 1.0f;
    const float uiScale = std::max(1.0f, contentScaleX);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyStyle(uiScale);
    const auto fonts = loadFonts(pixelScale, uiScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    {
        blitzdj::app::App app;

        if (!app.initialize())
        {
            std::fprintf(stderr, "Failed to initialize BlitzDJ.\n");
        }

        app.setFonts(fonts, uiScale);

        while (!glfwWindowShouldClose(window))
        {
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
            {
                glfwWaitEventsTimeout(0.25);
                app.update();
                continue;
            }

            glfwWaitEventsTimeout(app.wantsFastRefresh() ? 1.0 / 30.0 : 1.0 / 15.0);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            app.update();
            app.draw();

            ImGui::Render();

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.055f, 0.059f, 0.075f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
