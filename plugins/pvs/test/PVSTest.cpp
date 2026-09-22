//
// Created by Zach Lee on 2026/2/24.
//

#include <gtest/gtest.h>

#include <pvs/PVSLoader.h>
#include <pvs/PVSVisibility.h>

#include <core/archive/StreamArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <memory>
#include <sstream>

using namespace sky;

TEST(PVSTest, VisibilityViewIDLayout)
{
    PVSVisibilityViewID id;
    id.indexInBytes = 1024U;
    id.maskInBytes = 0x08;

    ASSERT_EQ(sizeof(PVSVisibilityViewID), sizeof(PVSObjectID));
    ASSERT_EQ((id.value) & 0xFF, 0x08);
    ASSERT_EQ((id.value >> 8) & 0xFFFFFF, 1024);
}

// =========================================================================
// Visibility query (fail-safe)
// =========================================================================

TEST(PVSVisibilityQueryTest, VisibleAndCulledBits)
{
    const uint8_t data[2] = {0b00000101U, 0b00000010U}; // objects 0, 2 and 9 visible

    EXPECT_TRUE(QueryPVSObjectVisible(data, sizeof(data), 0));
    EXPECT_FALSE(QueryPVSObjectVisible(data, sizeof(data), 1));
    EXPECT_TRUE(QueryPVSObjectVisible(data, sizeof(data), 2));
    EXPECT_TRUE(QueryPVSObjectVisible(data, sizeof(data), 9));
    EXPECT_FALSE(QueryPVSObjectVisible(data, sizeof(data), 8));
}

TEST(PVSVisibilityQueryTest, NullDataIsVisible)
{
    EXPECT_TRUE(QueryPVSObjectVisible(nullptr, 0, 3));
}

TEST(PVSVisibilityQueryTest, OutOfRangeByteIsVisible)
{
    const uint8_t data[1] = {0x00};
    // object 100 needs byte 12, which is outside the known size -> fail safe
    EXPECT_TRUE(QueryPVSObjectVisible(data, sizeof(data), 100));
}

TEST(PVSVisibilityQueryTest, InvalidObjectIdIsVisible)
{
    std::vector<uint8_t> data((MAX_OBJECTS + 8) / 8, 0xFF);
    EXPECT_TRUE(QueryPVSObjectVisible(data.data(), static_cast<uint32_t>(data.size()), INVALID_PVS_OBJECT));
}

// =========================================================================
// Streaming
// =========================================================================

namespace {

    // In-memory sector provider so the loader/streaming can be tested without
    // any file system or legacy render dependency.
    class FakeSectorProvider : public IPVSSectorProvider {
    public:
        explicit FakeSectorProvider(const PVSConfig &inConfig, uint32_t inCellDataSize)
            : config(inConfig)
            , cellDataSize(inCellDataSize)
        {
        }

        bool LoadHeader(PVSConfig &out) override
        {
            out = config;
            return true;
        }

        bool LoadSector(const PVSSectorCoord & /*coord*/, PVSSector &out) override
        {
            out.version = 1;
            out.chunkSize = cellDataSize * config.cellsPerChunk;
            out.cells.resize(config.cellsInSectorXZ * config.cellsInSectorXZ);
            for (auto &cell : out.cells) {
                cell.chunkIndex = 0;
                cell.dataOffset = 0;
            }

            PVSChunk chunk;
            chunk.storage = std::make_unique<uint8_t[]>(out.chunkSize);
            out.chunks.emplace_back(std::move(chunk));
            return true;
        }

        PVSConfig config;
        uint32_t cellDataSize;
    };

    PVSConfig MakeStreamingConfig()
    {
        PVSConfig config;
        config.worldOffset     = Vector3(0.f, 0.f, 0.f);
        config.cellSize        = 100.f;
        config.cellSizeY       = 50.f;
        config.cellsInSectorXZ = 8;   // sector size = 800
        config.cellsPerChunk   = 16;
        return config;
    }

} // namespace

