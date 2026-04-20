/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "remote_server.h"
#include <utility>
#include "android/android.h"
#include "api/replay/riderduck_replay.h"
#include "api/replay/version.h"
#include "common/threading.h"
#include "core/core.h"
#include "core/settings.h"
#include "os/os_specific.h"
#include "replay/replay_controller.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "replay_proxy.h"

RDOC_CONFIG(uint32_t, RemoteServer_TimeoutMS, 5000,
            "Timeout in milliseconds for remote server operations.");

RDOC_CONFIG(bool, RemoteServer_DebugLogging, false,
            "Output a verbose logging file in the system's temporary folder containing the "
            "traffic to and from the remote server.");

#define MAKE_REMOTE_SERVER_VERSION(maj, min) uint32_t((maj)*1000) + (min)

static const uint32_t RemoteServerProtocolVersion =
    MAKE_REMOTE_SERVER_VERSION(RIDERDUCK_VERSION_MAJOR, RIDERDUCK_VERSION_MINOR);

enum class RemoteServerPacket
{
  // fixed packets. These are used cross-version so MUST NOT CHANGE
  Noop = 1,
  Handshake,
  VersionMismatch,
  Busy,
  VersionMismatch2,    // sent for versions 1.23 and above, including the version info

  // variable packets. These are used only after a handshake has been established with an identical
  // version so can be freely changed
  Ping,
  RemoteDriverList,
  TakeOwnershipCapture,
  CopyCaptureToRemote,
  CopyCaptureFromRemote,
  OpenLog,
  LogOpenProgress,
  LogOpened,
  HasCallstacks,
  InitResolver,
  ResolverProgress,
  GetResolve,
  CloseLog,
  HomeDir,
  ListDir,
  ExecuteAndInject,
  ShutdownServer,
  GetDriverName,
  GetSectionCount,
  FindSectionByName,
  FindSectionByType,
  GetSectionProperties,
  GetSectionContents,
  WriteSection,
  GetAvailableGPUs,
  EmbedDependenciesIntoCapture,
  RemoveDependenciesFromCapture,
  HasEmbeddedDependencies,
  HasPendingDependencies,
  GetPendingDependenciesNicknames,
  // This must be last
  Count,
};

DECLARE_REFLECTION_ENUM(RemoteServerPacket);

RDCCOMPILE_ASSERT((int)RemoteServerPacket::Count < (int)eReplayProxy_First,
                  "Remote server and Replay Proxy packets overlap");

template <>
rdcstr DoStringise(const RemoteServerPacket &el)
{
  BEGIN_ENUM_STRINGISE(RemoteServerPacket);
  {
    STRINGISE_ENUM_NAMED(RemoteServerPacket::Noop, "No-op");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::Handshake, "Handshake");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::VersionMismatch, "VersionMismatch");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::Busy, "Busy");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::VersionMismatch2, "VersionMismatch");

    STRINGISE_ENUM_NAMED(RemoteServerPacket::Ping, "Ping");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::RemoteDriverList, "RemoteDriverList");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::TakeOwnershipCapture, "TakeOwnershipCapture");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::CopyCaptureToRemote, "CopyCaptureToRemote");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::CopyCaptureFromRemote, "CopyCaptureFromRemote");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::OpenLog, "OpenLog");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::LogOpenProgress, "LogOpenProgress");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::LogOpened, "LogOpened");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::HasCallstacks, "HasCallstacks");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::InitResolver, "InitResolver");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::ResolverProgress, "ResolverProgress");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetResolve, "GetResolve");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::CloseLog, "CloseLog");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::HomeDir, "HomeDir");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::ListDir, "ListDir");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::ExecuteAndInject, "ExecuteAndInject");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::ShutdownServer, "ShutdownServer");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetDriverName, "GetDriverName");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetSectionCount, "GetSectionCount");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::FindSectionByName, "FindSectionByName");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::FindSectionByType, "FindSectionByType");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetSectionProperties, "GetSectionProperties");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetSectionContents, "GetSectionContents");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::WriteSection, "WriteSection");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetAvailableGPUs, "GetAvailableGPUs");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::EmbedDependenciesIntoCapture,
                         "EmbedDependenciesIntoCapture");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::RemoveDependenciesFromCapture,
                         "RemoveDependenciesFromCapture");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::HasEmbeddedDependencies, "HasEmbeddedDependencies");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::HasPendingDependencies, "HasPendingDependencies");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::GetPendingDependenciesNicknames,
                         "GetPendingDependenciesNicknames");
    STRINGISE_ENUM_NAMED(RemoteServerPacket::Count, "Count");
  }
  END_ENUM_STRINGISE();
}

rdcstr GetRemoteServerChunkName(uint32_t idx)
{
  if(idx <= (uint32_t)RemoteServerPacket::Count)
    return ToStr((RemoteServerPacket)idx);

  if(idx >= eReplayProxy_First)
    return ToStr((ReplayProxyPacket)idx);

  return StringFormat::Fmt("Invalid RemoteServerChunkIndex %u", idx);
}

#define WRITE_DATA_SCOPE() WriteSerialiser &ser = writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = reader;

struct ClientThread
{
  ClientThread()
      : socket(NULL), allowExecution(false), killThread(false), killServer(false), thread(0)
  {
  }

  Network::Socket *socket;

  bool allowExecution;
  bool killThread;
  bool killServer;

  Threading::ThreadHandle thread;
};

struct ActiveClient
{
  Threading::CriticalSection lock;
  ClientThread *active = NULL;
};

