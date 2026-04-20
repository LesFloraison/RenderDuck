API Reference: Performance Counters
===================================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Counters
--------

.. autoclass:: riderduck.CounterDescription
  :members:

.. autoclass:: riderduck.CounterUnit
  :members:

.. autoclass:: riderduck.Uuid
  :members:

Counter Types
-------------

.. autoclass:: riderduck.GPUCounter
  :members:

.. autofunction:: riderduck.IsAMDCounter
.. autofunction:: riderduck.IsARMCounter
.. autofunction:: riderduck.IsGenericCounter
.. autofunction:: riderduck.IsIntelCounter
.. autofunction:: riderduck.IsNvidiaCounter
.. autofunction:: riderduck.IsVulkanExtendedCounter

Results
-------

.. autoclass:: riderduck.CounterResult
  :members:

.. autoclass:: riderduck.CounterValue
  :members:
