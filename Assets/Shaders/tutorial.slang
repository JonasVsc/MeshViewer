struct VSInput
{
    float3 position;
    float3 color;
    float2 texCoord;
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
    float3 color;
    float2 texCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;

    output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
    output.color = input.color;
    output.texCoord = input.texCoord;

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    return texture.Sample(input.texCoord);
}