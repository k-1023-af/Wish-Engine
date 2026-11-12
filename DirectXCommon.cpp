#include "DirectXCommon.h"
#include <cassert>
#include <format>
#include <thread>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

// helper
ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisible) {

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		: D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

void DirectXCommon::Initialize(WinApp* winApp) {
	assert(winApp);
	this->winApp = winApp;

	InitializeFixFPS();

	Device();
	Command();
	SwapChain();
	//Buffer();
	DescriptorHeap();
	RenderTargetView();
	DepthStencilView();
	Fence();
	ViewportScissor();
	DXCompiler();

#ifdef USE_IMGUI
	ImGuiInitialize();
#endif

	//// prepare first command list
	//hr = commandAllocator->Reset();
	//assert(SUCCEEDED(hr));
	//hr = commandList->Reset(commandAllocator.Get(), nullptr);
	//assert(SUCCEEDED(hr));
}

void DirectXCommon::PreDraw() {
	// === Render ===
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// Transition to Render Target
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//set descriptor heap (required for ImGui)
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void DirectXCommon::PostDraw() {
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// Transition back to Present
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// Execute
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	swapChain->Present(1, 0);

	// Fence synchronization
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	if (fence->GetCompletedValue() != fenceValue) {
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		fence->SetEventOnCompletion(fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		//CloseHandle(event);
	}

	UpdateFixFPS();

	// Reset command list
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::Device() {
	///device initialization

#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	//HRESULTはWindows系のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	//ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。最初にはnullptrを入れておく
	ComPtr<IDXGIAdapter4> useAdapter;

	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, 
		IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
		++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		useAdapter->GetDesc3(&adapterDesc);
		//assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			//Log(std::format(L"Use Adapter:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	//Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, 
		D3D_FEATURE_LEVEL_12_1, 
		D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			//Log(std::format("Feature level: {}\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device != nullptr);

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//D3D12_MESSAGE_ID denyIds[] = {
		//	D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		//};
		//D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		//D3D12_INFO_QUEUE_FILTER filter{};
		//filter.DenyList.NumIDs = _countof(denyIds);
		//filter.DenyList.pIDList = denyIds;
		//filter.DenyList.NumSeverities = _countof(severities);
		//filter.DenyList.pSeverityList = severities;
		//
		//infoQueue->PushStorageFilter(&filter);
		//infoQueue->Release();
	}

#endif
}

void DirectXCommon::Command() {
	//Command Allocator
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	//Command List
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	//Command queue
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::SwapChain() {
	//Swap Chain
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swapChain1;
	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(), 
		winApp->GetHwnd(), 
		&swapChainDesc, 
		nullptr, nullptr, 
		&swapChain1);
	assert(SUCCEEDED(hr));

	hr = swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
	assert(SUCCEEDED(hr));
}

//void DirectXCommon::Buffer() {
//	///Buffer initialization
//	//Index (combine 2 triangles)
//	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
//	indexResourceSprite->SetName(L"indexResourceSprite");
//	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
//	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
//	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
//	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
//
//	uint32_t* indexDataSprite = nullptr;
//	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
//	indexDataSprite[0] = 0;
//	indexDataSprite[1] = 1;
//	indexDataSprite[2] = 2;
//	indexDataSprite[3] = 1;
//	indexDataSprite[4] = 3;
//	indexDataSprite[5] = 2;
//
//	//WVP Resource (For use for one size)
//	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
//	wvpResource->SetName(L"wvpResource");
//	Matrix4x4* wvpData = nullptr;
//	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
//	MakeIdentity4x4(*wvpData);	//Identity matrix for WVP (World-View-Projection)
//
//	//Material Resource (For simplicity, using a single color as material data. In a real application, this would likely be more complex and involve textures, multiple parameters, etc.)
//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device, sizeof(Material));
//	materialResource->SetName(L"materialResource");
//	Material* materialData = nullptr;
//	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
//	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; //-----Color of the Triangle--------//
//	materialData->uvTransform = { //---UV Transform of the Triangle----//
//			1.0f, 0.0f, 0.0f,
//			0.0f, 1.0f, 0.0f,
//			0.0f, 0.0f, 1.0f
//	};
//
//	//Material Resource for Lighting 
//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));
//	materialResourceSprite->SetName(L"materialResourceSprite");
//	Material* materialSpriteData = nullptr;
//	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialSpriteData));
//	materialSpriteData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; //Lighting color
//	materialSpriteData->enableLighting = 0;
//	materialSpriteData->lightingMode = 1;
//	materialSpriteData->halfLambertPower = 2.0f;
//	//materialSpriteData->uvTransform = {
//	//	 1.0f, 1.0f, 1.0f,
//	//	 0.0f, 0.0f, 0.0f,
//	//	 0.0f, 0.0f, 0.0f 
//	//};
//
//	//Directional light values
//	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceLight = CreateBufferResource(device, sizeof(DirectionalLight));
//	materialResourceLight->SetName(L"materialResourceLight");
//	DirectionalLight* directionalLightData = nullptr;
//	materialResourceLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
//	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
//	directionalLightData->direction = { 0.0f, -1.0f, 2.0f };
//	directionalLightData->intensity = 1.0f;
//	float halfLambertPower = 2.0f;
//}

void DirectXCommon::DescriptorHeap() {
	///DescriptorHeap initialization

	// RTV / SRV / DSV heaps
	rtvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	srvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	dsvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// Cache descriptor sizes
	descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void DirectXCommon::RenderTargetView() {
	//rtv settings
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	//Pull Resources from Swap Chain (create rtv)
	for (UINT i = 0; i < 2; ++i) {
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i])); // hr =
		assert(SUCCEEDED(hr));
		
		//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
		rtvHandles[i] = GetCPUDescriptorHandle(rtvDescriptorHeap.Get(), descriptorSizeRTV, i);
		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);
	}
}