static bool HandleHandshakeClient(ActiveClient &activeClient, ClientThread *threadData)
{
  uint32_t ip = threadData->socket->GetRemoteIP();

  uint32_t version = 0;

  bool activeConnectionDesired = false;
  bool activeConnectionEstablished = false;

  {
    ReadSerialiser ser(new StreamReader(threadData->socket, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    // this thread just handles receiving the handshake and sending a busy signal without blocking
    // the server thread
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(ser.IsErrored() || type != RemoteServerPacket::Handshake)
    {
      RDCWARN("Didn't receive proper handshake");
      return activeConnectionEstablished;
    }

    SERIALISE_ELEMENT(version);
    SERIALISE_ELEMENT(activeConnectionDesired);

    ser.EndChunk();
  }

  {
    WriteSerialiser ser(new StreamWriter(threadData->socket, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    if(version != RemoteServerProtocolVersion)
    {
      RDCLOG("Connection using protocol %u, but we are running %u", version,
             RemoteServerProtocolVersion);

      // as of 1.23 we started serialising our version so the other end knows what it's talking
      // to.
      if(version >= MAKE_REMOTE_SERVER_VERSION(1, 23))
      {
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::VersionMismatch2);

        SERIALISE_ELEMENT(RemoteServerProtocolVersion);
      }
      else
      {
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::VersionMismatch);
      }
    }
    else
    {
      bool busy = false;

      {
        SCOPED_LOCK(activeClient.lock);
        busy = activeClient.active != NULL;

        // if we're not busy, and the connection wants to be active, promote it.
        if(!busy && activeConnectionDesired)
        {
          RDCLOG("Promoting connection from %u.%u.%u.%u to active.", Network::GetIPOctet(ip, 0),
                 Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));
          activeConnectionEstablished = true;
          activeClient.active = threadData;
        }
      }

      // if we were busy, return that status
      if(busy)
      {
        RDCLOG("Returning busy signal for connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
               Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::Busy);
      }
      // otherwise we return a successful handshake. For active connections this begins the active
      // thread, for passive connection checks this is enough
      else
      {
        RDCLOG("Returning OK signal for connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
               Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::Handshake);
      }
    }
  }

  // return whether or not an active connection was established
  return activeConnectionEstablished;
}

static void ActiveRemoteClientThread(ClientThread *threadData,
                                     RIDERDUCK_PreviewWindowCallback previewWindow)
{
  Threading::SetCurrentThreadName("ActiveRemoteClientThread");

  Network::Socket *&client = threadData->socket;

  client->SetTimeout(RemoteServer_TimeoutMS());

  uint32_t ip = client->GetRemoteIP();

  rdcarray<rdcstr> tempFiles;
  IRemoteDriver *remoteDriver = NULL;
  IReplayDriver *replayDriver = NULL;
  ReplayProxy *proxy = NULL;
  RDCFile *rdc = NULL;
  Callstack::StackResolver *resolver = NULL;

  FileIO::LogFileHandle *debugLog = NULL;

  WriteSerialiser writer(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);
  ReadSerialiser reader(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

  if(RemoteServer_DebugLogging())
  {
    reader.ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);
    writer.ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);

    rdcstr filename = FileIO::GetTempFolderFilename() + "/RiderDuck/RemoteServer_Server.log";

    RDCLOG("Logging remote server work to '%s'", filename.c_str());

    // truncate the log
    debugLog = FileIO::logfile_open(filename);
    FileIO::logfile_close(debugLog, filename);
    debugLog = FileIO::logfile_open(filename);

    reader.EnableDumping(debugLog);
    writer.EnableDumping(debugLog);
  }

  writer.SetStreamingMode(true);
  reader.SetStreamingMode(true);

  uint32_t captureNum = 0;

  while(client)
  {
    if(client && !client->Connected())
      break;

    if(threadData->killThread)
      break;

    // this will block until a packet comes in.
    RemoteServerPacket type = reader.ReadChunk<RemoteServerPacket>();

    if(reader.IsErrored() || writer.IsErrored())
      break;

    if(client == NULL)
      continue;

    if(type == RemoteServerPacket::Ping)
    {
      reader.EndChunk();

      if(proxy)
        proxy->RefreshPreviewWindow();

      // insert a dummy line into our logcat so we can keep track of our progress
      Android::TickDeviceLogcat();

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(RemoteServerPacket::Ping);
    }
    else if(type == RemoteServerPacket::RemoteDriverList)
    {
      reader.EndChunk();

      std::map<RDCDriver, rdcstr> drivers = RiderDuck::Inst().GetRemoteDrivers();
      uint32_t count = (uint32_t)drivers.size();

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(RemoteServerPacket::RemoteDriverList);
      SERIALISE_ELEMENT(count);

      for(auto it = drivers.begin(); it != drivers.end(); ++it)
      {
        RDCDriver driverType = it->first;
        const rdcstr &driverName = it->second;

        SERIALISE_ELEMENT(driverType);
        SERIALISE_ELEMENT(driverName);
      }
    }
    else if(type == RemoteServerPacket::HomeDir)
    {
      reader.EndChunk();

      rdcstr home = FileIO::GetHomeFolderFilename();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HomeDir);
        SERIALISE_ELEMENT(home);
      }
    }
    else if(type == RemoteServerPacket::ListDir)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      rdcarray<PathEntry> files;
      FileIO::GetFilesInDirectory(path, files);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ListDir);
        SERIALISE_ELEMENT(files);
      }
    }
    else if(type == RemoteServerPacket::CopyCaptureFromRemote)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::CopyCaptureFromRemote);

        StreamReader fileStream(FileIO::fopen(path, FileIO::ReadBinary));
        ser.SerialiseStream(path, fileStream);
      }
    }
    else if(type == RemoteServerPacket::CopyCaptureToRemote)
    {
      rdcstr path;
      rdcstr dummy, dummy2;
      FileIO::GetDefaultFiles("remotecopy", path, dummy, dummy2);

      // remove the .rdc
      path.erase(path.size() - 4, 4);

      // append a process- and capture- specific suffix to avoid clashes
      path += StringFormat::Fmt("_remotecopy_%u_%u.rdc", Process::GetCurrentPID(), captureNum);
      captureNum++;

      RDCLOG("Copying file to local path '%s'.", path.c_str());

      FileIO::CreateParentDirectory(path);

      {
        READ_DATA_SCOPE();

        StreamWriter streamWriter(FileIO::fopen(path, FileIO::WriteBinary), Ownership::Stream);

        ser.SerialiseStream(path, streamWriter, NULL);
      }

      reader.EndChunk();

      if(reader.IsErrored())
      {
        FileIO::Delete(path);

        RDCERR("Network error receiving file");
        break;
      }

      RDCLOG("File received.");

      tempFiles.push_back(path);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::CopyCaptureToRemote);
        SERIALISE_ELEMENT(path);
      }
    }
    else if(type == RemoteServerPacket::TakeOwnershipCapture)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      RDCLOG("Taking ownership of capture.");

      tempFiles.push_back(path);
    }
    else if(type == RemoteServerPacket::GetAvailableGPUs)
    {
      reader.EndChunk();

      rdcarray<GPUDevice> gpus = RiderDuck::Inst().GetAvailableGPUs();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetAvailableGPUs);
        SERIALISE_ELEMENT(gpus);
      }
    }
    else if(type == RemoteServerPacket::ShutdownServer)
    {
      reader.EndChunk();

      RDCLOG("Requested to shut down.");

      threadData->killServer = true;
      threadData->killThread = true;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ShutdownServer);
      }
    }
    else if(type == RemoteServerPacket::OpenLog)
    {
      rdcstr path;
      ReplayOptions opts;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
        SERIALISE_ELEMENT(opts);
      }

      reader.EndChunk();

      RDCASSERT(remoteDriver == NULL && proxy == NULL && rdc == NULL);

      rdc = new RDCFile();
      rdc->Open(path);

      RDResult result = rdc->Error();

      if(result == ResultCode::Succeeded)
      {
        if(RiderDuck::Inst().HasRemoteDriver(rdc->GetDriver()))
        {
          bool kill = false;
          float progress = 0.0f;

          RiderDuck::Inst().SetProgressCallback<LoadProgress>([&progress](float p) { progress = p; });

          Threading::ThreadHandle ticker = Threading::CreateThread([&writer, &kill, &progress]() {
            while(!kill)
            {
              {
                WRITE_DATA_SCOPE();
                SCOPED_SERIALISE_CHUNK(RemoteServerPacket::LogOpenProgress);
                SERIALISE_ELEMENT(progress);
              }
              Threading::Sleep(100);
            }
          });

          // This has to be before the driver is created for the capture
          RiderDuck::Inst().ClearTrackedFiles();
          if(rdc->SectionIndex(SectionType::EmbeddedExternalFiles) >= 0)
          {
            ResultDetails ret = RiderDuck::Inst().ReadExternalFiles(rdc);
            if(!ret.OK())
            {
              RDCERR("ReadExternalFiles failed Code:'%s' Message:'%s'", ToStr(ret.code).c_str(),
                     ret.Message().c_str());
            }
          }

          // if we have a replay driver, try to create it so we can display a local preview e.g.
          if(RiderDuck::Inst().HasReplayDriver(rdc->GetDriver()))
          {
            result = RiderDuck::Inst().CreateReplayDriver(rdc, opts, &replayDriver);
            if(replayDriver)
              remoteDriver = replayDriver;
          }
          else
          {
            result = RiderDuck::Inst().CreateRemoteDriver(rdc, opts, &remoteDriver);
          }

          if(result != ResultCode::Succeeded || remoteDriver == NULL)
          {
            RDCERR("Failed to create remote driver for driver '%s'", rdc->GetDriverName().c_str());
          }
          else
          {
            result = remoteDriver->ReadLogInitialisation(rdc, false);

            if(result != ResultCode::Succeeded)
            {
              RDCERR("Failed to initialise remote driver.");

              remoteDriver->Shutdown();
              remoteDriver = NULL;
            }
          }

          RiderDuck::Inst().SetProgressCallback<LoadProgress>(RIDERDUCK_ProgressCallback());

          kill = true;
          Threading::JoinThread(ticker);
          Threading::CloseThread(ticker);

          if(result == ResultCode::Succeeded && remoteDriver)
          {
            proxy = new ReplayProxy(reader, writer, remoteDriver, replayDriver, previewWindow);
          }
        }
        else
        {
          SET_ERROR_RESULT(result, ResultCode::APIUnsupported,
                           "File needs driver for '%s' which isn't supported!",
                           rdc->GetDriverName().c_str());
        }
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::LogOpened);
        SERIALISE_ELEMENT(result);
      }
    }
    else if(type == RemoteServerPacket::HasCallstacks)
    {
      reader.EndChunk();

      bool HasCallstacks = rdc && rdc->SectionIndex(SectionType::ResolveDatabase) >= 0;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasCallstacks);
        SERIALISE_ELEMENT(HasCallstacks);
      }
    }
    else if(type == RemoteServerPacket::InitResolver)
    {
      reader.EndChunk();

      RDResult res;

      int sectionIndex = rdc ? rdc->SectionIndex(SectionType::ResolveDatabase) : -1;

      SAFE_DELETE(resolver);
      if(sectionIndex >= 0)
      {
        StreamReader *sectionReader = rdc->ReadSection(sectionIndex);

        bytebuf buf;
        buf.resize((size_t)sectionReader->GetSize());
        bool success = sectionReader->Read(buf.data(), sectionReader->GetSize());

        res = sectionReader->GetError();

        delete sectionReader;

        if(success && res == ResultCode::Succeeded)
        {
          float progress = 0.0f;

          Threading::ThreadHandle ticker = Threading::CreateThread([&writer, &resolver, &progress]() {
            while(!resolver)
            {
              {
                WRITE_DATA_SCOPE();
                SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ResolverProgress);
                SERIALISE_ELEMENT(progress);
              }
              Threading::Sleep(100);
            }
          });

          resolver = Callstack::MakeResolver(false, buf.data(), buf.size(),
                                             [&progress](float p) { progress = p; });

          Threading::JoinThread(ticker);
          Threading::CloseThread(ticker);
        }
        else
        {
          res.message = "Failed to read resolve database. " + res.message;
        }
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::InitResolver);
        SERIALISE_ELEMENT(res);
      }
    }
    else if(type == RemoteServerPacket::GetResolve)
    {
      rdcarray<uint64_t> StackAddresses;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(StackAddresses);
      }

      reader.EndChunk();

      rdcarray<rdcstr> StackFrames;

      if(resolver)
      {
        StackFrames.reserve(StackAddresses.size());
        for(uint64_t frame : StackAddresses)
        {
          Callstack::AddressDetails info = resolver->GetAddr(frame);
          StackFrames.push_back(info.formattedString());
        }
      }
      else
      {
        StackFrames = {""};
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetResolve);
        SERIALISE_ELEMENT(StackFrames);
      }
    }
    else if(type == RemoteServerPacket::GetDriverName)
    {
      reader.EndChunk();

      rdcstr driver = rdc ? rdc->GetDriverName() : "";
      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetDriverName);
        SERIALISE_ELEMENT(driver);
      }
    }
    else if(type == RemoteServerPacket::GetSectionCount)
    {
      reader.EndChunk();

      int count = rdc ? rdc->NumSections() : 0;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionCount);
        SERIALISE_ELEMENT(count);
      }
    }
    else if(type == RemoteServerPacket::FindSectionByName)
    {
      rdcstr name;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(name);
      }

      reader.EndChunk();

      int index = rdc ? rdc->SectionIndex(name) : -1;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::FindSectionByName);
        SERIALISE_ELEMENT(index);
      }
    }
    else if(type == RemoteServerPacket::FindSectionByType)
    {
      SectionType sectionType;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(sectionType);
      }

      reader.EndChunk();

      int index = rdc ? rdc->SectionIndex(sectionType) : -1;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::FindSectionByType);
        SERIALISE_ELEMENT(index);
      }
    }
    else if(type == RemoteServerPacket::GetSectionProperties)
    {
      int index = -1;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(index);
      }

      reader.EndChunk();

      SectionProperties props;
      if(rdc && index >= 0 && index < rdc->NumSections())
        props = rdc->GetSectionProperties(index);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionProperties);
        SERIALISE_ELEMENT(props);
      }
    }
    else if(type == RemoteServerPacket::GetSectionContents)
    {
      int index = -1;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(index);
      }

      reader.EndChunk();

      bytebuf contents;

      if(rdc && index >= 0 && index < rdc->NumSections())
      {
        StreamReader *sectionReader = rdc->ReadSection(index);

        contents.resize((size_t)sectionReader->GetSize());
        bool success = sectionReader->Read(contents.data(), sectionReader->GetSize());

        if(!success)
          contents.clear();

        delete sectionReader;
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionContents);
        SERIALISE_ELEMENT(contents);
      }
    }
    else if(type == RemoteServerPacket::WriteSection)
    {
      SectionProperties props;
      bytebuf contents;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(props);
        SERIALISE_ELEMENT(contents);
      }

      reader.EndChunk();

      RDResult result;

      if(rdc)
      {
        StreamWriter *sectionWriter = rdc->WriteSection(props);

        if(sectionWriter)
        {
          sectionWriter->Write(contents.data(), contents.size());
          delete sectionWriter;
        }
        else
        {
          SET_ERROR_RESULT(result, ResultCode::FileIOFailed, "Failed to write section");
        }
      }
      else
      {
        SET_ERROR_RESULT(result, ResultCode::InternalError,
                         "Attempt to write section with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::WriteSection);
        SERIALISE_ELEMENT(result);
      }
    }
    else if(type == RemoteServerPacket::CloseLog)
    {
      reader.EndChunk();

      SAFE_DELETE(proxy);

      if(remoteDriver)
        remoteDriver->Shutdown();
      remoteDriver = NULL;
      replayDriver = NULL;

      SAFE_DELETE(rdc);
      SAFE_DELETE(resolver);
    }
    else if(type == RemoteServerPacket::ExecuteAndInject)
    {
      rdcstr app, workingDir, cmdLine, logfile;
      CaptureOptions opts;
      rdcarray<EnvironmentModification> env;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(app);
        SERIALISE_ELEMENT(workingDir);
        SERIALISE_ELEMENT(cmdLine);
        SERIALISE_ELEMENT(opts);
        SERIALISE_ELEMENT(env);
      }

      reader.EndChunk();

      RDResult res;
      uint32_t ident = 0;

      if(threadData->allowExecution)
      {
        rdctie(res, ident) =
            Process::LaunchAndInjectIntoProcess(app, workingDir, cmdLine, env, "", opts, false);
      }
      else
      {
        RDCWARN("Requested to execute program - disallowing based on configuration");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ExecuteAndInject);
        SERIALISE_ELEMENT(res);
        SERIALISE_ELEMENT(ident);
      }
    }
    else if(type == RemoteServerPacket::EmbedDependenciesIntoCapture)
    {
      reader.EndChunk();

      RDResult result;
      if(rdc)
      {
        result = RiderDuck::Inst().EmbedExternalFiles(rdc);
      }
      else
      {
        SET_ERROR_RESULT(result, ResultCode::InternalError,
                         "Attempt to embed external dependency files with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::EmbedDependenciesIntoCapture);
        SERIALISE_ELEMENT(result);
      }
    }
    else if(type == RemoteServerPacket::RemoveDependenciesFromCapture)
    {
      reader.EndChunk();

      RDResult result;
      if(rdc)
      {
        result = RiderDuck::Inst().RemoveExternalFiles(rdc);
      }
      else
      {
        SET_ERROR_RESULT(result, ResultCode::InternalError,
                         "Attempt to remove external files with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::RemoveDependenciesFromCapture);
        SERIALISE_ELEMENT(result);
      }
    }
    else if(type == RemoteServerPacket::HasEmbeddedDependencies)
    {
      reader.EndChunk();

      bool res = false;
      if(rdc)
      {
        res = RiderDuck::Inst().HasEmbeddedFiles(rdc);
      }
      else
      {
        RDCWARN("Attempt to check for embedded files with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasEmbeddedDependencies);
        SERIALISE_ELEMENT(res);
      }
    }
    else if(type == RemoteServerPacket::HasPendingDependencies)
    {
      reader.EndChunk();

      bool res = false;
      if(rdc)
      {
        res = RiderDuck::Inst().HasTrackedFileData();
      }
      else
      {
        RDCWARN("Attempt to check for externally referenced files with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasPendingDependencies);
        SERIALISE_ELEMENT(res);
      }
    }
    else if(type == RemoteServerPacket::GetPendingDependenciesNicknames)
    {
      reader.EndChunk();

      rdcarray<rdcstr> res;
      if(rdc)
      {
        res = RiderDuck::Inst().GetTrackedFileNicknames();
      }
      else
      {
        RDCWARN("Attempt to get nickanmes of externally referenced files with no capture open");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetPendingDependenciesNicknames);
        SERIALISE_ELEMENT(res);
      }
    }
    else if((int)type >= eReplayProxy_First && proxy)
    {
      bool ok = proxy->Tick((int)type);

      if(!ok)
        break;

      continue;
    }
  }

  FileIO::logfile_close(debugLog, rdcstr());

  SAFE_DELETE(proxy);

  if(remoteDriver)
    remoteDriver->Shutdown();
  remoteDriver = NULL;
  replayDriver = NULL;
  SAFE_DELETE(rdc);
  SAFE_DELETE(resolver);

  for(size_t i = 0; i < tempFiles.size(); i++)
  {
    FileIO::Delete(tempFiles[i]);
  }

  RDCLOG("Closing active connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
         Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

  RDCLOG("Ready for new active connection...");

  SAFE_DELETE(client);
}

void RiderDuck::BecomeRemoteServer(const rdcstr &listenhost, uint16_t port,
                                   std::function<bool()> killReplay,
                                   RIDERDUCK_PreviewWindowCallback previewWindow)
{
  Network::Socket *sock = Network::CreateServerSocket(listenhost, port, 1);

  if(sock == NULL)
    return;

  rdcarray<rdcpair<uint32_t, uint32_t> > listenRanges;
  bool allowExecution = true;

  FILE *f = FileIO::fopen(FileIO::GetAppFolderFilename("remoteserver.conf"), FileIO::ReadText);

  rdcstr configFile;

  if(f)
  {
    FileIO::fseek64(f, 0, SEEK_END);
    configFile.resize((size_t)FileIO::ftell64(f));
    FileIO::fseek64(f, 0, SEEK_SET);

    FileIO::fread(configFile.data(), 1, configFile.size(), f);

    FileIO::fclose(f);
  }

  rdcarray<rdcstr> lines;
  split(configFile, lines, '\n');

  for(rdcstr &line : lines)
  {
    line.trim();

    if(line == "")
      continue;

    // skip comments
    if(line[0] == '#')
      continue;

    if(line.substr(0, sizeof("whitelist") - 1) == "whitelist")
    {
      uint32_t ip = 0, mask = 0;

      // CIDR notation
      bool found = Network::ParseIPRangeCIDR(line.substr(sizeof("whitelist")), ip, mask);

      if(found)
      {
        listenRanges.push_back(make_rdcpair(ip, mask));
        continue;
      }
      else
      {
        RDCLOG("Couldn't parse IP range from: %s", line.c_str() + sizeof("whitelist"));
      }

      continue;
    }
    else if(line.substr(0, sizeof("noexec") - 1) == "noexec")
    {
      allowExecution = false;

      continue;
    }

    RDCLOG("Malformed line '%s'. See documentation for file format.", line.c_str());
  }

  if(listenRanges.empty())
  {
    RDCLOG("No whitelist IP ranges configured - using default private IP ranges.");
    RDCLOG(
        "Create a config file remoteserver.conf in ~/.riderduck or %%APPDATA%%/riderduck to "
        "narrow "
        "this down or accept connections from more ranges.");

    listenRanges.push_back(make_rdcpair(Network::MakeIP(10, 0, 0, 0), 0xff000000));
    listenRanges.push_back(make_rdcpair(Network::MakeIP(172, 16, 0, 0), 0xfff00000));
    listenRanges.push_back(make_rdcpair(Network::MakeIP(192, 168, 0, 0), 0xffff0000));
  }

  RDCLOG("Allowing connections from:");

  for(size_t i = 0; i < listenRanges.size(); i++)
  {
    uint32_t ip = listenRanges[i].first;
    uint32_t mask = listenRanges[i].second;

    RDCLOG("%u.%u.%u.%u / %u.%u.%u.%u", Network::GetIPOctet(ip, 0), Network::GetIPOctet(ip, 1),
           Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3), Network::GetIPOctet(mask, 0),
           Network::GetIPOctet(mask, 1), Network::GetIPOctet(mask, 2), Network::GetIPOctet(mask, 3));
  }

  if(allowExecution)
    RDCLOG("Allowing execution commands");
  else
    RDCLOG("Blocking execution commands");

  RDCLOG("Replay host ready for requests...");

  ActiveClient activeClientData;

  rdcarray<ClientThread *> clients;

  while(!killReplay())
  {
    Network::Socket *client = sock->AcceptClient(0);

    {
      SCOPED_LOCK(activeClientData.lock);
      if(activeClientData.active && activeClientData.active->killServer)
        break;
    }

    // reap any dead client threads
    for(size_t i = 0; i < clients.size(); i++)
    {
      if(clients[i]->socket == NULL)
      {
        {
          SCOPED_LOCK(activeClientData.lock);
          if(activeClientData.active == clients[i])
            activeClientData.active = NULL;
        }

        Threading::JoinThread(clients[i]->thread);
        Threading::CloseThread(clients[i]->thread);
        delete clients[i];
        clients.erase(i);
        break;
      }
    }

    if(client == NULL)
    {
      if(!sock->Connected())
      {
        RDCERR("Error in accept - shutting down server");

        SAFE_DELETE(sock);
        return;
      }

      Threading::Sleep(5);

      continue;
    }

    uint32_t ip = client->GetRemoteIP();

    RDCLOG("Connection received from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
           Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

    bool valid = false;

    // always allow connections from localhost
    valid = Network::MatchIPMask(ip, Network::MakeIP(127, 0, 0, 1), ~0U);

    for(size_t i = 0; i < listenRanges.size(); i++)
    {
      if(Network::MatchIPMask(ip, listenRanges[i].first, listenRanges[i].second))
      {
        valid = true;
        break;
      }
    }

    if(!valid)
    {
      RDCLOG("Doesn't match any listen range, closing connection.");
      SAFE_DELETE(client);
      continue;
    }

    RDCLOG("Processing connection");

    ClientThread *clientThread = new ClientThread();
    clientThread->socket = client;
    clientThread->allowExecution = allowExecution;
    clientThread->thread =
        Threading::CreateThread([&activeClientData, clientThread, previewWindow]() {
          if(HandleHandshakeClient(activeClientData, clientThread))
          {
            ActiveRemoteClientThread(clientThread, previewWindow);
          }
          else
          {
            SAFE_DELETE(clientThread->socket);
          }
        });

    clients.push_back(clientThread);
  }

  {
    SCOPED_LOCK(activeClientData.lock);
    if(activeClientData.active)
      activeClientData.active->killThread = true;
    activeClientData.active = NULL;
  }

  // shut down client threads
  for(size_t i = 0; i < clients.size(); i++)
  {
    Threading::JoinThread(clients[i]->thread);
    Threading::CloseThread(clients[i]->thread);
    delete clients[i];
  }

  SAFE_DELETE(sock);
}

extern "C" RIDERDUCK_API ResultDetails RIDERDUCK_CC
RIDERDUCK_CreateRemoteServerConnection(const rdcstr &URL, IRemoteServer **rend)
{
  rdcstr host = "localhost";
  if(!URL.empty())
    host = URL;

  rdcstr deviceID = host;

  IDeviceProtocolHandler *protocol = RiderDuck::Inst().GetDeviceProtocol(deviceID);

  uint16_t port = RiderDuck_RemoteServerPort;

  if(protocol)
  {
    deviceID = protocol->GetDeviceID(deviceID);
    host = protocol->RemapHostname(deviceID);
    if(host.empty())
      return RDResult(ResultCode::NetworkIOFailed);

    port = protocol->RemapPort(deviceID, port);
  }
  else
  {
    int32_t idx = deviceID.indexOf(':');
    if(idx > 0)
    {
      host = deviceID.substr(0, idx);
      port = atoi(deviceID.substr(idx + 1).c_str()) & 0xffff;
    }
  }

  if(port == 0)
    return RDResult(ResultCode::NetworkIOFailed);

  Network::Socket *sock = Network::CreateClientSocket(host, port, 750);

  if(sock == NULL)
    return RDResult(ResultCode::NetworkIOFailed);

  uint32_t version = RemoteServerProtocolVersion;

  sock->SetTimeout(RemoteServer_TimeoutMS());

  bool activeConnection = (rend != NULL);

  {
    WriteSerialiser ser(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::Handshake);
    SERIALISE_ELEMENT(version);
    SERIALISE_ELEMENT(activeConnection);
  }

  if(!sock->Connected())
    return RDResult(ResultCode::NetworkIOFailed);

  {
    ReadSerialiser ser(new StreamReader(sock, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    uint32_t remoteVersion = 0;
    if(type == RemoteServerPacket::VersionMismatch2)
    {
      SERIALISE_ELEMENT(remoteVersion);
    }

    ser.EndChunk();

    if(type == RemoteServerPacket::Busy)
    {
      SAFE_DELETE(sock);
      return RDResult(ResultCode::NetworkRemoteBusy);
    }

    if(type == RemoteServerPacket::VersionMismatch || type == RemoteServerPacket::VersionMismatch2)
    {
      SAFE_DELETE(sock);

      rdcstr ver = StringFormat::Fmt("Server on v%d.%d", remoteVersion / 1000, remoteVersion % 1000);

      if(remoteVersion == 0)
        ver = "Server older than v1.23";

      return RDResult(ResultCode::NetworkVersionMismatch, ver);
    }

    if(ser.IsErrored() || type != RemoteServerPacket::Handshake)
    {
      RDCWARN("Didn't get proper handshake");
      SAFE_DELETE(sock);
      return RDResult(ResultCode::NetworkIOFailed);
    }
  }

  if(rend == NULL)
  {
    SAFE_DELETE(sock);
    return RDResult(ResultCode::Succeeded);
  }

  if(protocol)
    *rend = protocol->CreateRemoteServer(sock, deviceID);
  else
    *rend = new RemoteServer(sock, deviceID);

  if(*rend == NULL)
    return RDResult(ResultCode::NetworkIOFailed);

  return RDResult(ResultCode::Succeeded);
}

extern "C" RIDERDUCK_API ResultDetails RIDERDUCK_CC
RIDERDUCK_CheckRemoteServerConnection(const rdcstr &URL)
{
  return RIDERDUCK_CreateRemoteServerConnection(URL, NULL);
}

#undef WRITE_DATA_SCOPE
#undef READ_DATA_SCOPE
#define WRITE_DATA_SCOPE() WriteSerialiser &ser = *writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = *reader;

RemoteServer::RemoteServer(Network::Socket *sock, const rdcstr &deviceID)
    : m_Socket(sock), m_deviceID(deviceID)
{
  reader = new ReadSerialiser(new StreamReader(sock, Ownership::Nothing), Ownership::Stream);
  writer = new WriteSerialiser(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream);

  if(RemoteServer_DebugLogging())
  {
    reader->ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);
    writer->ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);

    rdcstr filename = FileIO::GetTempFolderFilename() + "/RiderDuck/RemoteServer_Client.log";

    RDCLOG("Logging remote server work to '%s'", filename.c_str());

    // truncate the log
    debugLog = FileIO::logfile_open(filename);
    FileIO::logfile_close(debugLog, filename);
    debugLog = FileIO::logfile_open(filename);

    reader->EnableDumping(debugLog);
    writer->EnableDumping(debugLog);
  }
  else
  {
    debugLog = NULL;
  }

  writer->SetStreamingMode(true);
  reader->SetStreamingMode(true);

  std::map<RDCDriver, rdcstr> m = RiderDuck::Inst().GetReplayDrivers();

  m_Proxies.reserve(m.size());
  for(auto it = m.begin(); it != m.end(); ++it)
    m_Proxies.push_back({it->first, it->second});
}

RemoteServer::~RemoteServer()
{
  FileIO::logfile_close(debugLog, rdcstr());
  SAFE_DELETE(writer);
  SAFE_DELETE(reader);
  SAFE_DELETE(m_Socket);
}

void RemoteServer::ShutdownConnection()
{
  delete this;
}

void RemoteServer::ShutdownServerAndConnection()
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ShutdownServer);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();
    ser.EndChunk();

    RDCASSERT(type == RemoteServerPacket::ShutdownServer);
  }

  delete this;
}

bool RemoteServer::Connected()
{
  return m_Socket != NULL && m_Socket->Connected();
}

ResultDetails RemoteServer::Ping()
{
  RDResult ret;

  if(!Connected())
  {
    ret = ResultCode::RemoteServerConnectionLost;
    return ret;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::Ping);
  }

  RemoteServerPacket type;

  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();
    ser.EndChunk();
  }

  if(type == RemoteServerPacket::Ping)
    ret = ResultCode::Succeeded;
  else
    ret = ResultCode::RemoteServerConnectionLost;

  return ret;
}

rdcarray<rdcstr> RemoteServer::LocalProxies()
{
  rdcarray<rdcstr> out;

  m_Proxies.reserve(m_Proxies.size());

  size_t i = 0;
  for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
    out.push_back(it->second);

  return out;
}

rdcarray<rdcstr> RemoteServer::RemoteSupportedReplays()
{
  rdcarray<rdcstr> out;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::RemoteDriverList);
  }

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::RemoteDriverList)
    {
      uint32_t count = 0;
      SERIALISE_ELEMENT(count);

      out.reserve(count);

      for(uint32_t i = 0; i < count; i++)
      {
        RDCDriver driverType = RDCDriver::Unknown;
        rdcstr driverName = "";

        SERIALISE_ELEMENT(driverType);
        SERIALISE_ELEMENT(driverName);

        out.push_back(driverName);
      }
    }
    else
    {
      RDCERR("Unexpected response to remote driver list request");
    }

    ser.EndChunk();
  }

  return out;
}

