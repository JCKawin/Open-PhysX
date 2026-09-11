#include <iostream>
#include "raylib.h"

#include "imgui.h"
#include "rlImGui.h"

int main()
{
    constexpr int screen_width = 1280;
    constexpr int screen_height = 800;
    const int FPS = 60;
    constexpr float FIXED_DT = 1.0f / static_cast<float>(FPS);
    float accumulator = 0.0f;


    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "Open PhysX");
    SetTargetFPS(FPS);

    rlImGuiSetup(true);
    RenderTexture2D viewportTarget = LoadRenderTexture(800, 600);

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

        UpdateCamera(&camera, CAMERA_ORBITAL);

        accumulator += GetFrameTime();
        while (accumulator >= FIXED_DT)
        {
            // world.Step(FIXED_DT);   <-- don't ask me what is world . it will exist in a while
            accumulator -= FIXED_DT;
        }

        const Color background{
            static_cast<unsigned char>(clear_color[0] * 255.0f),
            static_cast<unsigned char>(clear_color[1] * 255.0f),
            static_cast<unsigned char>(clear_color[2] * 255.0f),
            255,
        };

        BeginTextureMode(viewportTarget);
            BeginMode3D(camera);
                ClearBackground(background);
                    DrawGrid(10, 1.0f);
                    DrawCube({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, DARKBLUE);
                    DrawCubeWires({0.0f, 1.0f, 0.0f}, 2.0f, 2.0f, 2.0f, SKYBLUE);
            EndMode3D();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        rlImGuiBegin();



        if (show_demo)
            ImGui::ShowDemoWindow(&show_demo);

        ImGui::Begin("Open PhysX");
        if ( ImGui::BeginTabBar("Workspace")) {
            if (ImGui::BeginTabItem("Viewport")) {
                ImVec2 renderSize = ImGui::GetContentRegionAvail();
                rlImGuiImageRenderTexture(&viewportTarget);

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings")) {
                ImGui::Text("Put your configuration sliders here!");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
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

    } // main loop

    UnloadRenderTexture(viewportTarget);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
