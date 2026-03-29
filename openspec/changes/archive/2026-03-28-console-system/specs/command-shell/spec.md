## ADDED Requirements

### Requirement: CommandShell executes CVar get
The CommandShell SHALL interpret a single-token input matching a CVar name as a get operation, returning the current value with type and description.

#### Scenario: Query a CVar value
- **WHEN** the user inputs `r.ShadowQuality` (no arguments)
- **THEN** CommandShell SHALL return `CommandResult{OK, "r.ShadowQuality = 2  (int, \"Shadow quality level\")"}` or equivalent formatted output

### Requirement: CommandShell executes CVar set
The CommandShell SHALL interpret a two-token input where the first token matches a CVar name as a set operation, passing the second token to `SetFromString()`.

#### Scenario: Set a CVar value
- **WHEN** the user inputs `r.ShadowQuality 3`
- **THEN** CommandShell SHALL call `SetFromString("3")` and return output like `"r.ShadowQuality = 3 (was: 2)"`

#### Scenario: Set fails due to READ_ONLY
- **WHEN** the user inputs a set command on a READ_ONLY CVar
- **THEN** CommandShell SHALL return `CommandResult{ERROR, "r.BuildVersion is read-only"}`

### Requirement: CommandShell executes command functions
The CommandShell SHALL interpret the first token as a command name lookup. If found, the remaining tokens are passed as `CommandArgs` to the command handler.

#### Scenario: Execute help command
- **WHEN** the user inputs `help r`
- **THEN** the `help` command handler SHALL receive args `["r"]` and return a listing of all CVars/commands in category `"r"`

### Requirement: CommandShell reports unknown commands
If the first token matches neither a CVar name nor a registered command, CommandShell SHALL return `CommandResult{NOT_FOUND, "Unknown command: <name>"}`.

#### Scenario: Unknown command
- **WHEN** the user inputs `foobar 123`
- **THEN** CommandShell SHALL return `{NOT_FOUND, "Unknown command: foobar"}`

### Requirement: CommandShell exec file
The CommandShell SHALL provide `ExecFile(path)` which reads a `.cfg` file and executes each line as a command. Lines starting with `//` (after optional whitespace) SHALL be skipped as comments. Empty or whitespace-only lines SHALL be skipped.

#### Scenario: Execute a cfg file
- **WHEN** `ExecFile("startup.cfg")` is called and the file contains:
  ```
  // shadow setup
  r.ShadowQuality 3
  r.ShadowDistance 150.0

  // debug
  r.Wireframe 0
  ```
- **THEN** three commands SHALL be executed and the result SHALL report `"Executed 3 commands from startup.cfg"`

#### Scenario: Cfg file not found
- **WHEN** `ExecFile("nonexistent.cfg")` is called
- **THEN** the result SHALL be `CommandResult{ERROR, "File not found: nonexistent.cfg"}`

### Requirement: Output formatting
CommandShell SHALL format output as plain text. CVar query output SHALL include name, current value, type, and description. Error output SHALL be prefixed for clear identification.

#### Scenario: CVar query format
- **WHEN** querying a float CVar with value 100.0 and description "Shadow draw distance"
- **THEN** the output SHALL contain the CVar name, value `100`, type `float`, and description text

### Requirement: Built-in help command
CommandShell SHALL register a `help` command. When called with no arguments, it SHALL list all non-hidden CVars and commands. When called with a category prefix, it SHALL list only entries in that category.

#### Scenario: Help with category filter
- **WHEN** the user inputs `help phys`
- **THEN** the output SHALL list only CVars/commands whose category is `"phys"`

#### Scenario: Help with specific command name
- **WHEN** the user inputs `help sys.exit`
- **THEN** the output SHALL show the description for `sys.exit`

### Requirement: Built-in find command
CommandShell SHALL register a `find` command that searches CVar/command names and descriptions by substring match (case-insensitive).

#### Scenario: Find by substring
- **WHEN** the user inputs `find shadow`
- **THEN** the output SHALL list all CVars/commands containing "shadow" in their name or description

### Requirement: Built-in reset command
CommandShell SHALL register a `reset` command that restores a CVar to its default value.

#### Scenario: Reset a CVar
- **WHEN** the user inputs `reset r.ShadowQuality` and the default is 2
- **THEN** the CVar SHALL be set to 2 and the output SHALL indicate the reset

### Requirement: Built-in echo command
CommandShell SHALL register an `echo` command that concatenates its arguments with spaces and returns them as output.

#### Scenario: Echo text
- **WHEN** the user inputs `echo hello world`
- **THEN** the output SHALL be `"hello world"`