TEST(PVSStreamingTest, LoadsSectorsWithinRadius)
{
    PVSConfig config = MakeStreamingConfig();
    auto *loader = new PVSLoader(config);
    std::unique_ptr<PVSLoader> ptr(loader);

    const uint32_t cellDataSize = 4;
    loader->SetProvider(new FakeSectorProvider(config, cellDataSize));
    loader->SetStreamingConfig(PVSStreamingConfig{1, 1});
    loader->Update(Vector3(0.f, 0.f, 0.f));

    uint32_t loaded = 0;
    for (int32_t x = -1; x <= 1; ++x) {
        for (int32_t y = -1; y <= 1; ++y) {
            if (loader->FindSector(PVSSectorCoord{x, y}) != nullptr) {
                ++loaded;
            }
        }
    }
    EXPECT_EQ(loaded, 9U);
    EXPECT_EQ(loader->GetCellDataSize(), cellDataSize);
}

TEST(PVSStreamingTest, HysteresisKeepsSectorsUntilBeyondUnloadRadius)
{
    PVSConfig config = MakeStreamingConfig();
    PVSLoader loader(config);
    loader.SetProvider(new FakeSectorProvider(config, 4));
    loader.SetStreamingConfig(PVSStreamingConfig{1, 1}); // unloadRadius = 2

    loader.Update(Vector3(0.f, 0.f, 0.f)); // sector (0,0)
    ASSERT_NE(loader.FindSector(PVSSectorCoord{0, 0}), nullptr);

    // Move one sector east: (0,0) is within the hysteresis band, stays loaded.
    loader.Update(Vector3(900.f, 0.f, 0.f)); // sector (1,0)
    EXPECT_NE(loader.FindSector(PVSSectorCoord{0, 0}), nullptr);

    // Move far away: (0,0) is beyond loadRadius + unloadMargin, unloaded.
    loader.Update(Vector3(5000.f, 0.f, 0.f)); // sector (6,0)
    EXPECT_EQ(loader.FindSector(PVSSectorCoord{0, 0}), nullptr);
    EXPECT_NE(loader.FindSector(PVSSectorCoord{6, 0}), nullptr);
}

TEST(PVSStreamingTest, QueryAnswersCellsInNeighborSectors)
{
    PVSConfig config = MakeStreamingConfig();
    PVSLoader loader(config);
    loader.SetProvider(new FakeSectorProvider(config, 4));
    loader.SetStreamingConfig(PVSStreamingConfig{1, 1});

    loader.Update(Vector3(0.f, 0.f, 0.f)); // main view sector (0,0)

    // A cell in the streamed neighbor sector (1,0): x in [800, 1600)
    const auto neighborCell = config.CalculateCellCoordByWorldPosition(Vector3(900.f, 0.f, 0.f));
    EXPECT_NE(loader.QueryVisibility(neighborCell), nullptr);

    // A cell in a far, unloaded sector is unavailable (callers fail safe).
    const auto farCell = config.CalculateCellCoordByWorldPosition(Vector3(100000.f, 0.f, 0.f));
    EXPECT_EQ(loader.QueryVisibility(farCell), nullptr);
}

namespace {

    // Provider whose sector files are always missing.
    class MissingSectorProvider : public IPVSSectorProvider {
    public:
        bool LoadHeader(PVSConfig &) override { return true; }
        bool LoadSector(const PVSSectorCoord & /*coord*/, PVSSector & /*out*/) override { return false; }
    };

} // namespace

TEST(PVSStreamingTest, MissingSectorIsRecordedWithoutFailing)
{
    PVSConfig config = MakeStreamingConfig();
    PVSLoader loader(config);
    loader.SetProvider(new MissingSectorProvider());
    loader.SetStreamingConfig(PVSStreamingConfig{1, 0}); // 3x3 sectors attempted

    loader.Update(Vector3(0.f, 0.f, 0.f));

    EXPECT_EQ(loader.FindSector(PVSSectorCoord{0, 0}), nullptr);
    EXPECT_EQ(loader.GetMissingSectorCount(), 9u);
}

// =========================================================================
// Serialization round-trip (the core owns the PVS on-disk format)
// =========================================================================

TEST(PVSSerializationTest, ConfigRoundTrip)
{
    PVSConfig config;
    config.worldOffset     = Vector3(10.f, 20.f, 30.f);
    config.cellSize        = 100.f;
    config.cellSizeY       = 50.f;
    config.cellsInSectorXZ = 8;
    config.cellsPerChunk   = 16;

    std::stringstream stream;
    {
        OStreamArchive os(stream);
        BinaryOutputArchive out(os);
        config.Save(out);
    }

    stream.seekg(0);

    PVSConfig loaded;
    {
        IStreamArchive is(stream);
        BinaryInputArchive in(is);
        loaded.Load(in);
    }

    EXPECT_FLOAT_EQ(loaded.worldOffset.x, 10.f);
    EXPECT_FLOAT_EQ(loaded.worldOffset.y, 20.f);
    EXPECT_FLOAT_EQ(loaded.worldOffset.z, 30.f);
    EXPECT_FLOAT_EQ(loaded.cellSize, 100.f);
    EXPECT_FLOAT_EQ(loaded.cellSizeY, 50.f);
    EXPECT_EQ(loaded.cellsInSectorXZ, 8);
    EXPECT_EQ(loaded.cellsPerChunk, 16u);
}

