struct VSInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;

    output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
    output.uv = input.uv;

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    return texture.Sample(input.uv);
}