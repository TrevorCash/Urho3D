#pragma once
#include "Newton.h"
#include "Math/Color.h"


class NewtonBody;

namespace Urho3D
{
    class DebugRenderer;

    struct debugRenderOptions {
        Color color = Color::GRAY;
        DebugRenderer* debug;
        bool depthTest = false;
    };



    void NewtonBodyDebugDrawAABB(NewtonBody* body, DebugRenderer* debug, bool depthTest = false);
    void NewtonBodyDebugShowCollision(const NewtonBody* const body, DebugRenderer* debug, bool depthTest = false);




    void DebugShowGeometryCollision(void* userData, int vertexCount, const dFloat* const faceVertec, int id);



}
