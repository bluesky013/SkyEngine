#include <core/thread/ThreadDispatcher.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/ICookRunner.h>
#include <framework/asset/ISourceCatalog.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace sky;

namespace {
    FilePath ProductPath(const Uuid &id)
    {
        const auto text = id.ToString();
        return FilePath(text.substr(0, 2) + "/" + text + ".bin");
    }

    uint64_t ProductChecksum(std::span<const uint8_t> bytes)
    {
        uint64_t checksum = 14695981039346656037ULL;
        for (const auto byte : bytes)
            checksum = (checksum ^ byte) * 1099511628211ULL;
        return checksum;
    }

    template <typename T>
    T HeaderValue(const std::vector<uint8_t> &bytes, size_t offset)
    {
        T value{};
        if (offset <= bytes.size() && sizeof(T) <= bytes.size() - offset)
            std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    class FailingProductFileSystem : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        mutable uint32_t writes           = 0;
        mutable uint32_t replacements     = 0;
        uint32_t         failWrite        = 0;
        uint32_t         failReplace      = 0;
        bool             corruptWrite     = false;
        bool             failFlush        = false;
        bool             failIndex        = false;
        bool             unsupportedWrite = false;

        FileOperationStatus WriteFileExclusive(const FilePath &path, std::span<const uint8_t> bytes) const override
        {
            if (unsupportedWrite)
                return {FileOperationError::Unsupported, "injected unsupported write"};
            if (++writes == failWrite)
                return {FileOperationError::IOError, "injected write failure"};
            const auto status = NativeFileSystem::WriteFileExclusive(path, corruptWrite && !bytes.empty() ? bytes.first(bytes.size() - 1) : bytes);
            return status && failFlush ? FileOperationStatus{FileOperationError::IOError, "injected flush failure"} : status;
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (++replacements == failReplace || (failIndex && to.FileName() == "product.index"))
                return {FileOperationError::IOError, "injected replacement failure"};
            return NativeFileSystem::ReplaceFileAtomic(from, to);
        }
    };

    class ProductTestAsset : public AssetBase {
    public:
        uint32_t       value = 0;
        const uint8_t *GetData() const override
        {
            return reinterpret_cast<const uint8_t *>(&value);
        }
    };

    class ProductTestHandler : public AssetHandlerBase {
    public:
        bool                       failSave      = false;
        uint32_t                   rejectedValue = std::numeric_limits<uint32_t>::max();
        uint32_t                   failLoadValue = std::numeric_limits<uint32_t>::max();
        std::shared_ptr<AssetBase> CreateAsset() override
        {
            return std::make_shared<ProductTestAsset>();
        }
        bool Load(IStreamArchive &archive, const AssetPtr &asset) override
        {
            auto &value = std::static_pointer_cast<ProductTestAsset>(asset)->value;
            return archive.Load(value) && value != failLoadValue;
        }
        bool SaveChecked(OStreamArchive &archive, const AssetPtr &asset) override
        {
            const auto value = std::static_pointer_cast<ProductTestAsset>(asset)->value;
            return !failSave && value != rejectedValue && archive.Save(value);
        }
    };

    class DeferredProductCook : public ICookRunner, public ISourceCatalog {
    public:
        Uuid           id;
        Uuid           requestId;
        CookCompletion complete;
        uint32_t       requests = 0;
        void           SetCompletion(CookCompletion value) override
        {
            complete = std::move(value);
        }
        bool Request(const CookJob &job) override
        {
            requestId = job.requestId;
            ++requests;
            return true;
        }
        void Drain() override
        {
        }
        bool ResolvePath(const std::string &, Uuid &out) const override
        {
            out = id;
            return true;
        }
        bool Exists(const Uuid &value) const override
        {
            return id == value;
        }
        bool GetType(const Uuid &, std::string &out) const override
        {
            out = "ProductTest";
            return true;
        }
        bool GetTarget(const Uuid &, std::string &out) const override
        {
            out = "test";
            return true;
        }
        bool GetSourcePath(const Uuid &, std::string &out) const override
        {
            out = "deferred.test";
            return true;
        }
    };

