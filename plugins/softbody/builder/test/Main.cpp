#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/cook/mesh/AuroraMeshBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/builder/SoftBodyBuilder.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::aurora::AuroraReflection(sky::SerializationContext::Get());
    sky::softbody::RegisterSoftBodyAssets();
    sky::AssetBuilderManager::Get()->RegisterBuilder(new sky::aurora::AuroraMeshBuilder());
    sky::AssetBuilderManager::Get()->RegisterBuilder(new sky::softbody::SoftBodyBuilder());
    return RUN_ALL_TESTS();
}
