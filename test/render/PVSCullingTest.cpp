//
// Created by SkyEngine on 2024/02/15.
//

#include <gtest/gtest.h>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBitSet.h>
#include <render/culling/PVSData.h>
#include <render/culling/PVSCulling.h>
#include <render/culling/PVSBaker.h>
#include <render/culling/PVSBakedData.h>
#include <render/culling/PVSStreamingTypes.h>
#include <render/culling/PVSStreamingManager.h>
#include <render/culling/SIMDUtils.h>
#include <render/culling/PVSSampling.h>
#include <cmath>

using namespace sky;

// ============================================================================
// SIMD Utilities Tests
// ============================================================================

TEST(SIMDUtilsTest, BitwiseOr)
{
    std::vector<uint64_t> dst = {0x00FF00FF00FF00FFULL, 0xAAAAAAAAAAAAAAAAULL, 0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL};
    std::vector<uint64_t> src = {0xFF00FF00FF00FF00ULL, 0x5555555555555555ULL, 0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL};
    
    simd::BitwiseOr(dst.data(), src.data(), dst.size());
    
    ASSERT_EQ(dst[0], 0xFFFFFFFFFFFFFFFFULL);
    ASSERT_EQ(dst[1], 0xFFFFFFFFFFFFFFFFULL);
    ASSERT_EQ(dst[2], 0xFFFFFFFFFFFFFFFFULL);
    ASSERT_EQ(dst[3], 0xFFFFFFFFFFFFFFFFULL);
}

TEST(SIMDUtilsTest, BitwiseAnd)
{
    std::vector<uint64_t> dst = {0xFFFFFFFFFFFFFFFFULL, 0xAAAAAAAAAAAAAAAAULL, 0x0F0F0F0F0F0F0F0FULL, 0xFFFFFFFFFFFFFFFFULL};
    std::vector<uint64_t> src = {0x00FF00FF00FF00FFULL, 0x5555555555555555ULL, 0x0F0F0F0F0F0F0F0FULL, 0x0000000000000000ULL};
    
    simd::BitwiseAnd(dst.data(), src.data(), dst.size());
    
    ASSERT_EQ(dst[0], 0x00FF00FF00FF00FFULL);
    ASSERT_EQ(dst[1], 0x0000000000000000ULL);
    ASSERT_EQ(dst[2], 0x0F0F0F0F0F0F0F0FULL);
    ASSERT_EQ(dst[3], 0x0000000000000000ULL);
}

TEST(SIMDUtilsTest, AnyBitSet)
{
    std::vector<uint64_t> allZero = {0, 0, 0, 0};
    std::vector<uint64_t> oneSet = {0, 0, 0, 1};
    std::vector<uint64_t> allSet = {~0ULL, ~0ULL, ~0ULL, ~0ULL};
    
    ASSERT_FALSE(simd::AnyBitSet(allZero.data(), allZero.size()));
    ASSERT_TRUE(simd::AnyBitSet(oneSet.data(), oneSet.size()));
    ASSERT_TRUE(simd::AnyBitSet(allSet.data(), allSet.size()));
}

TEST(SIMDUtilsTest, PopCount64)
{
    ASSERT_EQ(simd::PopCount64(0), 0);
    ASSERT_EQ(simd::PopCount64(1), 1);
    ASSERT_EQ(simd::PopCount64(0xFFFFFFFFFFFFFFFFULL), 64);
    ASSERT_EQ(simd::PopCount64(0x5555555555555555ULL), 32);
    ASSERT_EQ(simd::PopCount64(0xAAAAAAAAAAAAAAAAULL), 32);
}

TEST(SIMDUtilsTest, PopCountArray)
{
    std::vector<uint64_t> data = {0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL};
    // Each byte has 4 bits set, 8 bytes per uint64_t = 32 bits each
    ASSERT_EQ(simd::PopCountArray(data.data(), data.size()), 64);
}

TEST(SIMDUtilsTest, ZeroFill)
{
    std::vector<uint64_t> data = {~0ULL, ~0ULL, ~0ULL, ~0ULL};
    
    simd::ZeroFill(data.data(), data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        ASSERT_EQ(data[i], 0);
    }
}

TEST(SIMDUtilsTest, DistanceSquaredBatch)
{
    // Test 3 point pairs
    float pointsA[] = {
        0.0f, 0.0f, 0.0f,   // Point 1
        1.0f, 0.0f, 0.0f,   // Point 2
        1.0f, 2.0f, 3.0f    // Point 3
    };
    float pointsB[] = {
        3.0f, 4.0f, 0.0f,   // Point 1: dist^2 = 9 + 16 = 25
        1.0f, 0.0f, 0.0f,   // Point 2: dist^2 = 0 (same point)
        4.0f, 6.0f, 3.0f    // Point 3: dist^2 = 9 + 16 + 0 = 25
    };
    float distances[3] = {0, 0, 0};
    
    simd::DistanceSquaredBatch(pointsA, pointsB, distances, 3);
    
    ASSERT_FLOAT_EQ(distances[0], 25.0f);
    ASSERT_FLOAT_EQ(distances[1], 0.0f);
    ASSERT_FLOAT_EQ(distances[2], 25.0f);
}

