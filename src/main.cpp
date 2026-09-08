#include "raylib.h"

#include "imgui.h"
#include "rlImGui.h"

int main()
{
    constexpr int screen_width = 1280;
    constexpr int screen_height = 800;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "Open PhysX");
    SetTargetFPS(60);

    rlImGuiSetup(true);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    Camera3D camera{};
    camera.position = {4.0f, 4.0f, 4.0f};
    camera.target = {0.0f, 1.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    bool show_demo = false;
    float clear_color[3] = {0.12f, 0.12f, 0.14f};

    while (!WindowShouldClose())
    {
        if (!io.WantCaptureMouse)
            UpdateCamera(&camera, CAMERA_ORBITAL);

        const Color background{
            static_cast<unsigned char>(clear_color[0] * 255.0f),
            static_cast<unsigned char>(clear_color[1] * 255.0f),
            static_cast<unsigned char>(clear_color[2] * 255.0f),
            255,
        };

        BeginDrawing();
        ClearBackground(background);

        BeginMode3D(camera);
        DrawGrid(10, 1.0f);
        DrawCube({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, DARKBLUE);
        DrawCubeWires({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, SKYBLUE);
        EndMode3D();

        DrawFPS(12, 12);

        rlImGuiBegin();

        if (show_demo)
            ImGui::ShowDemoWindow(&show_demo);

        ImGui::Begin("Open PhysX");
        ImGui::Text("raylib %s", RAYLIB_VERSION);
        ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::Separator();
        ImGui::Checkbox("ImGui Demo", &show_demo);
        ImGui::ColorEdit3("Clear color", clear_color);
        ImGui::TextUnformatted("Drag in the 3D view to orbit the camera.");
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