TEST(PVSSerializationTest, SectorRoundTrip)
{
    PVSConfig config;
    config.cellsInSectorXZ = 8;
    config.cellsPerChunk   = 16;

    const uint32_t cellDataSize = 4;

    PVSSector sector;
    sector.Init(config, cellDataSize);
    sector.version = 3;
    sector.cells[5].chunkIndex = 0;
    sector.cells[5].dataOffset = 2;

    PVSChunk chunk;
    chunk.storage = std::make_unique<uint8_t[]>(sector.chunkSize);
    chunk.storage[2] = 0xAB;
    sector.chunks.emplace_back(std::move(chunk));

    std::stringstream stream;
    {
        OStreamArchive os(stream);
        BinaryOutputArchive out(os);
        sector.Save(out);
    }

    stream.seekg(0);

    PVSSector loaded;
    {
        IStreamArchive is(stream);
        BinaryInputArchive in(is);
        loaded.Load(in);
    }

    EXPECT_EQ(loaded.version, 3u);
    EXPECT_EQ(loaded.chunkSize, sector.chunkSize);
    ASSERT_EQ(loaded.cells.size(), sector.cells.size());
    EXPECT_EQ(loaded.cells[5].chunkIndex, 0);
    EXPECT_EQ(loaded.cells[5].dataOffset, 2);
    ASSERT_EQ(loaded.chunks.size(), 1u);
    ASSERT_NE(loaded.chunks[0].storage, nullptr);
    EXPECT_EQ(loaded.chunks[0].storage[2], 0xAB);
}

// =========================================================================
// PVSConfig tests
// =========================================================================

class PVSConfigTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        config.worldOffset    = VEC3_ZERO;
        config.cellSize       = 100.f;
        config.cellSizeY      = 50.f;
        config.cellsInSectorXZ = 8;
        config.cellsPerChunk  = 16;
    }

    PVSConfig config;
};

TEST_F(PVSConfigTest, GetCellsPerSector)
{
    ASSERT_EQ(config.GetCellsPerSector(), 64u);   // 8 * 8
}

TEST_F(PVSConfigTest, GetSectorSize)
{
    ASSERT_FLOAT_EQ(config.GetSectorSize(), 800.f); // 8 * 100
}

// -- CalculateCellCoordByWorldPosition --

TEST_F(PVSConfigTest, CellCoordAtOrigin)
{
    auto coord = config.CalculateCellCoordByWorldPosition(Vector3(0.f, 0.f, 0.f));
    ASSERT_EQ(coord.x, 0);
    ASSERT_EQ(coord.y, 0);
    ASSERT_EQ(coord.z, 0);
}

TEST_F(PVSConfigTest, CellCoordPositive)
{
    // pos = (250, 75, 350) => x=floor(250/100)=2, y=floor(75/50)=1, z=floor(350/100)=3
    auto coord = config.CalculateCellCoordByWorldPosition(Vector3(250.f, 75.f, 350.f));
    ASSERT_EQ(coord.x, 2);
    ASSERT_EQ(coord.y, 1);
    ASSERT_EQ(coord.z, 3);
}

TEST_F(PVSConfigTest, CellCoordNegative)
{
    // pos = (-50, -25, -150) => x=floor(-50/100)=-1, y=floor(-25/50)=-1, z=floor(-150/100)=-2
    auto coord = config.CalculateCellCoordByWorldPosition(Vector3(-50.f, -25.f, -150.f));
    ASSERT_EQ(coord.x, -1);
    ASSERT_EQ(coord.y, -1);
    ASSERT_EQ(coord.z, -2);
}

TEST_F(PVSConfigTest, CellCoordOnBoundary)
{
    // Exactly on cell boundary: pos = (100, 50, 200) => x=1, y=1, z=2
    auto coord = config.CalculateCellCoordByWorldPosition(Vector3(100.f, 50.f, 200.f));
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 1);
    ASSERT_EQ(coord.z, 2);
}