    class AssetProductTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-product-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            fs    = new FailingProductFileSystem(FilePath(root));
            owner = Uuid::Create();
            mesh  = Uuid::Create();
            skin  = Uuid::Create();
        }
        void TearDown() override
        {
            AssetExecutor::Get()->WaitForAll();
            fs.Reset(nullptr);
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
        std::shared_ptr<ProductTestAsset> Build(AssetManager &manager, const Uuid &id, uint32_t value, bool bulk = false)
        {
            auto asset = std::static_pointer_cast<ProductTestAsset>(manager.CreateBuildAsset(id, Name("ProductTest")));
            EXPECT_TRUE(asset);
            if (asset) {
                asset->value = value;
                if (bulk)
                    asset->AddBulk(&value, sizeof(value));
            }
            return asset;
        }
        std::vector<uint8_t> ReadProduct(const Uuid &id)
        {
            std::vector<uint8_t> bytes;
            const auto           file = fs->OpenFile(ProductPath(id));
            EXPECT_TRUE(file);
            if (file)
                EXPECT_TRUE(file->ReadBin(bytes));
            return bytes;
        }
        void ReplaceProduct(const Uuid &id, std::span<const uint8_t> bytes)
        {
            const FilePath staged("test-" + id.ToString() + ".tmp");
            ASSERT_TRUE(fs->WriteFileExclusive(staged, bytes));
            ASSERT_TRUE(fs->ReplaceFileAtomic(staged, ProductPath(id)));
        }
        std::filesystem::path                root;
        CounterPtr<FailingProductFileSystem> fs;
        Uuid                                 owner, mesh, skin;
    };
} // namespace

TEST_F(AssetProductTest, OfflineCodecPublishesFlatProductsWithoutRuntimeHandlerAndLoadsAsynchronously)
{
    AssetManager manager;
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    EXPECT_EQ(manager.CreateBuildAsset(mesh, Name("ProductTest")), nullptr);
    const uint32_t   value = 73;
    const auto      *bytes = reinterpret_cast<const uint8_t *>(&value);
    AssetProductData product{mesh, "ProductTest", {}, {bytes, bytes + sizeof(value)}};
    ASSERT_TRUE(manager.SaveProductData(std::span(&product, 1), "test"));
    const auto published = ReadProduct(mesh);
    ASSERT_GE(published.size(), ASSET_PRODUCT_HEADER_BYTES);
    EXPECT_EQ(HeaderValue<uint32_t>(published, 0), ASSET_PRODUCT_MAGIC);
    EXPECT_EQ(HeaderValue<uint32_t>(published, 4), ASSET_PRODUCT_VERSION);
    EXPECT_EQ(HeaderValue<Uuid>(published, 8), mesh);
    EXPECT_EQ(HeaderValue<uint64_t>(published, 24), published.size() - ASSET_PRODUCT_HEADER_BYTES);
    EXPECT_EQ(HeaderValue<uint64_t>(published, 32), ProductChecksum(std::span(published).subspan(ASSET_PRODUCT_HEADER_BYTES)));
    AssetProductInspection inspected;
    ASSERT_TRUE(manager.InspectProduct(mesh, "test", inspected));
    EXPECT_EQ(inspected.type, "ProductTest");
    EXPECT_EQ(inspected.bulkSize, 0u);
    uint32_t read = 0;
    ASSERT_TRUE(inspected.payload->Load(read));
    EXPECT_EQ(read, value);

    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    const auto request = manager.LoadAssetAsync(mesh, AssetLoadPolicy::ProductsOnly);
    AssetExecutor::Get()->WaitForAll();
    ASSERT_TRUE(request->IsResolved());
    const auto loaded = request->GetAsset();
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(loaded)->value, value);

    const uint32_t changed = 74;
    std::memcpy(product.payload.data(), &changed, sizeof(changed));
    ASSERT_TRUE(manager.SaveProductData(std::span(&product, 1), "test"));
    const auto currentRequest = manager.LoadAssetAsync(mesh, AssetLoadPolicy::ProductsOnly);
    AssetExecutor::Get()->WaitForAll();
    ASSERT_TRUE(currentRequest->IsResolved());
    const auto current = currentRequest->GetAsset();
    ASSERT_NE(current, nullptr);
    current->BlockUntilLoaded();
    ASSERT_TRUE(current->IsLoaded());
    EXPECT_NE(current->GetInstanceId(), loaded->GetInstanceId());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(current)->value, changed);
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(loaded)->value, value);

    const auto beforeRejected = ReadProduct(mesh);
    product.dependencies      = {mesh};
    EXPECT_EQ(manager.SaveProductData(std::span(&product, 1), "test").error, ProductError::InvalidData);
    EXPECT_EQ(ReadProduct(mesh), beforeRejected);
    const auto missingRequest = manager.LoadAssetAsync(Uuid::Create(), AssetLoadPolicy::ProductsOnly);
    AssetExecutor::Get()->WaitForAll();
    EXPECT_TRUE(missingRequest->IsResolved());
    EXPECT_EQ(missingRequest->GetAsset(), nullptr);
}

