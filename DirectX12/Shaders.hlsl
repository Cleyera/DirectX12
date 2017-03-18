cbuffer cbTansMatrix : register(b0){
	float4x4 WVP;
};

struct VS_INPUT{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float4 Color	: COLOR;
};

struct PS_INPUT{//(VS_OUTPUT)
	float4 Position : SV_POSITION;
	float4 Normal	: NORMAL;
	float4 Color	: COLOR;
};


PS_INPUT VSMain(VS_INPUT input){
	PS_INPUT output;


	float4 Pos = float4(input.Position, 1.0f);
	float4 Nrm = float4(input.Normal, 1.0f);
	output.Position = mul(Pos, WVP);
	output.Normal = mul(Nrm, WVP);
	output.Color = input.Color;

	return output;
}


float4 PSMain(PS_INPUT input) : SV_TARGET{
	return input.Color;
}
