## ADDED Requirements

### Requirement: iOS Jolt consumer RTTI parity

The pinned iOS Jolt Debug and Release SDK builds SHALL enable C++ RTTI to match the existing adaptor subclasses. C++ exceptions SHALL remain disabled, and the change SHALL NOT introduce RTTI downcasts or alter other platforms' effective build options.

#### Scenario: Link adaptor subclasses on an iOS device build

- **WHEN** the actual iOS application links the adaptor's group filter and geometry collector against the rebuilt SDK
- **THEN** their Jolt base-class typeinfo SHALL resolve without disabling RTTI in the application or adaptor