TEST_F(AssetProductTest, OfflineCodecValidatesAllMembersBeforeReplacingPublishedProducts)
{
    AssetManager manager;
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    const uint32_t                  value = 19;
    const auto                     *bytes = reinterpret_cast<const uint8_t *>(&value);
    std::array<AssetProductData, 2> products{
        {{mesh, "ProductTest", {}, {bytes, bytes + sizeof(value)}}, {skin, "ProductTest", {}, {bytes, bytes + sizeof(value)}}}};
    ASSERT_TRUE(manager.SaveProductData(std::span(products).first(1), "test"));
    const auto published = ReadProduct(mesh);
    products[0].payload.front() ^= 1;
    for (uint32_t invalid = 0; invalid < 4; ++invalid) {
        SCOPED_TRACE(invalid);
        auto candidate = products;
        if (invalid == 0)
            candidate[1].payload.clear();
        if (invalid == 1)
            candidate[1].uuid = mesh;
        if (invalid == 2)
            candidate[1].dependencies = {owner, owner};
        if (invalid == 3)
            candidate[1].type = std::string("ProductTest\0invalid", 19);
        EXPECT_EQ(manager.SaveProductData(candidate, "test").error, ProductError::InvalidData);
        EXPECT_EQ(ReadProduct(mesh), published);
        EXPECT_FALSE(manager.HasProduct(skin, "test"));
    }
}

TEST_F(AssetProductTest, FlatProductHasVersionIdentityLengthAndChecksum)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    ASSERT_TRUE(manager.SaveAsset(Build(manager, mesh, 42, true), "test"));
    const auto bytes = ReadProduct(mesh);
    ASSERT_GT(bytes.size(), 40u);
    EXPECT_EQ(HeaderValue<uint32_t>(bytes, 0), 0x44525053u);
    EXPECT_EQ(HeaderValue<uint32_t>(bytes, 4), 1u);
    EXPECT_EQ(HeaderValue<Uuid>(bytes, 8), mesh);
    EXPECT_EQ(HeaderValue<uint64_t>(bytes, 24), bytes.size() - 40);
    const auto checksum = ProductChecksum(std::span(bytes).subspan(40));
    EXPECT_EQ(HeaderValue<uint64_t>(bytes, 32), checksum);
    AssetProductInspection inspection;
    ASSERT_TRUE(manager.InspectProduct(mesh, "test", inspection));
    EXPECT_EQ(inspection.type, "ProductTest");
    EXPECT_EQ(inspection.formatVersion, 1u);
    EXPECT_EQ(inspection.checksum, checksum);
    EXPECT_EQ(inspection.bulkSize, sizeof(uint32_t));
    uint32_t value = 0;
    ASSERT_TRUE(inspection.payload->Load(value));
    EXPECT_EQ(value, 42u);
}

