/*
 * MIT License
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>

int main()
{
    Compositor compositor;

    while (compositor.IsValid())
    {
        compositor.RenderFrame();
    }

    return 0;
}
