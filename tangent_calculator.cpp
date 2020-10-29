#include "tangent_calculator.hpp"

void TangentCalculator::Calculate(void * data)
{
    SMikkTSpaceInterface iface;
    iface.m_getNumFaces = getNumFaces;
    iface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    iface.m_getPosition = getPosition;
    iface.m_getNormal = getNormal;
    iface.m_getTexCoord = getTexCoord;
    iface.m_setTSpaceBasic = setTSpaceBasic;
    iface.m_setTSpace = nullptr;

    SMikkTSpaceContext context;
    context.m_pInterface = &iface;
    context.m_pUserData = data;

    genTangSpaceDefault(&context);
}

int TangentCalculator::getNumFaces(const SMikkTSpaceContext *context)
{
    return 0; // debug
}

int TangentCalculator::getNumVerticesOfFace(const SMikkTSpaceContext *context,
                                            const int primnum)
{
    return 0; // debug
}

void TangentCalculator::getPosition(const SMikkTSpaceContext *context,
                                    float outpos[],
                                    const int primnum,
                                    const int vtxnum)
{
}

void TangentCalculator::getNormal(const SMikkTSpaceContext *context,
                                  float outnormal[],
                                  const int primnum,
                                  const int vtxnum)
{
}

void TangentCalculator::getTexCoord(const SMikkTSpaceContext *context,
                                    float outuv[],
                                    const int primnum,
                                    const int vtxnum)
{
}

void TangentCalculator::setTSpaceBasic(const SMikkTSpaceContext *context,
                                       const float tangentu[],
                                       const float sign,
                                       const int primnum,
                                       const int vtxnum)
{
}
