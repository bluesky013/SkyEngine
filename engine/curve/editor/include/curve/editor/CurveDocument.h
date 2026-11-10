#pragma once

#include <core/util/Uuid.h>
#include <curve/Curve.h>
#include <editor/core/command/CommandService.h>
#include <editor/core/document/Document.h>
#include <editor/core/document/SourceDocumentState.h>
#include <editor/core/document/SourceDocumentStorage.h>

#include <functional>
#include <string>
#include <string_view>

namespace sky::curve::editor {

    using CurveSourceStorage = sky::editor::SourceDocumentStorage;
    CurveSourceStorage CurveAssetStorage(const Uuid &asset);

    class CurveDocument final : public sky::editor::Document {
    public:
        explicit CurveDocument(Uuid asset, CurveSourceStorage storage);
        explicit CurveDocument(Uuid asset);
        ~CurveDocument() override;

        bool                         Load() override;
        bool                         Save() override;
        void                         RequestSave();
        bool                         FlushPendingSave();
        bool                         IsSavePending() const;
        bool                         IsLoaded() const;
        const Curve                 &GetCurve() const;
        const std::string           &LastError() const;
        uint64_t                     Revision() const;
        const Uuid                  &AssetId() const;
        sky::editor::CommandService &Commands();
        sky::editor::UndoCommandPtr  MakeEdit(std::string label, Curve next);
        bool                         Apply(std::string label, Curve next);

    private:
        class Edit;
        void                             SetCurve(const Curve &next);
        Uuid                             asset;
        CurveSourceStorage               storage;
        Curve                            curve;
        std::string                      lastError;
        sky::editor::SourceDocumentState state;
        sky::editor::CommandService      commands;
    };

} // namespace sky::curve::editor
