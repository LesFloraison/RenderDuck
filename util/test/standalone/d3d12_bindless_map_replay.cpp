// Replays the capture produced by d3d12_bindless_map_capture and checks both submissions.
#include <cstdio>
#include <cstring>
#include "riderduck/api/replay/riderduck_replay.h"
REPLAY_PROGRAM_MARKER();

static uint32_t LastEvent(const rdcarray<ActionDescription> &actions)
{
  uint32_t last = 0;
  for(const ActionDescription &action : actions)
  {
    if(action.eventId > last)
      last = action.eventId;
    uint32_t child = LastEvent(action.children);
    if(child > last)
      last = child;
  }
  return last;
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: bindless_map_replay CAPTURE [gpu-validation]\n");
    return 2;
  }
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  RIDERDUCK_InitialiseReplay(env, {});
  RIDERDUCK_SetConfigSetting("D3D12.Debug.EnableDRED")->data.basic.b = true;
  RIDERDUCK_SetConfigSetting("D3D12.Debug.EnableGPUVA")->data.basic.b = argc > 2;
  ICaptureFile *capture = RIDERDUCK_OpenCaptureFile();
  int exitCode = 1;
  if(capture->OpenFile(argv[1], "rdc", nullptr).OK())
  {
    ReplayOptions options;
    options.apiValidation = true;
    auto opened = capture->OpenCapture(options, nullptr);
    if(opened.first.OK() && opened.second)
    {
      IReplayController *controller = opened.second;
      controller->SetFrameEvent(LastEvent(controller->GetRootActions()), true);
      for(const ResourceDescription &resource : controller->GetResources())
      {
        if(resource.name != "bindless map output")
          continue;
        bytebuf data = controller->GetBufferData(resource.resourceId, 0, 8);
        uint32_t values[2] = {};
        if(data.size() == sizeof(values))
          memcpy(values, data.data(), sizeof(values));
        printf("replayed output: %x %x (expected 1234 5678)\n", values[0], values[1]);
        exitCode =
            data.size() == sizeof(values) && values[0] == 0x1234 && values[1] == 0x5678 ? 0 : 1;
      }
      for(const DebugMessage &message : controller->GetDebugMessages())
      {
        fprintf(stderr, "validation event %u: %s\n", message.eventId, message.description.c_str());
        if(message.severity == MessageSeverity::High)
          exitCode = 1;
      }
      controller->Shutdown();
    }
  }
  capture->Shutdown();
  RIDERDUCK_ShutdownReplay();
  return exitCode;
}