rdcstr RemoteServer::GetHomeFolder()
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HomeDir);
  }

  rdcstr home;

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::HomeDir)
    {
      SERIALISE_ELEMENT(home);
    }
    else
    {
      RDCERR("Unexpected response to home folder request");
    }

    ser.EndChunk();
  }

  return home;
}

rdcarray<PathEntry> RemoteServer::ListFolder(const rdcstr &path)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ListDir);
    SERIALISE_ELEMENT(path);
  }

  rdcarray<PathEntry> files;

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::ListDir)
    {
      SERIALISE_ELEMENT(files);
    }
    else
    {
      RDCERR("Unexpected response to list directory request");
      files.resize(1);
      files[0].filename = path;
      files[0].flags = PathProperty::ErrorUnknown;
    }

    ser.EndChunk();
  }

  return files;
}

ExecuteResult RemoteServer::ExecuteAndInject(const rdcstr &app, const rdcstr &workingDir,
                                             const rdcstr &cmdline,
                                             const rdcarray<EnvironmentModification> &env,
                                             const CaptureOptions &opts)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::ExecuteAndInject);
    SERIALISE_ELEMENT(app);
    SERIALISE_ELEMENT(workingDir);
    SERIALISE_ELEMENT(cmdline);
    SERIALISE_ELEMENT(opts);
    SERIALISE_ELEMENT(env);
  }

  ExecuteResult ret = {};

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::ExecuteAndInject)
    {
      SERIALISE_ELEMENT_LOCAL(result, RDResult());
      SERIALISE_ELEMENT_LOCAL(ident, uint32_t());

      ret.result = result;
      ret.ident = ident;
    }
    else
    {
      RDCERR("Unexpected response to execute and inject request");
    }

    ser.EndChunk();
  }

  return ret;
}

