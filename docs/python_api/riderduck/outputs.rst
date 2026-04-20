API Reference: Replay Outputs
=============================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

General
-------

.. autoclass:: ReplayOutput
  :members:

.. autoclass:: ReplayOutputType
  :members:

.. autofunction:: riderduck.SetColors

Window Configuration
--------------------

.. autoclass:: WindowingData
  :members:

.. autoclass:: WindowingSystem
  :members:

.. autofunction:: riderduck.CreateHeadlessWindowingData
.. autofunction:: riderduck.CreateWin32WindowingData
.. autofunction:: riderduck.CreateXlibWindowingData
.. autofunction:: riderduck.CreateXCBWindowingData
.. autofunction:: riderduck.CreateWaylandWindowingData
.. autofunction:: riderduck.CreateAndroidWindowingData
.. autofunction:: riderduck.CreateMacOSWindowingData

Texture View
------------

.. autoclass:: TextureDisplay
  :members:

.. autoclass:: DebugOverlay
  :members:

Mesh View
---------

.. autoclass:: MeshDisplay
  :members:

.. autoclass:: MeshDataStage
  :members:

.. autoclass:: MeshletSize
  :members:

.. autoclass:: TaskGroupSize
  :members:

.. autoclass:: MeshFormat
  :members:

.. autoclass:: Visualisation
  :members:

.. autoclass:: Camera
  :members:

.. autoclass:: CameraType
  :members:

.. autoclass:: AxisMapping
  :members:

.. autofunction:: riderduck.InitCamera
