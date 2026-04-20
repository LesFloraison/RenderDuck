/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
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

#pragma once

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Documentation for the API is available at https://riderduck.org/docs/in_application_api.html
//

#if !defined(RIDERDUCK_NO_STDINT)
#include <stdint.h>
#endif

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define RIDERDUCK_CC __cdecl
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__sun__) || defined(__OpenBSD__)
#define RIDERDUCK_CC
#elif defined(__APPLE__)
#define RIDERDUCK_CC
#else
#error "Unknown platform"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
// Constants not used directly in below API

// This is a GUID/magic value used for when applications pass a path where shader debug
// information can be found to match up with a stripped shader.
// the define can be used like so: const GUID RIDERDUCK_ShaderDebugMagicValue =
// RIDERDUCK_ShaderDebugMagicValue_value
#define RIDERDUCK_ShaderDebugMagicValue_struct                                \
  {                                                                           \
    0xeab25520, 0x6670, 0x4865, 0x84, 0x29, 0x6c, 0x8, 0x51, 0x54, 0x00, 0xff \
  }

// as an alternative when you want a byte array (assuming x86 endianness):
#define RIDERDUCK_ShaderDebugMagicValue_bytearray                                                 \
  {                                                                                               \
    0x20, 0x55, 0xb2, 0xea, 0x70, 0x66, 0x65, 0x48, 0x84, 0x29, 0x6c, 0x8, 0x51, 0x54, 0x00, 0xff \
  }

// truncated version when only a uint64_t is available (e.g. Vulkan tags):
#define RIDERDUCK_ShaderDebugMagicValue_truncated 0x48656670eab25520ULL

// this is a magic value for vulkan user tags to indicate which dispatchable API objects are which
// for object annotations
#define RIDERDUCK_APIObjectAnnotationHelper 0xfbb3b337b664d0adULL

//////////////////////////////////////////////////////////////////////////////////////////////////
// RiderDuck capture options
//

