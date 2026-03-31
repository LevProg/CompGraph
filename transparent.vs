cbuffer GeomBuffer : register(b0) {
    float4x4 model;
    float4 color;
};

cbuffer SceneBuffer : register(b1) {
    float4x4 vp;
};

struct VSInput {
    float3 pos : POSITION;
};

struct VSOutput {
    float4 pos : SV_Position;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    result.pos = mul(vp, mul(model, float4(vertex.pos, 1.0)));
    return result;
}
