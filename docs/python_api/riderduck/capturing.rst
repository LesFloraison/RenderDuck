API Reference: Capturing
========================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Execution & Injection
---------------------

.. autofunction:: riderduck.ExecuteAndInject
.. autofunction:: riderduck.InjectIntoProcess

.. autoclass:: riderduck.CaptureOptions
  :members:
  
.. autofunction:: riderduck.GetDefaultCaptureOptions

.. autoclass:: riderduck.EnvironmentModification
  :members:

.. autoclass:: riderduck.EnvMod
  :members:

.. autoclass:: riderduck.EnvSep
  :members:

.. autoclass:: riderduck.ExecuteResult
  :members:

Global Hooking
--------------

.. autofunction:: riderduck.StartGlobalHook
.. autofunction:: riderduck.StopGlobalHook
.. autofunction:: riderduck.IsGlobalHookActive
.. autofunction:: riderduck.CanGlobalHook

Target Control
--------------

.. autofunction:: riderduck.EnumerateRemoteTargets
.. autofunction:: riderduck.CreateTargetControl

.. autoclass:: riderduck.TargetControl
  :members:

.. autoclass:: riderduck.TargetControlMessage
  :members:

.. autoclass:: riderduck.TargetControlMessageType
  :members:

.. autoclass:: riderduck.NewCaptureData
  :members:

.. autoclass:: riderduck.APIUseData
  :members:

.. autoclass:: riderduck.BusyData
  :members:

.. autoclass:: riderduck.NewChildData
  :members:

