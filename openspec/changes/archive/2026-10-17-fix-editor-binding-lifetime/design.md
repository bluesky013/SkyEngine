## Context

ReflectedFormView owns fields and widgets while its host owns the model. The host can replace model storage before notifying the form, as with WorldConfigPanel::RefreshDocument after a world reload. A ColorPicker live preview captures a PropertyField descriptor and Close restores its opening value. The existing unconditional ResetInteractions in Bind/destruction therefore dereferences released model data. AssetViewer also owns model members while UIElement's base owns child forms, so default C++ destruction releases the models first.

## Decisions

### Explicit reset intent

ReflectedInteractionReset has Cancel and Abandon modes. Cancel requires a valid model and permits preview rollback. Abandon only drops callbacks, capture, and target pointers; it never reads or writes old data. All built-in stateful widgets receive the mode. ColorPicker::Abandon clears its callbacks without invoking the live callback; Close retains the existing rollback behavior.

Bind and Unbind use Abandon, and the destructor uses Abandon after removing notifier/history subscriptions. This makes binding replacement safe even after old model release. Refresh retains live-model cancellation because it rebuilds the current model and therefore already requires that model to remain valid. A host invalidating model storage must detach/rebind before any further input or synchronization; this is not automatic lifetime tracking of arbitrary raw pointers.

### Binding callback generation

Each form exposes a weak BindingLifetime token separate from its weak service-subscription token. Abandon expires it before resetting widgets. Cancel keeps it valid through rollback, then replaces it. Color preview/commit callbacks and wrapped inline commits check the captured token before using fields or their host. This guards copied callbacks against a later field generation while preserving synchronous successful commit/refresh and retained-widget dispatch.

Custom widgets must implement the reset-mode contract and use the binding token for callbacks retained outside their resettable state. A token is a single-threaded validity signal, not shared ownership of the model or synchronization across threads.

### Host replacement and destruction

- WorldConfigPanel::RefreshDocument abandons the old binding before querying or creating new config storage. Explicit document/provider replacement can therefore notify after old storage is gone. A selected-subsystem switch still has a live model, so it cancels before saving and rebinding.
- AssetViewer::Reload cancels and unbinds each form before assigning a new EditorCookSettings Any. Open and Close cancel live form interactions. The viewer destructor cancels while its owned models are valid, clears raw modal content/capture pointers, and destroys child elements in the destructor body before model members are destroyed.
- Preferences owns stable registry page models. Page switch/close explicitly cancel; snapshot restoration cancels before overwriting page values so an old rollback cannot overwrite the restored snapshot.
- Inspector and preset-provider Bind call sites may learn about a replacement after old storage release. They retain safe default abandonment, rather than assuming their old raw model pointer remains valid.

### Compatibility and scope

Implicit Bind/destructor rollback is deliberately replaced by safe abandonment. Existing callers requiring rollback before a valid-model replacement must use CancelInteractions explicitly. Custom ReflectedWidget::ResetInteraction overrides add a mode argument. Context service references still must outlive the form, and shared default services retain their cross-DLL singleton storage.

PropertyEditCommand still stores raw model targets. This change does not make old history safe after Reload or document destruction, and does not change shell history entry points. Those remain T04/T05.

## Verification

Persistent widget tests exercise live cancellation, normal Escape, abandon without old-model writes, released heap model Unbind/Bind/destruction, notification after detach, and binding-token expiration. Shell tests use a reflected color subsystem config and heap-backed cook settings to exercise actual host reload, close, and destruction paths. Run EditorWidgetsTest and EditorShellTest under ASan as well as the existing regression suites. Native windows/GPU behavior is unrelated to this CPU model-lifetime change.
