struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct FrameData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> Frame;

[shader("vertex")]
VSOutput VertexMain(VSInput Input)
{
    VSOutput Output;

    Output.Position = float4(Input.Position, 1.0);
    Output.Normal = Input.Normal;
    Output.UV = Input.UV;

    return Output;
}

[shader("fragment")]
float4 FragmentMain(VSOutput Input) : SV_Target
{
    float4 Output;

    Output = float4(1.0, 1.0, 1.0, 1.0);

    return Output;
}