typedef enum RIDERDUCK_CaptureOption
{
  // Allow the application to enable vsync
  //
  // Default - enabled
  //
  // 1 - The application can enable or disable vsync at will
  // 0 - vsync is force disabled
  eRIDERDUCK_Option_AllowVSync = 0,

  // Allow the application to enable fullscreen
  //
  // Default - enabled
  //
  // 1 - The application can enable or disable fullscreen at will
  // 0 - fullscreen is force disabled
  eRIDERDUCK_Option_AllowFullscreen = 1,

  // Record API debugging events and messages
  //
  // Default - disabled
  //
  // 1 - Enable built-in API debugging features and records the results into
  //     the capture, which is matched up with events on replay
  // 0 - no API debugging is forcibly enabled
  eRIDERDUCK_Option_APIValidation = 2,
  eRIDERDUCK_Option_DebugDeviceMode = 2,    // deprecated name of this enum

  // Capture CPU callstacks for API events
  //
  // Default - disabled
  //
  // 1 - Enables capturing of callstacks
  // 0 - no callstacks are captured
  eRIDERDUCK_Option_CaptureCallstacks = 3,

  // When capturing CPU callstacks, only capture them from actions.
  // This option does nothing without the above option being enabled
  //
  // Default - disabled
  //
  // 1 - Only captures callstacks for actions.
  //     Ignored if CaptureCallstacks is disabled
  // 0 - Callstacks, if enabled, are captured for every event.
  eRIDERDUCK_Option_CaptureCallstacksOnlyDraws = 4,
  eRIDERDUCK_Option_CaptureCallstacksOnlyActions = 4,

  // Specify a delay in seconds to wait for a debugger to attach, after
  // creating or injecting into a process, before continuing to allow it to run.
  //
  // 0 indicates no delay, and the process will run immediately after injection
  //
  // Default - 0 seconds
  //
  eRIDERDUCK_Option_DelayForDebugger = 5,

  // Verify buffer access. This includes checking the memory returned by a Map() call to
  // detect any out-of-bounds modification, as well as initialising buffers with undefined contents
  // to a marker value to catch use of uninitialised memory.
  //
  // NOTE: This option is only valid for OpenGL and D3D11. Explicit APIs such as D3D12 and Vulkan do
  // not do the same kind of interception & checking and undefined contents are really undefined.
  //
  // Default - disabled
  //
  // 1 - Verify buffer access
  // 0 - No verification is performed, and overwriting bounds may cause crashes or corruption in
  //     RiderDuck.
  eRIDERDUCK_Option_VerifyBufferAccess = 6,

  // The old name for eRIDERDUCK_Option_VerifyBufferAccess was eRIDERDUCK_Option_VerifyMapWrites.
  // This option now controls the filling of uninitialised buffers with 0xdddddddd which was
  // previously always enabled
  eRIDERDUCK_Option_VerifyMapWrites = eRIDERDUCK_Option_VerifyBufferAccess,

  // Hooks any system API calls that create child processes, and injects
  // RiderDuck into them recursively with the same options.
  //
  // Default - disabled
  //
  // 1 - Hooks into spawned child processes
  // 0 - Child processes are not hooked by RiderDuck
  eRIDERDUCK_Option_HookIntoChildren = 7,

  // By default RiderDuck only includes resources in the final capture necessary
  // for that frame, this allows you to override that behaviour.
  //
  // Default - disabled
  //
  // 1 - all live resources at the time of capture are included in the capture
  //     and available for inspection
  // 0 - only the resources referenced by the captured frame are included
  eRIDERDUCK_Option_RefAllResources = 8,

  // **NOTE**: As of RiderDuck v1.1 this option has been deprecated. Setting or
  // getting it will be ignored, to allow compatibility with older versions.
  // In v1.1 the option acts as if it's always enabled.
  //
  // By default RiderDuck skips saving initial states for resources where the
  // previous contents don't appear to be used, assuming that writes before
  // reads indicate previous contents aren't used.
  //
  // Default - disabled
  //
  // 1 - initial contents at the start of each captured frame are saved, even if
  //     they are later overwritten or cleared before being used.
  // 0 - unless a read is detected, initial contents will not be saved and will
  //     appear as black or empty data.
  eRIDERDUCK_Option_SaveAllInitials = 9,

  // In APIs that allow for the recording of command lists to be replayed later,
  // RiderDuck may choose to not capture command lists before a frame capture is
  // triggered, to reduce overheads. This means any command lists recorded once
  // and replayed many times will not be available and may cause a failure to
  // capture.
  //
  // NOTE: This is only true for APIs where multithreading is difficult or
  // discouraged. Newer APIs like Vulkan and D3D12 will ignore this option
  // and always capture all command lists since the API is heavily oriented
  // around it and the overheads have been reduced by API design.
  //
  // 1 - All command lists are captured from the start of the application
  // 0 - Command lists are only captured if their recording begins during
  //     the period when a frame capture is in progress.
  eRIDERDUCK_Option_CaptureAllCmdLists = 10,

  // Mute API debugging output when the API validation mode option is enabled
  //
  // Default - enabled
  //
  // 1 - Mute any API debug messages from being displayed or passed through
  // 0 - API debugging is displayed as normal
  eRIDERDUCK_Option_DebugOutputMute = 11,

  // Option to allow vendor extensions to be used even when they may be
  // incompatible with RiderDuck and cause corrupted replays or crashes.
  //
  // Default - inactive
  //
  // No values are documented, this option should only be used when absolutely
  // necessary as directed by a RiderDuck developer.
  eRIDERDUCK_Option_AllowUnsupportedVendorExtensions = 12,

  // Define a soft memory limit which some APIs may aim to keep overhead under where
  // possible. Anything above this limit will where possible be saved directly to disk during
  // capture.
  // This will cause increased disk space use (which may cause a capture to fail if disk space is
  // exhausted) as well as slower capture times.
  //
  // Not all memory allocations may be deferred like this so it is not a guarantee of a memory
  // limit.
  //
  // Units are in MBs, suggested values would range from 200MB to 1000MB.
  //
  // Default - 0 Megabytes
  eRIDERDUCK_Option_SoftMemoryLimit = 13,
} RIDERDUCK_CaptureOption;

// Sets an option that controls how RiderDuck behaves on capture.
//
// Returns 1 if the option and value are valid
// Returns 0 if either is invalid and the option is unchanged
typedef int(RIDERDUCK_CC *pRIDERDUCK_SetCaptureOptionU32)(RIDERDUCK_CaptureOption opt, uint32_t val);
typedef int(RIDERDUCK_CC *pRIDERDUCK_SetCaptureOptionF32)(RIDERDUCK_CaptureOption opt, float val);

// Gets the current value of an option as a uint32_t
//
// If the option is invalid, 0xffffffff is returned
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_GetCaptureOptionU32)(RIDERDUCK_CaptureOption opt);

// Gets the current value of an option as a float
//
// If the option is invalid, -FLT_MAX is returned
typedef float(RIDERDUCK_CC *pRIDERDUCK_GetCaptureOptionF32)(RIDERDUCK_CaptureOption opt);

