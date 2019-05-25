#pragma once

#include "irenderingcontext.hpp"

class IScene
{
public:

    virtual ~IScene() {};

    virtual bool Init(IRenderingContext &ctx) = 0;
    virtual void Destroy() = 0;
    virtual void Animate(IRenderingContext &ctx) = 0;
    virtual void Render(IRenderingContext &ctx) = 0;
    virtual bool GetAmbientColor(float (&rgba)[4]) = 0;
};
