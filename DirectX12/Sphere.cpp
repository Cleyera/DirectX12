#include <fstream>
#include <vector>
#include "Sphere.h"
#include "D3D12Manager.h"

namespace{
constexpr float PI = 3.14159265358979323846f;
}

Sphere::Sphere():
	vertex_buffer_{},
	index_buffer_{},
	constant_buffer_{},
	texture_{},
	dh_texture_{}{}

HRESULT Sphere::Initialize(ID3D12Device *device){
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


	//定数バッファの作成
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constant_buffer_));
	if(FAILED(hr)){
		return hr;
	}
	
	
	//頂点バッファの作成
	resource_desc.Width = sizeof(Vertex3D) * VERT_NUM * ARC_NUM;
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffer_));
	if(FAILED(hr)){
		return hr;
	}

	//インデックスバッファの作成
	resource_desc.Width = sizeof(uint16) * (VERT_NUM - 1) * ARC_NUM * 6;
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&index_buffer_));
	if(FAILED(hr)){
		return hr;
	}

	
	
	Vertex3D *vb{};
	hr = vertex_buffer_->Map(0, nullptr, (void**)&vb);
	if(FAILED(hr)){
		return hr;
	}

	uint16 *ib{};
	hr = index_buffer_->Map(0, nullptr, (void**)&ib);
	if(FAILED(hr)){
		return hr;
	}


	const float pd = 2.0f * PI / (ARC_NUM - 1);
	const float td = PI / (VERT_NUM - 1);
	float phi{};

	//頂点データの書き込み
	for(int i = 0; i < ARC_NUM; ++i){
		float t{};
		for(int j = 0; j < VERT_NUM; ++j){
			vb[i * VERT_NUM + j].Position = {sinf(t) * cosf(phi), cosf(t), sinf(t) * sinf(phi)};
			vb[i * VERT_NUM + j].Normal = vb[i * VERT_NUM + j].Position;//半径1の球なので、座標がそのまま法線として使える
			vb[i * VERT_NUM + j].UV = {i / (float)(ARC_NUM - 1), j / (float)(VERT_NUM - 1)};
			t += td;
		}

		phi += pd;
	}


	//インデックスの書き込み
	int idx{};
	for(int i = 0; i < ARC_NUM - 1; ++i){
		for(int j = 0; j < VERT_NUM - 1; ++j){
			ib[0 + idx] = (VERT_NUM * i + j);
			ib[1 + idx] = (VERT_NUM * i + j + 1);
			ib[2 + idx] = (VERT_NUM * (i + 1) + j);
			ib[3 + idx] = (VERT_NUM * (i + 1) + j);
			ib[4 + idx] = (VERT_NUM * i + j + 1);
			ib[5 + idx] = (VERT_NUM * (i + 1) + j + 1);
			idx += 6;
		}
	}
	

	index_buffer_->Unmap(0, nullptr);
	ib = nullptr;

	vertex_buffer_->Unmap(0, nullptr);
	vb = nullptr;




	//画像の読み込み
	std::vector<byte> img;
	std::fstream fs("earth", std::ios_base::in | std::ios_base::binary);
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
	descriptor_heap_desc.NumDescriptors = 1;
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


	//画像データの書き込み
	D3D12_BOX box = {0, 0, 0, (UINT)width, (UINT)height, 1};
	hr = texture_->WriteToSubresource(0, &box, &img[0], 4 * width, 4 * width * height);
	if(FAILED(hr)){
		return hr;
	}

	return S_OK;
}

HRESULT Sphere::Draw(ID3D12GraphicsCommandList *command_list){
	HRESULT hr;
	static int Cnt{};
	++Cnt;

	//カメラの設定
	XMMATRIX view = XMMatrixLookAtLH({0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 640.0f / 480.0f, 1.0f, 20.0f);

	//オブジェクトの回転の設定
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(Cnt % 1800)) / 5.0f);
	

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



	D3D12_VERTEX_BUFFER_VIEW vertex_view{};
	vertex_view.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
	vertex_view.StrideInBytes  = sizeof(Vertex3D);
	vertex_view.SizeInBytes    = sizeof(Vertex3D) * VERT_NUM * ARC_NUM;

	
	D3D12_INDEX_BUFFER_VIEW index_view{};
	index_view.BufferLocation = index_buffer_->GetGPUVirtualAddress();
	index_view.SizeInBytes    = sizeof(uint16) * (VERT_NUM - 1) * ARC_NUM * 6;
	index_view.Format         = DXGI_FORMAT_R16_UINT;



	//定数バッファをシェーダのレジスタにセット
	command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());


	//テクスチャをシェーダのレジスタにセット
	ID3D12DescriptorHeap* heaps[] = {dh_texture_.Get()};
	command_list->SetDescriptorHeaps(_countof(heaps), heaps);
	command_list->SetGraphicsRootDescriptorTable(1, dh_texture_->GetGPUDescriptorHandleForHeapStart());


	//インデックスを使用し、トライアングルリストを描画
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_view);
	command_list->IASetIndexBuffer(&index_view);

	//描画
	command_list->DrawIndexedInstanced((VERT_NUM - 1) * ARC_NUM * 6, 1, 0, 0, 0);

	return S_OK;
}



