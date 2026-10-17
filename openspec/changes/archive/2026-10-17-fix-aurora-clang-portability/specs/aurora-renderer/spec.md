## ADDED Requirements

### Requirement: Aurora renderer sources build under Clang and GCC

Aurora renderer sources SHALL compile with Clang and GCC as well as MSVC. Member-template calls in dependent contexts SHALL use an explicit non-dependent type or the `template` disambiguator; relying on MSVC's permissive parsing SHALL NOT be treated as portable.

#### Scenario: Clang full build

- **WHEN** the full project is configured and built with AppleClang
- **THEN** the Aurora renderer targets SHALL compile without dependent-template-name errors

#### Scenario: Dependent read call

- **WHEN** a view iterates rows and calls the `Read<T>` member template with a dependent type pack
- **THEN** the call SHALL be written so Clang parses it as a template invocation

#### Scenario: Dependent registry call

- **WHEN** a render command applies component operations through a registry obtained inside a generic lambda
- **THEN** `Add<T>`, `Get<T>`, and `Pool<T>` calls SHALL parse unambiguously under Clang and GCC

#### Scenario: No behavior change

- **WHEN** the portability fix is applied
- **THEN** renderer behavior and public interfaces SHALL remain unchanged