TEST_F(PVSConfigTest, CellCoordWithWorldOffset)
{
    config.worldOffset = Vector3(500.f, 100.f, 300.f);
    // pos = (650, 125, 450) => (650-500)/100=1.5 => floor=1, (125-100)/50=0.5 => floor=0, (450-300)/100=1.5 => floor=1
    auto coord = config.CalculateCellCoordByWorldPosition(Vector3(650.f, 125.f, 450.f));
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 0);
    ASSERT_EQ(coord.z, 1);
}

// -- CalculateSectorCoordByWorldPosition --

TEST_F(PVSConfigTest, SectorCoordAtOrigin)
{
    auto coord = config.CalculateSectorCoordByWorldPosition(Vector3(0.f, 0.f, 0.f));
    ASSERT_EQ(coord.x, 0);
    ASSERT_EQ(coord.y, 0);
}

TEST_F(PVSConfigTest, SectorCoordPositive)
{
    // sectorSize = 800, pos.x=900 => floor(900/800)=1, pos.z=1700 => floor(1700/800)=2
    auto coord = config.CalculateSectorCoordByWorldPosition(Vector3(900.f, 0.f, 1700.f));
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 2);
}

TEST_F(PVSConfigTest, SectorCoordNegative)
{
    // pos.x=-100 => floor(-100/800)=-1, pos.z=-900 => floor(-900/800)=-2
    auto coord = config.CalculateSectorCoordByWorldPosition(Vector3(-100.f, 0.f, -900.f));
    ASSERT_EQ(coord.x, -1);
    ASSERT_EQ(coord.y, -2);
}

TEST_F(PVSConfigTest, SectorCoordIgnoresY)
{
    // Y position should not affect sector coord
    auto a = config.CalculateSectorCoordByWorldPosition(Vector3(50.f,    0.f, 50.f));
    auto b = config.CalculateSectorCoordByWorldPosition(Vector3(50.f, 9999.f, 50.f));
    ASSERT_EQ(a, b);
}

// -- CalculateSectorCoordByCellCoord --

TEST_F(PVSConfigTest, SectorCoordByCellCoordPositive)
{
    // cell (10, *, 5), cellsInSectorXZ=8 => sector.x = floor(10/8)=1, sector.y = floor(5/8)=0
    auto coord = config.CalculateSectorCoordByCellCoord(PVSCellCoord{10, 0, 5});
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 0);
}

TEST_F(PVSConfigTest, SectorCoordByCellCoordNegative)
{
    // cell (-1, *, -1), cellsInSectorXZ=8 => sector.x = floorDiv(-1,8)=-1, sector.y = floorDiv(-1,8)=-1
    auto coord = config.CalculateSectorCoordByCellCoord(PVSCellCoord{-1, 0, -1});
    ASSERT_EQ(coord.x, -1);
    ASSERT_EQ(coord.y, -1);
}

TEST_F(PVSConfigTest, SectorCoordByCellCoordOnBoundary)
{
    // cell (8, *, 16) => sector.x = 1, sector.y = 2
    auto coord = config.CalculateSectorCoordByCellCoord(PVSCellCoord{8, 0, 16});
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 2);
}

// -- CalculateCellIndexInSector --

TEST_F(PVSConfigTest, CellIndexInSectorOrigin)
{
    // pos at origin => cell(0,0,0), sector(0,0), localX=0, localZ=0 => index=0
    ASSERT_EQ(config.CalculateCellIndexInSector(Vector3(0.f, 0.f, 0.f)), 0u);
}

TEST_F(PVSConfigTest, CellIndexInSectorLocalOffset)
{
    // pos = (350, 0, 250) => cell(3,0,2), sector(0,0), localX=3, localZ=2 => index = 2*8+3 = 19
    ASSERT_EQ(config.CalculateCellIndexInSector(Vector3(350.f, 0.f, 250.f)), 19u);
}

TEST_F(PVSConfigTest, CellIndexInSectorCrossBoundary)
{
    // pos = (850, 0, 150) => cell(8,0,1), sector(1,0), localX=8-8=0, localZ=1-0=1 => index = 1*8+0 = 8
    ASSERT_EQ(config.CalculateCellIndexInSector(Vector3(850.f, 0.f, 150.f)), 8u);
}

