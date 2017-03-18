#include "Plane.h"
#include "D3D12Manager.h"

Plane::Plane():
	vertex_buffer_{},
	constant_buffer_{}{}

HRESULT Plane::Initialize(ID3D12Device *device){
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


	return S_OK;
}

HRESULT Plane::Draw(ID3D12GraphicsCommandList *command_list){
	HRESULT hr;
	static int Cnt{};
	++Cnt;

	//カメラの設定
	XMMATRIX view = XMMatrixLookAtLH({0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 640.0f / 480.0f, 1.0f, 20.0f);

	//オブジェクトの回転の設定
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(Cnt % 360)));
	rotate *= XMMatrixRotationX(XMConvertToRadians(static_cast<float>((Cnt % 1080)) / 3.0f));
	rotate *= XMMatrixRotationZ(XMConvertToRadians(static_cast<float>(Cnt % 1800)) / 5.0f);

	XMFLOAT4X4 Mat;
	XMStoreFloat4x4(&Mat, XMMatrixTranspose(rotate * view * projection));

	XMFLOAT4X4 *buffer{};
	hr = constant_buffer_->Map(0, nullptr, (void**)&buffer);
	if(FAILED(hr)){
		return hr;
	}

	//行列を定数バッファに書き込み
	*buffer = Mat;

	constant_buffer_->Unmap(0, nullptr);
	buffer = nullptr;



	D3D12_VERTEX_BUFFER_VIEW	 vertex_view{};
	vertex_view.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
	vertex_view.StrideInBytes  = sizeof(Vertex3D);
	vertex_view.SizeInBytes    = sizeof(Vertex3D) * 4;

	//定数バッファをシェーダのレジスタにセット
	command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//インデックスを使用しないトライアングルストリップで描画
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	command_list->IASetVertexBuffers(0, 1, &vertex_view);

	//描画
	command_list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}
