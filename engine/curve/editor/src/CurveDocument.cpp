#include <curve/editor/CurveDocument.h>

#include <curve/assets/CurveSource.h>

namespace sky::curve::editor {
    CurveSourceStorage CurveAssetStorage(const Uuid &asset)
    {
        return sky::editor::MakeAssetSourceStorage(asset, MAX_CURVE_SOURCE_BYTES);
    }

    class CurveDocument::Edit final : public sky::editor::UndoCommand {
    public:
        Edit(CurveDocument &document, std::string label, Curve next)
            : UndoCommand(std::move(label)), document(document), before(document.GetCurve()), after(std::move(next))
        {
        }
        void Do() override
        {
            document.SetCurve(after);
        }
        void Undo() override
        {
            document.SetCurve(before);
        }

    private:
        CurveDocument &document;
        Curve          before, after;
    };

    CurveDocument::CurveDocument(Uuid inAsset, CurveSourceStorage inStorage)
        : Document(inAsset.ToString()), asset(inAsset), storage(std::move(inStorage))
    {
    }
    CurveDocument::CurveDocument(Uuid inAsset) : CurveDocument(inAsset, CurveAssetStorage(inAsset))
    {
    }
    CurveDocument::~CurveDocument()
    {
        commands.Clear();
    }
    bool CurveDocument::Load()
    {
        std::string text, error;
        const auto  status = storage.read ? storage.read(text) : FileOperationStatus{FileOperationError::Unsupported, "Curve reader unavailable"};
        if (!status) {
            lastError = status.message;
            return false;
        }
        Curve next;
        if (!ParseCurveSource(text, next, error)) {
            lastError = error;
            return false;
        }
        if (!WriteCurveSource(next, text)) {
            lastError = "Curve source cannot be serialized";
            return false;
        }
        curve = std::move(next);
        lastError.clear();
        state.Loaded(*this, commands, std::move(text));
        return true;
    }
    bool CurveDocument::Save()
    {
        if (!state.BeginSave(lastError))
            return false;
        std::string text;
        if (!WriteCurveSource(curve, text)) {
            lastError = "Curve cannot be serialized";
            return false;
        }
        const auto status = storage.write ? storage.write(text) : FileOperationStatus{FileOperationError::Unsupported, "Curve writer unavailable"};
        if (!status) {
            lastError = status.message;
            return false;
        }
        lastError.clear();
        state.Saved(*this, std::move(text));
        if (storage.requestCook)
            storage.requestCook(asset);
        return true;
    }
    void CurveDocument::RequestSave()
    {
        state.RequestSave();
    }
    bool CurveDocument::FlushPendingSave()
    {
        if (!state.IsSavePending())
            return true;
        return Save();
    }
    bool CurveDocument::IsSavePending() const
    {
        return state.IsSavePending();
    }
    bool CurveDocument::IsLoaded() const
    {
        return state.IsLoaded();
    }
    const Curve &CurveDocument::GetCurve() const
    {
        return curve;
    }
    const std::string &CurveDocument::LastError() const
    {
        return lastError;
    }
    uint64_t CurveDocument::Revision() const
    {
        return state.Revision();
    }
    const Uuid &CurveDocument::AssetId() const
    {
        return asset;
    }
    sky::editor::CommandService &CurveDocument::Commands()
    {
        return commands;
    }
    void CurveDocument::SetCurve(const Curve &next)
    {
        curve = next;
        std::string error;
        if (!curve.Validate(error))
            lastError = std::move(error);
        else
            lastError.clear();
        std::string encoded;
        const bool  valid = WriteCurveSource(curve, encoded);
        state.Changed(*this, encoded, valid);
    }
    sky::editor::UndoCommandPtr CurveDocument::MakeEdit(std::string label, Curve next)
    {
        std::string error;
        if (!next.Validate(error)) {
            lastError = std::move(error);
            return {};
        }
        lastError.clear();
        std::string before, after;
        if (!WriteCurveSource(curve, before) || !WriteCurveSource(next, after)) {
            lastError = "Curve cannot be serialized";
            return {};
        }
        if (before == after)
            return {};
        return std::make_unique<Edit>(*this, std::move(label), std::move(next));
    }
    bool CurveDocument::Apply(std::string label, Curve next)
    {
        auto command = MakeEdit(std::move(label), std::move(next));
        return command ? commands.Execute(std::move(command)) : lastError.empty();
    }
} // namespace sky::curve::editor
