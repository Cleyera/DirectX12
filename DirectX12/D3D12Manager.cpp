#include <fstream>
#include "D3D12Manager.h"

D3D12Manager::D3D12Manager(HWND hwnd, int window_width, int window_height):
	window_handle_(hwnd),
	window_width_(window_width),
	window_height_(window_height),
	frames_{},
	rtv_index_{}{

	CreateFactory();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain();
	CreateRenderTargetView();
	CreateDepthStencilBuffer();
	CreateCommandList();
	CreateRootSignature();
	CreatePipelineStateObject();
	CreateLightBuffer();
	CreateShadowBuffer();
	CreateShadowMapPipelineState();

	viewport_.TopLeftX = 0.f; 
	viewport_.TopLeftY = 0.f;
	viewport_.Width    = (FLOAT)window_width_;
	viewport_.Height   = (FLOAT)window_height_;
	viewport_.MinDepth = 0.f;
	viewport_.MaxDepth = 1.f;

	scissor_rect_.top    = 0;
	scissor_rect_.left   = 0;
	scissor_rect_.right  = window_width_;
	scissor_rect_.bottom = window_height_;

	plane_.Initialize(device_.Get(), shadow_buffer_.Get());
	sphere_.Initialize(device_.Get(), shadow_buffer_.Get());
	sm_debug_.Initialize(device_.Get());

}

D3D12Manager::~D3D12Manager(){}


//ファクトリの作成
HRESULT D3D12Manager::CreateFactory(){
	HRESULT hr{};
	UINT flag{};

	//デバッグモードの場合はデバッグレイヤーを有効にする
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debug;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));
		if(FAILED(hr)){
			return hr;
		}
		debug->EnableDebugLayer();
	}

	flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	//ファクトリの作成
	hr = CreateDXGIFactory2(flag, IID_PPV_ARGS(factory_.GetAddressOf()));

	return hr;
}


//デバイスの作成
HRESULT D3D12Manager::CreateDevice(){
	HRESULT hr{};

	//最初に見つかったアダプターを使用する
	hr = factory_->EnumAdapters(0, (IDXGIAdapter**)adapter_.GetAddressOf());
	
	if(SUCCEEDED(hr)){

		//見つかったアダプタを使用してデバイスを作成する
		hr = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&device_));

	}

	return hr;
}


//コマンドキューの作成
HRESULT D3D12Manager::CreateCommandQueue(){
	HRESULT hr{};
	D3D12_COMMAND_QUEUE_DESC command_queue_desc{};

	// コマンドキューを生成.
	command_queue_desc.Type			= D3D12_COMMAND_LIST_TYPE_DIRECT;
	command_queue_desc.Priority		= 0;
	command_queue_desc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE;
	command_queue_desc.NodeMask		= 0;
	hr = device_->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(command_queue_.GetAddressOf()));
	if (FAILED(hr)) {
		return hr;
	}

	//コマンドキュー用のフェンスの生成
	fence_event_ = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if(fence_event_ == NULL ){
		return E_FAIL;
	}
	hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(queue_fence_.GetAddressOf()));
	
	return hr;
}


//スワップチェインの作成
HRESULT D3D12Manager::CreateSwapChain(){
	HRESULT hr{};
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	ComPtr<IDXGISwapChain> swap_chain{};

	swap_chain_desc.BufferDesc.Width	= window_width_;
	swap_chain_desc.BufferDesc.Height	= window_height_;
	swap_chain_desc.OutputWindow		= window_handle_;
	swap_chain_desc.Windowed			= TRUE;
	swap_chain_desc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount			= RTV_NUM;//裏と表で２枚作りたいので2を指定する
	swap_chain_desc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.Flags				= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator   = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferDesc.Format			= DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swap_chain_desc.BufferDesc.Scaling			= DXGI_MODE_SCALING_UNSPECIFIED;
	swap_chain_desc.SampleDesc.Count   = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	

	hr = factory_->CreateSwapChain(command_queue_.Get(), &swap_chain_desc, swap_chain.GetAddressOf());
	if(FAILED(hr)){
		return hr;
	}

	hr = swap_chain->QueryInterface(swap_chain_.GetAddressOf());
	if(FAILED(hr)){
		return hr;
	}

	//カレントのバックバッファのインデックスを取得する
	rtv_index_ = swap_chain_->GetCurrentBackBufferIndex();

	return S_OK;
}


