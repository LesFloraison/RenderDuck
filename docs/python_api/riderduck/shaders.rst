API Reference: Shaders
======================

This is the API reference for the functions, classes, and enums in the ``riderduck`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: riderduck

Descriptors
-----------

.. autoclass:: riderduck.Descriptor
  :members:

.. autoclass:: riderduck.SamplerDescriptor
  :members:

.. autoclass:: riderduck.DescriptorFlags
  :members:
   
.. autoclass:: riderduck.DescriptorCategory
  :members:
   
.. autoclass:: riderduck.DescriptorType
  :members:
   
.. autofunction:: riderduck.CategoryForDescriptorType
.. autofunction:: riderduck.IsConstantBlockDescriptor
.. autofunction:: riderduck.IsReadOnlyDescriptor
.. autofunction:: riderduck.IsReadWriteDescriptor
.. autofunction:: riderduck.IsSamplerDescriptor

.. autoclass:: riderduck.DescriptorLogicalLocation
  :members:

.. autoclass:: riderduck.DescriptorRange
  :members:

.. autoclass:: riderduck.DescriptorAccess
  :members:

Reflection
----------

.. autoclass:: riderduck.ShaderReflection
  :members:

.. autoclass:: riderduck.ShaderStage
  :members:

.. autoclass:: riderduck.ShaderStageMask
  :members:

.. autofunction:: riderduck.MaskForStage
.. autofunction:: riderduck.FirstStageForMask

.. autoclass:: riderduck.SigParameter
  :members:

.. autoclass:: riderduck.ShaderBuiltin
  :members:

.. autoclass:: riderduck.ConstantBlock
  :members:

.. autoclass:: riderduck.ShaderSampler
  :members:

.. autoclass:: riderduck.ShaderResource
  :members:

Debug Info
----------

.. autoclass:: riderduck.ShaderDebugInfo
  :members:

.. autoclass:: riderduck.ShaderEncoding
  :members:
  
.. autoclass:: riderduck.KnownShaderTool
  :members:
  
.. autofunction:: riderduck.ToolExecutable
.. autofunction:: riderduck.ToolInput
.. autofunction:: riderduck.ToolOutput

.. autofunction:: riderduck.IsTextRepresentation

.. autoclass:: riderduck.ShaderEntryPoint
  :members:

.. autoclass:: riderduck.ShaderSourceFile
  :members:

.. autoclass:: riderduck.ShaderCompileFlags
  :members:

.. autoclass:: riderduck.ShaderCompileFlag
  :members:

.. autoclass:: riderduck.ShaderSourcePrefix
  :members:

Shader Constants
----------------

.. autoclass:: riderduck.ShaderConstant
  :members:

.. autoclass:: riderduck.ShaderConstantType
  :members:

.. autoclass:: riderduck.ShaderVariableFlags
  :members:

.. autoclass:: riderduck.VarType
  :members:

.. autofunction:: riderduck.VarTypeByteSize
.. autofunction:: riderduck.VarTypeCompType

Shader Debugging
----------------

.. autoclass:: riderduck.ShaderDebugTrace
  :members:

.. autoclass:: riderduck.ShaderDebugger
  :members:

.. autoclass:: riderduck.SourceVariableMapping
  :members:

.. autoclass:: riderduck.DebugVariableReference
  :members:

.. autoclass:: riderduck.DebugVariableType
  :members:

.. autoclass:: riderduck.LineColumnInfo
  :members:

.. autoclass:: riderduck.InstructionSourceInfo
  :members:

.. autoclass:: riderduck.ShaderDebugState
  :members:

.. autoclass:: riderduck.ShaderEvents
  :members:

.. autoclass:: riderduck.ShaderVariableChange
  :members:

Shader Variables
----------------
  
.. autoclass:: riderduck.ShaderVariable
  :members:

.. autoclass:: riderduck.ShaderValue
  :members:

.. autoclass:: riderduck.PointerVal
  :members:

.. autoclass:: riderduck.ShaderBindIndex
  :members:

.. autoclass:: riderduck.ShaderDirectAccess
  :members:
