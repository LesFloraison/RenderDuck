import riderduck as rd
import rdtest


class VK_Annotations(rdtest.Annotations):
    demos_test_name = 'VK_Annotations'
    internal = False

    def check_capture(self):
        super().check_resource_annotations()
        super().check_command_annotations(True)