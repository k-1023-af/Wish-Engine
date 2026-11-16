#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <array>
#include <chrono>
#include <wrl.h>
#include <string>
#include <dxcapi.h>

#include "WinApp.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "StringUtility.h"

using namespace Microsoft::WRL;
//using namespace StringUtility;


//Healper function to create Descriptor Heap
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, UINT index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
	return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, UINT descriptorSize, UINT index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
	return handle;
}

static DirectX::ScratchImage LoadTexture(const std::wstring& filePath) {
	DirectX::ScratchImage image{};
	//std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;
}

// Blend mode enum (needed by the pipeline arrays)
enum BlendMode {
	kBlendModeNone,
	kBlendModeNormal,
	kBlendModeAdd,
	kBlendModeSubtract,
	kBlendModeMultiply,
	kBlendModeScreen,
	kCountOfBlendMode,
};

class DirectXCommon {
public:
	void Initialize(WinApp* winApp);
	void PreDraw();
	void PostDraw();

	// Initialization functions for each component
	void Device();
	void Command();
	void SwapChain();
	//void Buffer();
	void DescriptorHeap();
	void RenderTargetView();
	void DepthStencilView();
	void Fence();
	void ViewportScissor();
	void DXCompiler();
	void ImGuiInitialize();

	void ExecuteAndWait();

	// Getters
	ID3D12Device* GetDevice() { return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() { return commandList.Get(); }
	ID3D12CommandQueue* GetCommandQueue() { return commandQueue.Get(); }
	IDXGISwapChain4* GetSwapChain() { return swapChain.Get(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap.Get(); }


	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	// Utility
	ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath, const wchar_t* profile);
	ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* textureResource, const DirectX::ScratchImage& mipImages);

	// Public so main can set them for now (can be moved later)
	float clearColor[4] = { 0.1f, 0.25f, 0.5f, 1.0f };
	int currentBlendMode = kBlendModeNormal;

	// Pipeline objects - PreDraw needs them
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> graphicsPipelineStates[kCountOfBlendMode];

	ComPtr<ID3D12RootSignature> particleRootSignature;
	ComPtr<ID3D12PipelineState> particlePipelineStates[kCountOfBlendMode];

private:
	void InitializeFixFPS();
	void UpdateFixFPS();

	// Core DirectX objects
	ComPtr<ID3D12Device > device;
	ComPtr<IDXGIFactory7> dxgiFactory;
	WinApp* winApp = nullptr;

	// Command Objects
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;

	// Swap Chain
	ComPtr<IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	std::array<ComPtr<ID3D12Resource>, 2> swapChainResources;

	// RTV
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	// DSV + Depth buffer
	ComPtr<ID3D12Resource> depthStencilResource;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};

	// SRV / CBV / UAV heap (ImGui + textures/particles)
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	UINT descriptorSizeSRV = 0;
	UINT descriptorSizeRTV = 0;
	UINT descriptorSizeDSV = 0;

	// Fence
	UINT64 fenceValue = 0;
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;

	// Viewport / Scissor
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	// Resource barrier
	D3D12_RESOURCE_BARRIER barrier{};

	// DXCompiler
	ComPtr<IDxcUtils> dxcUtils;
	ComPtr<IDxcCompiler3> dxcCompiler;
	ComPtr<IDxcIncludeHandler> includeHandler;

	// FPS
	std::chrono::steady_clock::time_point reference_;

	//
	HRESULT hr = S_OK;
};

