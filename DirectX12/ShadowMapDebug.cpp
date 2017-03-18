#include <vector>
#include <fstream>
#include "ShadowMapDebug.h"
#include "D3D12Manager.h"

ShadowMapDebug::ShadowMapDebug():
	vertex_buffer_{},
	constant_buffer_{}{}

HRESULT ShadowMapDebug::Initialize(ID3D12Device *device){
	HRESULT hr{};
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC   resource_desc{};

	heap_properties.Type					= D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty			= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask		= 0;
	heap_properties.VisibleNodeMask			= 0;

	resource_desc.Dimension				= D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Width					= 256;
	resource_desc.Height				= 1;
	resource_desc.DepthOrArraySize		= 1;
	resource_desc.MipLevels				= 1;
	resource_desc.Format				= DXGI_FORMAT_UNKNOWN;
	resource_desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.SampleDesc.Count		= 1;
	resource_desc.SampleDesc.Quality	= 0;

	//頂点バッファの作成
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffer_));
	if(FAILED(hr)){
		return hr;
	}

	//定数バッファの作成
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constant_buffer_));
	if(FAILED(hr)){
		return hr;
	}


	//頂点データの書き込み
	Vertex3D *vb{};
	hr = vertex_buffer_->Map(0, nullptr, (void**)&vb);
	if(FAILED(hr)){
		return hr;
	}
	vb[0].Position = {-1.0f,  1.0f, 0.0f};
	vb[1].Position = { 1.0f,  1.0f, 0.0f};
	vb[2].Position = {-1.0f, -1.0f, 0.0f};
	vb[3].Position = { 1.0f, -1.0f, 0.0f};
	vb[0].Normal = {0.0f, 0.0f, -1.0f};
	vb[1].Normal = {0.0f, 0.0f, -1.0f};
	vb[2].Normal = {0.0f, 0.0f, -1.0f};
	vb[3].Normal = {0.0f, 0.0f, -1.0f};
	vb[0].UV = {0.0f, 0.0f};
	vb[1].UV = {0.0f, 1.0f};
	vb[2].UV = {1.0f, 0.0f};
	vb[3].UV = {1.0f, 1.0f};

	vertex_buffer_->Unmap(0, nullptr);
	vb = nullptr;



	XMMATRIX view = XMMatrixLookAtLH({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	XMMATRIX projection = XMMatrixOrthographicLH(8.0f, 6.0f, 1.0f, 3.0f);
	XMMATRIX scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX trans = XMMatrixTranslation(-2.9f, 1.9f, 0.0f);

	XMFLOAT4X4 mat;
	XMStoreFloat4x4(&mat, XMMatrixTranspose(scale * trans * view * projection));

	XMFLOAT4X4 *cb{};
	hr = constant_buffer_->Map(0, nullptr, (void**)&cb);
	if(FAILED(hr)){
		return hr;
	}

	//行列を定数バッファに書き込み
	cb[0] = mat;

	constant_buffer_->Unmap(0, nullptr);
	cb = nullptr;



	hr = CreateRootSignature(device);
	if(FAILED(hr)){
		return hr;
	}
	
	hr = CreatePSO(device);
	if(FAILED(hr)){
		return hr;
	}
	
	return S_OK;
}


HRESULT ShadowMapDebug::CreateRootSignature(ID3D12Device * device){
	HRESULT hr{};
	D3D12_DESCRIPTOR_RANGE		range[1]{};
	D3D12_ROOT_PARAMETER		root_parameters[2]{};
	D3D12_ROOT_SIGNATURE_DESC	root_signature_desc{};
	D3D12_STATIC_SAMPLER_DESC	sampler_desc{};
	ComPtr<ID3DBlob> blob{};

	//変換行列用の定数バッファ	
	root_parameters[0].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[0].ShaderVisibility				= D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[0].Descriptor.ShaderRegister	= 0;
	root_parameters[0].Descriptor.RegisterSpace		= 0;


	//テクスチャ
	range[0].NumDescriptors		= 1;
	range[0].BaseShaderRegister = 0;
	range[0].RangeType			= D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	root_parameters[1].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[1].ShaderVisibility				= D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[1].DescriptorTable.pDescriptorRanges   = &range[0];


	//サンプラ
	sampler_desc.Filter				= D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc.AddressV			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc.AddressW			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc.MipLODBias			= 0.0f;
	sampler_desc.MaxAnisotropy		= 16;
	sampler_desc.ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc.BorderColor		= D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler_desc.MinLOD				= 0.0f;
	sampler_desc.MaxLOD				= D3D12_FLOAT32_MAX;
	sampler_desc.ShaderRegister		= 0;
	sampler_desc.RegisterSpace		= 0;
	sampler_desc.ShaderVisibility	= D3D12_SHADER_VISIBILITY_ALL;


	root_signature_desc.Flags				= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	root_signature_desc.NumParameters		= _countof(root_parameters);
	root_signature_desc.pParameters			= root_parameters;
	root_signature_desc.NumStaticSamplers	= 1;
	root_signature_desc.pStaticSamplers		= &sampler_desc;

	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
	if(FAILED(hr)){
		return hr;
	}
	hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_sugnature_));

	return hr;
}