void RemoteServer::CopyCaptureFromRemote(const rdcstr &remotepath, const rdcstr &localpath,
                                         RIDERDUCK_ProgressCallback progress)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::CopyCaptureFromRemote);
    SERIALISE_ELEMENT(remotepath);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::CopyCaptureFromRemote)
    {
      StreamWriter streamWriter(FileIO::fopen(localpath, FileIO::WriteBinary), Ownership::Stream);

      ser.SerialiseStream(localpath, streamWriter, progress);

      if(ser.IsErrored())
      {
        RDCERR("Network error receiving file");
        return;
      }
    }
    else
    {
      RDCERR("Unexpected response to capture copy request");
    }

    ser.EndChunk();
  }
}

rdcstr RemoteServer::CopyCaptureToRemote(const rdcstr &filename, RIDERDUCK_ProgressCallback progress)
{
  FILE *fileHandle = FileIO::fopen(filename, FileIO::ReadBinary);

  if(!fileHandle)
  {
    RDCERR("Can't open file '%s'", filename.c_str());
    return "";
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::CopyCaptureToRemote);

    // this will take ownership of and close the file
    StreamReader fileStream(fileHandle);
    ser.SerialiseStream(filename, fileStream, progress);
  }

  rdcstr path;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::CopyCaptureToRemote)
    {
      SERIALISE_ELEMENT(path);
    }
    else
    {
      RDCERR("Unexpected response to capture copy request");
    }

    ser.EndChunk();
  }

  return path;
}

