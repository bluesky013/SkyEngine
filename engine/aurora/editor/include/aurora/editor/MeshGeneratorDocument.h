#pragma once

#include <aurora/editor/MeshGeneratorDocumentContract.h>
#include <editor/ui/AssetViewProvider.h>

#if defined(_WIN32)
    #if defined(AURORA_EDITOR_IMPLEMENTATION)
        #define AURORA_EDITOR_API __declspec(dllexport)
    #else
        #define AURORA_EDITOR_API __declspec(dllimport)
    #endif
#else
    #define AURORA_EDITOR_API __attribute__((visibility("default")))
#endif

namespace sky {
    class SerializationContext;
}

namespace sky::aurora::editor {
    class IMeshGeneratorDocument : public sky::editor::IAssetViewDocument {
    public:
        virtual const MeshGeneratorInputData     *Source() const                                      = 0;
        virtual const MeshGeneratorOutputData    *Generated() const                                   = 0;
        virtual const Any                        &Settings() const                                    = 0;
        virtual std::string                       GeneratorId() const                                 = 0;
        virtual sky::editor::AssetGeneratorStatus SelectGenerator(const std::string &id)              = 0;
        virtual sky::editor::AssetGeneratorStatus SetControls(std::vector<MeshControlPoint> controls) = 0;
        virtual sky::editor::AssetGeneratorStatus SetSettings(const Any &settings)                    = 0;
        virtual sky::editor::AssetGeneratorStatus RequestGenerate()                                   = 0;
        virtual void                              CancelGeneration()                                  = 0;
        virtual void                              SetSavePath(std::string displayPath)                = 0;
        virtual Uuid                              SavedOwner() const                                  = 0;
    };

    AURORA_EDITOR_API std::unique_ptr<IMeshGeneratorDocument> CreateMeshGeneratorDocument(const Uuid &asset);
    void                                                      ReflectMeshGeneratorDocument(SerializationContext *context);
    void                                                      RegisterMeshGeneratorDocument();
    void                                                      UnregisterMeshGeneratorDocument();
} // namespace sky::aurora::editor