//レンダーターゲットの作成
HRESULT D3D12Manager::CreateRenderTargetView(){
	HRESULT hr{};
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

	//RTV用デスクリプタヒープの作成
	heap_desc.NumDescriptors	= RTV_NUM;
	heap_desc.Type				= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.Flags				= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heap_desc.NodeMask			= 0;
	hr = device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(dh_rtv_.GetAddressOf()));
	if (FAILED(hr)) {
		return hr;
	}

	//レンダーターゲットを作成する処理
	UINT size = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for(UINT i = 0; i < RTV_NUM; ++i){
		//スワップチェインからバッファを受け取る
		hr = swap_chain_->GetBuffer(i, IID_PPV_ARGS(&render_target_[i]));
		if(FAILED(hr)){
			return hr;
		}

		//RenderTargetViewを作成してヒープデスクリプタに登録
		rtv_handle_[i] = dh_rtv_->GetCPUDescriptorHandleForHeapStart();
		rtv_handle_[i].ptr += size * i;
		device_->CreateRenderTargetView(render_target_[i].Get(), nullptr, rtv_handle_[i]);
	}

	return hr;
}


//深度バッファの作成
HRESULT D3D12Manager::CreateDepthStencilBuffer(){
	HRESULT hr;

	//深度バッファ用のデスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 1; 
	descriptor_heap_desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_DSV; 
	descriptor_heap_desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_NONE; 
	descriptor_heap_desc.NodeMask		= 0;
	hr = device_->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&dh_dsv_));
	if(FAILED(hr)){
		return hr;
	}


	//深度バッファの作成
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC resource_desc{};
	D3D12_CLEAR_VALUE clear_value{};

	heap_properties.Type					= D3D12_HEAP_TYPE_DEFAULT;
	heap_properties.CPUPageProperty			= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask		= 0;
	heap_properties.VisibleNodeMask			= 0;

	resource_desc.Dimension				= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Width					= window_width_;
	resource_desc.Height				= window_height_;
	resource_desc.DepthOrArraySize		= 1;
	resource_desc.MipLevels				= 0;
	resource_desc.Format				= DXGI_FORMAT_R32_TYPELESS;
	resource_desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.SampleDesc.Count		= 1;
	resource_desc.SampleDesc.Quality	= 0;
	resource_desc.Flags					= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	clear_value.Format					= DXGI_FORMAT_D32_FLOAT;
	clear_value.DepthStencil.Depth		= 1.0f;
	clear_value.DepthStencil.Stencil	= 0;

	hr = device_->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&depth_buffer_));
	if(FAILED(hr)){
		return hr;
	}


	//深度バッファのビューの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};

	dsv_desc.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Format				= DXGI_FORMAT_D32_FLOAT;
	dsv_desc.Texture2D.MipSlice = 0;
	dsv_desc.Flags				= D3D12_DSV_FLAG_NONE;

	dsv_handle_ = dh_dsv_->GetCPUDescriptorHandleForHeapStart();

	device_->CreateDepthStencilView(depth_buffer_.Get(), &dsv_desc, dsv_handle_);

	return hr;
}

//コマンドリストの作成
HRESULT D3D12Manager::CreateCommandList(){
	HRESULT hr;

	//コマンドアロケータの作成
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator_));
	if (FAILED(hr)) {
		return hr;
	}

	//コマンドアロケータとバインドしてコマンドリストを作成する
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator_.Get(), nullptr, IID_PPV_ARGS(&command_list_));
	
	return hr;
}


