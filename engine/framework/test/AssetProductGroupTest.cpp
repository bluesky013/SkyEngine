#include <framework/asset/AssetManager.h>
#include <framework/asset/ICookRunner.h>
#include <framework/asset/ISourceCatalog.h>
#include <framework/platform/PlatformBase.h>
#include <framework/platform/Process.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "ProductProcessHelper.h"

using namespace sky;

namespace {
    class FailingProductFileSystem : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        mutable uint32_t writes       = 0;
        uint32_t         failWrite    = 0;
        bool             failCommit   = false;
        bool             corruptWrite = false;
        bool             failFlush    = false;

        FileOperationStatus WriteFileExclusive(const FilePath &path, std::span<const uint8_t> bytes) const override
        {
            if (++writes == failWrite) {
                return {FileOperationError::IOError, "injected write failure"};
            }
            const auto status = NativeFileSystem::WriteFileExclusive(path, corruptWrite && !bytes.empty() ? bytes.first(bytes.size() - 1) : bytes);
            return status && failFlush ? FileOperationStatus{FileOperationError::IOError, "injected flush failure"} : status;
        }
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (failCommit && to.FileName() == "current.bin") {
                return {FileOperationError::IOError, "injected commit failure"};
            }
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
        bool                       failSave = false;
        std::shared_ptr<AssetBase> CreateAsset() override
        {
            return std::make_shared<ProductTestAsset>();
        }
        bool Load(IStreamArchive &archive, const AssetPtr &asset) override
        {
            return archive.Load(std::static_pointer_cast<ProductTestAsset>(asset)->value);
        }
        void Save(OStreamArchive &archive, const AssetPtr &asset) override
        {
            SaveChecked(archive, asset);
        }
        bool SaveChecked(OStreamArchive &archive, const AssetPtr &asset) override
        {
            return !failSave && archive.Save(std::static_pointer_cast<ProductTestAsset>(asset)->value);
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
            out = "GroupTest";
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

    class AssetProductGroupTest : public testing::Test {
    protected:
        void SetUp() override
        {
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-product-group-" + Uuid::Create().ToString());
            ASSERT_FALSE(error);
            fs    = new FailingProductFileSystem(FilePath(root));
            owner = Uuid::Create();
            mesh  = Uuid::Create();
            skin  = Uuid::Create();
        }
        void TearDown() override
        {
            fs.Reset(nullptr);
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
        ProductGroupSnapshotPtr Publish(uint8_t value)
        {
            AssetProductGroupStore  store(fs);
            ProductGroupTransaction transaction;
            ProductGroupSnapshotPtr snapshot;
            EXPECT_TRUE(store.Begin(owner, std::array<Uuid, 2>{mesh, skin}, transaction));
            EXPECT_TRUE(store.Stage(transaction, mesh, std::array<uint8_t, 2>{value, 1}));
            EXPECT_TRUE(store.Stage(transaction, skin, std::array<uint8_t, 2>{value, 2}));
            EXPECT_TRUE(store.Commit(transaction, snapshot));
            return snapshot;
        }
        std::vector<uint8_t> Read(const ProductGroupSnapshot &snapshot, const Uuid &member)
        {
            FilePtr file;
            EXPECT_TRUE(AssetProductGroupStore(fs).Open(snapshot, member, file));
            std::vector<uint8_t> bytes;
            if (file) {
                EXPECT_TRUE(file->ReadBin(bytes));
            }
            return bytes;
        }
        std::filesystem::path                root;
        CounterPtr<FailingProductFileSystem> fs;
        Uuid                                 owner, mesh, skin;
    };
} // namespace

TEST_F(AssetProductGroupTest, PinnedGroupSurvivesRecookAndRestartWithoutMixedMembers)
{
    const auto first = Publish(1);
    ASSERT_NE(first, nullptr);
    const auto second = Publish(2);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->revision, first->revision + 1);
    EXPECT_NE(first->generation, second->generation);
    EXPECT_EQ(Read(*first, mesh).front(), 1);
    EXPECT_EQ(Read(*first, skin).front(), 1);
    EXPECT_EQ(Read(*second, mesh).front(), 2);
    EXPECT_EQ(Read(*second, skin).front(), 2);
    const AssetProductGroupStore restarted(new NativeFileSystem(FilePath(root)));
    ProductGroupSnapshotPtr      latest;
    ASSERT_TRUE(restarted.Resolve(skin, latest));
    EXPECT_EQ(latest->generation, second->generation);
}

TEST_F(AssetProductGroupTest, OfflineCodecPublishesWithoutLoadingRuntimeHandler)
{
    AssetManager manager;
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    const uint32_t   value = 73;
    AssetProductData product;
    product.uuid      = mesh;
    product.type      = "GroupTest";
    const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
    product.payload.assign(bytes, bytes + sizeof(value));
    ProductGroupTransaction transaction;
    ProductGroupSnapshotPtr snapshot;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array{mesh}, transaction));
    ASSERT_TRUE(manager.SaveProductData(transaction, "test", std::span(&product, 1), snapshot));
    AssetProductInspection inspection;
    ASSERT_TRUE(manager.InspectProduct(mesh, "test", inspection));
    EXPECT_EQ(inspection.type, "GroupTest");
    uint32_t inspected = 0;
    ASSERT_TRUE(inspection.payload->Load(inspected));
    EXPECT_EQ(inspected, value);
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler());
    const auto request = manager.LoadAssetAsync(mesh, AssetLoadPolicy::ProductsOnly);
    AssetExecutor::Get()->WaitForAll();
    ASSERT_TRUE(request->IsResolved());
    const auto loaded = request->GetAsset();
    ASSERT_NE(loaded, nullptr);
    loaded->BlockUntilLoaded();
    ASSERT_TRUE(loaded->IsLoaded());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(loaded)->value, value);

    ProductGroupTransaction rejected;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array{mesh}, rejected));
    product.dependencies = {mesh};
    EXPECT_FALSE(manager.SaveProductData(rejected, "test", std::span(&product, 1), snapshot));
    ProductGroupSnapshotPtr stillPublished;
    ASSERT_TRUE(AssetProductGroupStore(fs).Resolve(mesh, stillPublished));
    EXPECT_EQ(stillPublished->revision, 1u);
}