void RemoteServer::TakeOwnershipCapture(const rdcstr &filename)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::TakeOwnershipCapture);
    SERIALISE_ELEMENT(filename);
  }
}

rdcpair<ResultDetails, IReplayController *> RemoteServer::OpenCapture(
    uint32_t proxyid, const rdcstr &filename, const ReplayOptions &opts,
    RIDERDUCK_ProgressCallback progress)
{
  rdcpair<ResultDetails, IReplayController *> ret;
  ret.first = ResultCode::InternalError;
  ret.second = NULL;

  if(proxyid != ~0U && proxyid >= m_Proxies.size())
  {
    RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
    return ret;
  }

  RDCLOG("Opening capture remotely");

  LogReplayOptions(opts);

  // if the proxy id is ~0U, then we just don't care so let RiderDuck pick the most
  // appropriate supported proxy for the current platform.
  RDCDriver proxydrivertype = proxyid == ~0U ? RDCDriver::Unknown : m_Proxies[proxyid].first;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::OpenLog);
    SERIALISE_ELEMENT(filename);
    SERIALISE_ELEMENT(opts);
  }

  RemoteServerPacket type = RemoteServerPacket::Noop;
  while(!reader->IsErrored())
  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();

    if(reader->IsErrored() || type != RemoteServerPacket::LogOpenProgress)
      break;

    float progressValue = 0.0f;

    SERIALISE_ELEMENT(progressValue);

    ser.EndChunk();

    if(progress)
      progress(progressValue);
  }

  RDCLOG("Capture open complete");

  if(reader->IsErrored() || type != RemoteServerPacket::LogOpened)
  {
    RDCERR("Error opening capture");
    ret.first = ResultCode::NetworkIOFailed;
    return ret;
  }

  RDResult result = ResultCode::Succeeded;
  {
    READ_DATA_SCOPE();
    SERIALISE_ELEMENT(result);
    ser.EndChunk();
  }

  if(progress)
    progress(1.0f);

  if(result != ResultCode::Succeeded)
  {
    RDCERR("Capture open failed: %s", ResultDetails(result).Message().c_str());
    ret.first = result;
    return ret;
  }

  RDCLOG("Capture ready on replay host");

  IReplayDriver *proxyDriver = NULL;
  result = RiderDuck::Inst().CreateProxyReplayDriver(proxydrivertype, &proxyDriver);

  if(result != ResultCode::Succeeded || !proxyDriver)
  {
    RDCERR("Creating proxy driver failed: %s", ResultDetails(result).Message().c_str());
    if(proxyDriver)
      proxyDriver->Shutdown();
    ret.first = result;
    return ret;
  }

  ReplayController *rend = new ReplayController();

  ReplayProxy *proxy = new ReplayProxy(*reader, *writer, proxyDriver);
  result = rend->SetDevice(proxy);

  if(result != ResultCode::Succeeded)
  {
    rend->Shutdown();
    ret.first = result;
    return ret;
  }

  // ReplayController takes ownership of the ProxySerialiser (as IReplayDriver)
  // and it cleans itself up in Shutdown.

  RDCLOG("Remote capture open complete & proxy ready");

  ret.first = ResultCode::Succeeded;
  ret.second = rend;
  return ret;
}

