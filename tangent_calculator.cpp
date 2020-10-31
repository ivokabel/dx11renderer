#include "tangent_calculator.hpp"

bool TangentCalculator::Calculate(ScenePrimitive &primitive)
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
    context.m_pUserData = &primitive;

    return genTangSpaceDefault(&context) == 1;
}


ScenePrimitive& TangentCalculator::GetPrimitive(const SMikkTSpaceContext *context)
{
    return *static_cast<ScenePrimitive*>(context->m_pUserData);
}


int TangentCalculator::getNumFaces(const SMikkTSpaceContext *context)
{
    return GetPrimitive(context).GetFacesCount();
}


int TangentCalculator::getNumVerticesOfFace(const SMikkTSpaceContext *context,
                                            const int primnum)
{
    return GetPrimitive(context).GetVerticesPerFace();
}


void TangentCalculator::getPosition(const SMikkTSpaceContext *context,
                                    float outpos[],
                                    const int primnum,
                                    const int vtxnum)
{
    auto &primitive = GetPrimitive(context);

    //...
}


void TangentCalculator::getNormal(const SMikkTSpaceContext *context,
                                  float outnormal[],
                                  const int primnum,
                                  const int vtxnum)
{
    auto &primitive = GetPrimitive(context);

    //...
}


void TangentCalculator::getTexCoord(const SMikkTSpaceContext *context,
                                    float outuv[],
                                    const int primnum,
                                    const int vtxnum)
{
    auto &primitive = GetPrimitive(context);

    //...
}


void TangentCalculator::setTSpaceBasic(const SMikkTSpaceContext *context,
                                       const float tangentu[],
                                       const float sign,
                                       const int primnum,
                                       const int vtxnum)
{
    auto &primitive = GetPrimitive(context);

    //...
}
