//
// Created by blues on 2024/3/29.
//

#pragma once

#include <string>
#include <functional>
#include <imgui.h>
//#include <implot.h>

namespace sky {
    struct ImContext {
        ImGuiContext *imContext = nullptr;
//        ImPlotContext *plotContext = nullptr;

        void Init();
        void Destroy();

        void MakeCurrent() const;
    };

    using UIFunc = std::function<void(ImContext &context)>;

    class ImWidget {
    public:
        explicit ImWidget(const std::string &name_) : name(name_) {}
        virtual ~ImWidget() = default;

        virtual void Execute(ImContext &context) = 0;

        inline const std::string &GetName() const { return name; }

    protected:
        std::string name;
    };

    class LambdaWidget : public ImWidget {
    public:
        template <typename Func>
        explicit LambdaWidget(const std::string &name, Func &&f) : ImWidget(name), fn(std::forward<Func>(f)) {}
        ~LambdaWidget() override = default;

        void Execute(ImContext &context) override { fn(context); }

    private:
        UIFunc fn;
    };

} // namespace sky