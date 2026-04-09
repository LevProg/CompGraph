struct Light {
    float4 pos;
    float4 color;
};

cbuffer SceneBuffer : register(b0) {
    float4x4 vp;
    float4 cameraPos;
    int4 lightCount;
    Light lights[10];
    float4 ambientColor;
    float4 frustum[6];
};

cbuffer CullParams : register(b1) {
    uint4 numShapes;
    float4 bbMin[100];
    float4 bbMax[100];
};

RWStructuredBuffer<uint> indirectArgs : register(u0);
RWStructuredBuffer<uint4> objectIds : register(u1);

bool IsBoxInside(in float4 fr[6], in float3 bMin, in float3 bMax) {
    for (int i = 0; i < 6; i++) {
        const float3 norm = fr[i].xyz;
        float4 p = float4(
            norm.x < 0 ? bMin.x : bMax.x,
            norm.y < 0 ? bMin.y : bMax.y,
            norm.z < 0 ? bMin.z : bMax.z,
            1.0
        );
        float s = dot(p, fr[i]);
        if (s < 0.0f)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void cs(uint3 globalThreadId : SV_DispatchThreadID) {
    if (globalThreadId.x >= numShapes.x) {
        return;
    }

    if (IsBoxInside(frustum, bbMin[globalThreadId.x].xyz, bbMax[globalThreadId.x].xyz)) {
        uint id = 0;
        InterlockedAdd(indirectArgs[1], 1, id);
        objectIds[id] = uint4(globalThreadId.x, 0, 0, 0);
    }
}
