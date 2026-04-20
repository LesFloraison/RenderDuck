API Reference: Structured Data
==============================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Type information
----------------

.. autoclass:: SDType
  :members:

.. autoclass:: SDBasic
  :members:

.. autoclass:: SDTypeFlags
  :members:

Objects
-------

.. autoclass:: SDObject
  :members:

.. autoclass:: SDObjectData
  :members:

.. autoclass:: SDObjectPODData
  :members:

Chunks
------

.. autoclass:: SDChunk
  :members:

.. autoclass:: SDChunkMetaData
  :members:

.. autoclass:: SDChunkFlags
  :members:

Structured File
---------------

.. autoclass:: SDFile
  :members:

Creation Helper Functions
-------------------------

.. autofunction:: riderduck.makeSDArray
.. autofunction:: riderduck.makeSDBool
.. autofunction:: riderduck.makeSDEnum
.. autofunction:: riderduck.makeSDFloat
.. autofunction:: riderduck.makeSDInt32
.. autofunction:: riderduck.makeSDInt64
.. autofunction:: riderduck.makeSDResourceId
.. autofunction:: riderduck.makeSDString
.. autofunction:: riderduck.makeSDStruct
.. autofunction:: riderduck.makeSDUInt32
.. autofunction:: riderduck.makeSDUInt64
