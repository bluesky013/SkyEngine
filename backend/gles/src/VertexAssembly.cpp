//
// Created by Zach Lee on 2023/2/2.
//

#include <gles/VertexAssembly.h>
#include <gles/BufferView.h>
#include <gles/VertexInput.h>
#include <gles/Core.h>
#include <gles/Device.h>

namespace sky::gles {

#define VA_OFFSET(offset) (static_cast<char *>(0) + (offset))

    VertexAssembly::~VertexAssembly()
    {
        auto deleteVAO = [](Queue &queue, GLuint vao) {
            queue.CreateTask([vao]() {
                CHECK(glDeleteVertexArrays(1, &vao));
            });
        };
        for (uint32_t i = 0; i < objects.size(); ++i) {
            auto &vao = objects[i];
            if (vao != 0) {
                deleteVAO(*device.GetQueue(static_cast<rhi::QueueType>(i)), vao);
            }
        }
    }

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        descriptor = desc;
        objects.resize(device.getQueueNumber(), 0);
        return true;
    }

    GLuint VertexAssembly::AcquireNativeHandle(uint32_t queueIndex)
    {
        auto &vao = objects[queueIndex];
        if (vao == 0) {
            CHECK(glGenVertexArrays(1, &vao));
            InitInternal(vao);
        }
        return vao;
    }

    void VertexAssembly::InitInternal(GLuint vao)
    {
        CHECK(glBindVertexArray(vao));
        auto vi = std::static_pointer_cast<VertexInput>(descriptor.vertexInput);
        auto &attributes = vi->GetAttributes();
        auto &bindings = vi->GetBindings();

        auto iter = attributes.begin();
        for (auto &binding : bindings) {
            auto bufferView = std::static_pointer_cast<BufferView>(descriptor.vertexBuffers[binding.binding]);
            CHECK(glBindBuffer(GL_ARRAY_BUFFER, bufferView->GetNativeHandle()));
            for (; iter != attributes.end() && (*iter).binding == binding.binding; ++iter) {
                auto &attribute = *iter;
                CHECK(glVertexAttribPointer(attribute.location, attribute.format.size, attribute.format.type, attribute.format.normalized, binding.stride, VA_OFFSET(attribute.offset)));
                CHECK(glVertexAttribDivisor(attribute.location, binding.inputRate == rhi::VertexInputRate::PER_INSTANCE ? 1 : 0));
                CHECK(glEnableVertexAttribArray(attribute.location));
            }
        }

        if (descriptor.indexBuffer) {
            CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, std::static_pointer_cast<BufferView>(descriptor.indexBuffer)->GetNativeHandle()));
        }

        // resume
        CHECK(glBindVertexArray(0));
        CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
        CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }
}