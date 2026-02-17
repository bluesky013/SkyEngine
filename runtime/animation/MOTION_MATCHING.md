# Motion Matching System

## Overview

This motion matching system provides advanced character animation through data-driven motion selection. It allows characters to naturally select and transition between animation clips based on desired movement characteristics.

## Components

### MotionDatabase

The `MotionDatabase` class stores a collection of animation clips and their extracted features for fast querying.

**Key Features:**
- Stores motion frames with extracted features (pose, velocity, trajectory)
- Supports multiple animation clips
- Configurable sampling rate for feature extraction

**Usage:**
```cpp
// Create database
auto database = std::make_shared<MotionDatabase>();

// Add animation clips
database->AddClip(walkClip, 30);  // Sample at 30 fps
database->AddClip(runClip, 30);
database->AddClip(jumpClip, 30);
```

### MotionMatcher

The `MotionMatcher` class performs the motion matching algorithm, finding the best matching frame in the database for a given query.

**Key Features:**
- Multi-factor cost calculation (pose, velocity, trajectory)
- Configurable weights for different matching factors
- Fast search through database

**Usage:**
```cpp
MotionMatcher matcher;
matcher.SetDatabase(database);

// Create query
MotionQuery query;
query.desiredRootVelocity = Vector3(1.0f, 0.0f, 0.0f);
query.desiredTrajectory = {Vector3(0.2f, 0.0f, 0.0f), Vector3(0.4f, 0.0f, 0.0f)};

// Find best match
MotionMatchResult result = matcher.FindBestMatch(query);
```

### MotionMatchingNode

The `MotionMatchingNode` is an animation graph node that integrates motion matching into the animation system.

**Key Features:**
- Automatic motion selection based on desired velocity and trajectory
- Smooth transitions between matched motions
- Configurable search interval for performance optimization
- Root motion support
- Blending between current and target animations

**Configuration:**
```cpp
MotionMatchingNode::PersistentData data;
data.database = database;          // Motion database
data.rootMotion = true;            // Enable root motion
data.blendTime = 0.2f;            // 200ms transition time
data.searchInterval = 3;           // Search every 3 frames

MotionMatchingNode node(data);
```

**Runtime Control:**
```cpp
// Set desired movement
node.SetDesiredVelocity(Vector3(2.0f, 0.0f, 1.0f));
node.SetDesiredTrajectory({
    Vector3(0.2f, 0.0f, 0.1f),
    Vector3(0.4f, 0.0f, 0.2f),
    Vector3(0.6f, 0.0f, 0.3f)
});

// Adjust parameters
node.SetBlendTime(0.3f);
node.SetSearchInterval(5);
```

## Motion Matching Algorithm

The motion matching algorithm works as follows:

1. **Feature Extraction**: When clips are added to the database, features are extracted at regular intervals:
   - Joint positions (relative to root)
   - Joint velocities
   - Root velocity
   - Future trajectory positions

2. **Query Construction**: At runtime, a query is constructed with:
   - Desired root velocity
   - Desired future trajectory
   - Current pose (optional)

3. **Matching**: The matcher compares the query against all frames in the database using a weighted cost function:
   ```
   Cost = (PoseCost × PoseWeight) + (VelocityCost × VelocityWeight) + (TrajectoryCost × TrajectoryWeight)
   ```

4. **Transition**: When a better match is found, the system smoothly blends from the current animation to the new matched animation over the specified blend time.

## Integration with Animation System

The MotionMatchingNode follows the standard AnimNode interface:

```cpp
// Initialize
AnimContext context;
node.InitAny(context);

// Update (per frame)
AnimLayerContext layerContext;
node.TickAny(layerContext, deltaTime);

// Evaluate pose
AnimationEval evalContext(skeleton);
node.EvalAny(evalContext);
```

## Performance Considerations

- **Search Interval**: Set higher values (e.g., 5-10 frames) for better performance
- **Database Size**: Larger databases provide more natural motion but increase search time
- **Blend Time**: Longer blend times are smoother but less responsive
- **Feature Weights**: Adjust weights to prioritize different aspects of matching

## Future Enhancements

Potential areas for improvement:

1. **Feature Extraction**: Full implementation of pose and velocity extraction from animation clips
2. **Spatial Acceleration**: Use spatial data structures (KD-tree, etc.) for faster searches
3. **Pose Mirroring**: Support for mirrored animations to reduce database size
4. **Constraint Matching**: Add support for environmental constraints (walls, obstacles)
5. **Multi-threaded Search**: Parallelize database search for better performance
6. **Animation Tagging**: Support for semantic tags to filter search space

## References

Motion matching is based on research from:
- "Motion Matching and The Road to Next-Gen Animation" (GDC 2016)
- "Learned Motion Matching" (ACM SIGGRAPH 2020)
