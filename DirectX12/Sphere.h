#ifndef SPHERE_HEADER_
#define SPHERE_HEADER_

#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Sphere{
public:
	typedef unsigned char byte;
	typedef unsigned short uint16;
	static constexpr int VERT_NUM = 32;//ˆê‚Â‚ÌŒÊ‚ğì‚é’¸“_‚Ì”
	static constexpr int ARC_NUM = 32;//‹…‚ğì‚éŒÊ‚Ì”

public:
	Sphere();
	~Sphere(){}
	HRESULT Initialize(ID3D12Device *device);
	HRESULT Draw(ID3D12GraphicsCommandList *command_list);

private:
	ComPtr<ID3D12Resource>			vertex_buffer_;
	ComPtr<ID3D12Resource>			index_buffer_;
	ComPtr<ID3D12Resource>			constant_buffer_;
	ComPtr<ID3D12Resource>			texture_;
	ComPtr<ID3D12DescriptorHeap>	dh_texture_;
};

#endif
