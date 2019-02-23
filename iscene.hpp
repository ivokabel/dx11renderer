#pragma once

#include "irenderer.hpp"

class IScene
{
public:
    virtual ~IScene() {};

    virtual bool Init(IRenderer &renderer) = 0;
    virtual void Destroy() = 0;
    virtual void Animate(IRenderer &renderer) = 0;
    virtual void Render(IRenderer &renderer) = 0;
};