void RemoteServer::CloseCapture(IReplayController *rend)
{
  rend->Shutdown();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::CloseLog);
  }
}

rdcstr RemoteServer::DriverName()
{
  if(!Connected())
    return "";

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetDriverName);
  }

  rdcstr driverName = "";

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetDriverName)
    {
      SERIALISE_ELEMENT(driverName);
    }
    else
    {
      RDCERR("Unexpected response to GetDriverName");
    }

    ser.EndChunk();
  }

  return driverName;
}

rdcarray<GPUDevice> RemoteServer::GetAvailableGPUs()
{
  if(!Connected())
    return {};

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetAvailableGPUs);
  }

  rdcarray<GPUDevice> gpus;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetAvailableGPUs)
    {
      SERIALISE_ELEMENT(gpus);
    }
    else
    {
      RDCERR("Unexpected response to GetAvailableGPUs");
    }

    ser.EndChunk();
  }

  return gpus;
}

int RemoteServer::GetSectionCount()
{
  if(!Connected())
    return 0;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionCount);
  }

  int count = 0;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetSectionCount)
    {
      SERIALISE_ELEMENT(count);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionCount");
    }

    ser.EndChunk();
  }

  return count;
}

int RemoteServer::FindSectionByName(const rdcstr &name)
{
  if(!Connected())
    return -1;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::FindSectionByName);
    SERIALISE_ELEMENT(name);
  }

  int index = -1;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::FindSectionByName)
    {
      SERIALISE_ELEMENT(index);
    }
    else
    {
      RDCERR("Unexpected response to FindSectionByName");
    }

    ser.EndChunk();
  }

  return index;
}

