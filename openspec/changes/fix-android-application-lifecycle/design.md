## Context

The original Android launcher derived from GameActivity 1.2.2. Its onKeyDown/onKeyUp implementation forwarded directly to native handlers. A later committed-gesture run also exposed a native text-input use-after-free after Activity recreation. This change adopts official stable GameActivity 4.4.2 and its native-owner lifecycle fix. The launcher targets SDK 33 and retains its existing AppCompat/AndroidX Back dispatcher. The bridge must preserve the actual platform decision to background or finish the Activity; a live PID proves neither that an Activity survived nor that shutdown failed.

The native application also owns callbacks referenced by later Android commands. An explicit exit or failed initialization can stop the game loop before DESTROY arrives. Separately, Activity recreation can construct another application in the same process while its process-shared metadata and code leases remain valid.

## Goals / Non-Goals

**Goals:** standard Back routing; symmetric background/resume; one finish request and one application shutdown during explicit exit; lifecycle drain through DESTROY with live callback owners; stable process metadata across same-process recreation.

**Non-Goals:** forcing root Back to terminate a process, calling killProcess/System.exit, upgrading unrelated AndroidX dependencies, adding a parallel native Activity owner or vendor patch, clearing the shared Environment, adding registration guards, changing physics/rendering behavior, or introducing modules/targets.

## Decisions

### Preserve the system Back policy

SkyEngineActivity handles only Back keys. A first legacy key-down starts tracking; repeated downs are consumed without navigation. A key-up navigates only when tracking remains valid and the event is not canceled. Other keys retain the existing native path. The action is sent through the existing AndroidX dispatcher, whose fallback reaches the platform Activity policy.

API 33+ uses an Activity-owned OnBackInvokedCallback at default priority and explicit manifest opt-in. A nested API-specific helper isolates those types from older devices. The callback forwards to the same AndroidX dispatcher and is unregistered before the Activity's superclass onDestroy. GameActivity 4.4.2's actual Maven POM does not select a newer AndroidX Activity: the retained AppCompat 1.3.0 path still uses Activity 1.2.3, so this explicit bridge remains necessary. It does not promise predictive system animations while a consuming callback is installed.

GameActivity 4.4.2 now calls superclass key handlers when native input declines an event, but a native filter that accepts Back still consumes it first. The explicit legacy Back route therefore remains at the launcher boundary. Calling finish directly would replace the system policy and is not used. The actual cached GameActivity/AppCompat/Fragment chain has no onBackPressed override; the existing AndroidX dispatcher fallback reaches android.app.Activity. HOME independently verifies the ordinary background/pause path.

### Separate renderer completion drain from native lifecycle wait

Ordinary paused/background operation retains WAIT polling, so it does not spin or advance game simulation. Resume reactivates the existing application and frame clock through the existing lifecycle path.

A business exit requests Activity finish once and stops/shuts down the application once. While RendererHost waits for CPU/GPU work to reach STOPPED, lifecycle pumping uses NON_BLOCKING mode so renderer completion is observed even if Android delivers no further event. After renderer/application shutdown completes, the native host may use normal WAIT(-1) while waiting for DESTROY, continuing to dispatch and acknowledge commands with a live owner. It must not spin solely because DESTROY is pending. Duplicate requests and a late DESTROY cannot trigger another shutdown, and no new game tick is needed for teardown.

The application and its callback owner remain alive through renderer drain and the subsequent native lifecycle wait. Initialization failure follows the same ownership boundary: request the platform close, preserve callbacks through late commands, then release the owner after DESTROY. No callback may retain a pointer to an already-destroyed stack object.

### Drain renderer terminal events completely

The host continues polling through a real empty result even after renderer state becomes STOPPED. RuntimeRenderer consumes successful internal CPU completions until it reaches a public event or actual CPU queue exhaustion, so those private completions cannot hide a later failure/stopped event behind an apparent zero. Existing private teardown/completion helpers carry this behavior; no public injection API or target is added.

### Keep metadata in its process definition

Built-in Core reflection belongs to creation of the process-shared serialization context. Its initialization factory receives that context explicitly; it must not recursively request the singleton while the singleton is being constructed. Module metadata continues to belong to the existing RegisterMeta process definition and its code leases.

Applications, repeated runtime sessions and test fixtures consume these definitions. They do not call Core reflection again or use Once/flags/existing-type checks to hide duplicate owners. Same-process recreation does not reset the Environment or discard definitions to make the next initialization succeed. Test executables/test environments remain the unique owners of any additional fixture metadata.

### Use the upstream native Activity owner

A new GameActivity can start before the previous Activity's OS destruction completes. Official [GameActivity 4.4.2](https://developer.android.com/jetpack/androidx/releases/games) is the single native lifecycle owner. Its actual classes.jar uses a static native handle and owner: onCreate synchronously terminates any previous NativeCode before initializing the replacement, and a later onDestroy/callback from the obsolete Activity does not own that replacement. This joins the old native application teardown before replacement initialization; no additional launcher session gate or PendingActivity lifecycle proxy remains.

The bundled GameTextInput implementation still uses shared process state; it is not described as per-instance. Correct serialization of its enclosing NativeCode lifetime prevents an old destructor from freeing text-input state already reused by a new Activity. The 4.4.2 insets callback also checks native ownership before calling JNI. The provisional local active flag, native-handle check, insets override, listener detach and replay are removed rather than kept as a second ownership protocol. No upstream native source is patched.

Main-role cleanup remains necessary for a replacement native thread. It uses an explicit, result-returning release with owner, pending-work and active-dispatch checks under one lock. Existing Clear keeps its drop behavior and other roles keep their current contract. Application teardown collects executor completions and Main notifications before/after unload, then clears its module manager and tick callback, collects the final notifications, and only then releases Main for the next native session. It does not reset the Environment or mask owner assertions.

## Risks / Trade-offs

- Back may background or finish the Activity while leaving the process alive: verify the actual Activity transition and corresponding pause or ordered shutdown rather than requiring one outcome or equating PID survival with failure.
- API 33+ gesture delivery differs from legacy key delivery: verify both paths on the actual packaged Activity, including returning to the existing task.
- DESTROY can arrive after exit or initialization failure: retain the native callback owner and test late command dispatch with shutdown counters and no dangling callbacks.
- A shared serialization context can outlive many application sessions: verify stable type identity and retained module leases instead of weakening duplicate-registration assertions.
- Sequential recreation does not prove overlapping clear-task handoff: keep the old and new native lifecycle owners distinct until the old engine session is released, and verify the exact device case.
- Full rendering/physics/package acceptance belongs to existing changes; this change validates lifecycle behavior and does not claim a new visual acceptance result.

## Migration and validation

Update the public launcher Java/manifest and the ignored AndroidPhysicsDemo fixture consistently, rebuild the existing package, and verify the installed package identity before device tests. Test ordinary Back and API 33+ gesture against observed system policy, HOME background/resume, explicit exit through DESTROY, same-process Activity recreation and initialization failure with late lifecycle delivery. Host regressions cover state transitions, terminal-event draining and metadata ownership. Record actual commands, logs, counts and coverage boundaries in verification.md.
