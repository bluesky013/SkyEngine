//
// Created on 2026/09/21.
//

#include <editor/core/document/DocumentService.h>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace sky::editor {
    namespace {

        class StringDocument : public Document {
        public:
            explicit StringDocument(std::string assetId) : Document("mem://" + assetId), assetId(std::move(assetId)) {}

            bool Load() override
            {
                content = "loaded:" + assetId;
                ClearDirty();
                return true;
            }

            bool Save() override
            {
                saved = content;
                ClearDirty();
                return true;
            }

            const std::string &GetAssetId() const { return assetId; }

            std::string content;
            std::string saved;

        private:
            std::string assetId;
        };

    } // namespace

    TEST(DocumentServiceTest, OpenFindClose)
    {
        DocumentService service;
        service.RegisterFactory("txt", [](const std::string &assetId) -> DocumentPtr {
            auto document = std::make_shared<StringDocument>(assetId);
            document->Load();
            return document;
        });

        Document *opened = service.Open("asset/a", "txt");
        ASSERT_NE(opened, nullptr);
        EXPECT_EQ(service.Find("asset/a"), opened);
        EXPECT_EQ(service.GetDocuments().size(), 1u);

        // Reopening returns the same instance.
        EXPECT_EQ(service.Open("asset/a", "txt"), opened);
        EXPECT_EQ(service.GetDocuments().size(), 1u);

        EXPECT_TRUE(service.Close("asset/a"));
        EXPECT_EQ(service.Find("asset/a"), nullptr);
        EXPECT_TRUE(service.GetDocuments().empty());
    }

    TEST(DocumentServiceTest, DirtyAndSave)
    {
        DocumentService service;
        service.RegisterFactory("txt", [](const std::string &assetId) -> DocumentPtr {
            auto document = std::make_shared<StringDocument>(assetId);
            document->Load();
            return document;
        });

        Document *document = service.Open("b", "txt");
        ASSERT_NE(document, nullptr);
        EXPECT_FALSE(document->IsDirty());

        document->MarkDirty();
        EXPECT_TRUE(document->IsDirty());

        EXPECT_TRUE(document->Save());
        EXPECT_FALSE(document->IsDirty());
    }

    TEST(DocumentServiceTest, UnknownTypeReturnsNull)
    {
        DocumentService service;
        EXPECT_EQ(service.Open("a", "unknown"), nullptr);
        EXPECT_EQ(service.Find("a"), nullptr);
    }

} // namespace sky::editor
