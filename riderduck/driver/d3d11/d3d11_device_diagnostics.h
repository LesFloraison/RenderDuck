#pragma once

#include "common/common.h"
#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/d3d11sdklayers.h"

// Read-only diagnostics: preserve the original API result and queued validation messages.
inline void LogD3D11Failure(ID3D11Device *device, const char *operation, HRESULT result)
{
  if(SUCCEEDED(result) || device == NULL)
    return;

  const HRESULT reason = device->GetDeviceRemovedReason();
  RDCERR("D3D11 diagnostic: %s returned 0x%08x; GetDeviceRemovedReason=0x%08x; device=%p",
         operation, (uint32_t)result, (uint32_t)reason, device);

  ID3D11InfoQueue *queue = NULL;
  if(FAILED(device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&queue)))
  {
    RDCLOG("D3D11 diagnostic: debug queue unavailable; enable API Validation before launch");
    return;
  }

  const UINT64 count = queue->GetNumStoredMessagesAllowedByRetrievalFilter();
  RDCLOG("D3D11 diagnostic: %llu queued validation messages (showing at most the last 32)", count);
  for(UINT64 i = count > 32 ? count - 32 : 0; i < count; ++i)
  {
    SIZE_T length = 0;
    if(FAILED(queue->GetMessage(i, NULL, &length)) || length < sizeof(D3D11_MESSAGE) ||
       length > 1024 * 1024)
      continue;

    byte *storage = new byte[length];
    D3D11_MESSAGE *message = (D3D11_MESSAGE *)storage;
    if(SUCCEEDED(queue->GetMessage(i, message, &length)))
      RDCLOG("D3D11 diagnostic: severity=%u id=%u: %s", (uint32_t)message->Severity,
             (uint32_t)message->ID, message->pDescription ? message->pDescription : "");
    delete[] storage;
  }
  queue->Release();
}