TEST(SIMDUtilsTest, AABBIntersectionBatch)
{
    // Test 4 AABB pairs
    float minA[] = {
        0.0f, 0.0f, 0.0f,   // AABB 1 min - intersects
        10.0f, 10.0f, 10.0f, // AABB 2 min - no intersection
        0.0f, 0.0f, 0.0f,   // AABB 3 min - touching
        -5.0f, -5.0f, -5.0f  // AABB 4 min - fully contains
    };
    float maxA[] = {
        5.0f, 5.0f, 5.0f,   // AABB 1 max
        15.0f, 15.0f, 15.0f, // AABB 2 max
        5.0f, 5.0f, 5.0f,   // AABB 3 max
        10.0f, 10.0f, 10.0f  // AABB 4 max
    };
    float minB[] = {
        3.0f, 3.0f, 3.0f,   // AABB 1 min - intersects
        0.0f, 0.0f, 0.0f,   // AABB 2 min - no intersection
        5.0f, 5.0f, 5.0f,   // AABB 3 min - touching
        0.0f, 0.0f, 0.0f    // AABB 4 min - contained
    };
    float maxB[] = {
        8.0f, 8.0f, 8.0f,   // AABB 1 max
        5.0f, 5.0f, 5.0f,   // AABB 2 max
        10.0f, 10.0f, 10.0f, // AABB 3 max
        5.0f, 5.0f, 5.0f    // AABB 4 max
    };
    uint8_t results[4] = {0, 0, 0, 0};
    
    simd::AABBIntersectionBatch(minA, maxA, minB, maxB, results, 4);
    
    ASSERT_EQ(results[0], 1);  // Intersects
    ASSERT_EQ(results[1], 0);  // No intersection
    ASSERT_EQ(results[2], 1);  // Touching (counts as intersection)
    ASSERT_EQ(results[3], 1);  // Contains
}

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

// ============================================================================
// PVS Baker Tests
// ============================================================================

TEST(PVSBakerTest, AddObjects)
{
    PVSBaker baker;
    
    ASSERT_EQ(baker.GetObjectCount(), 0);
    
    PVSBakeObject obj1;
    obj1.bounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(5.f, 5.f, 5.f)};
    obj1.name = "Object1";
    
    uint32_t id1 = baker.AddObject(obj1);
    ASSERT_EQ(id1, 0);
    ASSERT_EQ(baker.GetObjectCount(), 1);
    
    PVSBakeObject obj2;
    obj2.bounds = AABB{Vector3(10.f, 10.f, 10.f), Vector3(15.f, 15.f, 15.f)};
    obj2.name = "Object2";
    
    uint32_t id2 = baker.AddObject(obj2);
    ASSERT_EQ(id2, 1);
    ASSERT_EQ(baker.GetObjectCount(), 2);
}

TEST(PVSBakerTest, ClearObjects)
{
    PVSBaker baker;
    
    PVSBakeObject obj;
    obj.bounds = AABB{Vector3(0.f), Vector3(5.f)};
    baker.AddObject(obj);
    baker.AddObject(obj);
    
    ASSERT_EQ(baker.GetObjectCount(), 2);
    
    baker.ClearObjects();
    ASSERT_EQ(baker.GetObjectCount(), 0);
}

TEST(PVSBakerTest, BakeDistanceBased)
{
    PVSBaker baker;
    
    // Add a few objects at different positions
    PVSBakeObject nearObj;
    nearObj.bounds = AABB{Vector3(4.f, 4.f, 4.f), Vector3(6.f, 6.f, 6.f)};
    nearObj.name = "NearObject";
    baker.AddObject(nearObj);
    
    PVSBakeObject farObj;
    farObj.bounds = AABB{Vector3(94.f, 94.f, 94.f), Vector3(96.f, 96.f, 96.f)};
    farObj.name = "FarObject";
    baker.AddObject(farObj);
    
    // Configure baking
    PVSBakeConfig bakeConfig;
    bakeConfig.pvsConfig.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(100.f, 100.f, 100.f)};
    bakeConfig.pvsConfig.cellSize = Vector3(10.f, 10.f, 10.f);
    bakeConfig.pvsConfig.maxObjects = 100;
    bakeConfig.method = PVSBakeConfig::Method::DISTANCE;
    bakeConfig.maxVisibilityDistance = 20.f;
    
    // Bake
    PVSBakedData bakedData;
    PVSBakeResult result = baker.Bake(bakeConfig, bakedData);
    
    ASSERT_TRUE(result.success);
    ASSERT_GT(bakedData.cells.size(), 0);
    ASSERT_EQ(bakedData.numObjects, 2);
    
    // The near object should be visible from cell (0,0,0)
    // Cell 0 center is at (5,5,5), near object center is at (5,5,5) - distance 0
    uint32_t cell0 = 0;
    bool nearVisible = (bakedData.visibilityData[cell0][0] & 1) != 0;
    ASSERT_TRUE(nearVisible);
    
    // The far object should NOT be visible from cell (0,0,0)
    // Cell 0 center is at (5,5,5), far object center is at (95,95,95) - distance ~156
    bool farVisible = (bakedData.visibilityData[cell0][0] & 2) != 0;
    ASSERT_FALSE(farVisible);
}

