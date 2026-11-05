// Multithreaded resource creation/release across capture start, capture, and capture end.
// Build/run instructions are in d3d12_capture_regressions.txt.
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include "riderduck/api/app/riderduck_app.h"

int main(int argc, char **argv)
{
  if(argc < 3)
  {
    fprintf(stderr, "usage: capture_stress DLL CAPTURE_PREFIX [churn]\n");
    return 2;
  }
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  HMODULE module = LoadLibraryA(argv[1]);
  if(!module)
    return 3;
  auto getAPI = (pRIDERDUCK_GetAPI)GetProcAddress(module, "RIDERDUCK_GetAPI");
  RIDERDUCK_API_1_7_0 *api = nullptr;
  if(!getAPI || !getAPI(eRIDERDUCK_API_Version_1_7_0, (void **)&api))
    return 4;
  api->SetCaptureFilePathTemplate(argv[2]);
  api->MaskOverlayBits(0, 0);
  ID3D12Device *device = nullptr;
  HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
  if(FAILED(hr))
    return 5;
  D3D12_COMMAND_QUEUE_DESC qdesc = {};
  ID3D12CommandQueue *queue = nullptr;
  if(FAILED(device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue))))
    return 6;
  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = 65536;
  desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  std::atomic<int> ready(0), failures(0);
  std::atomic<bool> go(false), running(true);
  bool churn = argc > 3;
  auto work = [&]() {
    ready++;
    while(!go)
      std::this_thread::yield();
    for(unsigned i = 0; churn ? running.load() : i < 512; ++i)
    {
      ID3D12Resource *resource = nullptr;
      HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                       IID_PPV_ARGS(&resource));
      if(FAILED(result))
      {
        failures++;
        break;
      }
      resource->Release();
    }
  };
  for(int round = 0; round < 5; ++round)
  {
    ready = 0;
    go = false;
    running = true;
    std::vector<std::thread> workers;
    if(!churn)
      api->StartFrameCapture(device, nullptr);
    for(int i = 0; i < 8; ++i)
      workers.emplace_back(work);
    while(ready != 8)
      std::this_thread::yield();
    go = true;
    if(churn)
    {
      Sleep(100);
      api->StartFrameCapture(device, nullptr);
      Sleep(50);
    }
    if(!churn)
      for(auto &worker : workers)
        worker.join();
    unsigned ok = api->EndFrameCapture(device, nullptr);
    if(churn)
    {
      running = false;
      for(auto &worker : workers)
        worker.join();
    }
    printf("round %d captured=%u creation_failures=%d\n", round, ok, failures.load());
    fflush(stdout);
    if(!ok || failures)
      return 7;
  }
  queue->Release();
  device->Release();
  return 0;
}