typedef enum RIDERDUCK_InputButton
{
  // '0' - '9' matches ASCII values
  eRIDERDUCK_Key_0 = 0x30,
  eRIDERDUCK_Key_1 = 0x31,
  eRIDERDUCK_Key_2 = 0x32,
  eRIDERDUCK_Key_3 = 0x33,
  eRIDERDUCK_Key_4 = 0x34,
  eRIDERDUCK_Key_5 = 0x35,
  eRIDERDUCK_Key_6 = 0x36,
  eRIDERDUCK_Key_7 = 0x37,
  eRIDERDUCK_Key_8 = 0x38,
  eRIDERDUCK_Key_9 = 0x39,

  // 'A' - 'Z' matches ASCII values
  eRIDERDUCK_Key_A = 0x41,
  eRIDERDUCK_Key_B = 0x42,
  eRIDERDUCK_Key_C = 0x43,
  eRIDERDUCK_Key_D = 0x44,
  eRIDERDUCK_Key_E = 0x45,
  eRIDERDUCK_Key_F = 0x46,
  eRIDERDUCK_Key_G = 0x47,
  eRIDERDUCK_Key_H = 0x48,
  eRIDERDUCK_Key_I = 0x49,
  eRIDERDUCK_Key_J = 0x4A,
  eRIDERDUCK_Key_K = 0x4B,
  eRIDERDUCK_Key_L = 0x4C,
  eRIDERDUCK_Key_M = 0x4D,
  eRIDERDUCK_Key_N = 0x4E,
  eRIDERDUCK_Key_O = 0x4F,
  eRIDERDUCK_Key_P = 0x50,
  eRIDERDUCK_Key_Q = 0x51,
  eRIDERDUCK_Key_R = 0x52,
  eRIDERDUCK_Key_S = 0x53,
  eRIDERDUCK_Key_T = 0x54,
  eRIDERDUCK_Key_U = 0x55,
  eRIDERDUCK_Key_V = 0x56,
  eRIDERDUCK_Key_W = 0x57,
  eRIDERDUCK_Key_X = 0x58,
  eRIDERDUCK_Key_Y = 0x59,
  eRIDERDUCK_Key_Z = 0x5A,

  // leave the rest of the ASCII range free
  // in case we want to use it later
  eRIDERDUCK_Key_NonPrintable = 0x100,

  eRIDERDUCK_Key_Divide,
  eRIDERDUCK_Key_Multiply,
  eRIDERDUCK_Key_Subtract,
  eRIDERDUCK_Key_Plus,

  eRIDERDUCK_Key_F1,
  eRIDERDUCK_Key_F2,
  eRIDERDUCK_Key_F3,
  eRIDERDUCK_Key_F4,
  eRIDERDUCK_Key_F5,
  eRIDERDUCK_Key_F6,
  eRIDERDUCK_Key_F7,
  eRIDERDUCK_Key_F8,
  eRIDERDUCK_Key_F9,
  eRIDERDUCK_Key_F10,
  eRIDERDUCK_Key_F11,
  eRIDERDUCK_Key_F12,

  eRIDERDUCK_Key_Home,
  eRIDERDUCK_Key_End,
  eRIDERDUCK_Key_Insert,
  eRIDERDUCK_Key_Delete,
  eRIDERDUCK_Key_PageUp,
  eRIDERDUCK_Key_PageDn,

  eRIDERDUCK_Key_Backspace,
  eRIDERDUCK_Key_Tab,
  eRIDERDUCK_Key_PrtScrn,
  eRIDERDUCK_Key_Pause,

  eRIDERDUCK_Key_Max,
} RIDERDUCK_InputButton;

// Sets which key or keys can be used to toggle focus between multiple windows
//
// If keys is NULL or num is 0, toggle keys will be disabled
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetFocusToggleKeys)(RIDERDUCK_InputButton *keys, int num);

// Sets which key or keys can be used to capture the next frame
//
// If keys is NULL or num is 0, captures keys will be disabled
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetCaptureKeys)(RIDERDUCK_InputButton *keys, int num);

typedef enum RIDERDUCK_OverlayBits
{
  // This single bit controls whether the overlay is enabled or disabled globally
  eRIDERDUCK_Overlay_Enabled = 0x1,

  // Show the average framerate over several seconds as well as min/max
  eRIDERDUCK_Overlay_FrameRate = 0x2,

  // Show the current frame number
  eRIDERDUCK_Overlay_FrameNumber = 0x4,

  // Show a list of recent captures, and how many captures have been made
  eRIDERDUCK_Overlay_CaptureList = 0x8,

  // Default values for the overlay mask
  eRIDERDUCK_Overlay_Default = (eRIDERDUCK_Overlay_Enabled | eRIDERDUCK_Overlay_FrameRate |
                                eRIDERDUCK_Overlay_FrameNumber | eRIDERDUCK_Overlay_CaptureList),

  // Enable all bits
  eRIDERDUCK_Overlay_All = 0x7ffffff,

  // Disable all bits
  eRIDERDUCK_Overlay_None = 0,
} RIDERDUCK_OverlayBits;

