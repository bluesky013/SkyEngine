#pragma once

#include <animation/assets/GraphAsset.h>
#include <core/file/FileTransaction.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/document/Document.h>
#include <functional>
#include <span>

namespace sky::animation::editor {
    struct GraphDocumentStorage {
        std::function<FileOperationStatus(std::string &)>    read;
        std::function<FileOperationStatus(std::string_view)> write;
        std::function<void(const Uuid &)>                    requestCook;
    };
    GraphDocumentStorage GraphSourceStorage(const Uuid &asset);

    class GraphDocument final : public sky::editor::Document {
    public:
        explicit GraphDocument(Uuid asset, GraphDocumentStorage storage);
        explicit GraphDocument(Uuid asset);
        ~GraphDocument() override;
        bool Load() override;
        bool Save() override;
        bool IsLoaded() const
        {
            return loaded;
        }
        void RequestSave()
        {
            savePending = true;
        }
        bool FlushPendingSave();
        bool IsSavePending() const
        {
            return savePending;
        }
        const GraphSource &Source() const
        {
            return source;
        }
        const GraphValidation &Diagnostics() const
        {
            return diagnostics;
        }
        const std::string &LastError() const
        {
            return lastError;
        }
        uint64_t Revision() const
        {
            return revision;
        }
        const Uuid &AssetId() const
        {
            return asset;
        }
        sky::editor::CommandService &Commands()
        {
            return commands;
        }
        GraphValidation             Apply(std::string label, GraphSource next);
        sky::editor::UndoCommandPtr MakeEdit(std::string label, GraphSource next);
        Uuid                        AddNode(GraphNodeKind kind, float x, float y);
        Uuid                        AddParameter(std::string name, GraphParameterType type);
        Uuid                        AddState(std::string name, const Uuid &output = {});
        Uuid                        AddTransition(const Uuid &from, const Uuid &to);
        GraphValidation             Connect(const Uuid &from, const Uuid &to, GraphPin input);
        GraphValidation             Disconnect(const Uuid &to, GraphPin input);
        GraphValidation             Remove(std::span<const Uuid> ids);
        GraphValidation             MoveNodes(std::span<const GraphNodePosition> positions);
        std::vector<Uuid>           CopyNodes(std::span<const Uuid> ids, float offsetX, float offsetY);

    private:
        class Edit;
        void                        SetSource(const GraphSource &value);
        void                        Refresh();
        Uuid                        asset;
        GraphDocumentStorage        storage;
        GraphSource                 source;
        GraphValidation             diagnostics;
        std::string                 saved, lastError;
        uint64_t                    revision    = 0;
        bool                        savePending = false, loaded = false;
        sky::editor::CommandService commands;
    };
} // namespace sky::animation::editor
