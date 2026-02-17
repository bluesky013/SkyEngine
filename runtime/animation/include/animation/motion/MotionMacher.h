//
// Created by Zach Lee on 2025/6/3.
//

#pragma once

#include <animation/motion/MotionDatabase.h>
#include <core/math/Vector3.h>

namespace sky {

    // Motion query describes what we're looking for in the database
    struct MotionQuery {
        // Current joint positions (for pose matching)
        std::vector<Vector3> currentJointPositions;
        // Current joint velocities
        std::vector<Vector3> currentJointVelocities;
        // Desired root velocity
        Vector3 desiredRootVelocity;
        // Desired future trajectory
        std::vector<Vector3> desiredTrajectory;
        
        // Weight factors for different features
        float poseWeight = 1.0f;
        float velocityWeight = 1.0f;
        float trajectoryWeight = 2.0f;
    };

    // Motion matching result
    struct MotionMatchResult {
        size_t frameIndex;      // Index in the motion database
        float cost;             // Match cost (lower is better)
        bool valid;             // Whether the result is valid
        
        MotionMatchResult() : frameIndex(0), cost(0.f), valid(false) {}
    };

    // Motion matcher finds the best matching frame in the database
    class MotionMatcher {
    public:
        MotionMatcher() = default;
        ~MotionMatcher() = default;

        // Set the motion database to search
        void SetDatabase(const MotionDatabasePtr& db);
        
        // Find the best matching frame for the given query
        MotionMatchResult FindBestMatch(const MotionQuery& query) const;
        
        // Calculate cost between query and a database frame
        float CalculateCost(const MotionQuery& query, const MotionFeature& feature) const;

    private:
        // Calculate pose matching cost
        float CalculatePoseCost(const MotionQuery& query, const MotionFeature& feature) const;
        
        // Calculate velocity matching cost
        float CalculateVelocityCost(const MotionQuery& query, const MotionFeature& feature) const;
        
        // Calculate trajectory matching cost
        float CalculateTrajectoryCost(const MotionQuery& query, const MotionFeature& feature) const;
        
        MotionDatabasePtr database;
    };

} // namespace sky
