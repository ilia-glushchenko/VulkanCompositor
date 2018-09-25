/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>

int main()
{
    vkc::Compositor compositor;
    compositor.Init();

    while (compositor.IsValid())
    {
        compositor.RenderFrame();
    }

    return 0;
}