TEST_F(AssetProductGroupTest, WriteReadbackFlushAndCommitFailuresKeepPreviousGroup)
{
    const auto first = Publish(1);
    ASSERT_NE(first, nullptr);
    AssetProductGroupStore store(fs);
    for (uint32_t failure = 0; failure != 4; ++failure) {
        ProductGroupTransaction transaction;
        ASSERT_TRUE(store.Begin(owner, std::array<Uuid, 2>{mesh, skin}, transaction));
        if (failure == 0) {
            fs->failWrite = fs->writes + 2;
        }
        if (failure == 1) {
            fs->corruptWrite = true;
        }
        if (failure == 2) {
            fs->failFlush = true;
        }
        if (failure == 3) {
            fs->failCommit = true;
        }
        const auto              firstStage  = store.Stage(transaction, mesh, std::array<uint8_t, 2>{9, 1});
        const auto              secondStage = store.Stage(transaction, skin, std::array<uint8_t, 2>{9, 2});
        ProductGroupSnapshotPtr attempted;
        EXPECT_FALSE(store.Commit(transaction, attempted));
        EXPECT_FALSE(attempted);
        if (failure < 3) {
            EXPECT_FALSE(firstStage && secondStage);
        }
        fs->failWrite    = 0;
        fs->corruptWrite = fs->failFlush = fs->failCommit = false;
        ProductGroupSnapshotPtr latest;
        ASSERT_TRUE(store.Resolve(mesh, latest));
        EXPECT_EQ(latest->generation, first->generation);
        EXPECT_EQ(Read(*latest, skin).front(), 1);
    }
}

TEST_F(AssetProductGroupTest, SupersededRequestAndAbandonedStagingCannotPublish)
{
    const auto first = Publish(1);
    ASSERT_NE(first, nullptr);
    AssetProductGroupStore  store(fs);
    ProductGroupTransaction stale;
    ASSERT_TRUE(store.Begin(owner, std::array<Uuid, 1>{mesh}, stale));
    ASSERT_TRUE(store.Stage(stale, mesh, std::array<uint8_t, 1>{3}));
    const auto latest = Publish(2);
    ASSERT_NE(latest, nullptr);
    ProductGroupSnapshotPtr output;
    EXPECT_EQ(store.Commit(stale, output).error, ProductError::StaleRequest);
    {
        ProductGroupTransaction abandoned;
        ASSERT_TRUE(store.Begin(owner, std::array<Uuid, 1>{mesh}, abandoned));
        ASSERT_TRUE(store.Stage(abandoned, mesh, std::array<uint8_t, 1>{4}));
    }
    AssetProductGroupStore restarted(fs);
    ASSERT_TRUE(restarted.Resolve(mesh, output));
    EXPECT_EQ(output->generation, latest->generation);
    EXPECT_EQ(Read(*output, skin).front(), 2);
}

TEST_F(AssetProductGroupTest, RemovedChildAndConflictingOwnerNeverResolveToAnotherBinding)
{
    const auto first = Publish(1);
    ASSERT_NE(first, nullptr);
    AssetProductGroupStore  store(fs);
    ProductGroupTransaction next;
    ASSERT_TRUE(store.Begin(owner, std::array<Uuid, 1>{mesh}, next));
    ASSERT_TRUE(store.Stage(next, mesh, std::array<uint8_t, 1>{2}));
    ProductGroupSnapshotPtr output;
    ASSERT_TRUE(store.Commit(next, output));
    EXPECT_EQ(store.Resolve(skin, output).error, ProductError::NotFound);
    EXPECT_EQ(Read(*first, skin).front(), 1);
    ProductGroupTransaction conflict;
    ASSERT_TRUE(store.Begin(Uuid::Create(), std::array<Uuid, 1>{skin}, conflict));
    ASSERT_TRUE(store.Stage(conflict, skin, std::array<uint8_t, 1>{9}));
    EXPECT_EQ(store.Commit(conflict, output).error, ProductError::OwnerConflict);
}

TEST_F(AssetProductGroupTest, AtomicReplacementAndExclusiveLockPreserveOldFileOnFailure)
{
    const std::array<uint8_t, 2> oldBytes{1, 2};
    ASSERT_TRUE(fs->WriteFileExclusive("head.bin", oldBytes));
    EXPECT_FALSE(fs->ReplaceFileAtomic("missing.bin", "head.bin"));
    std::vector<uint8_t> read;
    ASSERT_TRUE(fs->OpenFile("head.bin")->ReadBin(read));
    EXPECT_EQ(read, std::vector<uint8_t>(oldBytes.begin(), oldBytes.end()));
    EXPECT_EQ(fs->WriteFileExclusive("head.bin", oldBytes).error, FileOperationError::AlreadyExists);
    EXPECT_EQ(fs->WriteFileExclusive("../escape.bin", oldBytes).error, FileOperationError::InvalidPath);
    FileLockPtr first, second;
    ASSERT_TRUE(fs->AcquireFileLock("publish.lock", first));
    EXPECT_EQ(fs->AcquireFileLock("publish.lock", second).error, FileOperationError::Busy);
    first.reset();
    EXPECT_TRUE(fs->AcquireFileLock("publish.lock", second));
}

TEST_F(AssetProductGroupTest, NewAssetSnapshotsRetainOldBulkAndRejectFailedEncoding)
{
    AssetManager manager;
    auto        *handler = new ProductTestHandler;
    manager.RegisterAssetHandler("GroupTest", handler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    const auto publish = [&](uint32_t value, ProductGroupSnapshotPtr &snapshot) {
        ProductGroupTransaction transaction;
        auto                    asset = std::static_pointer_cast<ProductTestAsset>(manager.CreateBuildAsset(mesh, Name("GroupTest")));
        asset->value                  = value;
        asset->AddBulk(&value, sizeof(value));
        const auto begin = manager.BeginProductGroup(owner, "test", std::array<Uuid, 1>{mesh}, transaction);
        if (!begin) {
            return begin;
        }
        return manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 1>{asset}, snapshot);
    };
    ProductGroupSnapshotPtr first, second;
    ASSERT_TRUE(publish(11, first));
    const auto oldAsset = manager.LoadAssetSnapshot(mesh, "test", first);
    ASSERT_NE(oldAsset, nullptr);
    oldAsset->BlockUntilLoaded();
    ASSERT_TRUE(oldAsset->IsLoaded());
    ASSERT_TRUE(publish(22, second));
    const auto newAsset = manager.LoadAsset(mesh);
    ASSERT_NE(newAsset, nullptr);
    newAsset->BlockUntilLoaded();
    ASSERT_TRUE(newAsset->IsLoaded());
    EXPECT_NE(oldAsset.get(), newAsset.get());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(oldAsset)->value, 11U);
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(newAsset)->value, 22U);
    const auto oldBulk = oldAsset->ReadBulk(0, 4);
    const auto newBulk = newAsset->ReadBulk(0, 4);
    ASSERT_EQ(oldBulk.size(), 4U);
    ASSERT_EQ(newBulk.size(), 4U);
    EXPECT_EQ(oldBulk.front(), 11U);
    EXPECT_EQ(newBulk.front(), 22U);
    handler->failSave = true;
    ProductGroupSnapshotPtr rejected;
    EXPECT_FALSE(publish(33, rejected));
    EXPECT_EQ(manager.LoadAsset(mesh).get(), newAsset.get());
    EXPECT_EQ(oldAsset->GetProductGroup()->generation, first->generation);
}