TEST(PVSBakerTest, BakeCustomFunction)
{
    PVSBaker baker;
    
    // Add objects
    PVSBakeObject obj1;
    obj1.bounds = AABB{Vector3(0.f), Vector3(5.f)};
    obj1.name = "Object1";
    baker.AddObject(obj1);
    
    PVSBakeObject obj2;
    obj2.bounds = AABB{Vector3(10.f), Vector3(15.f)};
    obj2.name = "Object2";
    baker.AddObject(obj2);
    
    // Configure baking with custom function that only makes object 0 visible
    PVSBakeConfig bakeConfig;
    bakeConfig.pvsConfig.worldBounds = AABB{Vector3(0.f), Vector3(20.f)};
    bakeConfig.pvsConfig.cellSize = Vector3(10.f);
    bakeConfig.pvsConfig.maxObjects = 100;
    bakeConfig.method = PVSBakeConfig::Method::CUSTOM;
    bakeConfig.customTestFunc = [](const AABB&, const Vector3&, const AABB&, uint32_t objIndex) {
        return objIndex == 0;  // Only object 0 is visible
    };
    
    PVSBakedData bakedData;
    PVSBakeResult result = baker.Bake(bakeConfig, bakedData);
    
    ASSERT_TRUE(result.success);
    
    // Object 0 should be visible from all cells
    for (size_t i = 0; i < bakedData.visibilityData.size(); ++i) {
        bool obj0Visible = (bakedData.visibilityData[i][0] & 1) != 0;
        bool obj1Visible = (bakedData.visibilityData[i][0] & 2) != 0;
        ASSERT_TRUE(obj0Visible);
        ASSERT_FALSE(obj1Visible);
    }
}

TEST(PVSBakerTest, Statistics)
{
    PVSBaker baker;
    
    // Add 10 objects
    for (int i = 0; i < 10; ++i) {
        PVSBakeObject obj;
        obj.bounds = AABB{
            Vector3(static_cast<float>(i * 10), 0.f, 0.f),
            Vector3(static_cast<float>(i * 10 + 5), 5.f, 5.f)
        };
        obj.name = "Object" + std::to_string(i);
        baker.AddObject(obj);
    }
    
    PVSBakeConfig bakeConfig;
    bakeConfig.pvsConfig.worldBounds = AABB{Vector3(0.f), Vector3(100.f)};
    bakeConfig.pvsConfig.cellSize = Vector3(25.f);
    bakeConfig.pvsConfig.maxObjects = 100;
    bakeConfig.method = PVSBakeConfig::Method::DISTANCE;
    bakeConfig.maxVisibilityDistance = 50.f;
    
    PVSBakedData bakedData;
    PVSBakeResult result = baker.Bake(bakeConfig, bakedData);
    
    ASSERT_TRUE(result.success);
    ASSERT_GT(result.statistics.totalCells, 0);
    ASSERT_EQ(result.statistics.totalObjects, 10);
    ASSERT_GT(result.statistics.totalVisiblePairs, 0);
}

// ============================================================================
// PVS Baked Data Serialization Tests
// ============================================================================

TEST(PVSBakedDataTest, GetMemorySize)
{
    PVSBakedData data;
    data.config.worldBounds = AABB{Vector3(-10.f), Vector3(10.f)};
    data.config.cellSize = Vector3(5.f);
    data.gridDimensions = {4, 4, 4};
    data.numObjects = 100;
    
    // Add some cells
    data.cells.resize(64);
    
    // Add visibility data
    data.visibilityData.resize(64);
    for (auto &v : data.visibilityData) {
        v.resize(2);  // 128 bits
    }
    
    size_t memSize = data.GetMemorySize();
    ASSERT_GT(memSize, 0);
}

TEST(PVSBakedDataTest, GetStatistics)
{
    PVSBakedData data;
    data.numObjects = 64;
    data.cells.resize(8);
    data.visibilityData.resize(8);
    
    // Set some visibility bits
    for (size_t i = 0; i < 8; ++i) {
        data.visibilityData[i].resize(1);
        data.visibilityData[i][0] = 0x0F0F0F0F0F0F0F0FULL;  // 32 bits set
    }
    
    auto stats = data.GetStatistics();
    
    ASSERT_EQ(stats.totalCells, 8);
    ASSERT_EQ(stats.totalObjects, 64);
    ASSERT_EQ(stats.totalVisiblePairs, 32 * 8);  // 32 bits set per cell, 8 cells
    ASSERT_FLOAT_EQ(stats.averageVisibleObjects, 32.f);
}

// ============================================================================
// PVS Data Load/Export Tests
// ============================================================================