// returns the overlay bits that have been set
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_GetOverlayBits)(void);
// sets the overlay bits with an and & or mask
typedef void(RIDERDUCK_CC *pRIDERDUCK_MaskOverlayBits)(uint32_t And, uint32_t Or);

// this function will attempt to remove RiderDuck's hooks in the application.
//
// Note: that this can only work correctly if done immediately after
// the module is loaded, before any API work happens. RiderDuck will remove its
// injected hooks and shut down. Behaviour is undefined if this is called
// after any API functions have been called, and there is still no guarantee of
// success.
typedef void(RIDERDUCK_CC *pRIDERDUCK_RemoveHooks)(void);

// DEPRECATED: compatibility for code compiled against pre-1.4.1 headers.
typedef pRIDERDUCK_RemoveHooks pRIDERDUCK_Shutdown;

// This function will unload RiderDuck's crash handler.
//
// If you use your own crash handler and don't want RiderDuck's handler to
// intercede, you can call this function to unload it and any unhandled
// exceptions will pass to the next handler.
typedef void(RIDERDUCK_CC *pRIDERDUCK_UnloadCrashHandler)(void);

// Sets the capture file path template
//
// pathtemplate is a UTF-8 string that gives a template for how captures will be named
// and where they will be saved.
//
// Any extension is stripped off the path, and captures are saved in the directory
// specified, and named with the filename and the frame number appended. If the
// directory does not exist it will be created, including any parent directories.
//
// If pathtemplate is NULL, the template will remain unchanged
//
// Example:
//
// SetCaptureFilePathTemplate("my_captures/example");
//
// Capture #1 -> my_captures/example_frame123.rdc
// Capture #2 -> my_captures/example_frame456.rdc
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetCaptureFilePathTemplate)(const char *pathtemplate);

// returns the current capture path template, see SetCaptureFileTemplate above, as a UTF-8 string
typedef const char *(RIDERDUCK_CC *pRIDERDUCK_GetCaptureFilePathTemplate)(void);

// DEPRECATED: compatibility for code compiled against pre-1.1.2 headers.
typedef pRIDERDUCK_SetCaptureFilePathTemplate pRIDERDUCK_SetLogFilePathTemplate;
typedef pRIDERDUCK_GetCaptureFilePathTemplate pRIDERDUCK_GetLogFilePathTemplate;

// returns the number of captures that have been made
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_GetNumCaptures)(void);

// This function returns the details of a capture, by index. New captures are added
// to the end of the list.
//
// filename will be filled with the absolute path to the capture file, as a UTF-8 string
// pathlength will be written with the length in bytes of the filename string
// timestamp will be written with the time of the capture, in seconds since the Unix epoch
//
// Any of the parameters can be NULL and they'll be skipped.
//
// The function will return 1 if the capture index is valid, or 0 if the index is invalid
// If the index is invalid, the values will be unchanged
//
// Note: when captures are deleted in the UI they will remain in this list, so the
// capture path may not exist anymore.
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_GetCapture)(uint32_t idx, char *filename,
                                                      uint32_t *pathlength, uint64_t *timestamp);

// Sets the comments associated with a capture file. These comments are displayed in the
// UI program when opening.
//
// filePath should be a path to the capture file to add comments to. If set to NULL or ""
// the most recent capture file created made will be used instead.
// comments should be a NULL-terminated UTF-8 string to add as comments.
//
// Any existing comments will be overwritten.
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetCaptureFileComments)(const char *filePath,
                                                              const char *comments);

// returns 1 if the RiderDuck UI is connected to this application, 0 otherwise
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_IsTargetControlConnected)(void);

// DEPRECATED: compatibility for code compiled against pre-1.1.1 headers.
// This was renamed to IsTargetControlConnected in API 1.1.1, the old typedef is kept here for
// backwards compatibility with old code, it is castable either way since it's ABI compatible
// as the same function pointer type.
typedef pRIDERDUCK_IsTargetControlConnected pRIDERDUCK_IsRemoteAccessConnected;

// This function will launch the Replay UI associated with the RiderDuck library injected
// into the running application.
//
// if connectTargetControl is 1, the Replay UI will be launched with a command line parameter
// to connect to this application
// cmdline is the rest of the command line, as a UTF-8 string. E.g. a captures to open
// if cmdline is NULL, the command line will be empty.
//
// returns the PID of the replay UI if successful, 0 if not successful.
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_LaunchReplayUI)(uint32_t connectTargetControl,
                                                          const char *cmdline);