TEST_F(AssetProductGroupTest, IncompleteDeclaredGroupCannotCommit)
{
    AssetProductGroupStore  store(fs);
    ProductGroupTransaction transaction;
    ASSERT_TRUE(store.Begin(owner, std::array<Uuid, 2>{mesh, skin}, transaction));
    ASSERT_TRUE(store.Stage(transaction, mesh, std::array<uint8_t, 1>{1}));
    ProductGroupSnapshotPtr result;
    EXPECT_EQ(store.Commit(transaction, result).error, ProductError::InvalidData);
    EXPECT_EQ(store.Resolve(mesh, result).error, ProductError::NotFound);
    ASSERT_TRUE(store.Stage(transaction, skin, std::array<uint8_t, 1>{2}));
    EXPECT_TRUE(store.Commit(transaction, result));
}

TEST_F(AssetProductGroupTest, FailedFirstGenerationKeepsLegacyProductUntilSuccessfulMigration)
{
    AssetManager manager;
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    {
        auto old   = std::static_pointer_cast<ProductTestAsset>(manager.CreateBuildAsset(mesh, Name("GroupTest")));
        old->value = 7;
        manager.SaveAsset(old, "test");
    }
    auto newer   = std::static_pointer_cast<ProductTestAsset>(manager.CreateBuildAsset(mesh, Name("GroupTest")));
    newer->value = 8;
    ProductGroupTransaction transaction;
    ProductGroupSnapshotPtr snapshot;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array<Uuid, 1>{mesh}, transaction));
    fs->failCommit = true;
    EXPECT_FALSE(manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 1>{newer}, snapshot));
    const auto old = manager.LoadAsset(mesh);
    ASSERT_NE(old, nullptr);
    old->BlockUntilLoaded();
    ASSERT_TRUE(old->IsLoaded());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(old)->value, 7U);
    EXPECT_FALSE(old->GetProductGroup());
    fs->failCommit = false;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array<Uuid, 1>{mesh}, transaction));
    ASSERT_TRUE(manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 1>{newer}, snapshot));
    const auto current = manager.LoadAsset(mesh);
    ASSERT_NE(current, nullptr);
    current->BlockUntilLoaded();
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(current)->value, 8U);
    EXPECT_TRUE(current->GetProductGroup());
    EXPECT_EQ(std::static_pointer_cast<ProductTestAsset>(old)->value, 7U);
}