int RemoteServer::FindSectionByType(SectionType sectionType)
{
  if(!Connected())
    return -1;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::FindSectionByType);
    SERIALISE_ELEMENT(sectionType);
  }

  int index = -1;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::FindSectionByType)
    {
      SERIALISE_ELEMENT(index);
    }
    else
    {
      RDCERR("Unexpected response to FindSectionByType");
    }

    ser.EndChunk();
  }

  return index;
}

SectionProperties RemoteServer::GetSectionProperties(int index)
{
  if(!Connected())
    return SectionProperties();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionProperties);
    SERIALISE_ELEMENT(index);
  }

  SectionProperties props;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetSectionProperties)
    {
      SERIALISE_ELEMENT(props);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionProperties");
    }

    ser.EndChunk();
  }

  return props;
}

bytebuf RemoteServer::GetSectionContents(int index)
{
  if(!Connected())
    return bytebuf();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetSectionContents);
    SERIALISE_ELEMENT(index);
  }

  bytebuf contents;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetSectionContents)
    {
      SERIALISE_ELEMENT(contents);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionContents");
    }

    ser.EndChunk();
  }

  return contents;
}

ResultDetails RemoteServer::WriteSection(const SectionProperties &props, const bytebuf &contents)
{
  RDResult ret;

  if(!Connected())
  {
    ret.code = ResultCode::RemoteServerConnectionLost;
    return ret;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::WriteSection);
    SERIALISE_ELEMENT(props);
    SERIALISE_ELEMENT(contents);
  }

  RDResult success;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::WriteSection)
    {
      SERIALISE_ELEMENT(success);
    }
    else
    {
      RDCERR("Unexpected response to write section request");
    }

    ser.EndChunk();
  }

  return success;
}

bool RemoteServer::HasCallstacks()
{
  if(!Connected())
    return false;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasCallstacks);
  }

  bool hasCallstacks = false;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::HasCallstacks)
    {
      SERIALISE_ELEMENT(hasCallstacks);
    }
    else
    {
      RDCERR("Unexpected response to has callstacks request");
    }

    ser.EndChunk();
  }

  return hasCallstacks;
}

ResultDetails RemoteServer::InitResolver(bool interactive, RIDERDUCK_ProgressCallback progress)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::InitResolver);
  }

  RemoteServerPacket type = RemoteServerPacket::Noop;
  while(!reader->IsErrored())
  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();

    if(reader->IsErrored() || type != RemoteServerPacket::ResolverProgress)
      break;

    float progressValue = 0.0f;

    SERIALISE_ELEMENT(progressValue);

    ser.EndChunk();

    if(progress)
      progress(progressValue);

    RDCLOG("% 3.0f%%...", progressValue * 100.0f);
  }

  RDResult res;

  if(reader->IsErrored() || type != RemoteServerPacket::InitResolver)
  {
    res = ResultCode::NetworkIOFailed;
    return res;
  }

  {
    READ_DATA_SCOPE();
    SERIALISE_ELEMENT(res);
    ser.EndChunk();
  }

  if(progress)
    progress(1.0f);

  return res;
}

rdcarray<rdcstr> RemoteServer::GetResolve(const rdcarray<uint64_t> &callstack)
{
  if(!Connected())
    return {""};

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetResolve);
    SERIALISE_ELEMENT(callstack);
  }

  rdcarray<rdcstr> StackFrames;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetResolve)
    {
      SERIALISE_ELEMENT(StackFrames);
    }
    else
    {
      RDCERR("Unexpected response to resolve request");
    }

    ser.EndChunk();
  }

  return StackFrames;
}

ResultDetails RemoteServer::EmbedDependenciesIntoCapture()
{
  RDResult ret;

  if(!Connected())
  {
    ret.code = ResultCode::RemoteServerConnectionLost;
    return ret;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::EmbedDependenciesIntoCapture);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::EmbedDependenciesIntoCapture)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to embed dependencies into capture request");
    }

    ser.EndChunk();
  }

  return ret;
}

ResultDetails RemoteServer::RemoveDependenciesFromCapture()
{
  RDResult ret;
  if(!Connected())
  {
    ret.code = ResultCode::RemoteServerConnectionLost;
    return ret;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::RemoveDependenciesFromCapture);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::RemoveDependenciesFromCapture)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to remove dependencies from capture request");
    }

    ser.EndChunk();
  }

  return ret;
}

bool RemoteServer::HasEmbeddedDependencies()
{
  bool ret = false;
  if(!Connected())
  {
    return false;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasEmbeddedDependencies);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::HasEmbeddedDependencies)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to has embedded dependencies request");
    }

    ser.EndChunk();
  }
  return ret;
}

bool RemoteServer::HasPendingDependencies()
{
  bool ret = false;
  if(!Connected())
  {
    return false;
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::HasPendingDependencies);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::HasPendingDependencies)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to has pending dependencies request");
    }

    ser.EndChunk();
  }
  return ret;
}

rdcarray<rdcstr> RemoteServer::GetPendingDependenciesNicknames()
{
  rdcarray<rdcstr> ret;
  if(!Connected())
  {
    return ret;
  }
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(RemoteServerPacket::GetPendingDependenciesNicknames);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == RemoteServerPacket::GetPendingDependenciesNicknames)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to get nicknmes of externally referenced files");
    }

    ser.EndChunk();
  }
  return ret;
}