// RiderDuck can return a higher version than requested if it's backwards compatible,
// this function returns the actual version returned. If a parameter is NULL, it will be
// ignored and the others will be filled out.
typedef void(RIDERDUCK_CC *pRIDERDUCK_GetAPIVersion)(int *major, int *minor, int *patch);

// Requests that the replay UI show itself (if hidden or not the current top window). This can be
// used in conjunction with IsTargetControlConnected and LaunchReplayUI to intelligently handle
// showing the UI after making a capture.
//
// This will return 1 if the request was successfully passed on, though it's not guaranteed that
// the UI will be on top in all cases depending on OS rules. It will return 0 if there is no current
// target control connection to make such a request, or if there was another error
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_ShowReplayUI)(void);

//////////////////////////////////////////////////////////////////////////
// Capturing functions
//

// A device pointer is a pointer to the API's root handle.
//
// This would be an ID3D11Device, HGLRC/GLXContext, ID3D12Device, etc
typedef void *RIDERDUCK_DevicePointer;

// A window handle is the OS's native window handle
//
// This would be an HWND, GLXDrawable, etc
typedef void *RIDERDUCK_WindowHandle;

// A helper macro for Vulkan, where the device handle cannot be used directly.
//
// Passing the VkInstance to this macro will return the RIDERDUCK_DevicePointer to use.
//
// Specifically, the value needed is the dispatch table pointer, which sits as the first
// pointer-sized object in the memory pointed to by the VkInstance. Thus we cast to a void** and
// indirect once.
#define RIDERDUCK_DEVICEPOINTER_FROM_VKINSTANCE(inst) (*((void **)(inst)))

// This sets the RiderDuck in-app overlay in the API/window pair as 'active' and it will
// respond to keypresses. Neither parameter can be NULL
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetActiveWindow)(RIDERDUCK_DevicePointer device,
                                                       RIDERDUCK_WindowHandle wndHandle);

// capture the next frame on whichever window and API is currently considered active
typedef void(RIDERDUCK_CC *pRIDERDUCK_TriggerCapture)(void);

// capture the next N frames on whichever window and API is currently considered active
typedef void(RIDERDUCK_CC *pRIDERDUCK_TriggerMultiFrameCapture)(uint32_t numFrames);

// When choosing either a device pointer or a window handle to capture, you can pass NULL.
// Passing NULL specifies a 'wildcard' match against anything. This allows you to specify
// any API rendering to a specific window, or a specific API instance rendering to any window,
// or in the simplest case of one window and one API, you can just pass NULL for both.
//
// In either case, if there are two or more possible matching (device,window) pairs it
// is undefined which one will be captured.
//
// Note: for headless rendering you can pass NULL for the window handle and either specify
// a device pointer or leave it NULL as above.

// Immediately starts capturing API calls on the specified device pointer and window handle.
//
// If there is no matching thing to capture (e.g. no supported API has been initialised),
// this will do nothing.
//
// The results are undefined (including crashes) if two captures are started overlapping,
// even on separate devices and/oror windows.
typedef void(RIDERDUCK_CC *pRIDERDUCK_StartFrameCapture)(RIDERDUCK_DevicePointer device,
                                                         RIDERDUCK_WindowHandle wndHandle);

// Returns whether or not a frame capture is currently ongoing anywhere.
//
// This will return 1 if a capture is ongoing, and 0 if there is no capture running
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_IsFrameCapturing)(void);

// Ends capturing immediately.
//
// This will return 1 if the capture succeeded, and 0 if there was an error capturing.
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_EndFrameCapture)(RIDERDUCK_DevicePointer device,
                                                           RIDERDUCK_WindowHandle wndHandle);

// Ends capturing immediately and discard any data stored without saving to disk.
//
// This will return 1 if the capture was discarded, and 0 if there was an error or no capture
// was in progress
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_DiscardFrameCapture)(RIDERDUCK_DevicePointer device,
                                                               RIDERDUCK_WindowHandle wndHandle);

// Only valid to be called between a call to StartFrameCapture and EndFrameCapture. Gives a custom
// title to the capture produced which will be displayed in the UI.
//
// If multiple captures are ongoing, this title will be applied to the first capture to end after
// this call. The second capture to end will have no title, unless this function is called again.
//
// Calling this function has no effect if no capture is currently running, and if it is called
// multiple times only the last title will be used.
typedef void(RIDERDUCK_CC *pRIDERDUCK_SetCaptureTitle)(const char *title);

