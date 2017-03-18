#ifndef PLANE_HEADER_
#define PLANE_HEADER_

#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Plane{
public:
	Plane();
	~Plane(){}
	HRESULT Initialize(ID3D12Device *device, ID3D12Resource *sm);
	HRESULT Update();
	HRESULT Draw(ID3D12GraphicsCommandList *command_list);

private:
	ComPtr<ID3D12Resource>			vertex_buffer_;
	ComPtr<ID3D12Resource>			constant_buffer_;
	ComPtr<ID3D12Resource>			texture_;
	ComPtr<ID3D12DescriptorHeap>	dh_texture_;
};

#endif