void DirectXCommon::DepthStencilView() {

	// Depth Buffer
	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Width = kClientWidth;
	depthDesc.Height = kClientHeight;
	depthDesc.MipLevels = 1;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProp{};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;

	hr = device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&depthStencilResource));
	assert(SUCCEEDED(hr));

	// Create DSV
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	//dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvHandle);
	//
	//D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//depthStencilDesc.DepthEnable = true;
	//depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//depthStencilDesc.StencilEnable = false;
}

void DirectXCommon::Fence() {
	
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	
	//Fence signal
	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent);
	
	//hr = commandList->Close();
	//assert(SUCCEEDED(hr));
	//
	//fenceValue++;
	//
	////Fence value for GPU
	//if (fence->GetCompletedValue() < fenceValue) {
	//	fence->SetEventOnCompletion(fenceValue, fenceEvent);
	//
	//	WaitForSingleObject(fenceEvent, INFINITE);
	//}
}

void DirectXCommon::ViewportScissor() {
	//Viewport and Scissor
	viewport.Width = static_cast<float>(kClientWidth);
	viewport.Height = static_cast<float>(kClientHeight);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;
}

void DirectXCommon::DXCompiler() {
	
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// not used, but create default include handler for later use
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::ImGuiInitialize() {
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(
		device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	//ImGuiIO& io = ImGui::GetIO();
	//io.Fonts->Build();
#endif
}

void DirectXCommon::ExecuteAndWait() {
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists);

	// Fence wait
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// Prepare for the next recording
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index) {
	return GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index) {
	return GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, index);
}

ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	
	// Load file
	ComPtr<IDxcBlobEncoding> shaderSource;
	hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = shaderSource->GetBufferPointer();
	sourceBuffer.Size = shaderSource->GetBufferSize();
	sourceBuffer.Encoding = DXC_CP_UTF8;

	// Compile
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi", L"-Qembed_debug",
		L"-Od",
		L"-Zpr"
	};

	ComPtr<IDxcResult> result;
	hr = dxcCompiler->Compile(
		&sourceBuffer,
		arguments, _countof(arguments),
		includeHandler.Get(),
		IID_PPV_ARGS(&result));
	assert(SUCCEEDED(hr));

	// Error check
	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		OutputDebugStringA(errors->GetStringPointer());
		assert(false);
	}

	ComPtr<IDxcBlob> shaderBlob;
	hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	return shaderBlob;
}

ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// Resource description for buffer
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//// FIXED BUG HERE: Buffers cannot be Depth Stencils!
	//resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resourceDesc.Format = DXGI_FORMAT_UNKNOWN;

	ComPtr<ID3D12Resource> resource;
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	
	return resource;
}

ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
	//1. metadata base resource settings
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//2. Heap settings
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	//3. Create Resource
	ComPtr<ID3D12Resource> resource = nullptr;
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	

	// Use the actual number of subresources the resource has
	// (safer than trusting subresources.size() blindly)
	D3D12_RESOURCE_DESC desc = texture->GetDesc();
	UINT numSubresources = static_cast<UINT>(subresources.size());

	// Optional safety clamp (helps catch the exact mismatch)
	UINT maxSubresources = desc.MipLevels * desc.DepthOrArraySize;
	if (numSubresources > maxSubresources) {
		OutputDebugStringA("UploadTextureData: subresource count mismatch!\n");
		numSubresources = maxSubresources;
	}

	const UINT64 intermediateSize = GetRequiredIntermediateSize(texture, 0, numSubresources);
	ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);
	//intermediateResource->SetName(L"intermediateResource");
	UpdateSubresources(commandList.Get(), texture, intermediateResource.Get(), 0, 0, numSubresources, subresources.data());

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

void DirectXCommon::InitializeFixFPS() {
	reference_ = std::chrono::steady_clock::now();

}

void DirectXCommon::UpdateFixFPS() {
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f)); // 60 FPS

	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f)); // 65 FPS

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	if (elapsed < kMinTime) {
		while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	reference_ = std::chrono::steady_clock::now();
}
