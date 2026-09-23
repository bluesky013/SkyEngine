//
// Created on 2026/09/22.
//

#include <vegetation/VegetationAsset.h>

#include <core/file/FileSystem.h>
#include <framework/serialization/BinaryArchive.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace sky;
using namespace sky::vegetation;

namespace {

    FileSystemPtr CreateTestFileSystem()
    {
        const auto dir = (std::filesystem::temp_directory_path() / "sky_vegetation_asset_test").string();
        return new NativeFileSystem(FilePath(dir));
    }

    VegetationAssetData BuildSampleData()
    {
        VegetationAssetData data;
        data.version = 1;
        data.config.seed                 = 99;
        data.config.pointsPerSquareMeter = 2.f;

        VegetationBiome biome;
        biome.id             = 7;
        biome.maxSlopeDeg    = 35.f;
        biome.layerIndex     = 0;
        biome.minLayerWeight = 0.5f;
        biome.density        = 0.75f;

        VegetationSpecies grass;
        grass.mesh           = Uuid::CreateFromString("11111111-1111-1111-1111-111111111111");
        grass.density        = 3.f;
        grass.scaleMin       = 0.5f;
        grass.scaleMax       = 1.5f;
        grass.rotationJitter = 360.f;
        grass.windResponse   = 0.8f;
        biome.species.push_back(grass);
        biome.species.push_back(grass);
        data.palette.biomes.push_back(biome);

        VegetationDensityMap map;
        map.origin     = Vector3(1.f, 0.f, 2.f);
        map.resolution = 4.f;
        map.sizeX      = 5;
        map.sizeY      = 3;
        map.data       = {1, 2, 3, 4, 5, 6};
        data.densityMaps.push_back(map);

        VegetationInstance instance;
        instance.position     = Vector3(3.f, 0.f, 5.f);
        instance.rotation     = 45.f;
        instance.scale        = 1.2f;
        instance.biomeId      = 7;
        instance.speciesIndex = 1;
        data.instances.push_back(instance);

        return data;
    }

} // namespace

TEST(VegetationAssetTest, SaveLoadRoundTrip)
{
    const auto src = BuildSampleData();

    auto fs   = CreateTestFileSystem();
    auto file = fs->CreateOrOpenFile(FilePath("vegetation.bin"));
    ASSERT_NE(file, nullptr);

    {
        auto                oa = file->WriteAsArchive();
        BinaryOutputArchive archive(*oa);
        src.Save(archive);
    }

    VegetationAssetData dst;
    {
        auto               ia = file->ReadAsArchive();
        BinaryInputArchive archive(*ia);
        dst.Load(archive);
    }

    EXPECT_EQ(dst.version, 1u);
    EXPECT_EQ(dst.config.seed, 99u);
    EXPECT_FLOAT_EQ(dst.config.pointsPerSquareMeter, 2.f);

    ASSERT_EQ(dst.palette.biomes.size(), 1u);
    const auto &biome = dst.palette.biomes[0];
    EXPECT_EQ(biome.id, 7u);
    EXPECT_FLOAT_EQ(biome.maxSlopeDeg, 35.f);
    EXPECT_FLOAT_EQ(biome.density, 0.75f);
    ASSERT_EQ(biome.species.size(), 2u);
    EXPECT_EQ(biome.species[0].mesh.ToString(), src.palette.biomes[0].species[0].mesh.ToString());
    EXPECT_FLOAT_EQ(biome.species[0].windResponse, 0.8f);

    ASSERT_EQ(dst.densityMaps.size(), 1u);
    EXPECT_FLOAT_EQ(dst.densityMaps[0].origin.z, 2.f);
    EXPECT_EQ(dst.densityMaps[0].sizeX, 5u);
    EXPECT_EQ(dst.densityMaps[0].data, src.densityMaps[0].data);

    ASSERT_EQ(dst.instances.size(), 1u);
    EXPECT_FLOAT_EQ(dst.instances[0].position.z, 5.f);
    EXPECT_FLOAT_EQ(dst.instances[0].rotation, 45.f);
    EXPECT_FLOAT_EQ(dst.instances[0].scale, 1.2f);
    EXPECT_EQ(dst.instances[0].speciesIndex, 1u);
}