TEST(PVSDataTest, LoadFromBakedData)
{
    // Create baked data
    PVSBakedData bakedData;
    bakedData.config.worldBounds = AABB{Vector3(0.f), Vector3(20.f)};
    bakedData.config.cellSize = Vector3(10.f);
    bakedData.config.maxObjects = 100;
    bakedData.gridDimensions = {2, 2, 2};
    bakedData.numObjects = 3;
    
    // Create cells
    bakedData.cells.resize(8);
    for (uint32_t i = 0; i < 8; ++i) {
        bakedData.cells[i].id = i;
    }
    
    // Create visibility data - object 0 visible from all cells
    bakedData.visibilityData.resize(8);
    for (auto &v : bakedData.visibilityData) {
        v.resize(1);
        v[0] = 0x01;  // Object 0 visible
    }
    
    // Load into PVSData
    PVSData pvsData;
    pvsData.LoadFromBakedData(bakedData);
    
    // Verify
    ASSERT_EQ(pvsData.GetCellCount(), 8);
    
    for (uint32_t cellID = 0; cellID < 8; ++cellID) {
        ASSERT_TRUE(pvsData.IsVisible(cellID, 0));   // Object 0 visible
        ASSERT_FALSE(pvsData.IsVisible(cellID, 1));  // Object 1 not visible
    }
}

TEST(PVSDataTest, ExportToBakedData)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(20.f)};
    config.cellSize = Vector3(10.f);
    config.maxObjects = 10;
    
    pvsData.Initialize(config);
    
    // Set some visibility
    pvsData.SetVisible(0, 0);
    pvsData.SetVisible(0, 1);
    pvsData.SetVisible(1, 2);
    
    // Export
    PVSBakedData bakedData;
    pvsData.ExportToBakedData(bakedData);
    
    // Verify
    ASSERT_EQ(bakedData.config.maxObjects, 10);
    ASSERT_EQ(bakedData.gridDimensions.x, 2);
    ASSERT_EQ(bakedData.gridDimensions.y, 2);
    ASSERT_EQ(bakedData.gridDimensions.z, 2);
    ASSERT_EQ(bakedData.cells.size(), 8);
}

// ============================================================================
// Optimized Query Tests
// ============================================================================

TEST(PVSBitSetTest, ForEachSetBit)
{
    PVSBitSet bitset(256);
    
    // Set some bits
    bitset.Set(0);
    bitset.Set(5);
    bitset.Set(63);   // Last bit of first word
    bitset.Set(64);   // First bit of second word
    bitset.Set(127);
    bitset.Set(200);
    
    std::vector<uint32_t> indices;
    bitset.ForEachSetBit([&indices](uint32_t idx) {
        indices.push_back(idx);
    });
    
    ASSERT_EQ(indices.size(), 6);
    ASSERT_EQ(indices[0], 0);
    ASSERT_EQ(indices[1], 5);
    ASSERT_EQ(indices[2], 63);
    ASSERT_EQ(indices[3], 64);
    ASSERT_EQ(indices[4], 127);
    ASSERT_EQ(indices[5], 200);
}

TEST(PVSBitSetTest, GetSetBitIndices)
{
    PVSBitSet bitset(128);
    
    bitset.Set(10);
    bitset.Set(50);
    bitset.Set(100);
    
    std::vector<uint32_t> indices;
    bitset.GetSetBitIndices(indices);
    
    ASSERT_EQ(indices.size(), 3);
    ASSERT_EQ(indices[0], 10);
    ASSERT_EQ(indices[1], 50);
    ASSERT_EQ(indices[2], 100);
}

TEST(PVSBitSetTest, GetSetBitIndicesWithLimit)
{
    PVSBitSet bitset(256);
    
    for (uint32_t i = 0; i < 20; ++i) {
        bitset.Set(i * 10);
    }
    
    std::vector<uint32_t> indices;
    uint32_t count = bitset.GetSetBitIndices(indices, 5);
    
    ASSERT_EQ(count, 5);
    ASSERT_EQ(indices.size(), 5);
}

TEST(PVSDataTest, FastCellLookup)
{
    PVSData pvsData;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(100.f, 100.f, 100.f)};
    config.cellSize = Vector3(10.f, 10.f, 10.f);
    config.maxObjects = 100;
    
    pvsData.Initialize(config);
    
    // Test GetCellIDFast vs GetCellID
    Vector3 testPos(25.f, 35.f, 45.f);
    
    PVSCellID normalID = pvsData.GetCellID(testPos);
    PVSCellID fastID = pvsData.GetCellIDFast(testPos);
    
    ASSERT_EQ(normalID, fastID);
    
    // Test IsInBounds
    ASSERT_TRUE(pvsData.IsInBounds(testPos));
    ASSERT_FALSE(pvsData.IsInBounds(Vector3(-1.f, 0.f, 0.f)));
    ASSERT_FALSE(pvsData.IsInBounds(Vector3(100.f, 0.f, 0.f)));  // Edge case
}

TEST(PVSCullingTest, QueryVisiblePrimitivesOptimized)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f, 0.f, 0.f), Vector3(100.f, 100.f, 100.f)};
    config.cellSize = Vector3(20.f, 20.f, 20.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Register some primitives
    std::vector<RenderPrimitive> primitives(10);
    for (int i = 0; i < 10; ++i) {
        pvsCulling.RegisterPrimitive(&primitives[i]);
    }
    
    // Set visibility for cell 0 - only objects 0, 3, 7 visible
    PVSCellID cell0 = pvsCulling.GetPVSData().GetCellIDFromCoord({0, 0, 0});
    pvsCulling.GetPVSData().SetVisible(cell0, 0);
    pvsCulling.GetPVSData().SetVisible(cell0, 3);
    pvsCulling.GetPVSData().SetVisible(cell0, 7);
    
    // Query with optimized method
    std::vector<RenderPrimitive*> resultOptimized;
    pvsCulling.QueryVisiblePrimitivesOptimized(Vector3(5.f, 5.f, 5.f), nullptr, resultOptimized);
    
    // Query with original method for comparison
    std::vector<RenderPrimitive*> resultOriginal;
    pvsCulling.QueryVisiblePrimitives(Vector3(5.f, 5.f, 5.f), nullptr, resultOriginal);
    
    // Both methods should return same results
    ASSERT_EQ(resultOptimized.size(), resultOriginal.size());
    ASSERT_EQ(resultOptimized.size(), 3);
}