HRESULT ShadowMapDebug::CreatePSO(ID3D12Device * device){
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};
	ComPtr<ID3DBlob> pixel_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(L"ShadowMapDebug.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
	if(FAILED(hr)){
		return hr;
	}

	hr = D3DCompileFromFile(L"ShadowMapDebug.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, &pixel_shader, nullptr);
	if(FAILED(hr)){
		return hr;
	}


	// 頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};

	//シェーダーの設定
	pipeline_state_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
	pipeline_state_desc.VS.BytecodeLength  = vertex_shader->GetBufferSize();
	pipeline_state_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
	pipeline_state_desc.PS.BytecodeLength  = pixel_shader->GetBufferSize();


	//インプットレイアウトの設定
	pipeline_state_desc.InputLayout.pInputElementDescs	= InputElementDesc;
	pipeline_state_desc.InputLayout.NumElements			= _countof(InputElementDesc);


	//サンプル系の設定
	pipeline_state_desc.SampleDesc.Count	= 1;
	pipeline_state_desc.SampleDesc.Quality	= 0;
	pipeline_state_desc.SampleMask			= UINT_MAX;

	//レンダーターゲットの設定
	pipeline_state_desc.NumRenderTargets = 1;
	pipeline_state_desc.RTVFormats[0]    = DXGI_FORMAT_B8G8R8A8_UNORM;

	//三角形に設定
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//ルートシグネチャ
	pipeline_state_desc.pRootSignature = root_sugnature_.Get();


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode				= D3D12_CULL_MODE_BACK;
	pipeline_state_desc.RasterizerState.FillMode				= D3D12_FILL_MODE_SOLID;
	pipeline_state_desc.RasterizerState.FrontCounterClockwise	= FALSE;
	pipeline_state_desc.RasterizerState.DepthBias				= 0;
	pipeline_state_desc.RasterizerState.DepthBiasClamp			= 0;
	pipeline_state_desc.RasterizerState.SlopeScaledDepthBias	= 0;
	pipeline_state_desc.RasterizerState.DepthClipEnable			= TRUE;
	pipeline_state_desc.RasterizerState.ConservativeRaster		= D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipeline_state_desc.RasterizerState.AntialiasedLineEnable	= FALSE;
	pipeline_state_desc.RasterizerState.MultisampleEnable		= FALSE;


	//ブレンドステートの設定
	for(int i = 0; i < _countof(pipeline_state_desc.BlendState.RenderTarget); ++i){
		pipeline_state_desc.BlendState.RenderTarget[i].BlendEnable				= FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlend					= D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlend				= D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOp					= D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlendAlpha			= D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlendAlpha			= D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOpAlpha				= D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].RenderTargetWriteMask	= D3D12_COLOR_WRITE_ENABLE_ALL;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOpEnable			= FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOp					= D3D12_LOGIC_OP_CLEAR;
	}
	pipeline_state_desc.BlendState.AlphaToCoverageEnable  = FALSE;
	pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;


	//デプスステンシルステートの設定
	pipeline_state_desc.DepthStencilState.DepthEnable		= TRUE;								//深度テストあり
	pipeline_state_desc.DepthStencilState.DepthFunc			= D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipeline_state_desc.DepthStencilState.DepthWriteMask	= D3D12_DEPTH_WRITE_MASK_ALL;
	pipeline_state_desc.DepthStencilState.StencilEnable		= FALSE;							//ステンシルテストなし
	pipeline_state_desc.DepthStencilState.StencilReadMask	= D3D12_DEFAULT_STENCIL_READ_MASK;
	pipeline_state_desc.DepthStencilState.StencilWriteMask	= D3D12_DEFAULT_STENCIL_WRITE_MASK;

	pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp		= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp	= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp		= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc			= D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp		= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp	= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp		= D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilFunc			= D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	hr = device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pso_));

	return hr;
}



HRESULT ShadowMapDebug::Draw(ID3D12GraphicsCommandList *command_list, ID3D12DescriptorHeap *dh_sm){
	D3D12_VERTEX_BUFFER_VIEW	 vertex_view{};
	vertex_view.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
	vertex_view.StrideInBytes  = sizeof(Vertex3D);
	vertex_view.SizeInBytes    = sizeof(Vertex3D) * 4;

	command_list->SetGraphicsRootSignature(root_sugnature_.Get());
	command_list->SetPipelineState(pso_.Get());

	command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());


	command_list->SetDescriptorHeaps(1, &dh_sm);
	command_list->SetGraphicsRootDescriptorTable(1, dh_sm->GetGPUDescriptorHandleForHeapStart());

	
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	command_list->IASetVertexBuffers(0, 1, &vertex_view);


	command_list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}


