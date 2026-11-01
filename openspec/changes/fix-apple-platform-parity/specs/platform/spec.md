## ADDED Requirements

### Requirement: Cocoa inactive polling waits without losing native service work

Cocoa SHALL honor WAIT_IF_INACTIVE when all frame-producing windows are inactive, and SHALL continue to process native events, timer/main-queue work, restore and exit requests. NON_BLOCKING and already-requested exit SHALL return without an inactive wait. Cross-thread quit requests SHALL wake the waiting event pump.

#### Scenario: Minimize all windows then restore
- **WHEN** the main loop polls with WAIT_IF_INACTIVE and every window is minimized
- **THEN** polling SHALL wait for native work with bounded service latency and resume when frame activity returns

#### Scenario: Drain without blocking
- **WHEN** a caller uses NON_BLOCKING during inactive or shutdown work
- **THEN** polling SHALL drain queued events and return without waiting for a future event

### Requirement: UIKit dialog native regression covers terminal ownership

UIKit native regression SHALL exercise the actual presented single-line/multiline/password editing UI, committed Unicode scalar limits, marked-text exclusion, confirmation/cancellation, background/scene loss and reentrant reopening. Each accepted request SHALL produce exactly one terminal event on its owner thread. Main-window teardown SHALL explicitly complete an active request; stale controller callbacks SHALL NOT terminate a newer request.

#### Scenario: Commit IME text at the scalar limit
- **WHEN** marked text is composed and then committed in a native editor with a Unicode scalar limit
- **THEN** preedit SHALL not emit CHANGED, committed text SHALL respect the limit, and confirmation SHALL report the latest accepted snapshot

#### Scenario: Close with an active dialog and reopen
- **WHEN** the main window or connected scene is closed with a dialog active
- **THEN** its request SHALL terminate once and a later valid session SHALL accept another request without old callbacks affecting it
