//
// Created by SkyEngine on 2024/02/15.
//

#include <gtest/gtest.h>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBitSet.h>
#include <render/culling/PVSData.h>
#include <render/culling/PVSCulling.h>

using namespace sky;

// ============================================================================
// PVSBitSet Tests
// ============================================================================

TEST(PVSBitSetTest, DefaultConstruction)
{
    PVSBitSet bitset;
    ASSERT_EQ(bitset.GetCapacity(), 0);
    ASSERT_TRUE(bitset.None());
}

TEST(PVSBitSetTest, ConstructionWithSize)
{
    PVSBitSet bitset(128);
    ASSERT_EQ(bitset.GetCapacity(), 128);
    ASSERT_TRUE(bitset.None());
}

TEST(PVSBitSetTest, SetAndTest)
{
    PVSBitSet bitset(256);
    
    ASSERT_FALSE(bitset.Test(0));
    ASSERT_FALSE(bitset.Test(100));
    ASSERT_FALSE(bitset.Test(255));
    
    bitset.Set(0);
    bitset.Set(100);
    bitset.Set(255);
    
    ASSERT_TRUE(bitset.Test(0));
    ASSERT_TRUE(bitset.Test(100));
    ASSERT_TRUE(bitset.Test(255));
    ASSERT_FALSE(bitset.Test(1));
    ASSERT_FALSE(bitset.Test(99));
}

TEST(PVSBitSetTest, Clear)
{
    PVSBitSet bitset(100);
    
    bitset.Set(50);
    ASSERT_TRUE(bitset.Test(50));
    
    bitset.Clear(50);
    ASSERT_FALSE(bitset.Test(50));
}

TEST(PVSBitSetTest, ClearAll)
{
    PVSBitSet bitset(100);
    
    bitset.Set(10);
    bitset.Set(50);
    bitset.Set(90);
    ASSERT_TRUE(bitset.Any());
    
    bitset.ClearAll();
    ASSERT_TRUE(bitset.None());
}

TEST(PVSBitSetTest, SetAll)
{
    PVSBitSet bitset(100);
    
    bitset.SetAll();
    
    ASSERT_TRUE(bitset.Test(0));
    ASSERT_TRUE(bitset.Test(50));
    ASSERT_TRUE(bitset.Test(99));
    ASSERT_EQ(bitset.CountSet(), 100);
}

TEST(PVSBitSetTest, CountSet)
{
    PVSBitSet bitset(256);
    
    ASSERT_EQ(bitset.CountSet(), 0);
    
    bitset.Set(0);
    bitset.Set(64);
    bitset.Set(128);
    bitset.Set(192);
    
    ASSERT_EQ(bitset.CountSet(), 4);
}

TEST(PVSBitSetTest, OrWith)
{
    PVSBitSet bitset1(100);
    PVSBitSet bitset2(100);
    
    bitset1.Set(10);
    bitset1.Set(20);
    
    bitset2.Set(20);
    bitset2.Set(30);
    
    bitset1.OrWith(bitset2);
    
    ASSERT_TRUE(bitset1.Test(10));
    ASSERT_TRUE(bitset1.Test(20));
    ASSERT_TRUE(bitset1.Test(30));
    ASSERT_EQ(bitset1.CountSet(), 3);
}

TEST(PVSBitSetTest, AndWith)
{
    PVSBitSet bitset1(100);
    PVSBitSet bitset2(100);
    
    bitset1.Set(10);
    bitset1.Set(20);
    bitset1.Set(30);
    
    bitset2.Set(20);
    bitset2.Set(30);
    bitset2.Set(40);
    
    bitset1.AndWith(bitset2);
    
    ASSERT_FALSE(bitset1.Test(10));
    ASSERT_TRUE(bitset1.Test(20));
    ASSERT_TRUE(bitset1.Test(30));
    ASSERT_FALSE(bitset1.Test(40));
    ASSERT_EQ(bitset1.CountSet(), 2);
}

TEST(PVSBitSetTest, OutOfRangeAccess)
{
    PVSBitSet bitset(50);
    
    // These should not crash
    bitset.Set(100);  // Out of range, should be ignored
    ASSERT_FALSE(bitset.Test(100));  // Out of range, should return false
    
    bitset.Clear(100);  // Out of range, should be ignored
}

// ============================================================================
// PVSData Tests
// ============================================================================

TEST(PVSDataTest, Initialization)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(-10.f, -10.f, -10.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    // 20/5 = 4 cells in each dimension = 4*4*4 = 64 cells
    ASSERT_EQ(pvsData.GetCellCount(), 64);
    
    auto dims = pvsData.GetGridDimensions();
    ASSERT_EQ(dims.x, 4);
    ASSERT_EQ(dims.y, 4);
    ASSERT_EQ(dims.z, 4);
}

