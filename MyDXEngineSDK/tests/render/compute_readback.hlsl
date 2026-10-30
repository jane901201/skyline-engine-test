cbuffer Parameters : register(b0)
{
    uint elementCount;
    uint seed;
};

RWStructuredBuffer<uint> outputValues : register(u0);
StructuredBuffer<uint> inputValues : register(t0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < elementCount)
        outputValues[id.x] = id.x * 3 + seed;
}

[numthreads(64, 1, 1)]
void ReadMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x < elementCount)
        outputValues[id.x] = inputValues[id.x] * 5 + seed;
}
