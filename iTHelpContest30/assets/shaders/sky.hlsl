#include "common.hlsl"

FullscreenVSOutput VSMain(uint vertexId : SV_VertexID)
{
    return GetFullscreenTriangleVertex(vertexId);
}


float4 PSMain(FullscreenVSOutput input) : SV_TARGET
{
    const float3 viewDirection =
        GetWorldViewDirection(
            input.uv,
            inverseProjection,
            inverseView
        );

    const float3 horizonColor =
        float3(
            0.65f,
            0.85f,
            1.0f
        );

    const float3 zenithColor =
        float3(
            0.08f,
            0.35f,
            0.85f
        );

    // 世界座標使用 Z 軸作為上方向。
    const float height =
        saturate(viewDirection.z);

    const float3 skyColor =
        lerp(
            horizonColor,
            zenithColor,
            height
        );

    return float4(
        skyColor,
        1.0f
    );
}