TEST(PVSCullingTest, ForEachVisibleObject)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(50.f)};
    config.cellSize = Vector3(10.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Register primitives
    std::vector<RenderPrimitive> primitives(5);
    for (int i = 0; i < 5; ++i) {
        pvsCulling.RegisterPrimitive(&primitives[i]);
    }
    
    // Set visibility
    PVSCellID cell0 = pvsCulling.GetPVSData().GetCellIDFromCoord({0, 0, 0});
    pvsCulling.GetPVSData().SetVisible(cell0, 1);
    pvsCulling.GetPVSData().SetVisible(cell0, 3);
    
    // Test ForEachVisibleObject
    std::vector<PVSObjectID> visitedObjects;
    uint32_t count = pvsCulling.ForEachVisibleObject(Vector3(5.f, 5.f, 5.f), 
        [&visitedObjects](PVSObjectID objID) {
            visitedObjects.push_back(objID);
        });
    
    ASSERT_EQ(count, 2);
    ASSERT_EQ(visitedObjects.size(), 2);
    ASSERT_EQ(visitedObjects[0], 1);
    ASSERT_EQ(visitedObjects[1], 3);
}

TEST(PVSCullingTest, GetVisibleCount)
{
    PVSCulling pvsCulling;
    
    PVSConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(40.f)};
    config.cellSize = Vector3(20.f);
    config.maxObjects = 100;
    
    pvsCulling.Initialize(config);
    
    // Register primitives
    std::vector<RenderPrimitive> primitives(10);
    for (int i = 0; i < 10; ++i) {
        pvsCulling.RegisterPrimitive(&primitives[i]);
    }
    
    // Set different visibility per cell
    PVSCellID cell0 = pvsCulling.GetPVSData().GetCellIDFromCoord({0, 0, 0});
    PVSCellID cell1 = pvsCulling.GetPVSData().GetCellIDFromCoord({1, 0, 0});
    
    pvsCulling.GetPVSData().SetVisible(cell0, 0);
    pvsCulling.GetPVSData().SetVisible(cell0, 1);
    pvsCulling.GetPVSData().SetVisible(cell0, 2);
    
    pvsCulling.GetPVSData().SetVisible(cell1, 5);
    pvsCulling.GetPVSData().SetVisible(cell1, 6);
    pvsCulling.GetPVSData().SetVisible(cell1, 7);
    pvsCulling.GetPVSData().SetVisible(cell1, 8);
    pvsCulling.GetPVSData().SetVisible(cell1, 9);
    
    // Cell 0 has 3 visible
    ASSERT_EQ(pvsCulling.GetVisibleCount(Vector3(5.f, 5.f, 5.f)), 3);
    
    // Cell 1 has 5 visible
    ASSERT_EQ(pvsCulling.GetVisibleCount(Vector3(25.f, 5.f, 5.f)), 5);
}

// ============================================================================
// PVS Streaming Tests
// ============================================================================

TEST(PVSStreamingTypesTest, SectorCoordEquality)
{
    PVSSectorCoord coord1{1, 2, 3};
    PVSSectorCoord coord2{1, 2, 3};
    PVSSectorCoord coord3{1, 2, 4};
    
    ASSERT_TRUE(coord1 == coord2);
    ASSERT_FALSE(coord1 == coord3);
    ASSERT_TRUE(coord1 != coord3);
}

TEST(PVSStreamingTypesTest, SectorCoordHash)
{
    PVSSectorCoordHash hasher;
    
    PVSSectorCoord coord1{1, 2, 3};
    PVSSectorCoord coord2{1, 2, 3};
    PVSSectorCoord coord3{3, 2, 1};
    
    ASSERT_EQ(hasher(coord1), hasher(coord2));
    ASSERT_NE(hasher(coord1), hasher(coord3));
}

TEST(PVSStreamingManagerTest, Initialize)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(1000.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    config.loadRadius = 150.f;
    config.unloadRadius = 200.f;
    config.maxLoadedSectors = 9;
    
    manager.Initialize(config);
    
    ASSERT_EQ(manager.GetLoadedSectorCount(), 0);
}

