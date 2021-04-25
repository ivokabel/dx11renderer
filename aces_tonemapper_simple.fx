// Simplified ACES tonemapping by Krzysztof Narkowicz (Unreal Engine 4?)
// Shared under CC0 1.0
float3 AcesTonemap(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}
