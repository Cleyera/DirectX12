cbuffer cbTansMatrix : register(b0){
	float4x4 WVP;
};

Texture2D<float> tex0 : register(t0);
SamplerState samp0 : register(s0);


struct VS_INPUT{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};


struct PS_INPUT{//(VS_OUTPUT)
	float4 Position : SV_POSITION;
	float2 UV		: TEXCOORD;
};


PS_INPUT VSMain(VS_INPUT input){
	PS_INPUT output;

	float4 Pos = float4(input.Position, 1.0f);
	output.Position = mul(Pos, WVP);
	output.UV.x = input.UV.x;
	output.UV.y = 1.0f - input.UV.y;

	return output;
}


float4 PSMain(PS_INPUT input) : SV_TARGET{
	float color = tex0.Sample(samp0, input.UV);
	return float4(color, color, color, 1.0f);
}