//通常描画用のルートシグネチャの作成
HRESULT D3D12Manager::CreateRootSignature(){
	HRESULT hr{};
	D3D12_DESCRIPTOR_RANGE		range[2]{};
	D3D12_ROOT_PARAMETER		root_parameters[4]{};
	D3D12_ROOT_SIGNATURE_DESC	root_signature_desc{};
	D3D12_STATIC_SAMPLER_DESC	sampler_desc[2]{};
	ComPtr<ID3DBlob> blob{};

	//変換行列用の定数バッファ	
	root_parameters[0].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[0].ShaderVisibility				= D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[0].Descriptor.ShaderRegister	= 0;
	root_parameters[0].Descriptor.RegisterSpace		= 0;

	//ライト用の定数バッファ
	root_parameters[1].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[1].ShaderVisibility				= D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[1].Descriptor.ShaderRegister	= 1;
	root_parameters[1].Descriptor.RegisterSpace		= 0;


	//テクスチャ
	range[0].NumDescriptors     = 2;
	range[0].BaseShaderRegister = 0;
	range[0].RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	root_parameters[2].ParameterType				= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[2].ShaderVisibility				= D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[2].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[2].DescriptorTable.pDescriptorRanges   = &range[0];


	//サンプラ
	sampler_desc[0].Filter				= D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc[0].AddressU			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].AddressV			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].AddressW			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].MipLODBias			= 0.0f;
	sampler_desc[0].MaxAnisotropy		= 16;
	sampler_desc[0].ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc[0].BorderColor			= D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler_desc[0].MinLOD				= 0.0f;
	sampler_desc[0].MaxLOD				= D3D12_FLOAT32_MAX;
	sampler_desc[0].ShaderRegister		= 0;
	sampler_desc[0].RegisterSpace		= 0;
	sampler_desc[0].ShaderVisibility	= D3D12_SHADER_VISIBILITY_ALL;

	sampler_desc[1].Filter				= D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc[1].AddressU			= D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].AddressV			= D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].AddressW			= D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].MipLODBias			= 0.0f;
	sampler_desc[1].MaxAnisotropy		= 16;
	sampler_desc[1].ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc[1].BorderColor			= D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler_desc[1].MinLOD				= 0.0f;
	sampler_desc[1].MaxLOD				= D3D12_FLOAT32_MAX;
	sampler_desc[1].ShaderRegister		= 1;
	sampler_desc[1].RegisterSpace		= 0;
	sampler_desc[1].ShaderVisibility	= D3D12_SHADER_VISIBILITY_ALL;


	root_signature_desc.Flags				= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	root_signature_desc.NumParameters		= _countof(root_parameters);
	root_signature_desc.pParameters			= root_parameters;
	root_signature_desc.NumStaticSamplers	= _countof(sampler_desc);
	root_signature_desc.pStaticSamplers		= sampler_desc;

	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
	if(FAILED(hr)){
		return hr;
	}
	hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_sugnature_));
	
	return hr;
}


//通常描画用のパイプラインステートの作成
HRESULT D3D12Manager::CreatePipelineStateObject(){
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};
	ComPtr<ID3DBlob> pixel_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
	if(FAILED(hr)){
		return hr;
	}

	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, &pixel_shader, nullptr);
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

	hr = device_->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state_));
	
	return hr;
}


//ライト用定数バッファの作成
HRESULT D3D12Manager::CreateLightBuffer(){
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


	hr = device_->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&light_buffer_));
	if(FAILED(hr)){
		return hr;
	}


	viewport_sm_.TopLeftX	= 0.f; 
	viewport_sm_.TopLeftY	= 0.f;
	viewport_sm_.Width		= 1024.0f;
	viewport_sm_.Height		= 1024.0f;
	viewport_sm_.MinDepth	= 0.f;
	viewport_sm_.MaxDepth	= 1.f;

	scissor_rect_sm_.top    = 0;
	scissor_rect_sm_.left   = 0;
	scissor_rect_sm_.right  = 1024;
	scissor_rect_sm_.bottom = 1024;


	struct Light{
		XMFLOAT4X4 light_vp;
		XMFLOAT4 color;
		XMFLOAT3 dir;
	};

	Light *p{};
	hr = light_buffer_->Map(0, nullptr, (void**)&p);
	if(FAILED(hr)){
		return hr;
	}

	light_pos_ = {2.0f, 6.5f, -1.0f};
	light_dst_ = {0.0f, 0.0f, 0.0f};
	light_dir_ = {
		light_pos_.x - light_dst_.x,
		light_pos_.y - light_dst_.y,
		light_pos_.z - light_dst_.z
	};
	iight_color_ = {1.0f, 1.0f, 1.0f, 1.0f};
	XMMATRIX view = XMMatrixLookAtLH(
		{light_pos_.x, light_pos_.y, light_pos_.z},
		{light_dst_.x, light_dst_.y, light_dst_.z},
		{0.0f, 1.0f, 0.0f}
	);

	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 1.0f, 1.0f, 15.0f);
	XMFLOAT4X4 mat;
	XMStoreFloat4x4(&mat, XMMatrixTranspose(view * projection));


	p->light_vp = mat;
	p->dir = light_dir_;
	p->color = iight_color_;
	

	light_buffer_->Unmap(0, nullptr);
	p = nullptr;

	return hr;
}


