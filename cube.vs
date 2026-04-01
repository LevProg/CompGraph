cbuffer GeomBuffer : register(b0) {
    float4x4 model;
    float4 shine;
};

cbuffer SceneBuffer : register(b1) {
    float4x4 vp;
};

struct VSInput {
    float3 pos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    float4 worldPos = mul(model, float4(vertex.pos, 1.0));
    result.pos = mul(vp, worldPos);
    result.worldPos = worldPos;
    float3x3 normalMatrix = (float3x3)model;
    result.tang = mul(normalMatrix, vertex.tang);
    result.norm = mul(normalMatrix, vertex.norm);
    result.uv = vertex.uv;
    return result;
}
