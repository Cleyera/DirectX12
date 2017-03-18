#include <vector>
#include <fstream>
#include "Plane.h"
#include "D3D12Manager.h"

Plane::Plane():
	vertex_buffer_{},
	constant_buffer_{},
	texture_{},
	dh_texture_{}{}

HRESULT Plane::Initialize(ID3D12Device *device, ID3D12Resource *sm){
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
	Vertex3D *buffer{};
	hr = vertex_buffer_->Map(0, nullptr, (void**)&buffer);
	if(FAILED(hr)){
		return hr;
	}
	buffer[0].Position = {-1.0f,  1.0f, 0.0f};
	buffer[1].Position = { 1.0f,  1.0f, 0.0f};
	buffer[2].Position = {-1.0f, -1.0f, 0.0f};
	buffer[3].Position = { 1.0f, -1.0f, 0.0f};
	buffer[0].Normal = {0.0f, 0.0f, -1.0f};
	buffer[1].Normal = {0.0f, 0.0f, -1.0f};
	buffer[2].Normal = {0.0f, 0.0f, -1.0f};
	buffer[3].Normal = {0.0f, 0.0f, -1.0f};
	buffer[0].UV = {0.0f, 0.0f};
	buffer[1].UV = {0.0f, 1.0f};
	buffer[2].UV = {1.0f, 0.0f};
	buffer[3].UV = {1.0f, 1.0f};

	vertex_buffer_->Unmap(0, nullptr);
	buffer = nullptr;





	//画像の読み込み
	std::vector<byte> img;
	std::fstream fs("wall", std::ios_base::in | std::ios_base::binary);
	if(!fs){
		return E_FAIL;
	}
	int width;
	int height;
	fs.read((char*)&width, sizeof(width));
	fs.read((char*)&height, sizeof(height));
	img.resize(width * height * 4);
	fs.read((char*)&img[0], img.size());
	fs.close();


	//テクスチャ用のリソースの作成
	heap_properties.Type                 = D3D12_HEAP_TYPE_CUSTOM;
	heap_properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	resource_desc.Dimension	= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Width		= width;
	resource_desc.Height	= height;
	resource_desc.Format	= DXGI_FORMAT_B8G8R8A8_UNORM;
	resource_desc.Layout	= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&texture_));
	if(FAILED(hr)){
		return hr;
	}


	//テクスチャ用のデスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 2;
	descriptor_heap_desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask		= 0;
	hr = device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&dh_texture_));
	if (FAILED(hr)) {
		return hr;
	}


	//シェーダリソースビューの作成
	D3D12_CPU_DESCRIPTOR_HANDLE handle_srv{};
	D3D12_SHADER_RESOURCE_VIEW_DESC resourct_view_desc{};

	resourct_view_desc.Format							= DXGI_FORMAT_B8G8R8A8_UNORM;
	resourct_view_desc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
	resourct_view_desc.Texture2D.MipLevels				= 1;
	resourct_view_desc.Texture2D.MostDetailedMip		= 0;
	resourct_view_desc.Texture2D.PlaneSlice				= 0;
	resourct_view_desc.Texture2D.ResourceMinLODClamp	= 0.0F;
	resourct_view_desc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle_srv = dh_texture_->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(texture_.Get(), &resourct_view_desc, handle_srv);

	resourct_view_desc.Format							= DXGI_FORMAT_R32_FLOAT;
	handle_srv.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(sm, &resourct_view_desc, handle_srv);

	//画像データの書き込み
	D3D12_BOX box = {0, 0, 0, (UINT)width, (UINT)height, 1};
	hr = texture_->WriteToSubresource(0, &box, &img[0], 4 * width, 4 * width * height);
	if(FAILED(hr)){
		return hr;
	}


	return S_OK;
}

HRESULT Plane::Update(){
	HRESULT hr;
	static constexpr float PI = 3.14159265358979323846264338f;
	static int cnt{};
	++cnt;

	//カメラの設定
	XMMATRIX view = XMMatrixLookAtLH({1.0f, 1.0f, -6.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 640.0f / 480.0f, 1.0f, 20.0f);

	//オブジェクトの回転の設定
	XMMATRIX scale = XMMatrixScaling(3.0f, 3.0f, 1.0f);
	XMMATRIX rotate = XMMatrixRotationX(XMConvertToRadians(90.0f + 10.0f * sin(PI * (cnt % 144) / 72.0f)));
	rotate *= XMMatrixRotationY(-PI * (cnt % 400) / 200.0f);
	XMMATRIX trans = XMMatrixTranslation(0.0f, -2.0f, 0.0f);

	XMFLOAT4X4 Mat;
	XMStoreFloat4x4(&Mat, XMMatrixTranspose(scale * rotate * trans * view * projection));

	XMFLOAT4X4 World;
	XMStoreFloat4x4(&World, XMMatrixTranspose(scale * rotate * trans));


	XMFLOAT4X4 *buffer{};
	hr = constant_buffer_->Map(0, nullptr, (void**)&buffer);
	if(FAILED(hr)){
		return hr;
	}

	//行列を定数バッファに書き込み
	buffer[0] = Mat;
	buffer[1] = World;

	constant_buffer_->Unmap(0, nullptr);
	buffer = nullptr;

	return hr;
}

HRESULT Plane::Draw(ID3D12GraphicsCommandList *command_list){
	D3D12_VERTEX_BUFFER_VIEW	 vertex_view{};
	vertex_view.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
	vertex_view.StrideInBytes  = sizeof(Vertex3D);
	vertex_view.SizeInBytes    = sizeof(Vertex3D) * 4;

	//定数バッファをシェーダのレジスタにセット
	command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//テクスチャをシェーダのレジスタにセット
	command_list->SetDescriptorHeaps(1, dh_texture_.GetAddressOf());
	command_list->SetGraphicsRootDescriptorTable(2, dh_texture_->GetGPUDescriptorHandleForHeapStart());

	//インデックスを使用しないトライアングルストリップで描画
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	command_list->IASetVertexBuffers(0, 1, &vertex_view);

	//描画
	command_list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}
