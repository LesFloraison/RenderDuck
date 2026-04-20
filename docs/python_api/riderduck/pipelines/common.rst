API Reference: Common Pipeline State Abstraction
================================================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

.. autoclass:: PipeState
  :members:

General
-------

.. autoclass:: riderduck.Offset
  :members:

Vertex Inputs
-------------

.. autoclass:: riderduck.BoundVBuffer
  :members:

.. autoclass:: riderduck.VertexInputAttribute
  :members:

.. autoclass:: riderduck.Topology
  :members:
  
.. autofunction:: riderduck.NumVerticesPerPrimitive
.. autofunction:: riderduck.VertexOffset
.. autofunction:: riderduck.PatchList_Count
.. autofunction:: riderduck.PatchList_Topology
.. autofunction:: riderduck.IsStrip

Shader Resource Bindings
------------------------

.. autoclass:: riderduck.UsedDescriptor
  :members:

.. autoclass:: riderduck.BindType
  :members:

.. autoclass:: riderduck.TextureSwizzle
  :members:

.. autoclass:: riderduck.TextureSwizzle4
  :members:

Samplers
--------

.. autoclass:: riderduck.AddressMode
  :members:

.. autoclass:: riderduck.TextureFilter
  :members:

.. autoclass:: riderduck.FilterMode
  :members:
  
.. autoclass:: riderduck.FilterFunction
  :members:

.. autoclass:: riderduck.ChromaSampleLocation
  :members:

.. autoclass:: riderduck.YcbcrConversion
  :members:

.. autoclass:: riderduck.YcbcrRange
  :members:

Viewport and Scissor
--------------------

.. autoclass:: riderduck.Viewport
  :members:

.. autoclass:: riderduck.Scissor
  :members:

Rasterizer
----------

.. autoclass:: riderduck.CullMode
  :members:

.. autoclass:: riderduck.FillMode
  :members:

.. autoclass:: riderduck.ConservativeRaster
  :members:

.. autoclass:: riderduck.LineRaster
  :members:

.. autoclass:: riderduck.ShadingRateCombiner
  :members:


Stencil
-------

.. autoclass:: riderduck.StencilFace
  :members:

.. autoclass:: riderduck.StencilOperation
  :members:

.. autoclass:: riderduck.CompareFunction
  :members:

Blending
--------

.. autoclass:: riderduck.ColorBlend
  :members:

.. autoclass:: riderduck.BlendEquation
  :members:

.. autoclass:: riderduck.BlendMultiplier
  :members:

.. autoclass:: riderduck.BlendOperation
  :members:

.. autoclass:: riderduck.LogicOperation
  :members:

Shader Messages
---------------

.. autoclass:: riderduck.ShaderMessage
  :members:

.. autoclass:: riderduck.ShaderMessageLocation
  :members:

.. autoclass:: riderduck.ShaderMeshMessageLocation
  :members:

.. autoclass:: riderduck.ShaderVertexMessageLocation
  :members:

.. autoclass:: riderduck.ShaderPixelMessageLocation
  :members:

.. autoclass:: riderduck.ShaderGeometryMessageLocation
  :members:

.. autoclass:: riderduck.ShaderComputeMessageLocation
  :members:


* qriderduck.ShaderMessageViewer
