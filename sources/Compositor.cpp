/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include <Compositor.hpp>

namespace vkc
{

bool Compositor::Init()
{
    if (device.Init())
    {
        m_pWindow = std::make_unique<Window>(device);
        if (m_pWindow->Init())
        {
            m_pRender = std::make_unique<Render>(device, *m_pWindow);
            return m_pRender->Init();
        }
    }

    return false;
}

bool Compositor::IsValid()
{
    return m_pWindow->IsValid();
}

void Compositor::RenderFrame()
{
    glfwPollEvents();
    m_pRender->Frame();
    m_pWindow->SwapBuffers();
}

} // vkc namespace