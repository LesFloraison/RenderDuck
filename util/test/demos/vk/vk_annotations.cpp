/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

RD_TEST(VK_Annotations, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Test annotations via the vulkan API.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    // these tags provide the layer a way (for dispatchable objects at least) to identify
    // dispatchable objects that may be wrapped by the loader
    VkDebugUtilsObjectTagInfoEXT tag = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT,
        NULL,
        VK_OBJECT_TYPE_INSTANCE,
        (uint64_t)instance,
        RIDERDUCK_APIObjectAnnotationHelper,
        sizeof(instance),
        instance,
    };

    vkSetDebugUtilsObjectTagEXT(device, &tag);

    tag.objectHandle = (uint64_t)phys;
    tag.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
    tag.pTag = phys;
    vkSetDebugUtilsObjectTagEXT(device, &tag);

    tag.objectHandle = (uint64_t)device;
    tag.objectType = VK_OBJECT_TYPE_DEVICE;
    tag.pTag = device;
    vkSetDebugUtilsObjectTagEXT(device, &tag);

    AllocatedImage img(this,
                       vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(img.image, "Annotated Image");
    setName(DefaultTriVB.buffer, "Vertex Buffer");

    // cache the device pointer we pass in
    void *d = RIDERDUCK_DEVICEPOINTER_FROM_VKINSTANCE(instance);

    if(rdoc)
    {
      rdoc->SetObjectAnnotation(d, img.image, "basic.bool", eRIDERDUCK_Bool, 0,
                                RDAnnotationHelper(true));
      rdoc->SetObjectAnnotation(d, img.image, "basic.int32", eRIDERDUCK_Int32, 0,
                                RDAnnotationHelper(-3));
      rdoc->SetObjectAnnotation(d, img.image, "basic.int64", eRIDERDUCK_Int64, 0,
                                RDAnnotationHelper((int64_t)-3000000000000LL));
      rdoc->SetObjectAnnotation(d, img.image, "basic.uint32", eRIDERDUCK_UInt32, 0,
                                RDAnnotationHelper(3));
      rdoc->SetObjectAnnotation(d, img.image, "basic.uint64", eRIDERDUCK_UInt64, 0,
                                RDAnnotationHelper((uint64_t)3000000000000LL));
      rdoc->SetObjectAnnotation(d, img.image, "basic.float", eRIDERDUCK_Float, 0,
                                RDAnnotationHelper(3.25f));
      rdoc->SetObjectAnnotation(d, img.image, "basic.double", eRIDERDUCK_Double, 0,
                                RDAnnotationHelper(3.25000000001));
      rdoc->SetObjectAnnotation(d, img.image, "basic.string", eRIDERDUCK_String, 0,
                                RDAnnotationHelper("Hello, World!"));

      RIDERDUCK_AnnotationValue val;
      val.apiObject = (void *)DefaultTriVB.buffer;
      rdoc->SetObjectAnnotation(d, img.image, "basic.object", eRIDERDUCK_APIObject, 0, &val);

      rdoc->SetObjectAnnotation(d, img.image, "basic.object.__offset", eRIDERDUCK_UInt32, 0,
                                RDAnnotationHelper(64));
      rdoc->SetObjectAnnotation(d, img.image, "basic.object.__size", eRIDERDUCK_UInt32, 0,
                                RDAnnotationHelper(32));
      rdoc->SetObjectAnnotation(d, img.image, "basic.object.__rd_format", eRIDERDUCK_String, 0,
                                RDAnnotationHelper("float4 vertex_data;"));

      rdoc->SetObjectAnnotation(d, DefaultTriVB.buffer, "__rd_format", eRIDERDUCK_String, 0,
                                RDAnnotationHelper("float3 pos;\n"
                                                   "float4 col;\n"
                                                   "float2 uv;\n"));

      val = {};
      val.vector.float32[0] = 1.1f;
      val.vector.float32[1] = 2.2f;
      val.vector.float32[2] = 3.3f;
      val.vector.float32[3] = 4.4f;    // should be ignored
      rdoc->SetObjectAnnotation(d, img.image, "basic.vec3", eRIDERDUCK_Float, 3, &val);

      rdoc->SetObjectAnnotation(d, img.image, "deep.nested.path.to.annotation", eRIDERDUCK_Int32, 0,
                                RDAnnotationHelper(-4));
      rdoc->SetObjectAnnotation(d, img.image, "deep.nested.path.to.annotation2", eRIDERDUCK_Int32,
                                0, RDAnnotationHelper(-5));
      rdoc->SetObjectAnnotation(d, img.image, "deep.alternate.path.to.annotation", eRIDERDUCK_Int32,
                                0, RDAnnotationHelper(-6));

      // deleted paths should not stay around
      rdoc->SetObjectAnnotation(d, img.image, "deleteme", eRIDERDUCK_Int32, 0,
                                RDAnnotationHelper(-7));
      rdoc->SetObjectAnnotation(d, img.image, "deleteme", eRIDERDUCK_Empty, 0, NULL);

      rdoc->SetObjectAnnotation(d, img.image, "path.deleted.by.parent", eRIDERDUCK_Int32, 0,
                                RDAnnotationHelper(-8));
      rdoc->SetObjectAnnotation(d, img.image, "path.deleted.by.parent2", eRIDERDUCK_Int32, 0,
                                RDAnnotationHelper(-9));

      // this will delete all children. `path` will still exist, but will be empty
      rdoc->SetObjectAnnotation(d, img.image, "path.deleted", eRIDERDUCK_Empty, 0, NULL);
    }

    while(Running())
    {
      if(rdoc)
      {
        // queue annotations are only included when in the captured frame
        if(curFrame == 2)
          rdoc->SetCommandAnnotation(d, queue, "queue.too_old", eRIDERDUCK_Int32, 0,
                                     RDAnnotationHelper(1000));

        rdoc->SetCommandAnnotation(d, queue, "queue.value", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(1000));

        rdoc->SetCommandAnnotation(d, queue, "command.overwritten", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(9999));

        rdoc->SetCommandAnnotation(d, queue, "command.inherited", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(1234));

        rdoc->SetCommandAnnotation(d, queue, "command.deleted", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(50));
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      setMarker(cmd, "Start");

      if(rdoc)
      {
        rdoc->SetCommandAnnotation(d, cmd, "new.value", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(2000));

        rdoc->SetCommandAnnotation(d, cmd, "command.overwritten", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(-3333));

        rdoc->SetCommandAnnotation(d, cmd, "command.new", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(3333));

        rdoc->SetCommandAnnotation(d, cmd, "command.deleted", eRIDERDUCK_Empty, 0, NULL);
      }

      setMarker(cmd, "Initial");

      VkImage swapimg = StartUsingBackbuffer(cmd);

      vkh::cmdClearImage(cmd, swapimg, vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f));

      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });

      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

      if(rdoc)
        rdoc->SetCommandAnnotation(d, cmd, "command.new", eRIDERDUCK_Float, 0,
                                   RDAnnotationHelper(1.75f));

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DefaultTriPipe);
      mainWindow->setViewScissor(cmd);
      vkh::cmdBindVertexBuffers(cmd, {DefaultTriVB.buffer});

      setMarker(cmd, "Pre-Draw");

      // deleting a value is fine if it's re-added before the next event
      if(rdoc)
      {
        rdoc->SetCommandAnnotation(d, cmd, "new.value", eRIDERDUCK_Empty, 0, NULL);
        rdoc->SetCommandAnnotation(d, cmd, "new.value", eRIDERDUCK_Int32, 0,
                                   RDAnnotationHelper(4000));
      }

      setMarker(cmd, "Draw 1");

      vkCmdDraw(cmd, 3, 1, 0, 0);

      VkViewport view = mainWindow->viewport;
      view.width /= 2;
      view.height /= 2;
      vkCmdSetViewport(cmd, 0, 1, &view);

      setMarker(cmd, "Draw 2");

      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd);

      vkEndCommandBuffer(cmd);

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
