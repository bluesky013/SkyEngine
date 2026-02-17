//
// Created by Zach Lee on 2025/6/3.
//

#include <animation/motion/MotionMatcher.h>
#include <limits>
#include <cmath>

namespace sky {

    void MotionMatcher::SetDatabase(const MotionDatabasePtr& db)
    {
        database = db;
    }

    MotionMatchResult MotionMatcher::FindBestMatch(const MotionQuery& query) const
    {
        MotionMatchResult result;
        
        if (!database || database->GetNumFrames() == 0) {
            return result;
        }
        
        float bestCost = std::numeric_limits<float>::max();
        size_t bestIndex = 0;
        
        // Search through all frames in the database
        const auto& frames = database->GetFrames();
        for (size_t i = 0; i < frames.size(); ++i) {
            float cost = CalculateCost(query, frames[i].feature);
            
            if (cost < bestCost) {
                bestCost = cost;
                bestIndex = i;
            }
        }
        
        result.frameIndex = bestIndex;
        result.cost = bestCost;
        result.valid = true;
        
        return result;
    }

    float MotionMatcher::CalculateCost(const MotionQuery& query, const MotionFeature& feature) const
    {
        float poseCost = CalculatePoseCost(query, feature);
        float velocityCost = CalculateVelocityCost(query, feature);
        float trajectoryCost = CalculateTrajectoryCost(query, feature);
        
        // Weighted sum of costs
        return poseCost * query.poseWeight + 
               velocityCost * query.velocityWeight + 
               trajectoryCost * query.trajectoryWeight;
    }

    float MotionMatcher::CalculatePoseCost(const MotionQuery& query, const MotionFeature& feature) const
    {
        if (query.currentJointPositions.empty() || feature.jointPositions.empty()) {
            return 0.f;
        }
        
        float totalCost = 0.f;
        size_t numJoints = std::min(query.currentJointPositions.size(), feature.jointPositions.size());
        
        for (size_t i = 0; i < numJoints; ++i) {
            Vector3 diff = query.currentJointPositions[i] - feature.jointPositions[i];
            totalCost += diff.LengthSquared();
        }
        
        return totalCost / static_cast<float>(numJoints);
    }

    float MotionMatcher::CalculateVelocityCost(const MotionQuery& query, const MotionFeature& feature) const
    {
        if (query.currentJointVelocities.empty() || feature.jointVelocities.empty()) {
            return 0.f;
        }
        
        float totalCost = 0.f;
        size_t numJoints = std::min(query.currentJointVelocities.size(), feature.jointVelocities.size());
        
        for (size_t i = 0; i < numJoints; ++i) {
            Vector3 diff = query.currentJointVelocities[i] - feature.jointVelocities[i];
            totalCost += diff.LengthSquared();
        }
        
        // Add root velocity cost
        Vector3 rootDiff = query.desiredRootVelocity - feature.rootVelocity;
        totalCost += rootDiff.LengthSquared();
        
        return totalCost / static_cast<float>(numJoints + 1);
    }

    float MotionMatcher::CalculateTrajectoryCost(const MotionQuery& query, const MotionFeature& feature) const
    {
        if (query.desiredTrajectory.empty() || feature.futureTrajectory.empty()) {
            return 0.f;
        }
        
        float totalCost = 0.f;
        size_t numPoints = std::min(query.desiredTrajectory.size(), feature.futureTrajectory.size());
        
        for (size_t i = 0; i < numPoints; ++i) {
            Vector3 diff = query.desiredTrajectory[i] - feature.futureTrajectory[i];
            totalCost += diff.LengthSquared();
        }
        
        return totalCost / static_cast<float>(numPoints);
    }

} // namespace sky