// Annotations API:
//
// These functions allow you to specify annotations either on a per-command level, or a per-object
// level.
//
// Basic types of annotations are supported, as well as vector versions and references to API objects.
//
// The annotations are stored as keys, with the key being a dot-separated path allowing arbitrary
// nesting and user organisation. The keys are sorted in human order so `foo.2.bar` will be displayed
// before `foo.10.bar` to allow creation of arrays if desired.
//
// Deleting an annotation can be done by assigning an empty value to it.

// the type of an annotation value, or Empty to delete an annotation
typedef enum RIDERDUCK_AnnotationType
{
  eRIDERDUCK_Empty,
  eRIDERDUCK_Bool,
  eRIDERDUCK_Int32,
  eRIDERDUCK_UInt32,
  eRIDERDUCK_Int64,
  eRIDERDUCK_UInt64,
  eRIDERDUCK_Float,
  eRIDERDUCK_Double,
  eRIDERDUCK_String,
  eRIDERDUCK_APIObject,
  eRIDERDUCK_AnnotationMax = 0x7FFFFFFF,
} RIDERDUCK_AnnotationType;

// a union with vector annotation value data
typedef union RIDERDUCK_AnnotationVectorValue
{
  bool boolean[4];
  int32_t int32[4];
  int64_t int64[4];
  uint32_t uint32[4];
  uint64_t uint64[4];
  float float32[4];
  double float64[4];
} RIDERDUCK_AnnotationVectorValue;

// a union with scalar annotation value data
typedef union RIDERDUCK_AnnotationValue
{
  bool boolean;
  int32_t int32;
  int64_t int64;
  uint32_t uint32;
  uint64_t uint64;
  float float32;
  double float64;

  RIDERDUCK_AnnotationVectorValue vector;

  const char *string;
  void *apiObject;
} RIDERDUCK_AnnotationValue;

// a struct for specifying a GL object, as we don't have pointers we can use so instead we specify a
// pointer to this struct giving both the type and the name
typedef struct RIDERDUCK_GLResourceReference
{
  // this is the same GLenum identifier as passed to glObjectLabel
  uint32_t identifier;
  uint32_t name;
} GLResourceReference;

// simple C++ helpers to avoid the need for a temporary objects for value passing and GL object specification
#ifdef __cplusplus
struct RDGLObjectHelper
{
  RIDERDUCK_GLResourceReference gl;

  RDGLObjectHelper(uint32_t identifier, uint32_t name)
  {
    gl.identifier = identifier;
    gl.name = name;
  }

  operator RIDERDUCK_GLResourceReference *() { return &gl; }
};

struct RDAnnotationHelper
{
  RIDERDUCK_AnnotationValue val;

  RDAnnotationHelper(bool b) { val.boolean = b; }
  RDAnnotationHelper(int32_t i) { val.int32 = i; }
  RDAnnotationHelper(int64_t i) { val.int64 = i; }
  RDAnnotationHelper(uint32_t i) { val.uint32 = i; }
  RDAnnotationHelper(uint64_t i) { val.uint64 = i; }
  RDAnnotationHelper(float f) { val.float32 = f; }
  RDAnnotationHelper(double d) { val.float64 = d; }
  RDAnnotationHelper(const char *s) { val.string = s; }

  operator RIDERDUCK_AnnotationValue *() { return &val; }
};
#endif

// The device is specified in the same way as other API calls that take a RIDERDUCK_DevicePointer
// to specify the device.
//
// The object or queue/commandbuffer will depend on the graphics API in question.
//
// Return value:
// 0 - The annotation was applied successfully.
// 1 - The device is unknown/invalid
// 2 - The device is valid but the annotation is not supported for API-specific reasons, such as an
//     unrecognised or invalid object or queue/commandbuffer
// 3 - The call is ill-formed or invalid e.g. empty is specified with a value pointer, or non-empty
//     is specified with a NULL value pointer
typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_SetObjectAnnotation)(RIDERDUCK_DevicePointer device,
                                                               void *object, const char *key,
                                                               RIDERDUCK_AnnotationType valueType,
                                                               uint32_t valueVectorWidth,
                                                               const RIDERDUCK_AnnotationValue *value);

typedef uint32_t(RIDERDUCK_CC *pRIDERDUCK_SetCommandAnnotation)(
    RIDERDUCK_DevicePointer device, void *queueOrCommandBuffer, const char *key,
    RIDERDUCK_AnnotationType valueType, uint32_t valueVectorWidth,
    const RIDERDUCK_AnnotationValue *value);

//////////////////////////////////////////////////////////////////////////////////////////////////
// RiderDuck API versions
//

