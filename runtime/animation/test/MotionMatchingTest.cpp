//
// Created by Zach Lee on 2025/6/3.
//

#include <gtest/gtest.h>
#include <animation/motion/MotionDatabase.h>
#include <animation/motion/MotionMacher.h>
#include <animation/graph/MotionMatchingNode.h>
#include <animation/core/AnimationClip.h>
#include <animation/core/Skeleton.h>

using namespace sky;

TEST(MotionMatchingTest, MotionDatabaseTest)
{
    // Create a simple skeleton
    SkeletonData skeletonData = {};
    skeletonData.boneData.emplace_back(BoneData{Name("Root")});
    skeletonData.boneData.emplace_back(BoneData{Name("Joint1")});
    skeletonData.refPos.emplace_back(Transform::GetIdentity());
    skeletonData.refPos.emplace_back(Transform::GetIdentity());
    
    SkeletonPtr skeleton = Skeleton::BuildSkeleton(skeletonData);
    
    // Create a simple animation clip
    Uuid clipId;
    AnimClipPtr clip = new AnimationClip(Name("TestClip"), clipId);
    clip->SetFrameRate(30.f);
    clip->SetNumFrame(30); // 1 second clip
    
    // Create motion database
    MotionDatabase database;
    ASSERT_EQ(database.GetNumFrames(), 0);
    
    // Add clip to database
    database.AddClip(clip, 30);
    
    // Check that frames were added
    ASSERT_GT(database.GetNumFrames(), 0);
    
    // Get first frame
    const MotionFrame* frame = database.GetFrame(0);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->clip, clip);
    
    // Clear database
    database.Clear();
    ASSERT_EQ(database.GetNumFrames(), 0);
}

TEST(MotionMatchingTest, MotionMatcherTest)
{
    // Create a motion database
    auto database = std::make_shared<MotionDatabase>();
    
    // Create a simple animation clip
    Uuid clipId;
    AnimClipPtr clip = new AnimationClip(Name("TestClip"), clipId);
    clip->SetFrameRate(30.f);
    clip->SetNumFrame(30);
    
    database->AddClip(clip, 30);
    
    // Create motion matcher
    MotionMatcher matcher;
    matcher.SetDatabase(database);
    
    // Create a query
    MotionQuery query;
    query.desiredRootVelocity = Vector3(1.0f, 0.0f, 0.0f);
    query.poseWeight = 1.0f;
    query.velocityWeight = 1.0f;
    query.trajectoryWeight = 1.0f;
    
    // Find best match
    MotionMatchResult result = matcher.FindBestMatch(query);
    
    // Should find a valid result
    ASSERT_TRUE(result.valid);
    ASSERT_LT(result.frameIndex, database->GetNumFrames());
}

TEST(MotionMatchingTest, MotionMatchingNodeTest)
{
    // Create a motion database
    auto database = std::make_shared<MotionDatabase>();
    
    // Create a simple animation clip
    Uuid clipId;
    AnimClipPtr clip = new AnimationClip(Name("TestClip"), clipId);
    clip->SetFrameRate(30.f);
    clip->SetNumFrame(30);
    
    database->AddClip(clip, 30);
    
    // Create motion matching node
    MotionMatchingNode::PersistentData data;
    data.database = database;
    data.rootMotion = false;
    data.blendTime = 0.2f;
    data.searchInterval = 3;
    
    MotionMatchingNode node(data);
    
    // Initialize the node
    AnimContext context;
    node.InitAny(context);
    
    // Set desired velocity
    node.SetDesiredVelocity(Vector3(1.0f, 0.0f, 0.0f));
    
    // Tick the node
    AnimLayerContext layerContext;
    node.TickAny(layerContext, 0.016f); // 60 fps
    
    // The node should work without crashing
    ASSERT_TRUE(true);
}

TEST(MotionMatchingTest, MotionMatchingNodeBlendTest)
{
    // Create a skeleton
    SkeletonData skeletonData = {};
    skeletonData.boneData.emplace_back(BoneData{Name("Root")});
    skeletonData.refPos.emplace_back(Transform::GetIdentity());
    
    SkeletonPtr skeleton = Skeleton::BuildSkeleton(skeletonData);
    
    // Create a motion database
    auto database = std::make_shared<MotionDatabase>();
    
    // Create animation clips
    Uuid clipId1, clipId2;
    AnimClipPtr clip1 = new AnimationClip(Name("Clip1"), clipId1);
    clip1->SetFrameRate(30.f);
    clip1->SetNumFrame(30);
    
    AnimClipPtr clip2 = new AnimationClip(Name("Clip2"), clipId2);
    clip2->SetFrameRate(30.f);
    clip2->SetNumFrame(30);
    
    database->AddClip(clip1, 30);
    database->AddClip(clip2, 30);
    
    // Create motion matching node
    MotionMatchingNode::PersistentData data;
    data.database = database;
    data.rootMotion = false;
    data.blendTime = 0.2f;
    data.searchInterval = 1; // Search every frame
    
    MotionMatchingNode node(data);
    
    // Initialize
    AnimContext context;
    node.InitAny(context);
    
    // Create evaluation context
    AnimationEval evalContext(skeleton);
    
    // Evaluate pose
    node.EvalAny(evalContext);
    
    // Should have valid transforms
    ASSERT_EQ(evalContext.pose.transforms.size(), 1);
}
