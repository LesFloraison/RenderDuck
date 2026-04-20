Getting Started (RiderDuck UI)
==============================

.. note::

  This document is aimed at users getting started with scripting within the RiderDuck UI.

  The module used here (``qriderduck``) is not available as a stand-alone module.

When working within the RiderDuck UI, the ``riderduck`` and ``qriderduck`` modules are implicitly imported when any script runs. In addition to this, an additional global ``pyriderduck`` is always available. It is an instance of :py:class:`~qriderduck.CaptureContext` and it represents the interface into the UI interface.

Loading a Capture
-----------------

Unlike :doc:`when using the base module directly <riderduck_intro>`, within the UI it is strongly recommended that you load captures using the UI interfaces itself rather than doing it entirely within python code, otherwise there is a risk of conflict between the two loaded captures in the same program.

To load a capture programmatically, we can use the ``pyriderduck`` global variable and call :py:meth:`~qriderduck.CaptureContext.LoadCapture`. When loading a local capture some of the parameters are redundant - we don't need to specify a different "actual" file vs the loaded file, and it is not a temporary handle. These parameters are primarily used by the UI itself when loading captures from remote hosts or immediately after they are generated while they are still stored temporarily on disk and the user needs to be prompted to save or delete them on close.

.. highlight:: python
.. code:: python

    filename = 'test.rdc'

    # Load a file, with the same 'original' name, that's not temporary, and is local
    pyriderduck.LoadCapture(filename, riderduck.ReplayOptions(), filename, False, True)

This will close any capture that is already loaded, but to just close an open capture you can use :py:meth:`~qriderduck.CaptureContext.CloseCapture`.

As part of opening the capture, RiderDuck will begin replay automatically and populate the various panels and internal data structures for easy access. It will also handle prompting the user if any errors happen or if replay isn't supported.

Once the capture is opened, all of the RiderDuck UI accessible data is immediately available and can be accessed directly, see :ref:`qriderduck-python-basics`.

Accessing Capture Analysis
--------------------------

To access the :py:class:`~riderduck.ReplayController` and any related core interfaces, a little bit of extra work is required. Within the UI, the replay work happens on a separate thread to prevent long-running tasks from causing the UI to become unresponsive. That means the :py:class:`~riderduck.ReplayController` is not immediately available, but is provided to a callback on the right thread.

To invoke onto the right thread, you can use :py:meth:`~qriderduck.ReplayManager.BlockInvoke` and pass it a callback that will be called with a single parameter - the :py:class:`~riderduck.ReplayController` instance for the currently open capture.

.. warning::

  There is another invoke function :py:meth:`~qriderduck.ReplayManager.AsyncInvoke`, but due to Python's limited threading capability the callback can't be called while the script is executing, meaning this has limited use. For Python it is recommended to use :py:meth:`~qriderduck.ReplayManager.BlockInvoke`.

.. highlight:: python
.. code:: python

    def myCallback(controller):
        print("%d top-level actions" % len(controller.GetRootActions()))

    pyriderduck.Replay().BlockInvoke(myCallback)

If there is no replay active, the callback will be silently dropped.
