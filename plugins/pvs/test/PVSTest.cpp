//
// Created by Zach Lee on 2026/2/24.
//

#include <gtest/gtest.h>
#include <pvs/PVSCulling.h>
#include <pvs/PVSLoader.h>

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