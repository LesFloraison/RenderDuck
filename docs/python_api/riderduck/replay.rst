API Reference: Replay Control
=============================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Initialisation and Shutdown
---------------------------

.. autofunction:: riderduck.InitialiseReplay
.. autofunction:: riderduck.ShutdownReplay

.. autoclass:: riderduck.GlobalEnvironment
  :members:

.. autoclass:: riderduck.ResultCode
  :members:

.. autoclass:: riderduck.ResultDetails
  :members:

Capture File Access
-------------------

.. autofunction:: riderduck.OpenCaptureFile

.. autoclass:: riderduck.CaptureAccess
  :members:

.. autoclass:: riderduck.CaptureFile
  :members:

.. autoclass:: riderduck.ReplaySupport
  :members:

.. autoclass:: riderduck.CaptureFileFormat
  :members:

.. autoclass:: riderduck.SectionProperties
  :members:

.. autoclass:: riderduck.SectionType
  :members:

.. autoclass:: riderduck.SectionFlags
  :members:

.. autoclass:: riderduck.Thumbnail
  :members:

GPU Enumeration
---------------

.. autoclass:: riderduck.GPUDevice
  :members:

.. autoclass:: riderduck.GPUVendor
  :members:

.. autofunction:: riderduck.GPUVendorFromPCIVendor

.. autoclass:: riderduck.GraphicsAPI
  :members:

.. autofunction:: riderduck.IsD3D

.. autofunction:: riderduck.GetDriverInformation

.. autoclass:: riderduck.DriverInformation
  :members:

Replay Controller
-----------------

.. autoclass:: riderduck.ReplayController
  :members:

.. autoclass:: riderduck.ReplayOptions
  :members:

.. autoclass:: riderduck.ReplayOptimisationLevel
  :members:

.. autoclass:: riderduck.APIProperties
  :members:

Device Protocols
----------------

.. autoclass:: riderduck.DeviceProtocolController
  :members:

.. autofunction:: riderduck.GetSupportedDeviceProtocols
.. autofunction:: riderduck.GetDeviceProtocolController

Remote Servers
--------------

.. autoclass:: riderduck.RemoteServer
  :members:

.. autofunction:: riderduck.CreateRemoteServerConnection
.. autofunction:: riderduck.CheckRemoteServerConnection
.. autofunction:: riderduck.BecomeRemoteServer

.. autoclass:: riderduck.PathEntry
  :members:

.. autoclass:: riderduck.PathProperty
  :members:
