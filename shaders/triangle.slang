struct PushConstants
{
    float4x4 viewProj;
    float4x4 model;
    float time;
    float2 resolution;
    uint vertexBufferIndex;
    uint indexBufferIndex;
    uint vertexCount;
    uint indexCount;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_Push;

struct Vertex
{
    float3 position;
    float3 normal;
    float3 color;
};

struct VertexOutput
{
    float3 color : COLOR0;
};

struct TaskPayload
{
    uint meshletIndex;
};

// Bindless storage buffer access - single array of raw buffers
[[vk::binding(2, 0)]] ByteAddressBuffer g_StorageBuffers[];

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(uint groupId : SV_GroupID)
{
    // Simple task shader - just dispatch one mesh shader workgroup
    TaskPayload payload;
    payload.meshletIndex = groupId;
    DispatchMesh(1, 1, 1, payload);
}

// Mesh shader - reads geometry from GPU buffers
[shader("mesh")]
[numthreads(36, 1, 1)]
[outputtopology("triangle")]
void meshMain(
    uint threadId : SV_GroupThreadID,
    in payload TaskPayload payload,
    OutputVertices<VertexOutput, 36> verts,
    OutputIndices<uint3, 12> tris,
    out vertices float4 positions[36] : SV_Position)
{
    uint numVertices = g_Push.vertexCount;
    uint numIndices = g_Push.indexCount;
    uint numPrimitives = numIndices / 3;  // 3 indices per triangle
    
    SetMeshOutputCounts(numVertices, numPrimitives);

    // Check if we should use hardcoded triangle (useful for debugging)
    bool useHardcoded = (g_Push.vertexBufferIndex == 9999);
    
    if (useHardcoded)
    {
        // Hardcoded triangle in clip space for debugging
        if (threadId < numVertices && threadId < 3)
        {
            float3 hardcodedPositions[3] = {
                float3(0.0, -0.5, 0.5),
                float3(0.5, 0.5, 0.5),
                float3(-0.5, 0.5, 0.5)
            };
            
            float3 hardcodedColors[3] = {
                float3(1.0, 0.0, 0.0),  // Red
                float3(0.0, 1.0, 0.0),  // Green
                float3(0.0, 0.0, 1.0)   // Blue
            };
            
            positions[threadId] = float4(hardcodedPositions[threadId], 1.0);
            verts[threadId].color = hardcodedColors[threadId];
        }
    }
    else
    {
        // Access vertex buffer as raw bytes
        ByteAddressBuffer vertexBuffer = g_StorageBuffers[g_Push.vertexBufferIndex];
        
        if (threadId < numVertices)
        {
            uint byteOffset = threadId * 36;  // sizeof(Vertex) = 36 bytes (3 vec3s)
            float3 position = asfloat(vertexBuffer.Load3(byteOffset));
            float3 normal = asfloat(vertexBuffer.Load3(byteOffset + 12));
            float3 color = asfloat(vertexBuffer.Load3(byteOffset + 24));
            
            // Apply MVP transformation
            float4 worldPos = mul(g_Push.model, float4(position, 1.0));
            float4 clipPos = mul(g_Push.viewProj, worldPos);
            
            positions[threadId] = clipPos;
            
            // DEBUG: Use cyan for all buffer-read vertices (if this color appears, indices are being read)
            verts[threadId].color = float3(0.0, 1.0, 1.0);  // Cyan
        }
    }

    if (threadId == 0)
    {
        uint triIdx = 0;
        
        if (useHardcoded)
        {
            // Hardcoded triangle
            tris[0] = uint3(0, 1, 2);
        }
        else
        {
            // Read and output all triangles from index buffer
            ByteAddressBuffer indexBuffer = g_StorageBuffers[g_Push.indexBufferIndex];
            
            for (uint i = 0; i < numIndices && triIdx < numPrimitives; i += 3)
            {
                uint idx0 = indexBuffer.Load(i * 4);
                uint idx1 = indexBuffer.Load((i + 1) * 4);
                uint idx2 = indexBuffer.Load((i + 2) * 4);
                
                // DEBUG: Output first few indices as vertex colors to verify they're read correctly
                if (i < 12)  // First 4 triangles
                {
                    verts[i].color = float3(float(idx0) / 255.0, float(idx1) / 255.0, float(idx2) / 255.0);
                }
                
                tris[triIdx] = uint3(idx0, idx1, idx2);
                triIdx++;
            }
        }
    }
}

[shader("fragment")]
float4 psMain(VertexOutput input) : SV_Target
{
    float pulse = 0.6 + 0.4 * sin(g_Push.time * 2.0);
    return float4(input.color * pulse, 1.0);
}

[shader("fragment")]
void psDepth(VertexOutput input)
{
}