TEST_F(PVSConfigTest, CellIndexInSectorByCellCoord)
{
    // cell(3,0,2), sector(0,0) => localX=3, localZ=2 => 2*8+3 = 19
    ASSERT_EQ(config.CalculateCellIndexInSector(PVSCellCoord{3, 0, 2}), 19u);
}

TEST_F(PVSConfigTest, CellIndexInSectorByCellCoordNegative)
{
    // cell(-1, 0, -1), sector(-1, -1)
    // localX = -1 - (-1*8) = -1+8 = 7, localZ = -1 - (-1*8) = 7
    // index = 7*8 + 7 = 63
    ASSERT_EQ(config.CalculateCellIndexInSector(PVSCellCoord{-1, 0, -1}), 63u);
}

// -- CalculateCellWorldMin --

TEST_F(PVSConfigTest, CellWorldMinOrigin)
{
    auto min = config.CalculateCellWorldMin(PVSCellCoord{0, 0, 0});
    ASSERT_FLOAT_EQ(min.x, 0.f);
    ASSERT_FLOAT_EQ(min.y, 0.f);
    ASSERT_FLOAT_EQ(min.z, 0.f);
}

TEST_F(PVSConfigTest, CellWorldMinPositive)
{
    // cell(3, 2, 5) => (3*100, 2*50, 5*100) = (300, 100, 500)
    auto min = config.CalculateCellWorldMin(PVSCellCoord{3, 2, 5});
    ASSERT_FLOAT_EQ(min.x, 300.f);
    ASSERT_FLOAT_EQ(min.y, 100.f);
    ASSERT_FLOAT_EQ(min.z, 500.f);
}

TEST_F(PVSConfigTest, CellWorldMinNegative)
{
    // cell(-2, -1, -3) => (-200, -50, -300)
    auto min = config.CalculateCellWorldMin(PVSCellCoord{-2, -1, -3});
    ASSERT_FLOAT_EQ(min.x, -200.f);
    ASSERT_FLOAT_EQ(min.y, -50.f);
    ASSERT_FLOAT_EQ(min.z, -300.f);
}

TEST_F(PVSConfigTest, CellWorldMinWithOffset)
{
    config.worldOffset = Vector3(500.f, 100.f, 300.f);
    // cell(1, 0, 2) => (1*100+500, 0*50+100, 2*100+300) = (600, 100, 500)
    auto min = config.CalculateCellWorldMin(PVSCellCoord{1, 0, 2});
    ASSERT_FLOAT_EQ(min.x, 600.f);
    ASSERT_FLOAT_EQ(min.y, 100.f);
    ASSERT_FLOAT_EQ(min.z, 500.f);
}

// -- Roundtrip: world pos → cell coord → cell world min → same cell coord --

TEST_F(PVSConfigTest, RoundtripPositive)
{
    Vector3 pos(350.f, 75.f, 250.f);
    auto cellCoord = config.CalculateCellCoordByWorldPosition(pos);
    auto cellMin = config.CalculateCellWorldMin(cellCoord);

    // The original pos must lie within [cellMin, cellMin + cellExtent)
    ASSERT_LE(cellMin.x, pos.x);
    ASSERT_LE(cellMin.y, pos.y);
    ASSERT_LE(cellMin.z, pos.z);
    ASSERT_GT(cellMin.x + config.cellSize,  pos.x);
    ASSERT_GT(cellMin.y + config.cellSizeY, pos.y);
    ASSERT_GT(cellMin.z + config.cellSize,  pos.z);

    // And re-computing cell coord from cellMin gives same result
    auto cellCoord2 = config.CalculateCellCoordByWorldPosition(cellMin);
    ASSERT_EQ(cellCoord, cellCoord2);
}

TEST_F(PVSConfigTest, RoundtripNegative)
{
    Vector3 pos(-350.f, -75.f, -250.f);
    auto cellCoord = config.CalculateCellCoordByWorldPosition(pos);
    auto cellMin = config.CalculateCellWorldMin(cellCoord);

    ASSERT_LE(cellMin.x, pos.x);
    ASSERT_LE(cellMin.y, pos.y);
    ASSERT_LE(cellMin.z, pos.z);
    ASSERT_GT(cellMin.x + config.cellSize,  pos.x);
    ASSERT_GT(cellMin.y + config.cellSizeY, pos.y);
    ASSERT_GT(cellMin.z + config.cellSize,  pos.z);

    auto cellCoord2 = config.CalculateCellCoordByWorldPosition(cellMin);
    ASSERT_EQ(cellCoord, cellCoord2);
}