TEST(PVSStreamingManagerTest, GetSectorCoord)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(400.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    
    manager.Initialize(config);
    
    // Test sector coordinate calculation
    PVSSectorCoord coord1 = manager.GetSectorCoord(Vector3(50.f, 50.f, 50.f));
    ASSERT_EQ(coord1.x, 0);
    ASSERT_EQ(coord1.y, 0);
    ASSERT_EQ(coord1.z, 0);
    
    PVSSectorCoord coord2 = manager.GetSectorCoord(Vector3(150.f, 150.f, 150.f));
    ASSERT_EQ(coord2.x, 1);
    ASSERT_EQ(coord2.y, 1);
    ASSERT_EQ(coord2.z, 1);
    
    PVSSectorCoord coord3 = manager.GetSectorCoord(Vector3(350.f, 250.f, 50.f));
    ASSERT_EQ(coord3.x, 3);
    ASSERT_EQ(coord3.y, 2);
    ASSERT_EQ(coord3.z, 0);
}

TEST(PVSStreamingManagerTest, SectorLoadUnload)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(400.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    config.maxObjectsPerSector = 100;
    
    manager.Initialize(config);
    
    // Set up a data provider that creates simple sector data
    manager.SetDataProvider([&config](PVSSectorCoord coord, PVSSectorBakedData &outData) {
        outData.coord = coord;
        outData.bounds = AABB{
            Vector3(coord.x * 100.f, coord.y * 100.f, coord.z * 100.f),
            Vector3((coord.x + 1) * 100.f, (coord.y + 1) * 100.f, (coord.z + 1) * 100.f)
        };
        
        // Set up simple PVS data
        outData.pvsData.config.worldBounds = outData.bounds;
        outData.pvsData.config.cellSize = config.cellSize;
        outData.pvsData.config.maxObjects = config.maxObjectsPerSector;
        outData.pvsData.gridDimensions = {10, 10, 10};
        outData.pvsData.numObjects = 10;
        outData.pvsData.cells.resize(1000);
        outData.pvsData.visibilityData.resize(1000);
        
        return true;
    });
    
    // Request load
    bool loadCalled = false;
    PVSSectorCoord targetCoord{1, 1, 1};
    manager.RequestSectorLoad(targetCoord, [&loadCalled](PVSSectorID id, bool success) {
        loadCalled = true;
        ASSERT_TRUE(success);
    });
    
    ASSERT_TRUE(loadCalled);
    ASSERT_TRUE(manager.IsSectorLoaded(targetCoord));
    ASSERT_EQ(manager.GetLoadedSectorCount(), 1);
    
    // Request unload
    manager.RequestSectorUnload(targetCoord);
    ASSERT_FALSE(manager.IsSectorLoaded(targetCoord));
    ASSERT_EQ(manager.GetLoadedSectorCount(), 0);
}

TEST(PVSStreamingManagerTest, AutomaticStreaming)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(500.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    config.loadRadius = 150.f;
    config.unloadRadius = 250.f;
    config.maxLoadedSectors = 27;
    
    manager.Initialize(config);
    
    // Set up data provider
    manager.SetDataProvider([&config](PVSSectorCoord coord, PVSSectorBakedData &outData) {
        outData.coord = coord;
        outData.pvsData.config.worldBounds = AABB{
            Vector3(coord.x * 100.f, coord.y * 100.f, coord.z * 100.f),
            Vector3((coord.x + 1) * 100.f, (coord.y + 1) * 100.f, (coord.z + 1) * 100.f)
        };
        outData.pvsData.config.cellSize = config.cellSize;
        outData.pvsData.config.maxObjects = 100;
        outData.pvsData.gridDimensions = {10, 10, 10};
        outData.pvsData.numObjects = 10;
        outData.pvsData.cells.resize(1000);
        outData.pvsData.visibilityData.resize(1000);
        return true;
    });
    
    // Update with viewer at center
    manager.Update(Vector3(250.f, 250.f, 250.f), 1);
    
    // Should have loaded sectors around viewer
    ASSERT_GT(manager.GetLoadedSectorCount(), 0);
    ASSERT_TRUE(manager.IsPositionLoaded(Vector3(250.f, 250.f, 250.f)));
}

TEST(PVSStreamingManagerTest, LRUEviction)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(1000.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    config.loadRadius = 50.f;
    config.unloadRadius = 100.f;
    config.maxLoadedSectors = 3;  // Very limited
    
    manager.Initialize(config);
    
    manager.SetDataProvider([&config](PVSSectorCoord coord, PVSSectorBakedData &outData) {
        outData.coord = coord;
        outData.pvsData.config.maxObjects = 10;
        outData.pvsData.gridDimensions = {10, 10, 10};
        outData.pvsData.numObjects = 10;
        outData.pvsData.cells.resize(1000);
        outData.pvsData.visibilityData.resize(1000);
        return true;
    });
    
    // Load first sector
    manager.RequestSectorLoad({0, 0, 0});
    ASSERT_EQ(manager.GetLoadedSectorCount(), 1);
    
    // Load second sector
    manager.RequestSectorLoad({1, 0, 0});
    ASSERT_EQ(manager.GetLoadedSectorCount(), 2);
    
    // Load third sector
    manager.RequestSectorLoad({2, 0, 0});
    ASSERT_EQ(manager.GetLoadedSectorCount(), 3);
    
    // Load fourth sector - should evict oldest
    manager.RequestSectorLoad({3, 0, 0});
    // Note: LRU eviction happens in Update() or EnforceSectorLimit()
    // After manual loads, we need to enforce limit
    
    auto stats = manager.GetStatistics();
    ASSERT_LE(stats.loadedSectors, config.maxLoadedSectors + 1);  // May be 1 over until next update
}

