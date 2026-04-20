filename = "test.rdc"
formatter = "float3 pos; half norms[16]; uint flags;"

pyriderduck.LoadCapture(filename, riderduck.ReplayOptions(), filename, False, True)

mybuf = riderduck.ResourceId.Null()

for buf in pyriderduck.GetBuffers():
    print("buf %s is %s" % (buf.resourceId, pyriderduck.GetResourceName(buf.resourceId)))

    # here put your actual selection criteria - i.e. look for a particular name
    if pyriderduck.GetResourceName(buf.resourceId) == "dataBuffer":
        mybuf = buf.resourceId
        break

print("selected %s" % pyriderduck.GetResourceName(mybuf))

if mybuf != riderduck.ResourceId.Null():
	# Open a new buffer viewer for this buffer, with the given format
	bufview = pyriderduck.ViewBuffer(0, 0, mybuf, formatter)

	# Show the buffer viewer on the main tool area
	pyriderduck.AddDockWindow(bufview.Widget(), qriderduck.DockReference.MainToolArea, None)