TEST(PVSDataTest, CellIDFromPosition)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    // 2x2x2 grid
    ASSERT_EQ(pvsData.GetCellCount(), 8);
    
    // Test cell at (0, 0, 0)
    PVSCellID cell1 = pvsData.GetCellID(Vector3(1.f, 1.f, 1.f));
    ASSERT_NE(cell1, INVALID_PVS_CELL);
    
    // Test cell at (1, 1, 1)
    PVSCellID cell2 = pvsData.GetCellID(Vector3(7.f, 7.f, 7.f));
    ASSERT_NE(cell2, INVALID_PVS_CELL);
    ASSERT_NE(cell1, cell2);
    
    // Test outside bounds
    PVSCellID invalidCell = pvsData.GetCellID(Vector3(-5.f, 0.f, 0.f));
    ASSERT_EQ(invalidCell, INVALID_PVS_CELL);
}

TEST(PVSDataTest, CellCoordinates)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    PVSCellCoord coord = pvsData.GetCellCoord(Vector3(7.f, 7.f, 7.f));
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 1);
    ASSERT_EQ(coord.z, 1);
    
    PVSCellID cellID = pvsData.GetCellIDFromCoord(coord);
    PVSCellCoord recoveredCoord = pvsData.GetCoordFromCellID(cellID);
    
    ASSERT_EQ(coord.x, recoveredCoord.x);
    ASSERT_EQ(coord.y, recoveredCoord.y);
    ASSERT_EQ(coord.z, recoveredCoord.z);
}

TEST(PVSDataTest, VisibilitySetAndQuery)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    PVSCellID cellID = 0;
    PVSObjectID obj1 = 0;
    PVSObjectID obj2 = 10;
    PVSObjectID obj3 = 50;
    
    ASSERT_FALSE(pvsData.IsVisible(cellID, obj1));
    ASSERT_FALSE(pvsData.IsVisible(cellID, obj2));
    
    pvsData.SetVisible(cellID, obj1);
    pvsData.SetVisible(cellID, obj2);
    
    ASSERT_TRUE(pvsData.IsVisible(cellID, obj1));
    ASSERT_TRUE(pvsData.IsVisible(cellID, obj2));
    ASSERT_FALSE(pvsData.IsVisible(cellID, obj3));
    
    pvsData.ClearVisible(cellID, obj1);
    ASSERT_FALSE(pvsData.IsVisible(cellID, obj1));
    ASSERT_TRUE(pvsData.IsVisible(cellID, obj2));
}

TEST(PVSDataTest, CellBounds)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    // Cell at (0, 0, 0) should have bounds [0,0,0] to [5,5,5]
    PVSCellID cell0 = pvsData.GetCellIDFromCoord({0, 0, 0});
    const PVSCell& cell = pvsData.GetCell(cell0);
    
    ASSERT_FLOAT_EQ(cell.bounds.min.x, 0.f);
    ASSERT_FLOAT_EQ(cell.bounds.min.y, 0.f);
    ASSERT_FLOAT_EQ(cell.bounds.min.z, 0.f);
    ASSERT_FLOAT_EQ(cell.bounds.max.x, 5.f);
    ASSERT_FLOAT_EQ(cell.bounds.max.y, 5.f);
    ASSERT_FLOAT_EQ(cell.bounds.max.z, 5.f);
}

// ============================================================================
// PVSCulling Tests
// ============================================================================

TEST(PVSCullingTest, Initialization)
{
    PVSCulling pvsCulling;
    
    ASSERT_FALSE(pvsCulling.IsInitialized());
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(-10.f, -10.f, -10.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    ASSERT_TRUE(pvsCulling.IsInitialized());
    ASSERT_EQ(pvsCulling.GetObjectCount(), 0);
}

TEST(PVSCullingTest, PrimitiveRegistration)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(-10.f, -10.f, -10.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Create mock primitives
    RenderPrimitive primitive1;
    RenderPrimitive primitive2;
    
    PVSObjectID id1 = pvsCulling.RegisterPrimitive(&primitive1);
    PVSObjectID id2 = pvsCulling.RegisterPrimitive(&primitive2);
    
    ASSERT_NE(id1, INVALID_PVS_OBJECT);
    ASSERT_NE(id2, INVALID_PVS_OBJECT);
    ASSERT_NE(id1, id2);
    ASSERT_EQ(pvsCulling.GetObjectCount(), 2);
    
    // Double registration should return same ID
    PVSObjectID id1Again = pvsCulling.RegisterPrimitive(&primitive1);
    ASSERT_EQ(id1, id1Again);
    ASSERT_EQ(pvsCulling.GetObjectCount(), 2);
}

TEST(PVSCullingTest, PrimitiveUnregistration)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(-10.f, -10.f, -10.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    RenderPrimitive primitive;
    PVSObjectID id = pvsCulling.RegisterPrimitive(&primitive);
    ASSERT_NE(id, INVALID_PVS_OBJECT);
    
    pvsCulling.UnregisterPrimitive(&primitive);
    
    PVSObjectID newId = pvsCulling.GetObjectID(&primitive);
    ASSERT_EQ(newId, INVALID_PVS_OBJECT);
}

TEST(PVSCullingTest, NullPrimitiveHandling)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(-10.f, -10.f, -10.f), Vector3(10.f, 10.f, 10.f)};
    config.cellSize = Vector3(5.f, 5.f, 5.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Should handle null gracefully
    PVSObjectID id = pvsCulling.RegisterPrimitive(nullptr);
    ASSERT_EQ(id, INVALID_PVS_OBJECT);
    
    // Should not crash
    pvsCulling.UnregisterPrimitive(nullptr);
}

TEST(PVSCullingTest, VisibilityQuery)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(20.f, 20.f, 20.f)};
    config.cellSize = Vector3(10.f, 10.f, 10.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Create and register primitives with bounds
    RenderPrimitive primitive1;
    primitive1.worldBound = AABB{Vector3(5.f, 5.f, 5.f), Vector3(8.f, 8.f, 8.f)};
    
    RenderPrimitive primitive2;
    primitive2.worldBound = AABB{Vector3(15.f, 15.f, 15.f), Vector3(18.f, 18.f, 18.f)};
    
    PVSObjectID id1 = pvsCulling.RegisterPrimitive(&primitive1);
    PVSObjectID id2 = pvsCulling.RegisterPrimitive(&primitive2);
    
    // Set visibility: primitive1 visible from cell (0,0,0), primitive2 from cell (1,1,1)
    PVSCellID cell0 = pvsCulling.GetPVSData().GetCellIDFromCoord({0, 0, 0});
    PVSCellID cell1 = pvsCulling.GetPVSData().GetCellIDFromCoord({1, 1, 1});
    
    pvsCulling.GetPVSData().SetVisible(cell0, id1);
    pvsCulling.GetPVSData().SetVisible(cell1, id2);
    
    // Query from position in cell (0,0,0)
    std::vector<RenderPrimitive*> result;
    pvsCulling.QueryPVSVisiblePrimitives(Vector3(5.f, 5.f, 5.f), result);
    
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0], &primitive1);
    
    // Query from position in cell (1,1,1)
    result.clear();
    pvsCulling.QueryPVSVisiblePrimitives(Vector3(15.f, 15.f, 15.f), result);
    
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0], &primitive2);
}

TEST(PVSCullingTest, DistanceBasedVisibility)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(100.f, 100.f, 100.f)};
    config.cellSize = Vector3(10.f, 10.f, 10.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Create primitives at different positions
    RenderPrimitive nearPrimitive;
    nearPrimitive.worldBound = AABB{Vector3(4.f, 4.f, 4.f), Vector3(6.f, 6.f, 6.f)};
    
    RenderPrimitive farPrimitive;
    farPrimitive.worldBound = AABB{Vector3(94.f, 94.f, 94.f), Vector3(96.f, 96.f, 96.f)};
    
    pvsCulling.RegisterPrimitive(&nearPrimitive);
    pvsCulling.RegisterPrimitive(&farPrimitive);
    
    // Compute distance-based visibility with max distance of 20
    pvsCulling.ComputeDistanceBasedVisibility(20.f);
    
    // Query from first cell - near primitive should be visible
    std::vector<RenderPrimitive*> result;
    pvsCulling.QueryPVSVisiblePrimitives(Vector3(5.f, 5.f, 5.f), result);
    
    // Near primitive should be visible, far primitive should not
    bool nearFound = std::find(result.begin(), result.end(), &nearPrimitive) != result.end();
    bool farFound = std::find(result.begin(), result.end(), &farPrimitive) != result.end();
    
    ASSERT_TRUE(nearFound);
    ASSERT_FALSE(farFound);
}

TEST(PVSCullingTest, IsPrimitiveVisible)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(20.f, 20.f, 20.f)};
    config.cellSize = Vector3(10.f, 10.f, 10.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    RenderPrimitive primitive;
    PVSObjectID id = pvsCulling.RegisterPrimitive(&primitive);
    
    PVSCellID cell0 = pvsCulling.GetPVSData().GetCellIDFromCoord({0, 0, 0});
    pvsCulling.GetPVSData().SetVisible(cell0, id);
    
    // Should be visible from cell 0
    ASSERT_TRUE(pvsCulling.IsPrimitiveVisible(Vector3(5.f, 5.f, 5.f), &primitive));
    
    // Should not be visible from cell 1
    ASSERT_FALSE(pvsCulling.IsPrimitiveVisible(Vector3(15.f, 15.f, 15.f), &primitive));
}
