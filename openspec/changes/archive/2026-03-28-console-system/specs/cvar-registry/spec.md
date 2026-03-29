## ADDED Requirements

### Requirement: CVar typed declaration and auto-registration
The system SHALL provide a `CVar<T>` template class supporting types: `bool`, `int`, `float`, `double`, `std::string`. A `CVar<T>` instance SHALL automatically register itself with `CommandRegistry` upon construction and unregister upon destruction.

#### Scenario: Declare a CVar as a static variable
- **WHEN** a module declares `static CVar<int> r_shadowQuality("r.ShadowQuality", 2, "Shadow quality level")`
- **THEN** `CommandRegistry::Get()->FindCVar("r.ShadowQuality")` SHALL return a non-null `ICVar*` pointing to that CVar

#### Scenario: CVar destruction triggers unregistration
- **WHEN** a CVar goes out of scope or its owning module is unloaded
- **THEN** `CommandRegistry::Get()->FindCVar()` for that name SHALL return `nullptr`

### Requirement: CVar value storage is decentralized
Each `CVar<T>` SHALL store its value directly in its own memory. `CVar<T>::Get()` SHALL return a const reference to the local value with no indirection through the registry.

#### Scenario: Hot-path read performance
- **WHEN** code calls `cvar.Get()` in a per-frame loop
- **THEN** the read SHALL resolve to a direct memory access with no map lookup or pointer chase through CommandRegistry

### Requirement: CVar main-thread-write contract
`CVar<T>::Set()` and `CVar<T>::SetFromString()` SHALL only be called from the main thread. `CVar<T>::Get()` MAY be called from any thread.

#### Scenario: Set from console input on main thread
- **WHEN** the user types `r.ShadowQuality 3` in the console and CommandShell processes it during `ConsoleModule::Tick()`
- **THEN** `CVar::SetFromString("3")` is called on the main thread, and `Get()` returns 3 afterward

### Requirement: CVar flags
Each CVar SHALL support a `CVarFlags` bitmask with at least: `NONE`, `READ_ONLY`, `CHEAT`, `ARCHIVE`, `HIDDEN`, `REQUIRES_RESTART`.

#### Scenario: READ_ONLY CVar rejects writes
- **WHEN** `SetFromString()` is called on a CVar with `READ_ONLY` flag
- **THEN** the call SHALL return `false` and the value SHALL remain unchanged

#### Scenario: HIDDEN CVar excluded from Tab completion
- **WHEN** `CommandRegistry::FindByPrefix()` is called
- **THEN** CVars with the `HIDDEN` flag SHALL NOT appear in the results

### Requirement: CVar onChange callback
Each `CVar<T>` SHALL expose an `onChange` delegate of type `Delegate<void(const T& oldVal, const T& newVal)>`. The delegate SHALL be invoked whenever `Set()` or `SetFromString()` successfully changes the value.

#### Scenario: Render system reacts to CVar change
- **WHEN** a CVar's value changes from 2 to 3
- **THEN** the connected `onChange` delegate SHALL be called with `oldVal=2, newVal=3`

#### Scenario: No callback on same-value set
- **WHEN** `Set()` is called with the same value as current
- **THEN** the `onChange` delegate SHALL NOT be invoked

### Requirement: CVar string conversion
`ICVar` SHALL provide `ToString()` and `SetFromString()` methods. `ToString()` SHALL produce a human-readable representation. `SetFromString()` SHALL parse the string and return `true` on success, `false` on parse failure.

#### Scenario: Bool CVar accepts multiple representations
- **WHEN** `SetFromString("true")`, `SetFromString("1")`, or `SetFromString("on")` is called on a `CVar<bool>`
- **THEN** each SHALL set the value to `true` and return `true`

#### Scenario: Invalid string for int CVar
- **WHEN** `SetFromString("abc")` is called on a `CVar<int>`
- **THEN** the call SHALL return `false` and the value SHALL remain unchanged

### Requirement: CVar metadata
Each `ICVar` SHALL expose: `GetName()`, `GetDesc()`, `GetCategory()` (derived from dot prefix, e.g. `"r"` from `"r.ShadowQuality"`), `GetTypeName()` (e.g. `"int"`, `"float"`, `"bool"`, `"string"`), and `GetFlags()`.

#### Scenario: Category derived from name
- **WHEN** a CVar is registered with name `"phys.Gravity"`
- **THEN** `GetCategory()` SHALL return `"phys"`

#### Scenario: Name without dot has empty category
- **WHEN** a CVar is registered with name `"debugMode"`
- **THEN** `GetCategory()` SHALL return `""`

### Requirement: CommandRegistry singleton
`CommandRegistry` SHALL be a `Singleton<CommandRegistry>`. It SHALL provide: `RegisterCVar()`, `UnregisterCVar()`, `RegisterCommand()`, `UnregisterCommand()`, `FindCVar()`, `FindCommand()`, `FindByPrefix()`, `ForEachCVar()`, `ForEachCommand()`.

#### Scenario: Register and find a command function
- **WHEN** `RegisterCommand("sys.exit", "Exit engine", "sys", handler)` is called
- **THEN** `FindCommand("sys.exit")` SHALL return the registered command entry

#### Scenario: Prefix search
- **WHEN** three CVars `r.ShadowQuality`, `r.ShadowDistance`, `r.Wireframe` are registered and `FindByPrefix("r.Shadow")` is called
- **THEN** the result SHALL contain exactly `"r.ShadowQuality"` and `"r.ShadowDistance"`

### Requirement: Command function registration
The system SHALL support registering stateless command functions with `RegisterCommand(name, description, category, CommandFunc)` where `CommandFunc = std::function<CommandResult(CommandArgs)>`.

#### Scenario: Execute a registered command
- **WHEN** `FindCommand("echo")` returns a valid entry and its handler is called with args `["hello", "world"]`
- **THEN** the handler SHALL return `CommandResult{Status::OK, "hello world"}`

### Requirement: CommandToken tokenizer
The system SHALL provide a tokenizer that splits a command string into tokens. Tokens are separated by whitespace. Quoted strings (double quotes) SHALL be treated as a single token.

#### Scenario: Simple tokenization
- **WHEN** tokenizing `r.ShadowQuality 3`
- **THEN** the result SHALL be `["r.ShadowQuality", "3"]`

#### Scenario: Quoted string token
- **WHEN** tokenizing `echo "hello world"`
- **THEN** the result SHALL be `["echo", "hello world"]`
