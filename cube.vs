struct GeomBuffer {
    float4x4 model;
    float4x4 norm;
    float4 shineSpeedTexIdNM;
    float4 posAngle;
};

cbuffer SceneBuffer : register(b0) {
    float4x4 vp;
};

cbuffer GeomBufferInst : register(b1) {
    GeomBuffer geomBuffer[100];
};

cbuffer GeomBufferInstVis : register(b2) {
    uint4 ids[100];
};

struct VSInput {
    float3 pos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    uint instanceId : SV_InstanceID;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    nointerpolation uint instanceId : INST_ID;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    uint idx = ids[vertex.instanceId].x;
    float4 worldPos = mul(geomBuffer[idx].model, float4(vertex.pos, 1.0));

    result.pos = mul(vp, worldPos);
    result.worldPos = worldPos;
    result.uv = vertex.uv;
    result.tang = mul(geomBuffer[idx].norm, float4(vertex.tang, 0)).xyz;
    result.norm = mul(geomBuffer[idx].norm, float4(vertex.norm, 0)).xyz;
    result.instanceId = idx;
    return result;
}
