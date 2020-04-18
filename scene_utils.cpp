#include "scene_utils.hpp"

#include "utils.hpp"

bool SceneUtils::CreateTextureSrvFromData(IRenderingContext &ctx,
                                          ID3D11ShaderResourceView *&srv,
                                          const UINT width,
                                          const UINT height,
                                          const DXGI_FORMAT dataFormat,
                                          const void *data,
                                          const UINT lineMemPitch)
{
    auto device = ctx.GetDevice();
    if (!device)
        return false;
    HRESULT hr = S_OK;
    ID3D11Texture2D *tex = nullptr;

    // Texture
    D3D11_TEXTURE2D_DESC descTex;
    ZeroMemory(&descTex, sizeof(D3D11_TEXTURE2D_DESC));
    descTex.ArraySize = 1;
    descTex.Usage = D3D11_USAGE_IMMUTABLE;
    descTex.Format = dataFormat;
    descTex.Width = width;
    descTex.Height = height;
    descTex.MipLevels = 1;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = { data, lineMemPitch, 0 };
    hr = device->CreateTexture2D(&descTex, &initData, &tex);
    if (FAILED(hr))
        return false;

    // Shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    descSRV.Texture2D.MostDetailedMip = 0;
    hr = device->CreateShaderResourceView(tex, &descSRV, &srv);
    Utils::ReleaseAndMakeNull(tex);
    if (FAILED(hr))
        return false;

    return true;
}


bool SceneUtils::CreateConstantTextureSRV(IRenderingContext &ctx,
                                          ID3D11ShaderResourceView *&srv,
                                          XMFLOAT4 color)
{
    return CreateTextureSrvFromData(ctx, srv,
                                    1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                    &color, sizeof(XMFLOAT4));
}


bool SceneUtils::ConvertImageToFloat(std::vector<unsigned char> &floatImage,
                                     const tinygltf::Image &srcImage,
                                     XMFLOAT4 constFactor)
{
    const size_t floatDataSize = srcImage.width * srcImage.height * 4 * sizeof(float);
    floatImage.resize(floatDataSize);
    if (floatImage.size() != floatDataSize)
        return false;

    auto floatPtr = reinterpret_cast<float*>(floatImage.data());

    if (srcImage.component == 4 &&
        srcImage.bits == 8 &&
        srcImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
    {
        auto srcPtr = reinterpret_cast<const uint8_t*>(srcImage.image.data());

        for (size_t x = 0; x < srcImage.width; x++)
            for (size_t y = 0; y < srcImage.height; y++)
            {
                for (size_t comp = 0; comp < srcImage.component; comp++, srcPtr++, floatPtr++)
                    *floatPtr = (*srcPtr / 255.f) * GetComponent(constFactor, comp);
            }

        return true;
    }
    else
        return false;
}


const FLOAT& SceneUtils::GetComponent(const XMFLOAT4 &vec, size_t comp)
{
    return *(reinterpret_cast<const FLOAT*>(&vec) + comp);
}


XMFLOAT4 SceneUtils::SrgbColorToLinear(uint8_t r, uint8_t g, uint8_t b, float intensity)
{
#ifdef CONVERT_SRGB_INPUT_TO_LINEAR
    return XMFLOAT4(pow((r / 255.f), 2.2f) * intensity,
                    pow((g / 255.f), 2.2f) * intensity,
                    pow((b / 255.f), 2.2f) * intensity,
                    1.0f);
#else
    return XMFLOAT4((r / 255.f) * intensity,
                    (g / 255.f) * intensity,
                    (b / 255.f) * intensity,
                    1.0f);
#endif
};
