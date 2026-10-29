## Implementation
- [x] Inspect analysis evidence and verify the handoff.
- [x] Create the independent PartySoftBody project using the standard generator.
- [x] Implement and test the existing-host runtime scene-source contract.
- [x] Implement independent seven-point solver and fixed-step project gameplay.
- [x] Add procedural geometry, owned rendering updates and interactive controls.
- [x] Inspect the complete script pack and patch overrides; trace main restitution, extra collision body and visual soft-body entry points.
- [x] Load static actors, lights and Physics configuration from the defaultScene World; dynamically create character visual actors in the same World.
- [x] Build real engine and project targets; run numerical and native smoke checks.
- [x] Document controls, evidence, approximations and validation.
- [x] Decode the original NeoX 5.0.4 character mesh, validate its geometry against GIM metadata, export OBJ and retain skin records.
- [x] Replace prototype character spheres with the imported mesh while preserving World, physics and CPU cage deformation.
- [x] Trace the original collision handoff, script status transitions, decoded punch thresholds and native velocity-difference calculation; record evidence and unresolved extra-body response semantics.
- [x] Add multi-step numerical checks and bounded native separation diagnostics for the experimental response.
- [x] Implement a first source-informed control loss/recovery and velocity handoff, with native recovery and extra-body approximations explicitly documented.
- [x] Verify the first implementation interactively, including rolling separation and normal/rolling replay controls.

- [x] Add close-up pair, wall/slide and landing demonstrations, opposing facing and independent soft cage/body tuning.
- [x] Add a project tool module with a reflected, undoable top-left tuning panel.
- [x] Raise the existing directional shadow map to 2048x2048 at the user's request.
- [x] Complete native showcase, live parameter editing, shared widget/history and shadow regression verification; record native keyboard-undo verification limits.

- [x] Limit wall compression through project extra-probe support, preserve sliding/escape and verify at 60/120 Hz plus native replay.

- [x] Preserve changed-scene resource subscriptions while viewports are suspended; reproduce and verify recovery with GPU pixel tests.

- [x] Import penguin/goose/peacock GLB bind meshes and model-specific spring/constraint profiles into a separate World2.
- [x] Add model switching, rig debug display and gameplay input takeover for jump/movement; build the preview and numerical regressions.
- [x] Complete all-model native collision/landing coverage, including the added Party model.
- [x] Confirm manual keyboard jump through native airborne capture and executed JUMP records during the user's interactive run.
- [x] Add peacock abdomen/head probes with local head contacts, fit the actual head influence region, and verify numerical constraints and native sphere placement.
- [x] Add Party selection on key 8 and fitted penguin/goose head probes with local center offsets; build and run the four-model native regression.
- [x] Recenter the goose main controller/abdomen probe on the actual abdomen, lower its center while preserving floor clearance, and shrink the head probe by 15%; verify geometry and native scenarios.
- [x] Add goose neck contact, graduated jelly response and reflected live rig inertia; validate movement reversal, settling, simultaneous segment contacts and four-model native cycling.
- [x] Fit bird cage contacts to abdomen dimensions, bound head/neck wall compression, remove depenetration launch velocity and tune restrained goose neck defaults; add numerical regressions.
- [x] Validate the final contact tuning through four-model 7,400-tick native regression and paused contact/recovery inspection; leave the goose wall preview running.
