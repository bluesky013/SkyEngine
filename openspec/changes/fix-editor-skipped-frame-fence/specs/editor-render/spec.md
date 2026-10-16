## ADDED Requirements

### Requirement: Completion of skipped editor frames

Every editor BeginFrame that resets a frame fence SHALL be paired with a submission signaling that fence and an EndFrame, including when no viewport or command buffer is usable. The prior-frame wait SHALL occur before viewport maintenance that can replace swapchain resources.

#### Scenario: Lost or minimized main viewport
- **WHEN** BeginFrame has reset the current frame fence and the main viewport Begin fails or its extent is zero
- **THEN** the renderer SHALL submit a fence-bearing batch without acquiring or releasing an image
- **AND** a subsequent frame SHALL NOT wait for an unsignaled fence left by that skipped frame

#### Scenario: Acquire failure
- **WHEN** main viewport Acquire fails after BeginFrame
- **THEN** the renderer SHALL signal the frame fence through submission and end the frame
- **AND** it SHALL NOT release an image that was not acquired

### Requirement: Balanced editor acquired-image synchronization

Every successfully acquired main, preview, or floating image SHALL have its acquire semaphore consumed and render-done semaphore signaled by the editor frame submission before Release/presentation. This obligation SHALL remain when an acquired image has no usable backbuffer. Acquired windows SHALL share the same combined frame submission and frame fence.

#### Scenario: Successful acquire without a backbuffer
- **WHEN** a viewport Acquire succeeds but GetBackbuffer returns null
- **THEN** the renderer SHALL omit drawing for that viewport
- **AND** the frame submission SHALL still wait on its acquire semaphore and signal its render-done semaphore before releasing the acquired image

#### Scenario: Several acquired windows including an unusable image
- **WHEN** main, preview, and floating images are acquired in one frame and one cannot be drawn
- **THEN** one combined submission SHALL include the recorded command buffers and synchronization pairs for every acquired image
- **AND** every acquired image SHALL be released after that submission
- **AND** the frame context SHALL advance exactly once