TEST_F(AssetProductTest, InvalidEnvelopeIsRejectedBeforeDeserialization)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    for (uint32_t mutation = 0; mutation < 10; ++mutation) {
        SCOPED_TRACE(mutation);
        const auto id = Uuid::Create();
        ASSERT_TRUE(manager.SaveAsset(Build(manager, id, 7), "test"));
        auto bytes = ReadProduct(id);
        ASSERT_GT(bytes.size(), 40u);
        if (mutation == 0)
            bytes[0] ^= 1;
        if (mutation == 1)
            bytes[4] = 2;
        if (mutation == 2)
            bytes[8] ^= 1;
        if (mutation == 3)
            std::fill(bytes.begin() + 24, bytes.begin() + 32, 0);
        if (mutation == 4)
            std::fill(bytes.begin() + 24, bytes.begin() + 32, 0xff);
        if (mutation == 5)
            bytes[32] ^= 1;
        if (mutation == 6)
            bytes.back() ^= 1;
        if (mutation == 7)
            bytes.resize(39);
        if (mutation == 8)
            bytes.pop_back();
        if (mutation == 9)
            bytes.push_back(0);
        ReplaceProduct(id, bytes);
        AssetProductInspection inspection;
        EXPECT_FALSE(manager.InspectProduct(id, "test", inspection));
        EXPECT_FALSE(inspection.payload);
        EXPECT_EQ(manager.LoadAsset(id, AssetLoadPolicy::ProductsOnly), nullptr);
    }
}

TEST_F(AssetProductTest, CheckedSaveFailuresNeverReplaceAnExistingProduct)
{
    AssetManager manager;
    auto        *handler = new ProductTestHandler;
    manager.RegisterAssetHandler("ProductTest", handler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    for (uint32_t failure = 0; failure < 5; ++failure) {
        SCOPED_TRACE(failure);
        const auto id = Uuid::Create();
        ASSERT_TRUE(manager.SaveAsset(Build(manager, id, 7), "test"));
        const auto original = ReadProduct(id);
        const auto serial   = manager.GetProductChangeSerial();
        if (failure == 0)
            handler->failSave = true;
        if (failure == 1)
            fs->failWrite = fs->writes + 1;
        if (failure == 2)
            fs->failFlush = true;
        if (failure == 3)
            fs->corruptWrite = true;
        if (failure == 4)
            fs->failReplace = fs->replacements + 1;
        EXPECT_FALSE(manager.SaveAsset(Build(manager, id, 8), "test"));
        handler->failSave = false;
        fs->failWrite     = 0;
        fs->failFlush = fs->corruptWrite = false;
        fs->failReplace                  = 0;
        EXPECT_EQ(ReadProduct(id), original);
        EXPECT_EQ(manager.GetProductChangeSerial(), serial);
    }
}

TEST_F(AssetProductTest, BatchEncodingFailureDoesNotTouchAnyDestination)
{
    AssetManager manager;
    auto        *handler = new ProductTestHandler;
    manager.RegisterAssetHandler("ProductTest", handler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    ASSERT_TRUE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, mesh, 11), Build(manager, skin, 22)}, "test"));
    const auto first       = ReadProduct(mesh);
    const auto second      = ReadProduct(skin);
    const auto writes      = fs->writes;
    handler->rejectedValue = 44;
    EXPECT_FALSE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, mesh, 33), Build(manager, skin, 44)}, "test"));
    EXPECT_EQ(fs->writes, writes);
    EXPECT_EQ(ReadProduct(mesh), first);
    EXPECT_EQ(ReadProduct(skin), second);
}

TEST_F(AssetProductTest, FreshBatchStagingFailureLeavesNoLoadableMembers)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    fs->failWrite = 2;
    EXPECT_FALSE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, mesh, 11), Build(manager, skin, 22)}, "test"));
    for (const auto &id : {mesh, skin}) {
        EXPECT_FALSE(fs->FileExist(ProductPath(id)));
        EXPECT_EQ(manager.LoadAsset(id, AssetLoadPolicy::ProductsOnly), nullptr);
    }
}

TEST_F(AssetProductTest, PartialBatchReplacementFailureMakesAllMembersUnavailable)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    ASSERT_TRUE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, mesh, 11), Build(manager, skin, 22)}, "test"));
    fs->failReplace = fs->replacements + 2;
    EXPECT_FALSE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, mesh, 33), Build(manager, skin, 44)}, "test"));
    for (const auto &id : {mesh, skin}) {
        EXPECT_FALSE(fs->FileExist(ProductPath(id)));
        EXPECT_EQ(manager.LoadAsset(id, AssetLoadPolicy::ProductsOnly), nullptr);
        AssetProductInspection inspection;
        EXPECT_FALSE(manager.InspectProduct(id, "test", inspection));
    }
}

