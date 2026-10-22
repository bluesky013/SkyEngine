## ADDED Requirements

### Requirement: Vulkan surface query negotiation budget

Vulkan SHALL query live capabilities and native drawable extent for checked surface validation, while reusing applied format, color space and present mode on a stable swapchain. Stable prepare and post-acquire validation SHALL perform no format or present-mode enumeration. Creation, recreation and native invalidation SHALL perform complete presentation-option negotiation. This SHALL preserve the checked acquisition and presentation retirement contract. The legacy direct protocol SHALL retain full-query behavior.

#### Scenario: Stable output
- **WHEN** an unchanged swapchain successfully renders a frame without native invalidation
- **THEN** prepare and post-acquire validation each query capabilities, with zero format/mode enumeration calls and unchanged creation revision

#### Scenario: Invalidation after successful acquire or present
- **WHEN** native acquire or present reports SUBOPTIMAL
- **THEN** the next surface validation re-enumerates format/color-space/mode options while retaining the successful image acquisition or queued presentation and all synchronization responsibilities

#### Scenario: Changed or unchanged negotiation
- **WHEN** invalidated negotiation succeeds
- **THEN** while selected options differ from the applied options, validation persistently reports OutOfDate until recreation; matching options clear the dirty state without rebuilding solely because of SUBOPTIMAL

#### Scenario: Failed negotiation is retried
- **WHEN** an invalidated format/mode query fails or returns INCOMPLETE
- **THEN** explicit failure or Busy is returned without a usable snapshot, and negotiation remains dirty for retry

#### Scenario: Resize and native out-of-date
- **WHEN** live capabilities change required extent, transform or other creation constraints, or native acquisition reports OUT_OF_DATE
- **THEN** existing retirement and recreation flow is used and recreated options are selected by full enumeration; capability-query failures and minimized windows never return stale Ready
