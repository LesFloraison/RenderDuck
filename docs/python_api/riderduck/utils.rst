API Reference: Utilities
========================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Maths
-----

.. autoclass:: FloatVector
  :members:

.. autofunction:: riderduck.HalfToFloat
.. autofunction:: riderduck.FloatToHalf

Logging & Versioning
--------------------

.. autofunction:: riderduck.LogMessage
.. autofunction:: riderduck.SetDebugLogFile
.. autofunction:: riderduck.GetLogFile
.. autofunction:: riderduck.GetCurrentProcessMemoryUsage
.. autofunction:: riderduck.DumpObject

.. autoclass:: LogType
  :members:


Versioning
----------

.. autofunction:: riderduck.GetVersionString
.. autofunction:: riderduck.GetCommitHash
.. autofunction:: riderduck.IsReleaseBuild

Settings
--------

.. autofunction:: riderduck.GetConfigSetting
.. autofunction:: riderduck.SetConfigSetting
.. autofunction:: riderduck.SaveConfigSettings

Self-hosted captures
--------------------

.. autofunction:: riderduck.CanSelfHostedCapture
.. autofunction:: riderduck.StartSelfHostCapture
.. autofunction:: riderduck.EndSelfHostCapture
