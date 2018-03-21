/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>
#include <chrono>
#include <thread>

int main()
{
    Compositor compositor;
    compositor.Initialize();

    while (compositor.IsValid())
    {
        static std::chrono::steady_clock::time_point nextFrameTime = std::chrono::steady_clock::now();
        static std::chrono::milliseconds const deltaTime(16);
        nextFrameTime += deltaTime;

        compositor.RenderFrame();

        std::this_thread::sleep_until(nextFrameTime);
    }

    return 0;
}
