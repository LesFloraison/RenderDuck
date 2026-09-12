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

// Internal Windows global-hook transaction. Kept independent of the capture runtime so that
// path handling and failure recovery can be exercised without changing machine registry values.
#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace GlobalHook
{
struct Component
{
  std::wstring shim, command, library;
  REGSAM view;
  const wchar_t *architecture;
};

inline std::wstring Error(const std::wstring &operation, DWORD code)
{
  return operation + L" (Windows error " + std::to_wstring(code) + L").";
}

inline bool CheckPathSyntax(const std::wstring &path, std::wstring &reason)
{
  if(path.size() >= MAX_PATH)
    reason = L"the complete component path must be shorter than MAX_PATH (260 characters)";
  else if(path.size() < 3 ||
          !((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) ||
          path[1] != L':' || path[2] != L'\\')
    reason =
        L"a local drive-letter absolute path is required; UNC and device paths are unsupported";
  else
  {
    for(wchar_t c : path)
    {
      if(c > 0x7e || c < 0x21 || c == L',')
      {
        reason =
            L"component paths must use ASCII characters without whitespace, control "
            L"characters or commas";
        return false;
      }
    }
    return true;
  }
  return false;
}

inline bool ValidatePath(std::wstring &path, const wchar_t *architecture, std::wstring &error)
{
  const std::wstring original = path;
  auto invalid = [&](const std::wstring &reason) {
    error = std::wstring(architecture) + L" Global Hook component '" + original + L"': " + reason +
            L". Use a directory such as D:\\Tools\\RenderDuck.";
    return false;
  };

  // Reject ambiguous/unsupported input before asking Windows to normalise it.
  std::wstring reason;
  if(!CheckPathSyntax(path, reason))
    return invalid(reason);

  DWORD required = GetFullPathNameW(path.c_str(), 0, NULL, NULL);
  if(!required)
    return invalid(Error(L"GetFullPathNameW failed", GetLastError()));
  std::vector<wchar_t> buffer(required);
  DWORD written = GetFullPathNameW(path.c_str(), required, buffer.data(), NULL);
  if(!written || written >= required)
    return invalid(
        Error(L"GetFullPathNameW failed", written ? ERROR_INSUFFICIENT_BUFFER : GetLastError()));
  path.assign(buffer.data(), written);
  if(!CheckPathSyntax(path, reason))
    return invalid(reason);

  auto localDrive = [&](const std::wstring &p) {
    UINT type = GetDriveTypeW(p.substr(0, 3).c_str());
    return type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_RAMDISK;
  };
  if(!localDrive(path))
    return invalid(
        L"the component must be on a local fixed, removable or RAM disk; mapped network drives are "
        L"unsupported");

  DWORD attributes = GetFileAttributesW(path.c_str());
  if(attributes == INVALID_FILE_ATTRIBUTES)
    return invalid(Error(L"cannot access the component", GetLastError()));
  if(attributes & FILE_ATTRIBUTE_DIRECTORY)
    return invalid(L"the component is a directory, not a file");

  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if(file == INVALID_HANDLE_VALUE)
    return invalid(Error(L"cannot open the component for reading", GetLastError()));

  // Also check the backing path so a local junction cannot hide a remote/unsupported location.
  required = GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  DWORD err = GetLastError();
  if(required)
  {
    buffer.resize(required + 1);
    written = GetFinalPathNameByHandleW(file, buffer.data(), DWORD(buffer.size()),
                                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    err = GetLastError();
  }
  else
    written = 0;
  CloseHandle(file);
  if(!written || written >= buffer.size())
    return invalid(Error(L"cannot resolve the component's backing path",
                         written ? ERROR_INSUFFICIENT_BUFFER : err));
  std::wstring resolved(buffer.data(), written);
  if(resolved.compare(0, 4, L"\\\\?\\") == 0)
    resolved.erase(0, 4);
  if(!CheckPathSyntax(resolved, reason))
    return invalid(L"unsupported backing path: " + reason);
  if(!localDrive(resolved))
    return invalid(L"the backing file is not on a supported local disk");
  // Retain the normalised installation path: command.exe and its sibling DLL must retain their
  // original directory relationship, including when a component is a file symlink.
  return true;
}

struct Value
{
  const wchar_t *name;
  bool exists = false;
  DWORD type = 0;
  std::vector<BYTE> bytes;
  bool modified = false;

  explicit Value(const wchar_t *n) : name(n) {}

  LSTATUS Read(HKEY key)
  {
    for(int attempt = 0; attempt < 8; ++attempt)
    {
      DWORD size = 0;
      LSTATUS ret = RegQueryValueExW(key, name, NULL, &type, NULL, &size);
      if(ret == ERROR_FILE_NOT_FOUND)
      {
        exists = false;
        bytes.clear();
        return ERROR_SUCCESS;
      }
      if(ret != ERROR_SUCCESS)
        return ret;
      bytes.resize(size);
      // Supply a non-null pointer even for an empty value, so this is a read, not a size query.
      BYTE empty = 0;
      ret = RegQueryValueExW(key, name, NULL, &type, size ? bytes.data() : &empty, &size);
      if(ret == ERROR_MORE_DATA || ret == ERROR_FILE_NOT_FOUND)
        continue;
      if(ret != ERROR_SUCCESS)
        return ret;
      bytes.resize(size);
      exists = true;
      return ERROR_SUCCESS;
    }
    return ERROR_RETRY;
  }

  LSTATUS Restore(HKEY key)
  {
    if(!modified)
      return ERROR_SUCCESS;
    LSTATUS ret = exists ? RegSetValueExW(key, name, 0, type, bytes.empty() ? NULL : bytes.data(),
                                          DWORD(bytes.size()))
                         : RegDeleteValueW(key, name);
    if(!exists && ret == ERROR_FILE_NOT_FOUND)
      ret = ERROR_SUCCESS;
    if(ret == ERROR_SUCCESS)
      modified = false;
    return ret;
  }

  std::wstring RegText() const
  {
    std::wstring text = L"\"" + std::wstring(name) + L"\"=";
    if(!exists)
      return text + L"-\r\n";
    wchar_t number[32];
    swprintf_s(number, L"hex(%x):", type);
    text += number;
    for(size_t i = 0; i < bytes.size(); ++i)
    {
      if(i)
        text += (i % 24 == 0) ? L",\\\r\n  " : L",";
      swprintf_s(number, L"%02x", unsigned(bytes[i]));
      text += number;
    }
    return text + L"\r\n";
  }
};

struct Target
{
  Component component;
  HKEY key = NULL;
  Value dlls{L"AppInit_DLLs"}, enabled{L"LoadAppInit_DLLs"};
  HANDLE pipe = NULL, process = NULL;
};

class Session
{
public:
  Session() = default;
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  std::wstring error, backupPath;
  std::vector<Target> targets;

  ~Session() { Cleanup(); }

  bool Start(const std::vector<Component> &components, const std::wstring &arguments)
  {
    for(Component component : components)
    {
      if(!ValidatePath(component.shim, component.architecture, error) ||
         !ValidatePath(component.command, component.architecture, error) ||
         !ValidatePath(component.library, component.architecture, error))
        return Abort();
      Target target;
      target.component = component;
      targets.push_back(target);
    }

    // A named object's lifetime acts as a lease, without thread-affine mutex ownership. This
    // prevents two instances from interleaving snapshots or overwriting the recovery file.
    lease = CreateMutexW(NULL, FALSE, L"Global\\RiderDuckGlobalHookTransaction");
    DWORD leaseError = GetLastError();
    if(!lease || leaseError == ERROR_ALREADY_EXISTS)
    {
      error = Error(L"Global Hook is already active or its transaction cannot be acquired",
                    lease ? ERROR_ALREADY_EXISTS : leaseError);
      return Abort();
    }

    // Older versions do not hold the lease. Do not overwrite their active hook either.
    for(const Target &target : targets)
    {
      const char *name = target.component.view == KEY_WOW64_64KEY ? "RiderDuckGlobalHookData64"
                                                                  : "RiderDuckGlobalHookData32";
      HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
      DWORD err = GetLastError();
      if(mapping)
      {
        CloseHandle(mapping);
        error = L"Global Hook is already active in another RiderDuck instance.";
        return Abort();
      }
      if(err != ERROR_FILE_NOT_FOUND)
      {
        error = Error(L"Could not check for an existing Global Hook", err);
        return Abort();
      }
    }

    // Snapshot every value in every required view before making the first write.
    for(Target &target : targets)
    {
      LSTATUS ret = RegOpenKeyExW(
          HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0,
          KEY_QUERY_VALUE | KEY_SET_VALUE | target.component.view, &target.key);
      if(ret != ERROR_SUCCESS)
        return Fail(target, L"open AppInit registry key", ret);
      ret = target.dlls.Read(target.key);
      if(ret != ERROR_SUCCESS)
        return Fail(target, L"read AppInit_DLLs", ret);
      ret = target.enabled.Read(target.key);
      if(ret != ERROR_SUCCESS)
        return Fail(target, L"read LoadAppInit_DLLs", ret);
    }
    if(!SaveBackup())
      return Abort();

    for(Target &target : targets)
    {
      const std::wstring &path = target.component.shim;
      LSTATUS ret = RegSetValueExW(target.key, target.dlls.name, 0, REG_SZ,
                                   reinterpret_cast<const BYTE *>(path.c_str()),
                                   DWORD((path.size() + 1) * sizeof(wchar_t)));
      if(ret != ERROR_SUCCESS)
        return Fail(target, L"write AppInit_DLLs", ret);
      target.dlls.modified = true;
      const DWORD one = 1;
      ret = RegSetValueExW(target.key, target.enabled.name, 0, REG_DWORD,
                           reinterpret_cast<const BYTE *>(&one), sizeof(one));
      if(ret != ERROR_SUCCESS)
        return Fail(target, L"write LoadAppInit_DLLs", ret);
      target.enabled.modified = true;
    }

    for(Target &target : targets)
      if(!Launch(target, arguments))
        return Abort();

    stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if(!stopEvent)
    {
      error = Error(L"Could not create Global Hook stop event", GetLastError());
      return Abort();
    }
    thread = CreateThread(NULL, 0, &Worker, this, 0, NULL);
    if(!thread)
    {
      error = Error(L"Could not create Global Hook worker thread", GetLastError());
      return Abort();
    }
    return true;
  }

  void Cleanup()
  {
    if(cleaned)
      return;
    cleaned = true;
    // Restore both views even if an individual restore fails. Retain the original failure.
    for(Target &target : targets)
    {
      if(target.key)
      {
        for(Value *value : {&target.enabled, &target.dlls})
        {
          LSTATUS ret = value->Restore(target.key);
          if(ret != ERROR_SUCCESS)
            Append(Error(
                std::wstring(target.component.architecture) + L" could not restore " + value->name,
                ret));
        }
        RegCloseKey(target.key);
        target.key = NULL;
      }
    }
    if(thread)
    {
      SetEvent(stopEvent);
      WaitForSingleObject(thread, INFINITE);
      CloseHandle(thread);
      thread = NULL;
    }
    ClosePipes();
    if(stopEvent)
    {
      CloseHandle(stopEvent);
      stopEvent = NULL;
    }
    for(Target &target : targets)
    {
      if(target.process)
      {
        DWORD wait = WaitForSingleObject(target.process, 5000);
        if(wait != WAIT_OBJECT_0)
          Append(Error(std::wstring(target.component.architecture) + L" hook helper did not exit",
                       wait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT));
        CloseHandle(target.process);
        target.process = NULL;
      }
    }
    if(lease)
    {
      CloseHandle(lease);
      lease = NULL;
    }
    if(!error.empty() && !backupPath.empty())
      Append(L"Registry recovery file: " + backupPath);
  }

private:
  HANDLE stopEvent = NULL, thread = NULL, lease = NULL;
  bool cleaned = false;

  void Append(const std::wstring &message)
  {
    if(!error.empty())
      error += L"\n";
    error += message;
  }
  bool Abort()
  {
    Cleanup();
    return false;
  }
  bool Fail(const Target &target, const wchar_t *operation, DWORD code)
  {
    error = Error(
        std::wstring(target.component.architecture) + L" Global Hook: could not " + operation, code);
    return Abort();
  }
  void ClosePipes()
  {
    for(Target &target : targets)
    {
      if(target.pipe)
      {
        // EOF wakes the helper's ReadFile without a potentially blocking WriteFile.
        CloseHandle(target.pipe);
        target.pipe = NULL;
      }
    }
  }
  static DWORD WINAPI Worker(void *param)
  {
    Session *session = static_cast<Session *>(param);
    WaitForSingleObject(session->stopEvent, INFINITE);
    session->ClosePipes();
    return 0;
  }

  bool SaveBackup()
  {
    std::vector<wchar_t> temp(32768);
    DWORD length = GetTempPathW(DWORD(temp.size()), temp.data());
    if(!length || length >= temp.size())
    {
      error = Error(L"Could not locate the registry recovery directory",
                    length ? ERROR_INSUFFICIENT_BUFFER : GetLastError());
      return false;
    }
    std::wstring destination(temp.data(), length);
    destination += L"RiderDuck_RestoreGlobalHook.reg";
    std::wstring temporary = destination + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    std::wstring text = L"\ufeffWindows Registry Editor Version 5.00\r\n";
    BOOL wow64 = FALSE;
    if(!IsWow64Process(GetCurrentProcess(), &wow64))
    {
      error = Error(L"Could not determine the registry recovery views", GetLastError());
      return false;
    }
    for(const Target &target : targets)
    {
      text += L"\r\n[HKEY_LOCAL_MACHINE\\SOFTWARE\\";
      if(target.component.view == KEY_WOW64_32KEY && (sizeof(void *) == 8 || wow64))
        text += L"Wow6432Node\\";
      text += L"Microsoft\\Windows NT\\CurrentVersion\\Windows]\r\n";
      text += target.dlls.RegText();
      text += target.enabled.RegText();
    }
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE)
    {
      error = Error(L"Could not create registry recovery file '" + temporary + L"'", GetLastError());
      return false;
    }
    DWORD written = 0;
    const DWORD size = DWORD(text.size() * sizeof(wchar_t));
    BOOL ok = WriteFile(file, text.data(), size, &written, NULL);
    DWORD err = ok ? ERROR_WRITE_FAULT : GetLastError();
    if(ok && written == size)
    {
      ok = FlushFileBuffers(file);
      err = GetLastError();
    }
    else
      ok = FALSE;
    CloseHandle(file);
    if(ok)
    {
      ok = MoveFileExW(temporary.c_str(), destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
      err = GetLastError();
    }
    if(!ok)
    {
      DeleteFileW(temporary.c_str());
      error = Error(L"Could not save registry recovery file '" + destination + L"'", err);
      return false;
    }
    backupPath = destination;
    return true;
  }

  bool Launch(Target &target, const std::wstring &arguments)
  {
    HANDLE readPipe = NULL, nullOutput = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if(!CreatePipe(&readPipe, &target.pipe, &security, 0))
    {
      error = Error(std::wstring(target.component.architecture) + L" could not create helper pipe",
                    GetLastError());
      return false;
    }
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
    std::vector<BYTE> storage;
    auto finish = [&](bool success) {
      if(attributes)
        DeleteProcThreadAttributeList(attributes);
      if(nullOutput != INVALID_HANDLE_VALUE)
        CloseHandle(nullOutput);
      CloseHandle(readPipe);
      return success;
    };
    if(!SetHandleInformation(target.pipe, HANDLE_FLAG_INHERIT, 0))
    {
      error = Error(L"Could not protect the helper pipe from inheritance", GetLastError());
      return finish(false);
    }
    nullOutput = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(nullOutput == INVALID_HANDLE_VALUE)
    {
      error = Error(L"Could not open helper output", GetLastError());
      return finish(false);
    }
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    if(!size)
    {
      error = Error(L"Could not size helper process attributes", GetLastError());
      return finish(false);
    }
    storage.resize(size);
    LPPROC_THREAD_ATTRIBUTE_LIST candidate =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(candidate, 1, 0, &size))
    {
      error = Error(L"Could not initialise helper process attributes", GetLastError());
      return finish(false);
    }
    attributes = candidate;
    HANDLE inherited[] = {readPipe, nullOutput};
    if(!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                  sizeof(inherited), NULL, NULL))
    {
      error = Error(L"Could not restrict helper handle inheritance", GetLastError());
      return finish(false);
    }
    STARTUPINFOEXW startup = {};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.StartupInfo.wShowWindow = SW_HIDE;
    startup.StartupInfo.hStdInput = readPipe;
    startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = nullOutput;
    startup.lpAttributeList = attributes;
    std::wstring command = L"\"" + target.component.command + L"\" " + arguments;
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(0);
    PROCESS_INFORMATION process = {};
    BOOL ok = CreateProcessW(target.component.command.c_str(), mutableCommand.data(), NULL, NULL,
                             TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                             &startup.StartupInfo, &process);
    if(!ok)
      error = Error(std::wstring(target.component.architecture) + L" could not launch '" +
                        target.component.command + L"'",
                    GetLastError());
    else
    {
      CloseHandle(process.hThread);
      target.process = process.hProcess;
    }
    return finish(ok != FALSE);
  }
};
}
