#pragma once

#include "logic/Simulation.h"
#include "renderer/Renderer.h"
#include "ui/Workspace.h"

namespace openphysx {

class Application
{
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void init();
    void shutdown();
    void frame();

    Simulation simulation_{};
    Renderer renderer_{};
    Workspace workspace_{};
    float accumulator_ = 0.0f;
    bool initialized_ = false;
};

} // namespace openphysx
