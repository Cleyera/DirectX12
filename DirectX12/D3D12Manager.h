#ifndef DEF_D3D12MANAGER_H
#define DEF_D3D12MANAGER_H

#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "Plane.h"
#include "Sphere.h"
#include "ShadowMapDebug.h"


using namespace DirectX;
using namespace Microsoft::WRL;

struct Vertex3D{
	XMFLOAT3 Position;	//位置
	XMFLOAT3 Normal;	//法線
	XMFLOAT2 UV;		//UV座標
};


class D3D12Manager{
public:
	static constexpr int RTV_NUM = 2;

public:
	D3D12Manager(HWND hwnd, int window_width, int window_height);
	~D3D12Manager();
	HRESULT CreateFactory();
	HRESULT CreateDevice();
	HRESULT CreateCommandQueue();
	HRESULT CreateSwapChain();
	HRESULT CreateRenderTargetView();
	HRESULT CreateDepthStencilBuffer();
	HRESULT CreateCommandList();
	HRESULT CreateRootSignature();
	HRESULT CreatePipelineStateObject();
	HRESULT CreateLightBuffer();
	HRESULT CreateShadowBuffer();
	HRESULT CreateShadowMapPipelineState();
	HRESULT WaitForPreviousFrame();
	HRESULT SetResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES BeforeState, D3D12_RESOURCE_STATES AfterState);
	HRESULT PopulateCommandList();
	HRESULT ExecuteCommandList();
	HRESULT Render();

private:
	HWND window_handle_;
	int window_width_;
	int window_height_;
	
	UINT64 frames_;
	UINT rtv_index_;

	ComPtr<IDXGIFactory4>				factory_;
	ComPtr<IDXGIAdapter3>				adapter_;
	ComPtr<ID3D12Device>				device_;
	ComPtr<ID3D12CommandQueue>			command_queue_;
	HANDLE								fence_event_;
	ComPtr<ID3D12Fence>					queue_fence_;
	ComPtr<IDXGISwapChain3>				swap_chain_;
	ComPtr<ID3D12GraphicsCommandList>	command_list_;
	ComPtr<ID3D12CommandAllocator>		command_allocator_;
	ComPtr<ID3D12Resource>				render_target_[RTV_NUM];
	ComPtr<ID3D12DescriptorHeap>		dh_rtv_;
	D3D12_CPU_DESCRIPTOR_HANDLE			rtv_handle_[RTV_NUM];
	ComPtr<ID3D12Resource>				depth_buffer_;
	ComPtr<ID3D12DescriptorHeap>		dh_dsv_;
	D3D12_CPU_DESCRIPTOR_HANDLE			dsv_handle_;
	ComPtr<ID3D12PipelineState>			pipeline_state_;
	ComPtr<ID3D12RootSignature>			root_sugnature_;
	
	
	ComPtr<ID3D12Resource>				light_buffer_;		//ライト用の定数バッファ
	ComPtr<ID3D12DescriptorHeap>		dh_shadow_buffer_;	//シャドウマップ用深度バッファ用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap>		dh_shadow_texture_;	//シャドウマップ用深度テクスチャ用デスクリプタヒープ
	ComPtr<ID3D12Resource>				shadow_buffer_;		//シャドウマップ用深度バッファ
	ComPtr<ID3D12PipelineState>			shadow_map_pso_;	//シャドウマップ用のパイプライン
	D3D12_RECT							scissor_rect_sm_;
	D3D12_VIEWPORT						viewport_sm_;


	XMFLOAT3							light_pos_;			//ライトの位置
	XMFLOAT3							light_dst_;			//ライトの注視点
	XMFLOAT3							light_dir_;			//ディレクショナルライトの向き
	XMFLOAT4							iight_color_;		//ディレクショナルライトの色
	
	
	D3D12_RECT							scissor_rect_;
	D3D12_VIEWPORT						viewport_;

	Plane plane_;
	Sphere sphere_;
	ShadowMapDebug sm_debug_;

};

#endif