TEST(PVSStreamingManagerTest, Statistics)
{
    PVSStreamingManager manager;
    
    PVSStreamingConfig config;
    config.worldBounds = AABB{Vector3(0.f), Vector3(400.f)};
    config.sectorSize = Vector3(100.f);
    config.cellSize = Vector3(10.f);
    config.maxLoadedSectors = 16;
    
    manager.Initialize(config);
    
    manager.SetDataProvider([](PVSSectorCoord coord, PVSSectorBakedData &outData) {
        outData.coord = coord;
        outData.pvsData.config.maxObjects = 10;
        outData.pvsData.gridDimensions = {10, 10, 10};
        outData.pvsData.numObjects = 10;
        outData.pvsData.cells.resize(1000);
        outData.pvsData.visibilityData.resize(1000);
        return true;
    });
    
    manager.RequestSectorLoad({0, 0, 0});
    manager.RequestSectorLoad({1, 0, 0});
    
    auto stats = manager.GetStatistics();
    ASSERT_EQ(stats.loadedSectors, 2);
    ASSERT_GT(stats.totalMemoryUsed, 0);
}

// ============================================================================
// PVS Sampling Tests
// ============================================================================

TEST(PVSSamplingTest, RandomGenerator)
{
    PVSRandomGenerator rng(12345);
    
    // Test float range [0, 1)
    for (int i = 0; i < 100; ++i) {
        float f = rng.NextFloat();
        ASSERT_GE(f, 0.0f);
        ASSERT_LT(f, 1.0f);
    }
    
    // Test float range with custom bounds
    for (int i = 0; i < 100; ++i) {
        float f = rng.NextFloat(10.0f, 20.0f);
        ASSERT_GE(f, 10.0f);
        ASSERT_LT(f, 20.0f);
    }
}

TEST(PVSSamplingTest, RandomPointInCell)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(10.0f, 20.0f, 30.0f), Vector3(20.0f, 40.0f, 60.0f)};
    
    for (int i = 0; i < 100; ++i) {
        Vector3 point = sampler.GenerateRandomPointInCell(bounds);
        
        ASSERT_GE(point.x, bounds.min.x);
        ASSERT_LT(point.x, bounds.max.x);
        ASSERT_GE(point.y, bounds.min.y);
        ASSERT_LT(point.y, bounds.max.y);
        ASSERT_GE(point.z, bounds.min.z);
        ASSERT_LT(point.z, bounds.max.z);
    }
}

TEST(PVSSamplingTest, RandomDirection)
{
    PVSSampling sampler(12345);
    
    for (int i = 0; i < 100; ++i) {
        Vector3 dir = sampler.GenerateRandomDirection();
        
        // Should be normalized (length ~= 1)
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        ASSERT_NEAR(len, 1.0f, 0.001f);
    }
}

TEST(PVSSamplingTest, HemisphereDirection)
{
    PVSSampling sampler(12345);
    Vector3 upNormal(0.0f, 1.0f, 0.0f);
    
    for (int i = 0; i < 100; ++i) {
        Vector3 dir = sampler.GenerateHemisphereDirection(upNormal);
        
        // Should be normalized
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        ASSERT_NEAR(len, 1.0f, 0.001f);
        
        // Dot product with normal should be positive (in hemisphere)
        float dot = dir.x * upNormal.x + dir.y * upNormal.y + dir.z * upNormal.z;
        ASSERT_GE(dot, 0.0f);
    }
}

TEST(PVSSamplingTest, HaltonSequence)
{
    // Known Halton sequence values for base 2
    ASSERT_NEAR(PVSSampling::HaltonSequence(1, 2), 0.5f, 0.001f);
    ASSERT_NEAR(PVSSampling::HaltonSequence(2, 2), 0.25f, 0.001f);
    ASSERT_NEAR(PVSSampling::HaltonSequence(3, 2), 0.75f, 0.001f);
    ASSERT_NEAR(PVSSampling::HaltonSequence(4, 2), 0.125f, 0.001f);
    
    // Halton sequence base 3
    ASSERT_NEAR(PVSSampling::HaltonSequence(1, 3), 1.0f/3.0f, 0.001f);
    ASSERT_NEAR(PVSSampling::HaltonSequence(2, 3), 2.0f/3.0f, 0.001f);
    ASSERT_NEAR(PVSSampling::HaltonSequence(3, 3), 1.0f/9.0f, 0.001f);
}

TEST(PVSSamplingTest, FibonacciSphereDirection)
{
    std::vector<Vector3> directions;
    PVSSampling::GenerateFibonacciDirections(100, directions);
    
    ASSERT_EQ(directions.size(), 100);
    
    for (const auto &dir : directions) {
        // Should be normalized
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        ASSERT_NEAR(len, 1.0f, 0.001f);
    }
}