// RiderDuck uses semantic versioning (http://semver.org/).
//
// MAJOR version is incremented when incompatible API changes happen.
// MINOR version is incremented when functionality is added in a backwards-compatible manner.
// PATCH version is incremented when backwards-compatible bug fixes happen.
//
// Note that this means the API returned can be higher than the one you might have requested.
// e.g. if you are running against a newer RiderDuck that supports 1.0.1, it will be returned
// instead of 1.0.0. You can check this with the GetAPIVersion entry point
typedef enum RIDERDUCK_Version
{
  eRIDERDUCK_API_Version_1_0_0 = 10000,    // RIDERDUCK_API_1_0_0 = 1 00 00
  eRIDERDUCK_API_Version_1_0_1 = 10001,    // RIDERDUCK_API_1_0_1 = 1 00 01
  eRIDERDUCK_API_Version_1_0_2 = 10002,    // RIDERDUCK_API_1_0_2 = 1 00 02
  eRIDERDUCK_API_Version_1_1_0 = 10100,    // RIDERDUCK_API_1_1_0 = 1 01 00
  eRIDERDUCK_API_Version_1_1_1 = 10101,    // RIDERDUCK_API_1_1_1 = 1 01 01
  eRIDERDUCK_API_Version_1_1_2 = 10102,    // RIDERDUCK_API_1_1_2 = 1 01 02
  eRIDERDUCK_API_Version_1_2_0 = 10200,    // RIDERDUCK_API_1_2_0 = 1 02 00
  eRIDERDUCK_API_Version_1_3_0 = 10300,    // RIDERDUCK_API_1_3_0 = 1 03 00
  eRIDERDUCK_API_Version_1_4_0 = 10400,    // RIDERDUCK_API_1_4_0 = 1 04 00
  eRIDERDUCK_API_Version_1_4_1 = 10401,    // RIDERDUCK_API_1_4_1 = 1 04 01
  eRIDERDUCK_API_Version_1_4_2 = 10402,    // RIDERDUCK_API_1_4_2 = 1 04 02
  eRIDERDUCK_API_Version_1_5_0 = 10500,    // RIDERDUCK_API_1_5_0 = 1 05 00
  eRIDERDUCK_API_Version_1_6_0 = 10600,    // RIDERDUCK_API_1_6_0 = 1 06 00
  eRIDERDUCK_API_Version_1_7_0 = 10700,    // RIDERDUCK_API_1_7_0 = 1 07 00
} RIDERDUCK_Version;

// API version changelog:
//
// 1.0.0 - initial release
// 1.0.1 - Bugfix: IsFrameCapturing() was returning false for captures that were triggered
//         by keypress or TriggerCapture, instead of Start/EndFrameCapture.
// 1.0.2 - Refactor: Renamed eRIDERDUCK_Option_DebugDeviceMode to eRIDERDUCK_Option_APIValidation
// 1.1.0 - Add feature: TriggerMultiFrameCapture(). Backwards compatible with 1.0.x since the new
//         function pointer is added to the end of the struct, the original layout is identical
// 1.1.1 - Refactor: Renamed remote access to target control (to better disambiguate from remote
//         replay/remote server concept in replay UI)
// 1.1.2 - Refactor: Renamed "log file" in function names to just capture, to clarify that these
//         are captures and not debug logging files. This is the first API version in the v1.0
//         branch.
// 1.2.0 - Added feature: SetCaptureFileComments() to add comments to a capture file that will be
//         displayed in the UI program on load.
// 1.3.0 - Added feature: New capture option eRIDERDUCK_Option_AllowUnsupportedVendorExtensions
//         which allows users to opt-in to allowing unsupported vendor extensions to function.
//         Should be used at the user's own risk.
//         Refactor: Renamed eRIDERDUCK_Option_VerifyMapWrites to
//         eRIDERDUCK_Option_VerifyBufferAccess, which now also controls initialisation to
//         0xdddddddd of uninitialised buffer contents.
// 1.4.0 - Added feature: DiscardFrameCapture() to discard a frame capture in progress and stop
//         capturing without saving anything to disk.
// 1.4.1 - Refactor: Renamed Shutdown to RemoveHooks to better clarify what is happening
// 1.4.2 - Refactor: Renamed 'draws' to 'actions' in callstack capture option.
// 1.5.0 - Added feature: ShowReplayUI() to request that the replay UI show itself if connected
// 1.6.0 - Added feature: SetCaptureTitle() which can be used to set a title for a
//         capture made with StartFrameCapture() or EndFrameCapture()
// 1.7.0 - Added feature: SetObjectAnnotation() / SetCommandAnnotation() for adding rich
//         annotations to objects and command streams

