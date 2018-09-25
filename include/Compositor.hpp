/*
 * Copyright (C) 2018 by Ilya Glushchenko
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#pragma once

#include <Device.hpp>
#include <Window.hpp>
#include <Render.hpp>

namespace vkc
{

class Compositor
{
public:
    bool Init();

    bool IsValid();

    void RenderFrame();

private:
    Device device;
    std::unique_ptr<Window> m_pWindow;
    std::unique_ptr<Render> m_pRender;
};

} // namespace vkc