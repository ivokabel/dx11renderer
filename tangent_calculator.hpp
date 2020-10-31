#pragma once

#include "scene.hpp"
#include "mikktspace.hpp"

class TangentCalculator
{
public:
    TangentCalculator() = delete; // force abstract

    static bool Calculate(ScenePrimitive &primitive);

private:
    static ScenePrimitive& GetPrimitive(const SMikkTSpaceContext *context);

    static int  getNumFaces(const SMikkTSpaceContext *context);
    static int  getNumVerticesOfFace(const SMikkTSpaceContext *context,
                                     const int face);
    static void getPosition(const SMikkTSpaceContext *context,
                            float pos[],
                            const int face,
                            const int vertex);
    static void getNormal(const SMikkTSpaceContext *context,
                          float normal[],
                          const int face,
                          const int vertex);
    static void getTexCoord(const SMikkTSpaceContext *context,
                            float uv[],
                            const int face,
                            const int vertex);
    static void setTSpaceBasic(const SMikkTSpaceContext *context,
                               const float tangentu[],
                               const float sign,
                               const int face,
                               const int vertex);
};