typedef struct RIDERDUCK_API_1_7_0
{
  pRIDERDUCK_GetAPIVersion GetAPIVersion;

  pRIDERDUCK_SetCaptureOptionU32 SetCaptureOptionU32;
  pRIDERDUCK_SetCaptureOptionF32 SetCaptureOptionF32;

  pRIDERDUCK_GetCaptureOptionU32 GetCaptureOptionU32;
  pRIDERDUCK_GetCaptureOptionF32 GetCaptureOptionF32;

  pRIDERDUCK_SetFocusToggleKeys SetFocusToggleKeys;
  pRIDERDUCK_SetCaptureKeys SetCaptureKeys;

  pRIDERDUCK_GetOverlayBits GetOverlayBits;
  pRIDERDUCK_MaskOverlayBits MaskOverlayBits;

  // Shutdown was renamed to RemoveHooks in 1.4.1.
  // These unions allow old code to continue compiling without changes
  union
  {
    pRIDERDUCK_Shutdown Shutdown;
    pRIDERDUCK_RemoveHooks RemoveHooks;
  };
  pRIDERDUCK_UnloadCrashHandler UnloadCrashHandler;

  // Get/SetLogFilePathTemplate was renamed to Get/SetCaptureFilePathTemplate in 1.1.2.
  // These unions allow old code to continue compiling without changes
  union
  {
    // deprecated name
    pRIDERDUCK_SetLogFilePathTemplate SetLogFilePathTemplate;
    // current name
    pRIDERDUCK_SetCaptureFilePathTemplate SetCaptureFilePathTemplate;
  };
  union
  {
    // deprecated name
    pRIDERDUCK_GetLogFilePathTemplate GetLogFilePathTemplate;
    // current name
    pRIDERDUCK_GetCaptureFilePathTemplate GetCaptureFilePathTemplate;
  };

  pRIDERDUCK_GetNumCaptures GetNumCaptures;
  pRIDERDUCK_GetCapture GetCapture;

  pRIDERDUCK_TriggerCapture TriggerCapture;

  // IsRemoteAccessConnected was renamed to IsTargetControlConnected in 1.1.1.
  // This union allows old code to continue compiling without changes
  union
  {
    // deprecated name
    pRIDERDUCK_IsRemoteAccessConnected IsRemoteAccessConnected;
    // current name
    pRIDERDUCK_IsTargetControlConnected IsTargetControlConnected;
  };
  pRIDERDUCK_LaunchReplayUI LaunchReplayUI;

  pRIDERDUCK_SetActiveWindow SetActiveWindow;

  pRIDERDUCK_StartFrameCapture StartFrameCapture;
  pRIDERDUCK_IsFrameCapturing IsFrameCapturing;
  pRIDERDUCK_EndFrameCapture EndFrameCapture;

  // new function in 1.1.0
  pRIDERDUCK_TriggerMultiFrameCapture TriggerMultiFrameCapture;

  // new function in 1.2.0
  pRIDERDUCK_SetCaptureFileComments SetCaptureFileComments;

  // new function in 1.4.0
  pRIDERDUCK_DiscardFrameCapture DiscardFrameCapture;

  // new function in 1.5.0
  pRIDERDUCK_ShowReplayUI ShowReplayUI;

  // new function in 1.6.0
  pRIDERDUCK_SetCaptureTitle SetCaptureTitle;

  // new functions in 1.7.0
  pRIDERDUCK_SetObjectAnnotation SetObjectAnnotation;
  pRIDERDUCK_SetCommandAnnotation SetCommandAnnotation;
} RIDERDUCK_API_1_7_0;

typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_0_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_0_1;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_0_2;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_1_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_1_1;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_1_2;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_2_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_3_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_4_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_4_1;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_4_2;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_5_0;
typedef RIDERDUCK_API_1_7_0 RIDERDUCK_API_1_6_0;

//////////////////////////////////////////////////////////////////////////////////////////////////
// RiderDuck API entry point
//
// This entry point can be obtained via GetProcAddress/dlsym if RiderDuck is available.
//
// The name is the same as the typedef - "RIDERDUCK_GetAPI"
//
// This function is not thread safe, and should not be called on multiple threads at once.
// Ideally, call this once as early as possible in your application's startup, before doing
// any API work, since some configuration functionality etc has to be done also before
// initialising any APIs.
//
// Parameters:
//   version is a single value from the RIDERDUCK_Version above.
//
//   outAPIPointers will be filled out with a pointer to the corresponding struct of function
//   pointers.
//
// Returns:
//   1 - if the outAPIPointers has been filled with a pointer to the API struct requested
//   0 - if the requested version is not supported or the arguments are invalid.
//
typedef int(RIDERDUCK_CC *pRIDERDUCK_GetAPI)(RIDERDUCK_Version version, void **outAPIPointers);

#ifdef __cplusplus
}    // extern "C"
#endif
