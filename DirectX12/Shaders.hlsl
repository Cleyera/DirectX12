cbuffer cbTansMatrix : register(b0){
	float4x4 WVP;
	float4x4 World;
};

cbuffer cbLight : register(b1){
	float4x4	LightVP;
	float4		LightColor;
	float3		LightDir;
};

Texture2D<float4> tex0 : register(t0);
Texture2D<float> shadow_map : register(t1);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct VS_INPUT{
	float3 Position : POSITION;
	float3 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};

struct PS_INPUT{//(VS_OUTPUT)
	float4 Position : SV_POSITION;
	float4 PosSM	: POSITION_SM;
	float4 Normal	: NORMAL;
	float2 UV		: TEXCOORD;
};


PS_INPUT VSMain(VS_INPUT input){
	PS_INPUT output;

	float4 Pos = float4(input.Position, 1.0f);
	float4 Nrm = float4(input.Normal, 0.0f);
	output.Position = mul(Pos, WVP);
	output.Normal = mul(Nrm, World);
	output.UV = input.UV;

	Pos = mul(Pos, World);
	Pos = mul(Pos, LightVP);
	Pos.xyz = Pos.xyz / Pos.w;
	output.PosSM.x = (1.0f + Pos.x) / 2.0f;
	output.PosSM.y = (1.0f - Pos.y) / 2.0f;
	output.PosSM.z = Pos.z;

	return output;
}


float4 PSMain(PS_INPUT input) : SV_TARGET{

	float sm = shadow_map.Sample(samp1, input.PosSM.xy);
	float sma = (input.PosSM.z - 0.005f < sm) ? 1.0f : 0.5f;
	
	return tex0.Sample(samp0, input.UV) * sma;
}


//シャドーマップ計算用頂点シェーダ
float4 VSShadowMap(VS_INPUT input) : SV_POSITION{

	float4 Pos = float4(input.Position, 1.0f);
	Pos = mul(Pos, World);
	Pos = mul(Pos, LightVP);

	return Pos;
}

