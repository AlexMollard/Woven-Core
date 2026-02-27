struct PushConstants
{
    float time;
    float2 resolution;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;

struct VertexOutput
{
    float3 color : COLOR0;
};

struct TaskPayload
{
    uint meshletIndex;
};

// Task shader - dispatches mesh shader workgroups
[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(uint groupId : SV_GroupID)
{
    // Simple task shader - just dispatch one mesh shader workgroup
    TaskPayload payload;
    payload.meshletIndex = groupId;
    DispatchMesh(1, 1, 1, payload);
}

// Mesh shader - generates triangle geometry
[shader("mesh")]
[numthreads(3, 1, 1)]
[outputtopology("triangle")]
void meshMain(
    uint threadId : SV_GroupThreadID,
    in payload TaskPayload payload,
    OutputVertices<VertexOutput, 3> verts,
    OutputIndices<uint3, 1> tris,
    out vertices float4 positions[3] : SV_Position)
{
    const uint numVertices = 3;
    const uint numPrimitives = 1;
    SetMeshOutputCounts(numVertices, numPrimitives);

    float2 pos[3] = {
        float2(0.0, -0.6),
        float2(0.6, 0.6),
        float2(-0.6, 0.6)
    };

    float3 colors[3] = {
        float3(1.0, 0.2, 0.2),
        float3(0.2, 1.0, 0.2),
        float3(0.2, 0.4, 1.0)
    };

    if (threadId < numVertices)
    {
        positions[threadId] = float4(pos[threadId], 0.0, 1.0);
        verts[threadId].color = colors[threadId];
    }

    if (threadId == 0)
    {
        tris[0] = uint3(0, 2, 1);
    }
}

[shader("fragment")]
float4 psMain(VertexOutput input) : SV_Target
{
    float pulse = 0.6 + 0.4 * sin(g_Push.time * 2.0);
    return float4(input.color * pulse, 1.0);
}