//シャドーマッピング用の深度バッファの作成
HRESULT D3D12Manager::CreateShadowBuffer(){
	HRESULT hr;


	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 1; 
	descriptor_heap_desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_DSV; 
	descriptor_heap_desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_NONE; 
	descriptor_heap_desc.NodeMask		= 0;
	hr = device_->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&dh_shadow_buffer_));
	if(FAILED(hr)){
		return hr;
	}

	descriptor_heap_desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; 
	descriptor_heap_desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; 
	hr = device_->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&dh_shadow_texture_));
	if(FAILED(hr)){
		return hr;
	}


	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC resource_desc{};
	D3D12_CLEAR_VALUE clear_value{};
	heap_properties.Type					= D3D12_HEAP_TYPE_DEFAULT;
	heap_properties.CPUPageProperty			= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask		= 0;
	heap_properties.VisibleNodeMask			= 0;


	resource_desc.Dimension				= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Width					= 1024;
	resource_desc.Height				= 1024;
	resource_desc.DepthOrArraySize		= 1;
	resource_desc.MipLevels				= 0;
	resource_desc.Format				= DXGI_FORMAT_R32_TYPELESS;
	resource_desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.SampleDesc.Count		= 1;
	resource_desc.SampleDesc.Quality	= 0;
	resource_desc.Flags					= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	clear_value.Format					= DXGI_FORMAT_D32_FLOAT;
	clear_value.DepthStencil.Depth		= 1.0f;
	clear_value.DepthStencil.Stencil	= 0;

	hr = device_->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, &clear_value, IID_PPV_ARGS(&shadow_buffer_));
	if(FAILED(hr)){
		return hr;
	}


	//深度バッファのビューの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
	dsv_desc.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Format				= DXGI_FORMAT_D32_FLOAT;
	dsv_desc.Texture2D.MipSlice = 0;
	dsv_desc.Flags				= D3D12_DSV_FLAG_NONE;
	device_->CreateDepthStencilView(shadow_buffer_.Get(), &dsv_desc, dh_shadow_buffer_->GetCPUDescriptorHandleForHeapStart());



	D3D12_SHADER_RESOURCE_VIEW_DESC resourct_view_desc{};
	resourct_view_desc.Format							= DXGI_FORMAT_R32_FLOAT;
	resourct_view_desc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
	resourct_view_desc.Texture2D.MipLevels				= 1;
	resourct_view_desc.Texture2D.MostDetailedMip		= 0;
	resourct_view_desc.Texture2D.PlaneSlice				= 0;
	resourct_view_desc.Texture2D.ResourceMinLODClamp	= 0.0F;
	resourct_view_desc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	device_->CreateShaderResourceView(shadow_buffer_.Get(), &resourct_view_desc, dh_shadow_texture_->GetCPUDescriptorHandleForHeapStart());

	return hr;
}


//シャドーマッピング用のパイプラインステートの作成
HRESULT D3D12Manager::CreateShadowMapPipelineState(){
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "VSShadowMap", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
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


	//インプットレイアウトの設定
	pipeline_state_desc.InputLayout.pInputElementDescs	= InputElementDesc;
	pipeline_state_desc.InputLayout.NumElements			= _countof(InputElementDesc);


	//サンプル系の設定
	pipeline_state_desc.SampleDesc.Count	= 1;
	pipeline_state_desc.SampleDesc.Quality	= 0;
	pipeline_state_desc.SampleMask			= UINT_MAX;

	//三角形に設定
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//ルートシグネチャ
	pipeline_state_desc.pRootSignature = root_sugnature_.Get();


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode				= D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;
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
	pipeline_state_desc.BlendState.RenderTarget[0].BlendEnable				= FALSE;
	pipeline_state_desc.BlendState.RenderTarget[0].SrcBlend					= D3D12_BLEND_ONE;
	pipeline_state_desc.BlendState.RenderTarget[0].DestBlend				= D3D12_BLEND_ZERO;
	pipeline_state_desc.BlendState.RenderTarget[0].BlendOp					= D3D12_BLEND_OP_ADD;
	pipeline_state_desc.BlendState.RenderTarget[0].SrcBlendAlpha			= D3D12_BLEND_ONE;
	pipeline_state_desc.BlendState.RenderTarget[0].DestBlendAlpha			= D3D12_BLEND_ZERO;
	pipeline_state_desc.BlendState.RenderTarget[0].BlendOpAlpha				= D3D12_BLEND_OP_ADD;
	pipeline_state_desc.BlendState.RenderTarget[0].RenderTargetWriteMask	= D3D12_COLOR_WRITE_ENABLE_ALL;
	pipeline_state_desc.BlendState.RenderTarget[0].LogicOpEnable			= FALSE;
	pipeline_state_desc.BlendState.RenderTarget[0].LogicOp					= D3D12_LOGIC_OP_CLEAR;
	
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

	hr = device_->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&shadow_map_pso_));

	return hr;
}


//描画の終了待ち
HRESULT D3D12Manager::WaitForPreviousFrame(){
	HRESULT hr;
	
	++frames_;
	const UINT64 fence = frames_;


	hr = command_queue_->Signal(queue_fence_.Get(), fence);
	if(FAILED(hr)){
		return hr; 
	}

	if (queue_fence_->GetCompletedValue() < fence){
		hr = queue_fence_->SetEventOnCompletion(fence, fence_event_);
		if(FAILED(hr)){
			return hr; 
		}

		WaitForSingleObject(fence_event_, INFINITE);
	}
	return S_OK;
}

//リソースの設定変更用
HRESULT D3D12Manager::SetResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES BeforeState, D3D12_RESOURCE_STATES AfterState){
	D3D12_RESOURCE_BARRIER resource_barrier{};
	
	resource_barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resource_barrier.Transition.pResource   = resource;
	resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resource_barrier.Transition.StateBefore = BeforeState;
	resource_barrier.Transition.StateAfter  = AfterState;

	command_list_->ResourceBarrier(1, &resource_barrier);
	return S_OK;
}


