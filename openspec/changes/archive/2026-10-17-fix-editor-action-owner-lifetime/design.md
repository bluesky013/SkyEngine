## Context

EditorActionRegistry is process-shared and returns copyable EditorAction records. EditorShell registers five actions with lambdas that address its assetViewerElement. Default shell destruction left those callbacks in the registry. The full ASan shell run reached a stale enabled predicate from a later standalone viewer's ActionBarBuilder call.

## Decision

Each shell exclusively owns a shared lifetime signal. Its destructor resets that signal before member/child teardown. Registry handlers and their copied snapshots capture only a weak signal plus the guarded raw shell pointer. Every invoke checks expiration before reading the shell; every enabled predicate returns false for an expired owner. Open/Save/Close preserve their existing enabled behavior while the owner is live, while Undo/Redo additionally evaluate current viewer history.

The handlers do not lock and retain the lifetime signal while invoking external behavior: a retained signal must not make a destroyed owner appear live to a reentrant callback. All registry/action dispatch remains on the existing editor UI thread; this is not concurrent destruction synchronization.

A live shell can also rebuild its child tree while its registry callbacks remain valid. Rebuild clears assetViewerElement before destroying that tree, so enabled predicates see no target while chrome is reconstructed and later address the newly created viewer. The shell-lifetime guard alone would not protect this temporary target replacement.

The destructor deliberately does not Clear the registry or Remove actions by shared scope/id. Another shell may already have replaced those entries, and old snapshots can survive even after removal. Weak invalidation protects both current entries and snapshots without deleting a newer owner's actions. Likewise this change does not blindly Pop a shared same-name scope; scope-instance ownership is separate work.

Module unload must still ensure callbacks implemented in that DLL are unregistered before unloading their code. The weak object signal addresses shell destruction while its module remains loaded.

## Verification

EditorActionOwnerLifetimeTest retains action snapshots after destroying a shell, evaluates all built-in enabled predicates and invoke functions, and opens an independent viewer against the retained process registry without test-only global clearing. A second regression registers a newer shell's same-ID actions, rebuilds that live shell with its viewer open, then destroys the older shell; the old callbacks become inert while the new Close action remains registered and operates on the newer viewer. Run both with the complete EditorShellTest suite under ASan so earlier registrations and later consumers share realistic process state.
