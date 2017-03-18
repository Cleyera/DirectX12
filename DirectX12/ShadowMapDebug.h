#ifndef SHADOW_MAP_DEBUG_HEADER_
#define SHADOW_MAP_DEBUG_HEADER_

#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class ShadowMapDebug{
public:
	ShadowMapDebug();
	~ShadowMapDebug(){}
	HRESULT Initialize(ID3D12Device *device);
	HRESULT Draw(ID3D12GraphicsCommandList *command_list, ID3D12DescriptorHeap *dh_sm);

private:
	HRESULT CreateRootSignature(ID3D12Device *device);
	HRESULT CreatePSO(ID3D12Device *device);

private:
	ComPtr<ID3D12RootSignature>		root_sugnature_;
	ComPtr<ID3D12PipelineState>		pso_;
	ComPtr<ID3D12Resource>			vertex_buffer_;
	ComPtr<ID3D12Resource>			constant_buffer_;

};

#endif