//描画コマンドを積む
HRESULT D3D12Manager::PopulateCommandList(){
	HRESULT hr;
	
	FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	sphere_.Update();
	plane_.Update();

	//シャドーマップ用の深度バッファの描画------------------------------------------------------------------------------------------

	//シャドーマップ用のテクスチャを深度書き込みに設定
	SetResourceBarrier(shadow_buffer_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//シャドーマップ用の深度バッファをクリア
	command_list_->ClearDepthStencilView(dh_shadow_buffer_->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


	//ルートシグネチャとPSOの設定
	command_list_->SetGraphicsRootSignature(root_sugnature_.Get());
	command_list_->SetPipelineState(shadow_map_pso_.Get());


	//ビューポートとシザー矩形の設定
	command_list_->RSSetViewports(1, &viewport_sm_);
	command_list_->RSSetScissorRects(1, &scissor_rect_sm_);


	//深度バッファの設定
	command_list_->OMSetRenderTargets(0, nullptr, FALSE, &dh_shadow_buffer_->GetCPUDescriptorHandleForHeapStart());


	//ライトの設定
	command_list_->SetGraphicsRootConstantBufferView(1, light_buffer_->GetGPUVirtualAddress());


	//球の描画
	sphere_.Draw(command_list_.Get());


	//板ポリの描画
	plane_.Draw(command_list_.Get());


	//シャドーマップ用のテクスチャをジェネリックリードに設定(そのうちちゃんと設定する)
	SetResourceBarrier(shadow_buffer_.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
	
	hr = command_list_->Close();
	ExecuteCommandList();




	//通常のモデルの描画------------------------------------------------------------------------------------------------------------
	
	
	//リソースの状態をプレゼント用からレンダーターゲット用に変更
	SetResourceBarrier(render_target_[rtv_index_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


	//深度バッファとレンダーターゲットのクリア
	command_list_->ClearDepthStencilView(dsv_handle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	command_list_->ClearRenderTargetView(rtv_handle_[rtv_index_], clear_color, 0, nullptr);
	
	//ルートシグネチャとPSOの設定
	command_list_->SetGraphicsRootSignature(root_sugnature_.Get());
	command_list_->SetPipelineState(pipeline_state_.Get());
	
	//ビューポートとシザー矩形の設定
	command_list_->RSSetViewports(1, &viewport_);
	command_list_->RSSetScissorRects(1, &scissor_rect_);

	//レンダーターゲットの設定
	command_list_->OMSetRenderTargets(1, &rtv_handle_[rtv_index_], TRUE, &dsv_handle_);

	//ライトの設定
	command_list_->SetGraphicsRootConstantBufferView(1, light_buffer_->GetGPUVirtualAddress());


	//球の描画
	sphere_.Draw(command_list_.Get());
	
	hr = command_list_->Close();
	ExecuteCommandList();






	//ルートシグネチャとPSOの設定
	command_list_->SetGraphicsRootSignature(root_sugnature_.Get());
	command_list_->SetPipelineState(pipeline_state_.Get());

	//ビューポートとシザー矩形の設定
	command_list_->RSSetViewports(1, &viewport_);
	command_list_->RSSetScissorRects(1, &scissor_rect_);

	//レンダーターゲットの設定
	command_list_->OMSetRenderTargets(1, &rtv_handle_[rtv_index_], TRUE, &dsv_handle_);

	//ライトの設定
	command_list_->SetGraphicsRootConstantBufferView(1, light_buffer_->GetGPUVirtualAddress());


	//板ポリの描画
	plane_.Draw(command_list_.Get());
	
	hr = command_list_->Close();
	ExecuteCommandList();


	//ルートシグネチャとPSOの設定
	command_list_->SetGraphicsRootSignature(root_sugnature_.Get());
	command_list_->SetPipelineState(pipeline_state_.Get());

	//ビューポートとシザー矩形の設定
	command_list_->RSSetViewports(1, &viewport_);
	command_list_->RSSetScissorRects(1, &scissor_rect_);

	//レンダーターゲットの設定
	command_list_->OMSetRenderTargets(1, &rtv_handle_[rtv_index_], TRUE, &dsv_handle_);


	//シャドウマップのデバッグ表示
	sm_debug_.Draw(command_list_.Get(), dh_shadow_texture_.Get());


	//リソースの状態をレンダーターゲットからプレゼント用に変更
	SetResourceBarrier(render_target_[rtv_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	hr = command_list_->Close();
	ExecuteCommandList();

	return hr;
}

HRESULT D3D12Manager::ExecuteCommandList(){
	HRESULT hr;

	ID3D12CommandList *const command_lists = command_list_.Get();
	command_queue_->ExecuteCommandLists(1, &command_lists);

	//実行したコマンドの終了待ち
	WaitForPreviousFrame();


	hr = command_allocator_->Reset();
	if(FAILED(hr)){
		return hr; 
	}

	hr = command_list_->Reset(command_allocator_.Get(), nullptr);
	if(FAILED(hr)){
		return hr; 
	}

	return hr;
}


//画面をフリップする
HRESULT D3D12Manager::Render(){
	HRESULT hr;


	PopulateCommandList();


	hr = swap_chain_->Present(1, 0);
	if(FAILED(hr)){
		return hr;
	}

	//カレントのバックバッファのインデックスを取得する
	rtv_index_ = swap_chain_->GetCurrentBackBufferIndex();

	return S_OK;
}

