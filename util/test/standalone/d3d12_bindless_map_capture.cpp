// Regression for persistent mapped inputs used after bindless resource tracking is enabled.
// Build/run instructions are in d3d12_capture_regressions.txt.
#include <d3d12.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include "riderduck/api/app/riderduck_app.h"
using Microsoft::WRL::ComPtr;
#define CHECK(expr)                                              \
  do                                                             \
  {                                                              \
    HRESULT hr = (expr);                                         \
    if(FAILED(hr))                                               \
    {                                                            \
      fprintf(stderr, "%s failed: %08x\n", #expr, unsigned(hr)); \
      return 1;                                                  \
    }                                                            \
  } while(0)

int main(int argc, char **argv)
{
  if(argc != 4)
  {
    fprintf(stderr, "usage: bindless_map_capture DLL CAPTURE_PREFIX table|direct\n");
    return 2;
  }
  HMODULE module = LoadLibraryA(argv[1]);
  if(!module)
    return 3;
  auto getAPI = (pRIDERDUCK_GetAPI)GetProcAddress(module, "RIDERDUCK_GetAPI");
  RIDERDUCK_API_1_7_0 *api = nullptr;
  if(!getAPI || !getAPI(eRIDERDUCK_API_Version_1_7_0, (void **)&api))
    return 4;
  api->SetCaptureFilePathTemplate(argv[2]);
  api->MaskOverlayBits(0, 0);
  bool direct = strcmp(argv[3], "direct") == 0;
  ComPtr<ID3D12Device> device;
  CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  ComPtr<ID3D12CommandQueue> queue;
  D3D12_COMMAND_QUEUE_DESC qdesc = {};
  CHECK(device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue)));
  ComPtr<ID3D12CommandAllocator> alloc;
  CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> list;
  CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr,
                                  IID_PPV_ARGS(&list)));
  CHECK(list->Close());
  ComPtr<ID3D12Fence> fence;
  CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  HANDLE event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if(!event)
    return 5;
  UINT64 fenceValue = 0;
  auto sync = [&]() -> HRESULT {
    HRESULT hr = queue->Signal(fence.Get(), ++fenceValue);
    if(FAILED(hr))
      return hr;
    hr = fence->SetEventOnCompletion(fenceValue, event);
    if(FAILED(hr))
      return hr;
    return WaitForSingleObject(event, 30000) == WAIT_OBJECT_0 ? S_OK : E_FAIL;
  };
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = 65536;
  desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  D3D12_HEAP_PROPERTIES props = {};
  props.Type = D3D12_HEAP_TYPE_UPLOAD;
  ComPtr<ID3D12Resource> input, output, readback;
  CHECK(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                        IID_PPV_ARGS(&input)));
  CHECK(input->SetName(L"bindless map input"));
  props.Type = D3D12_HEAP_TYPE_READBACK;
  CHECK(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                        IID_PPV_ARGS(&readback)));
  props.Type = D3D12_HEAP_TYPE_DEFAULT;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  CHECK(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                        IID_PPV_ARGS(&output)));
  CHECK(output->SetName(L"bindless map output"));
  D3D12_DESCRIPTOR_HEAP_DESC hd = {};
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  hd.NumDescriptors = direct ? 1 : 100001;
  hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ComPtr<ID3D12DescriptorHeap> heap;
  CHECK(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
  D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
  srv.Format = DXGI_FORMAT_R32_TYPELESS;
  srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Buffer.NumElements = 16384;
  srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  device->CreateShaderResourceView(input.Get(), &srv, heap->GetCPUDescriptorHandleForHeapStart());
  D3D12_DESCRIPTOR_RANGE range = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, hd.NumDescriptors, 0, 0, 0};
  D3D12_ROOT_PARAMETER params[3] = {};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable = {1, &range};
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  params[2].Constants.Num32BitValues = 1;
  D3D12_ROOT_SIGNATURE_DESC rs = {};
  rs.NumParameters = 3;
  rs.pParameters = params;
  if(direct)
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
  ComPtr<ID3DBlob> blob, errors;
  CHECK(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
  ComPtr<ID3D12RootSignature> sig;
  CHECK(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                    IID_PPV_ARGS(&sig)));
  const char *shader =
      "ByteAddressBuffer input:register(t0); RWByteAddressBuffer output:register(u0); cbuffer "
      "C:register(b0){uint offset;} [numthreads(1,1,1)] void "
      "main(){output.Store(offset,input.Load(0));}";
  CHECK(D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, &blob,
                   &errors));
  D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
  pd.pRootSignature = sig.Get();
  pd.CS = {blob->GetBufferPointer(), blob->GetBufferSize()};
  ComPtr<ID3D12PipelineState> pso;
  CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso)));
  UINT *mapped = nullptr;
  D3D12_RANGE empty = {};
  CHECK(input->Map(0, &empty, (void **)&mapped));
  memset(mapped, 0, 65536);
  api->StartFrameCapture(device.Get(), nullptr);
  for(UINT i = 0; i < 2; i++)
  {
    // Keep the mapping open and descriptor unchanged across capture and both submissions.
    mapped[0] = i == 0 ? 0x1234 : 0x5678;
    CHECK(alloc->Reset());
    CHECK(list->Reset(alloc.Get(), pso.Get()));
    ID3D12DescriptorHeap *heaps[] = {heap.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(sig.Get());
    list->SetComputeRootDescriptorTable(0, heap->GetGPUDescriptorHandleForHeapStart());
    list->SetComputeRootUnorderedAccessView(1, output->GetGPUVirtualAddress());
    list->SetComputeRoot32BitConstant(2, i * 4, 0);
    list->Dispatch(1, 1, 1);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = output.Get();
    list->ResourceBarrier(1, &barrier);
    CHECK(list->Close());
    ID3D12CommandList *commands[] = {list.Get()};
    queue->ExecuteCommandLists(1, commands);
    CHECK(sync());
  }
  CHECK(alloc->Reset());
  CHECK(list->Reset(alloc.Get(), nullptr));
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Transition.pResource = output.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  list->ResourceBarrier(1, &barrier);
  list->CopyBufferRegion(readback.Get(), 0, output.Get(), 0, 8);
  CHECK(list->Close());
  ID3D12CommandList *commands[] = {list.Get()};
  queue->ExecuteCommandLists(1, commands);
  CHECK(sync());
  if(!api->EndFrameCapture(device.Get(), nullptr))
    return 6;
  input->Unmap(0, nullptr);
  UINT *result = nullptr;
  D3D12_RANGE read = {0, 8};
  CHECK(readback->Map(0, &read, (void **)&result));
  printf("live output: %x %x\n", result[0], result[1]);
  bool passed = result[0] == 0x1234 && result[1] == 0x5678;
  readback->Unmap(0, &empty);
  CloseHandle(event);
  return passed ? 0 : 7;
}
