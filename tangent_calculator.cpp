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
    return (int)GetPrimitive(context).GetFacesCount();
}


int TangentCalculator::getNumVerticesOfFace(const SMikkTSpaceContext *context,
                                            const int face)
{
    face; // unused param

    return (int)GetPrimitive(context).GetVerticesPerFace();
}


void TangentCalculator::getPosition(const SMikkTSpaceContext *context,
                                    float outpos[],
                                    const int face,
                                    const int vertex)
{
    GetPrimitive(context).GetPosition(outpos, face, vertex);
}


void TangentCalculator::getNormal(const SMikkTSpaceContext *context,
                                  float outnormal[],
                                  const int face,
                                  const int vertex)
{
    GetPrimitive(context).GetNormal(outnormal, face, vertex);
}


void TangentCalculator::getTexCoord(const SMikkTSpaceContext *context,
                                    float outuv[],
                                    const int face,
                                    const int vertex)
{
    GetPrimitive(context).GetTextCoord(outuv, face, vertex);
}


void TangentCalculator::setTSpaceBasic(const SMikkTSpaceContext *context,
                                       const float tangent[],
                                       const float sign,
                                       const int face,
                                       const int vertex)
{
    GetPrimitive(context).SetTangent(tangent, sign, face, vertex);
}
