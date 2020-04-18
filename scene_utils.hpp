#pragma once

#include "Libs/tinygltf-2.2.0/tiny_gltf.h" // just the interfaces (no implementation)

#include "irenderingcontext.hpp"

// We are using an older version of DirectX headers which causes 
// "warning C4005: '...' : macro redefinition"
#pragma warning(push)
#pragma warning(disable: 4005)
#include <d3d11.h>
#include <d3dx11.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4838)
#include <xnamath.h>
#pragma warning(pop)

namespace SceneUtils
{
    bool CreateTextureSrvFromData(IRenderingContext &ctx,
                                  ID3D11ShaderResourceView *&srv,
                                  const UINT width,
                                  const UINT height,
                                  const DXGI_FORMAT dataFormat,
                                  const void *data,
                                  const UINT lineMemPitch);

    bool CreateConstantTextureSRV(IRenderingContext &ctx,
                                  ID3D11ShaderResourceView *&srv,
                                  XMFLOAT4 color);

    bool ConvertImageToFloat(std::vector<unsigned char> &floatImage,
                             const tinygltf::Image &srcImage,
                             XMFLOAT4 constFactor);

    const FLOAT& GetComponent(const XMFLOAT4 &vec, size_t comp);

    XMFLOAT4 SrgbColorToLinear(uint8_t r, uint8_t g, uint8_t b, float intensity = 1.0f);
}