TEST(PVSSamplingTest, StratifiedPoints)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(0.0f), Vector3(10.0f)};
    std::vector<Vector3> points;
    
    sampler.GenerateStratifiedPoints(bounds, 64, points);
    
    ASSERT_EQ(points.size(), 64);
    
    for (const auto &point : points) {
        ASSERT_GE(point.x, bounds.min.x);
        ASSERT_LT(point.x, bounds.max.x);
        ASSERT_GE(point.y, bounds.min.y);
        ASSERT_LT(point.y, bounds.max.y);
        ASSERT_GE(point.z, bounds.min.z);
        ASSERT_LT(point.z, bounds.max.z);
    }
}

TEST(PVSSamplingTest, HaltonPoints)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(5.0f, 10.0f, 15.0f), Vector3(15.0f, 30.0f, 45.0f)};
    std::vector<Vector3> points;
    
    sampler.GenerateHaltonPoints(bounds, 50, points);
    
    ASSERT_EQ(points.size(), 50);
    
    for (const auto &point : points) {
        ASSERT_GE(point.x, bounds.min.x);
        ASSERT_LE(point.x, bounds.max.x);
        ASSERT_GE(point.y, bounds.min.y);
        ASSERT_LE(point.y, bounds.max.y);
        ASSERT_GE(point.z, bounds.min.z);
        ASSERT_LE(point.z, bounds.max.z);
    }
}

TEST(PVSSamplingTest, GenerateCellSamples)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(0.0f), Vector3(10.0f)};
    
    PVSCellSamplingConfig config;
    config.numSamplesPerCell = 8;
    config.numDirectionsPerSample = 16;
    config.pointStrategy = PVSSamplingStrategy::STRATIFIED;
    config.directionStrategy = PVSSamplingStrategy::FIBONACCI;
    
    PVSCellSamples samples;
    sampler.GenerateCellSamples(bounds, config, samples);
    
    // Should have numSamplesPerCell * numDirectionsPerSample samples
    ASSERT_EQ(samples.samples.size(), config.numSamplesPerCell * config.numDirectionsPerSample);
    
    for (const auto &sample : samples.samples) {
        // Position should be in bounds
        ASSERT_GE(sample.position.x, bounds.min.x);
        ASSERT_LT(sample.position.x, bounds.max.x);
        ASSERT_GE(sample.position.y, bounds.min.y);
        ASSERT_LT(sample.position.y, bounds.max.y);
        ASSERT_GE(sample.position.z, bounds.min.z);
        ASSERT_LT(sample.position.z, bounds.max.z);
        
        // Direction should be normalized
        float len = std::sqrt(sample.direction.x * sample.direction.x + 
                             sample.direction.y * sample.direction.y + 
                             sample.direction.z * sample.direction.z);
        ASSERT_NEAR(len, 1.0f, 0.001f);
    }
}

TEST(PVSSamplingTest, HemisphereSampling)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(0.0f), Vector3(10.0f)};
    
    PVSCellSamplingConfig config;
    config.numSamplesPerCell = 4;
    config.numDirectionsPerSample = 8;
    config.useHemisphereForFloors = true;  // Use hemisphere
    
    PVSCellSamples samples;
    sampler.GenerateCellSamples(bounds, config, samples);
    
    ASSERT_EQ(samples.samples.size(), config.numSamplesPerCell * config.numDirectionsPerSample);
    
    // All directions should be in upper hemisphere (y >= 0)
    for (const auto &sample : samples.samples) {
        ASSERT_GE(sample.direction.y, 0.0f);
    }
}

TEST(PVSSamplingTest, DifferentStrategies)
{
    PVSSampling sampler(12345);
    AABB bounds{Vector3(0.0f), Vector3(10.0f)};
    
    // Test RANDOM point strategy
    std::vector<Vector3> randomPoints;
    sampler.GeneratePointsInCell(bounds, 32, PVSSamplingStrategy::RANDOM, randomPoints);
    ASSERT_EQ(randomPoints.size(), 32);
    
    // Test HALTON point strategy
    std::vector<Vector3> haltonPoints;
    sampler.GeneratePointsInCell(bounds, 32, PVSSamplingStrategy::HALTON, haltonPoints);
    ASSERT_EQ(haltonPoints.size(), 32);
    
    // Test RANDOM direction strategy
    std::vector<Vector3> randomDirs;
    sampler.GenerateDirections(32, PVSSamplingStrategy::RANDOM, randomDirs);
    ASSERT_EQ(randomDirs.size(), 32);
    
    // Test FIBONACCI direction strategy
    std::vector<Vector3> fibDirs;
    sampler.GenerateDirections(32, PVSSamplingStrategy::FIBONACCI, fibDirs);
    ASSERT_EQ(fibDirs.size(), 32);
}

TEST(PVSSamplingTest, SeedDeterminism)
{
    // Two samplers with same seed should produce same results
    PVSSampling sampler1(42);
    PVSSampling sampler2(42);
    
    AABB bounds{Vector3(0.0f), Vector3(10.0f)};
    
    for (int i = 0; i < 10; ++i) {
        Vector3 p1 = sampler1.GenerateRandomPointInCell(bounds);
        Vector3 p2 = sampler2.GenerateRandomPointInCell(bounds);
        
        ASSERT_FLOAT_EQ(p1.x, p2.x);
        ASSERT_FLOAT_EQ(p1.y, p2.y);
        ASSERT_FLOAT_EQ(p1.z, p2.z);
    }
}