TEST_F(AssetProductGroupTest, SnapshotWaitsForOnDemandDependencyWithoutBlockingLoader)
{
    DeferredProductCook runner;
    runner.id = skin;
    AssetManager manager;
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetCookRunner(&runner);
    manager.SetSourceCatalog(&runner);
    auto asset = manager.CreateBuildAsset(mesh, Name("GroupTest"));
    asset->AddDependencies(skin);
    ProductGroupTransaction transaction;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array<Uuid, 1>{mesh}, transaction));
    ProductGroupSnapshotPtr snapshot;
    ASSERT_TRUE(manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 1>{asset}, snapshot));
    const auto loaded = manager.LoadAssetSnapshot(mesh, "test", snapshot);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->GetStatus(), AssetBase::Status::LOADING);
    EXPECT_EQ(loaded->GetWaitHandle().wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    EXPECT_EQ(runner.requests, 1U);
    ProductGroupTransaction childTransaction;
    ASSERT_TRUE(manager.BeginProductGroup(skin, "test", std::array<Uuid, 1>{skin}, childTransaction));
    const auto              child = manager.CreateBuildAsset(skin, Name("GroupTest"));
    ProductGroupSnapshotPtr childSnapshot;
    ASSERT_TRUE(manager.SaveProductGroup(childTransaction, "test", std::array<AssetPtr, 1>{child}, childSnapshot));
    runner.complete({skin, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(loaded->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(loaded->IsLoaded());
    manager.SetCookRunner(nullptr);
    runner.complete = {};
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductGroupTest, ProductOnlyLoadsNeverConsultSourcesOrCookMissingDependencies)
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
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
    manager.AddAssetProductBundle(new HashedAssetBundle(fs, "test"));
    manager.SetCookRunner(&runner);
    manager.SetSourceCatalog(&runner);
    EXPECT_EQ(manager.LoadAsset(skin, AssetLoadPolicy::ProductsOnly), nullptr);
    auto parent = manager.CreateBuildAsset(mesh, Name("GroupTest"));
    parent->AddDependencies(skin);
    ProductGroupTransaction transaction;
    ProductGroupSnapshotPtr snapshot;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array{mesh}, transaction));
    ASSERT_TRUE(manager.SaveProductGroup(transaction, "test", std::array{parent}, snapshot));
    runner.queries         = 0;
    const auto unavailable = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(unavailable, nullptr);
    EXPECT_EQ(unavailable->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_EQ(runner.requests, 0u);
    EXPECT_EQ(runner.queries, 0u);

    manager.SetSourceCatalog(nullptr);
    ProductGroupTransaction childTransaction;
    ProductGroupSnapshotPtr childSnapshot;
    const auto              child = manager.CreateBuildAsset(skin, Name("GroupTest"));
    ASSERT_TRUE(manager.BeginProductGroup(skin, "test", std::array{skin}, childTransaction));
    ASSERT_TRUE(manager.SaveProductGroup(childTransaction, "test", std::array{child}, childSnapshot));
    const auto recovered = manager.LoadAsset(mesh, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(recovered, nullptr);
    recovered->BlockUntilLoaded();
    EXPECT_TRUE(recovered->IsLoaded());
    EXPECT_EQ(recovered->GetSnapshotGeneration(), snapshot->generation);
    EXPECT_EQ(unavailable->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_EQ(runner.requests, 0u);
    manager.SetCookRunner(nullptr);
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductGroupTest, GeneratedChildrenShareOwnerCookAndAllReachTerminalState)
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
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
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
    auto firstBuild = manager.CreateBuildAsset(mesh, Name("GroupTest"));
    firstBuild->AddDependencies(skin);
    const auto              secondBuild = manager.CreateBuildAsset(skin, Name("GroupTest"));
    ProductGroupTransaction transaction;
    ProductGroupSnapshotPtr snapshot;
    ASSERT_TRUE(manager.BeginProductGroup(owner, "test", std::array<Uuid, 2>{mesh, skin}, transaction));
    ASSERT_TRUE(manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 2>{firstBuild, secondBuild}, snapshot));
    runner.complete({owner, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(first->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(second->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(absent->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(first->IsLoaded());
    EXPECT_TRUE(second->IsLoaded());
    EXPECT_EQ(absent->GetStatus(), AssetBase::Status::FAILED);
    EXPECT_EQ(first->GetProductGroup()->generation, second->GetProductGroup()->generation);
    runner.complete({owner, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    manager.SetCookRunner(nullptr);
    runner.complete = {};
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductGroupTest, LateCompletionCannotFinishRetriedLoad)
{
    DeferredProductCook runner;
    runner.id = mesh;
    AssetManager manager;
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
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
    ProductGroupTransaction transaction;
    ProductGroupSnapshotPtr snapshot;
    const auto              product = manager.CreateBuildAsset(mesh, Name("GroupTest"));
    ASSERT_TRUE(manager.BeginProductGroup(mesh, "test", std::array<Uuid, 1>{mesh}, transaction));
    ASSERT_TRUE(manager.SaveProductGroup(transaction, "test", std::array<AssetPtr, 1>{product}, snapshot));
    runner.complete({mesh, "test", AssetBuildRetCode::SUCCESS, {}, runner.requestId});
    ASSERT_EQ(second->GetWaitHandle().wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(second->IsLoaded());
    manager.SetCookRunner(nullptr);
    AssetExecutor::Get()->WaitForAll();
}

TEST_F(AssetProductGroupTest, TruncatedAndOversizedLegacyEnvelopesFailBeforeAllocation)
{
    AssetManager manager;
    auto        *bundle = new HashedAssetBundle(fs, "test");
    manager.AddAssetProductBundle(bundle);
    manager.RegisterAssetHandler("GroupTest", new ProductTestHandler);
    for (const uint32_t size : {4U, 0xFFFFFFFFU}) {
        const Uuid id = Uuid::Create();
        {
            const auto file   = fs->CreateOrOpenFile(FilePath(id.ToString().substr(0, 2)) / FilePath(id.ToString() + ".bin"));
            const auto output = file->WriteAsArchive();
            output->Save(std::string("GroupTest"));
            output->Save(uint32_t{0});
            output->Save(std::string{});
            output->Save(size);
            output->Save(uint32_t{7});
        }
        EXPECT_EQ(manager.LoadAsset(id), nullptr);
    }
}

std::optional<int> RunProductProcessHelper(int argc, char **argv)
{
    if (argc != 5 || std::string(argv[1]) != "--product-group-crash-helper") {
        return std::nullopt;
    }
    NativeFileSystemPtr     filesystem = new NativeFileSystem(argv[2]);
    AssetProductGroupStore  store(filesystem);
    ProductGroupTransaction transaction;
    const Uuid              owner = Uuid::CreateFromString(argv[3]);
    const Uuid              mesh  = Uuid::CreateFromString(argv[4]);
    if (!store.Begin(owner, std::array<Uuid, 1>{mesh}, transaction) || !store.Stage(transaction, mesh, std::array<uint8_t, 1>{9})) {
        return 2;
    }
    FileLockPtr lock;
    if (!filesystem->AcquireFileLock(".groups/publish.lock", lock)) {
        return 3;
    }
    std::fputs("staged\n", stderr);
    std::fflush(stderr);
    std::getchar();
    return 0;
}

TEST_F(AssetProductGroupTest, KilledWriterLeavesOldGroupAndReleasesNativeLock)
{
#if SKY_PLATFORM_ANDROID || SKY_PLATFORM_IOS
    GTEST_SKIP() << "native subprocess validation requires a desktop host";
#else
    const auto first = Publish(1);
    ASSERT_NE(first, nullptr);
    if (!Platform::Get()->GetImpl()) {
        Platform::Get()->Init({});
    }
    ProcessDesc desc;
    const auto  executable = std::filesystem::path(Platform::Get()->GetBundlePath()) /
    #if SKY_PLATFORM_WINDOWS
                            "FrameworkTest.exe";
    #else
                            "FrameworkTest";
    #endif
    desc.args    = {executable.string(), "--product-group-crash-helper", root.string(), owner.ToString(), mesh.ToString()};
    auto process = CreateProcess();
    ASSERT_TRUE(process->Start(desc)) << executable.string() << ": " << static_cast<int>(process->GetStatus());
    std::string output;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (output.find("staged") == std::string::npos && std::chrono::steady_clock::now() < deadline) {
        uint8_t    bytes[1024];
        bool       eof  = false;
        const auto size = process->ReadStderr(bytes, sizeof(bytes), eof);
        output.append(reinterpret_cast<const char *>(bytes), size);
        if (eof) {
            break;
        }
    }
    EXPECT_NE(output.find("staged"), std::string::npos) << output;
    FileLockPtr blocked;
    EXPECT_EQ(fs->AcquireFileLock(".groups/publish.lock", blocked).error, FileOperationError::Busy);
    process->Kill();
    int exitCode = 0;
    ASSERT_TRUE(process->WaitFor(5000, exitCode));
    ASSERT_TRUE(fs->AcquireFileLock(".groups/publish.lock", blocked));
    ProductGroupSnapshotPtr recovered;
    ASSERT_TRUE(AssetProductGroupStore(fs).Resolve(mesh, recovered));
    EXPECT_EQ(recovered->generation, first->generation);
    EXPECT_EQ(Read(*recovered, skin).front(), 1);
#endif
}