TEST_F(AssetProductTest, GeneratedBatchKeepsSourcePathBoundToItsPrimaryProduct)
{
    class GeneratedPathCatalog final : public DeferredProductCook {
    public:
        Uuid child;
        bool Exists(const Uuid &value) const override
        {
            return value == id || value == child;
        }
        bool GetOwner(const Uuid &value, Uuid &out) const override
        {
            if (!Exists(value))
                return false;
            out = id;
            return true;
        }
        bool GetSourcePath(const Uuid &value, std::string &out) const override
        {
            if (!Exists(value))
                return false;
            out = "models/character.model";
            return true;
        }
    } catalog;
    catalog.id    = owner;
    catalog.child = skin;
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetSourceCatalog(&catalog);
    ASSERT_TRUE(manager.SaveProducts(std::array<AssetPtr, 2>{Build(manager, owner, 11), Build(manager, skin, 22)}, "test"));
    const auto  index = AssetIndexFile::Load(fs, FilePath("product.index"), "path");
    const auto *entry = index.Find("models/character.model");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, owner);
    ASSERT_EQ(index.Entries().size(), 1u);
    EXPECT_EQ(manager.ResolveProductPath("models/character.model"), owner);
    const auto loaded = manager.LoadAssetFromPath("models/character.model", AssetLoadPolicy::ProductsOnly);
    ASSERT_TRUE(loaded);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());
    EXPECT_EQ(loaded->GetUuid(), owner);
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(loaded)->value, 11u);
}

TEST_F(AssetProductTest, IndexFailureInvalidatesTheReplacedProduct)
{
    DeferredProductCook catalog;
    catalog.id = mesh;
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetSourceCatalog(&catalog);
    fs->failIndex = true;
    EXPECT_FALSE(manager.SaveAsset(Build(manager, mesh, 12), "test"));
    EXPECT_FALSE(fs->FileExist(ProductPath(mesh)));
    EXPECT_EQ(manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly), nullptr);
}

TEST_F(AssetProductTest, UnsupportedAtomicStorageReturnsFailure)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    fs->unsupportedWrite = true;
    EXPECT_EQ(manager.SaveAsset(Build(manager, mesh, 12), "test").error, ProductError::Unsupported);
    EXPECT_FALSE(fs->FileExist(ProductPath(mesh)));
}

TEST_F(AssetProductTest, ReloadCreatesANewInstanceAndRetainedBulkStaysBoundToItsBytes)
{
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    ASSERT_TRUE(manager.SaveAsset(Build(manager, mesh, 11, true), "test"));
    const auto old = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_TRUE(old);
    old->BlockUntilLoaded();
    ASSERT_TRUE(old->IsLoaded());
    const auto instance = old->GetInstanceId();
    const auto revision = old->GetRevision();
    ASSERT_TRUE(instance);
    EXPECT_NE(revision, 0u);
    ASSERT_TRUE(manager.SaveAsset(Build(manager, mesh, 22, true), "test"));
    const auto current = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_TRUE(current);
    current->BlockUntilLoaded();
    ASSERT_TRUE(current->IsLoaded());
    EXPECT_NE(current.get(), old.get());
    EXPECT_NE(current->GetInstanceId(), instance);
    EXPECT_EQ(old->GetInstanceId(), instance);
    EXPECT_EQ(old->GetRevision(), revision);
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(old)->value, 11u);
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(current)->value, 22u);
    const auto oldBulk = old->ReadBulk(0, sizeof(uint32_t));
    const auto newBulk = current->ReadBulk(0, sizeof(uint32_t));
    ASSERT_EQ(oldBulk.size(), sizeof(uint32_t));
    ASSERT_EQ(newBulk.size(), sizeof(uint32_t));
    EXPECT_EQ(oldBulk.front(), 11u);
    EXPECT_EQ(newBulk.front(), 22u);
}

TEST_F(AssetProductTest, ProductWaitsForOnDemandDependencyWithoutBlockingLoader)
{
    DeferredProductCook runner;
    runner.id = skin;
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetCookRunner(&runner);
    manager.SetSourceCatalog(&runner);
    auto asset = manager.CreateBuildAsset(mesh, Name("ProductTest"));
    asset->AddDependencies(skin);
    ASSERT_TRUE(manager.SaveAsset(asset, "test"));
    const auto loaded = manager.LoadAsset(mesh);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->GetStatus(), AssetBase::Status::LOADING);
    EXPECT_EQ(loaded->GetWaitHandle().wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    EXPECT_EQ(runner.requests, 1U);
    const auto child = manager.CreateBuildAsset(skin, Name("ProductTest"));
    ASSERT_TRUE(manager.SaveAsset(child, "test"));
    runner.complete({skin, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(loaded->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(loaded->IsLoaded());
    manager.SetCookRunner(nullptr);
    runner.complete = {};
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductTest, AsyncDependencyFailureCompletesParentAndNotifiesExactlyOnce)
{
    class LoadedProbe final : public IAssetEvent {
    public:
        uint32_t calls = 0;
        void     OnAssetLoaded() override
        {
            ++calls;
        }
    };
    for (uint32_t onDemand = 0; onDemand < 2; ++onDemand) {
        SCOPED_TRACE(onDemand);
        const auto          parentId     = Uuid::Create();
        const auto          dependencyId = Uuid::Create();
        DeferredProductCook runner;
        runner.id = parentId;
        AssetManager manager;
        auto        *handler = new ProductTestHandler;
        manager.RegisterAssetHandler("ProductTest", handler);
        manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
        ASSERT_TRUE(manager.SaveAsset(Build(manager, dependencyId, 22), "test"));
        const auto parentProduct = Build(manager, parentId, 11);
        parentProduct->AddDependencies(dependencyId);
        if (onDemand) {
            manager.SetCookRunner(&runner);
            manager.SetSourceCatalog(&runner);
        } else {
            ASSERT_TRUE(manager.SaveAsset(parentProduct, "test"));
        }
        handler->failLoadValue = 22;
        LoadedProbe              probe;
        EventBinder<IAssetEvent> subscription;
        subscription.Bind(&probe, parentId);
        const auto parent = manager.LoadAsset(parentId, onDemand ? AssetLoadPolicy::AllowCook : AssetLoadPolicy::ProductsOnly);
        ASSERT_TRUE(parent);
        if (onDemand) {
            ASSERT_EQ(runner.requests, 1u);
            ASSERT_TRUE(manager.SaveAsset(parentProduct, "test"));
            runner.complete({parentId, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
        }
        const auto completion = parent->GetWaitHandle();
        ASSERT_TRUE(completion.valid());
        ASSERT_EQ(completion.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        AssetExecutor::Get()->WaitForAll();
        EXPECT_EQ(parent->GetStatus(), AssetBase::Status::FAILED);
        const auto dependency = manager.FindAsset(dependencyId);
        ASSERT_TRUE(dependency);
        EXPECT_EQ(dependency->GetStatus(), AssetBase::Status::FAILED);
        EXPECT_EQ(probe.calls, 0u);
        ThreadDispatcher::Get()->Dispatch(ThreadRole::Main);
        EXPECT_EQ(probe.calls, 1u);
        ThreadDispatcher::Get()->Dispatch(ThreadRole::Main);
        EXPECT_EQ(probe.calls, 1u);
        manager.SetCookRunner(nullptr);
        runner.complete = {};
    }
}

TEST_F(AssetProductTest, ProductOnlyLoadsNeverConsultSourcesOrCookMissingDependencies)
{
    struct SourceProbe final : DeferredProductCook {
        mutable uint32_t queries = 0;
        bool             Exists(const Uuid &value) const override
        {
            ++queries;
            return DeferredProductCook::Exists(value);
        }
        bool GetGenerated(const Uuid &, GeneratedAssetRecord &) const override
        {
            ++queries;
            return false;
        }
    } runner;
    runner.id = skin;
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetCookRunner(&runner);
    manager.SetSourceCatalog(&runner);
    EXPECT_EQ(manager.LoadAsset(skin, AssetLoadPolicy::ProductsOnly), nullptr);
    auto parent = manager.CreateBuildAsset(mesh, Name("ProductTest"));
    parent->AddDependencies(skin);
    ASSERT_TRUE(manager.SaveAsset(parent, "test"));
    runner.queries         = 0;
    const auto unavailable = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(unavailable, nullptr);
    EXPECT_EQ(unavailable->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_EQ(runner.requests, 0u);
    EXPECT_EQ(runner.queries, 0u);

    manager.SetSourceCatalog(nullptr);
    const auto child = manager.CreateBuildAsset(skin, Name("ProductTest"));
    ASSERT_TRUE(manager.SaveAsset(child, "test"));
    const auto recovered = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(recovered, nullptr);
    recovered->BlockUntilLoaded();
    EXPECT_TRUE(recovered->IsLoaded());
    EXPECT_NE(recovered->GetInstanceId(), unavailable->GetInstanceId());
    EXPECT_EQ(unavailable->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_EQ(runner.requests, 0u);
    manager.SetCookRunner(nullptr);
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductTest, GeneratedChildrenShareOwnerCookAndAllReachTerminalState)
{
    class ChildCook : public DeferredProductCook {
    public:
        Uuid              owner;
        std::vector<Uuid> children;
        CookJob           requested;
        bool              Request(const CookJob &job) override
        {
            requested = job;
            return DeferredProductCook::Request(job);
        }
        bool Exists(const Uuid &value) const override
        {
            return value == owner || std::find(children.begin(), children.end(), value) != children.end();
        }
        bool GetOwner(const Uuid &value, Uuid &out) const override
        {
            if (!Exists(value)) {
                return false;
            }
            out = owner;
            return true;
        }
    } runner;
    const Uuid missing = Uuid::Create();
    runner.owner       = owner;
    runner.children    = {mesh, skin, missing};
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetCookRunner(&runner);
    manager.SetSourceCatalog(&runner);
    const auto first  = manager.LoadAsset(mesh);
    const auto second = manager.LoadAsset(skin);
    const auto absent = manager.LoadAsset(missing);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(absent, nullptr);
    EXPECT_EQ(runner.requests, 1U);
    EXPECT_EQ(runner.requested.uuid, owner);
    auto firstBuild = manager.CreateBuildAsset(mesh, Name("ProductTest"));
    firstBuild->AddDependencies(skin);
    const auto secondBuild = manager.CreateBuildAsset(skin, Name("ProductTest"));
    ASSERT_TRUE(manager.SaveProducts(std::array<AssetPtr, 2>{firstBuild, secondBuild}, "test"));
    runner.complete({owner, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(first->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(second->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(absent->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(first->IsLoaded());
    EXPECT_TRUE(second->IsLoaded());
    EXPECT_EQ(absent->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_NE(first->GetInstanceId(), second->GetInstanceId());
    runner.complete({owner, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    manager.SetCookRunner(nullptr);
    runner.complete = {};
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductTest, LateCompletionCannotFinishRetriedLoad)
{
    DeferredProductCook runner;
    runner.id = mesh;
    AssetManager manager;
    manager.RegisterAssetHandler("ProductTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetSourceCatalog(&runner);
    manager.SetCookRunner(&runner);
    const auto first = manager.LoadAsset(mesh);
    ASSERT_NE(first, nullptr);
    const auto obsolete = runner.requestId;
    runner.complete({mesh, "test", AssetBuildRetCode::FAILED, "first failed", obsolete});
    ASSERT_EQ(first->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    const auto second = manager.LoadAsset(mesh);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(runner.requestId, obsolete);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(first->GetStatus(), AssetBase::Status::FAILED);
    runner.complete({mesh, "test", AssetBuildRetCode::FAILED, "late duplicate", obsolete});
    EXPECT_EQ(second->GetWaitHandle().wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    const auto product = manager.CreateBuildAsset(mesh, Name("ProductTest"));
    ASSERT_TRUE(manager.SaveAsset(product, "test"));
    runner.complete({mesh, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(second->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(second->IsLoaded());
    manager.SetCookRunner(nullptr);
    AssetExecutor::Get()->WaitForAll();
}
