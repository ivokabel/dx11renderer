float4 VS(float4 Pos : POSITION) : SV_POSITION
{
    return Pos;
}


float4 PS(float4 Pos : SV_POSITION) : SV_Target
{
    return float4(0.95f, 0.80f, 0.30f, 1.0f);
}
