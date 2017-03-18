cbuffer cbTansMatrix : register(b0){
	float4x4 WVP;
};

Texture2D<float4> tex0 : register(t0);
SamplerState samp0 : register(s0);

struct VS_INPUT{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};

struct PS_INPUT{//(VS_OUTPUT)
	float4 Position : SV_POSITION;
	float4 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};


PS_INPUT VSMain(VS_INPUT input){
	PS_INPUT output;


	float4 Pos = float4(input.Position, 1.0f);
	float4 Nrm = float4(input.Normal, 1.0f);
	output.Position = mul(Pos, WVP);
	output.Normal = mul(Nrm, WVP);
	output.UV = input.UV;

	return output;
}


float4 PSMain(PS_INPUT input) : SV_TARGET{
	return tex0.Sample(samp0, input.UV);
}
