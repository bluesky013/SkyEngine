# editor-file-browser Specification

## Purpose
TBD - created by archiving change editor-file-browser-dialog. Update Purpose after archive.
## Requirements
### Requirement: Reusable file browser dialog
The editor SHALL provide a reusable, engine-drawn modal file browser dialog supporting three modes:
OpenFile, OpenProject, and SelectDirectory. The caller SHALL supply a title, an initial directory, filters,
and (for directory mode) a default name, and SHALL receive the chosen path on accept.

#### Scenario: Open file
- **WHEN** the dialog is opened in OpenFile mode and the user selects an existing file and confirms
- **THEN** the dialog SHALL report the selected file path to the caller and close

#### Scenario: Open project
- **WHEN** the dialog is opened in OpenProject mode
- **THEN** the listing SHALL be filtered to `*.skyproj` and confirming SHALL return the chosen project file

#### Scenario: Select directory
- **WHEN** the dialog is opened in SelectDirectory mode and the user navigates to a directory and confirms
- **THEN** the dialog SHALL report the current directory combined with the entered name

#### Scenario: Cancel
- **WHEN** the user presses Cancel or Esc
- **THEN** the dialog SHALL close and report no selection

### Requirement: Directory navigation and listing
The dialog SHALL display the entries of the current directory and allow navigation into subdirectories and
to a parent directory; it SHALL be UI-toolkit-free in its model and testable headlessly.

#### Scenario: Navigate
- **WHEN** the user activates a subdirectory entry
- **THEN** the listing SHALL show that directory's entries and the location SHALL update

#### Scenario: Parent
- **WHEN** the user activates the parent entry
- **THEN** the listing SHALL show the parent directory's entries

### Requirement: Rename and context menu
The browser SHALL provide a context menu on list entries. In write mode the menu SHALL offer New Folder; with an
entry targeted it SHALL offer Rename. Renaming SHALL edit the entry's name through the name field, commit on
Enter/Open (or blur), and cancel on Esc, reselecting the renamed entry. Creating a folder SHALL enter rename mode
on the new folder so its name can be changed immediately.

#### Scenario: Rename via context menu
- **WHEN** the user right-clicks an entry and chooses Rename
- **THEN** the name field SHALL be prefilled with the entry name and selected for editing

#### Scenario: Commit rename
- **WHEN** the user confirms the edited name
- **THEN** the entry SHALL be renamed in the current location and reselected

#### Scenario: Cancel rename
- **WHEN** the user presses Esc during rename
- **THEN** the original name SHALL be kept

#### Scenario: Create then rename
- **WHEN** New Folder is activated
- **THEN** a folder SHALL be created and the name field SHALL enter rename mode for it

### Requirement: Create folder
The browser SHALL provide a New Folder action that creates a directory in the current location (using a
unique name when none is given), refreshes the listing, selects it, and puts its name in the name field.
Sources that are read-only MAY decline.

#### Scenario: Create
- **WHEN** the user activates New Folder
- **THEN** a new directory SHALL be created in the current location, appear in the listing, and be selected

#### Scenario: Unsupported source
- **WHEN** the active source does not support creation
- **THEN** the action SHALL report the error and leave the listing unchanged

### Requirement: Extension filters
The dialog SHALL restrict OpenFile/OpenProject listings and acceptance to the configured extensions, while
still allowing directory navigation.

#### Scenario: Filtered listing
- **WHEN** OpenProject is configured with the `*.skyproj` filter
- **THEN** the listing SHALL show directories and only files matching the filter

### Requirement: Modal input
While the dialog is open it SHALL capture pointer and keyboard input, and the editor content behind it
SHALL NOT receive input.

#### Scenario: Captured
- **WHEN** the dialog is open and the user clicks or types
- **THEN** the input SHALL be handled by the dialog only

### Requirement: Pluggable location source
The browser SHALL be backed by a pluggable location source (`IFileBrowserSource`) rather than being
filesystem-specific, so the same dialog also serves asset selection. A filesystem source SHALL be the
default; the source SHALL provide navigation, listing, and optional sidebar places, and MAY annotate
entries with an asset type id.

#### Scenario: Filesystem default
- **WHEN** no source is supplied
- **THEN** the browser SHALL browse the filesystem using a built-in filesystem source

#### Scenario: Asset source reuse
- **WHEN** an asset source is supplied
- **THEN** the browser SHALL list its entries and expose each entry's asset type id to filtering

### Requirement: Extension and asset-type filters
Filters SHALL match entries by file extension and/or by asset type id. The dialog SHALL expose a filter
selector that switches the active filter, and directory entries SHALL always remain visible.

#### Scenario: Filter by extension
- **WHEN** the `*.skyproj` filter is active in OpenProject mode
- **THEN** only directories and `*.skyproj` files SHALL be listed

#### Scenario: Filter by asset type
- **WHEN** a filter listing asset types is active
- **THEN** only directories and entries whose asset type id matches SHALL be listed

#### Scenario: Switch to all files
- **WHEN** the user selects the "All Files" filter
- **THEN** entries of every extension SHALL be listed

### Requirement: Navigation affordances
The dialog SHALL present a places sidebar of shortcuts, an editable location/toolbar with an up action, a
columned listing (name and type), a name field, and confirm/cancel actions.

#### Scenario: Places shortcut
- **WHEN** the user activates a sidebar place
- **THEN** the listing SHALL show that place's location

#### Scenario: Type column
- **WHEN** the listing is shown
- **THEN** directories SHALL read as "Folder" and files SHALL show their type (extension or asset type)

