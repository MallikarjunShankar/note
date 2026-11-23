# PROJECT SPECIFICATION
# Note: A Lightweight, Personal, Cross-Platform Code Editor

Location in repository: `NOTE/PROJECT_SPEC.md`

Items marked **[PROVISIONAL]** are proposed values awaiting confirmation by the project owner.
Items in Appendix A are **not in scope** until the project owner approves them.

------------------------------------------------------------

## 1. PROJECT OVERVIEW

Note is a lightweight, minimalistic, highly customisable text editor designed primarily for reliable everyday editing.

It provides a comfortable, responsive and distraction-free environment for programming without the complexity of a traditional IDE.

The product is fundamentally a text editor with programming-oriented capabilities. It is not intended to compile, execute, debug, analyse or manage software projects.

V1 core features:

- Smart indentation.
- Configurable bracket and quote completion.
- Matching bracket highlighting.
- Multiple cursors.
- Find and replace.
- Customisable keyboard shortcuts.
- Configurable commands.
- Session restoration.
- Crash recovery.
- Extensive visual customisation.

The application supports conventional mouse interaction and keyboard-only operation equally well.

Windows is the initial V1 distribution target. Linux and macOS remain supported by the platform-independent architecture, and cross-platform behaviour must be preserved; platform-specific functionality is isolated behind common interfaces.

The project has two primary priorities, in this order:

1. **Lightweight performance and minimalism.** Note should feel responsive and use resources efficiently.
2. **Extensive visual and behavioural customisation.** Every user should be able to make Note feel like their own personal editor rather than a generic tool.

Reliable document management and recovery, configurable commands and keyboard interaction, and coding conveniences are important, but must not compromise either priority.

The Language System (Phase 05 and System 05) is deferred until after V1. V1 does not implement language detection, language registries, syntax definitions, syntax highlighting or incremental highlighting. The customisation architecture may retain data structures for future syntax-colour categories, but V1 has no syntax-highlighting runtime.

**Conflict rule:** where a feature cannot meet the Performance Budget (section 7.0), the feature is simplified, made optional or not shipped. Performance wins.

------------------------------------------------------------

## 2. PRODUCT PHILOSOPHY

### 2.1 Lightweight First

Performance is a fundamental product requirement, measured against the budget in section 7.0.

The application should:

- Start quickly.
- Consume minimal memory while idle.
- Remain responsive during normal editing.
- Avoid unnecessary background processes.
- Avoid unnecessary dependencies.
- Avoid loading resources that are not needed.
- Handle larger source files without excessive performance degradation.
- Avoid unnecessary processing of unchanged document contents.

The application must not become resource-intensive merely because customisation and editing conveniences are available.

All major systems are designed with performance in mind from the first line of code.

### 2.2 Minimalistic Interface

The default interface is deliberately simple.

The main editing area occupies most of the available window.

The default interface contains:

- A conventional menu bar.
- A document tab bar.
- The main editing area.
- A compact status bar.

There are no permanently visible IDE-style sidebars or panels.

Additional interface elements appear only when requested or when required by an active operation.

The application must not overwhelm users with settings, controls, toolbars or panels that are not necessary for ordinary editing.

### 2.3 Customisability

Customisation is the second-highest product priority.

Users adapt the application's appearance and behaviour without requiring plugins or manually editing configuration files.

Customisation includes:

- A complete visual theme editor.
- Custom syntax colours.
- Custom interface colours.
- Font selection and sizing.
- Line spacing.
- Interface layout preferences.
- Configurable keyboard shortcuts.
- Configurable indentation.
- Configurable bracket pairing.
- Configurable editor decorations.

The default experience remains simple even though the underlying customisation system is comprehensive.

Customisation must be implemented so that it adds no meaningful runtime cost (see 7.7).

### 2.4 User Control

The editor assists users without unnecessarily interfering with their typing.

Automatic indentation and bracket completion are desirable.

Autocomplete, aggressive formatting and unsolicited code suggestions are not part of the agreed scope.

Editing behaviour is predictable and configurable.

------------------------------------------------------------

## 3. EXPLICIT PRODUCT SCOPE

The following sections establish the agreed feature set.

These decisions are fixed unless explicitly changed by the project owner.

### 3.1 Core Editing

The editor must support:

- Text insertion and deletion.
- Text replacement.
- Undo and redo.
- Multiple document tabs.
- Cursor navigation.
- Text selection.
- Mouse-based selection.
- Keyboard-based selection.
- Multiple cursors.
- Line numbers.
- Relative line numbers.
- Current-line highlighting.
- Indentation guides.
- Matching occurrence highlighting.
- Whitespace visibility.
- Word wrapping.
- Configurable indentation.

### 3.2 Smart Indentation

The editor must support automatic indentation.

Required behaviour:

- Preserve indentation when creating a new line.
- Increase indentation after opening braces.
- Decrease indentation when closing braces.
- Support tabs and spaces.
- Allow configurable indentation width.
- Support indentation of selected lines.
- Support outdenting of selected lines.

This is indentation management, not automatic code formatting.

The editor must not rearrange or reformat existing source code automatically.

### 3.3 Bracket and Quote Completion

Support automatic pairing for:

- Parentheses: ()
- Square brackets: []
- Curly braces: {}
- Double quotes: ""
- Single quotes: ''

Required behaviour:

- Automatically insert closing characters when enabled.
- Keep the cursor between newly inserted pairs.
- Skip existing closing characters when appropriate.
- Support automatic deletion of empty pairs.
- Highlight matching opening and closing brackets.
- Allow individual pair types to be enabled or disabled.
- Do not auto-pair a quote character typed directly after a word character (so apostrophes such as in "don't" are not doubled).

Pairing behaviour is configurable through application settings.

The implementation avoids interfering unnecessarily with strings, comments and existing text.

### 3.4 Syntax Highlighting

**Deferred until after V1.** This section describes post-V1 direction only. V1 provides plain-text editing and does not implement language detection, syntax definitions or syntax highlighting. The customisation data model may reserve syntax-colour category structures for future use, but no runtime highlighting implementation is required for V1.

### 3.5 Language Detection

**Deferred until after V1.** V1 does not detect languages or expose manual language selection. Open documents use plain-text editing without a language identity.

### 3.6 Search

The editor must support document-level search and replacement.

Required functionality:

- Find.
- Replace.
- Case-sensitive search.
- Whole-word matching.
- Regular expression search.
- Next match.
- Previous match.
- Match highlighting.

Search operates on the active document.

Cross-file and project-wide search are not included.

### 3.7 Navigation and Interaction

The application must support:

- Traditional menus.
- Keyboard shortcuts.
- Mouse interaction.
- Context menus.
- Searchable command palette.
- Configurable keyboard shortcuts.

Keyboard-only operation must be fully supported.

The command palette provides searchable access to registered application commands.

Vim-style modal editing is not included.

### 3.8 File Management

The application must support:

- Creating documents.
- Opening files.
- Saving files.
- Save As.
- Closing documents.
- Multiple open tabs.
- Tab switching.
- Unsaved-change indicators.
- Drag-and-drop file opening.
- Opening files in external applications.

A permanent file explorer is not included.

The application is primarily file-oriented rather than project-oriented.

### 3.9 Encoding and Line Endings

The editor handles file encoding and line endings automatically where possible.

Requirements:

- Detect file encoding.
- Detect line-ending format.
- Preserve existing encoding when saving where possible.
- Preserve LF or CRLF conventions.
- Use sensible defaults for new documents.
- Avoid unintended file-format changes.

### 3.10 Recovery and Session Management

The application must support:

- Restoring previously open tabs.
- Restoring tab order.
- Restoring the active document.
- Restoring cursor positions.
- Recovering unsaved document contents.
- Periodic recovery snapshots.
- Recovery after unexpected shutdown.

Recovery snapshots are stored separately from original files.

Recovered content must not automatically overwrite original files.

### 3.11 Customisation

The customisation system is a major part of the application.

It includes a complete visual theme editor.

**Themes versus settings.** Themes and settings are separate configuration categories.

- A **theme** defines colours. A theme may optionally include a font and spacing preset, which is applied as a suggestion into settings and never overrides explicit user choices.
- **Settings** own typography, layout and editing preferences.
- Themes are data-only configuration files.

The theme editor supports:

EDITOR COLOURS:
- Editor background.
- Default text.
- Cursor.
- Selection.
- Current-line highlight.
- Line-number colours.
- Indentation-guide colours.
- Whitespace indicator colours.

SYNTAX COLOUR DATA (post-V1 reserve):
- Future syntax categories may be represented in theme data structures.
- V1 does not implement a syntax-highlighting runtime or apply these categories.

INTERFACE COLOURS:
- Window background.
- Menu background.
- Tab background.
- Active tab.
- Inactive tabs.
- Status bar.
- Borders and separators.
- Text and icon colours.
- Other existing interface elements.

THEME FUNCTIONALITY:
- Built-in themes.
- Custom theme creation.
- Individual colour editing.
- Live preview.
- Theme saving.
- Theme importing.
- Theme exporting.
- Resetting theme values.
- Missing or invalid theme values fall back to defaults and never break the application.

TYPOGRAPHY (settings):
- Font family.
- Font size.
- Font weight.
- Line spacing.
- Editor zoom.

LAYOUT (settings):
- Status bar visibility.
- Menu configuration.
- Available interface elements.
- Layout preferences.
- Optional toolbar. The toolbar is hidden by default and, when enabled, its contents are configurable.

EDITING PREFERENCES (settings):
- Indentation style.
- Indentation width.
- Automatic pairing.
- Individual pairing settings.
- Whitespace visibility.
- Indentation guides.
- Current-line highlighting.
- Matching occurrence highlighting.
- Line-number settings.

KEYBOARD (settings):
- Default shortcuts.
- Custom keybindings.
- Shortcut conflict detection.
- Restoring default mappings.

A plugin runtime or executable theme system is not required.

### 3.12 External Application Integration

The application supports:

- Opening the active file in an external application.
- Opening the system terminal in the directory containing the active file.

The terminal functionality is implemented through an external application launch.

There is no embedded terminal or terminal emulator.

The terminal command is accessible through a configurable keyboard shortcut and the command system.

------------------------------------------------------------

## 4. EXPLICIT EXCLUSIONS

The following functionality is deliberately outside the project scope.

Do not implement these features unless the project owner explicitly changes the specification.

- Automatic code formatting.
- Integrated terminal.
- Debugger.
- Git integration.
- AI-assisted coding.
- Autocomplete.
- Word completion.
- Keyword suggestions.
- Intelligent code completion.
- Language servers.
- Code diagnostics.
- Build systems.
- Compilation systems.
- Project management.
- Permanent file explorer.
- Cross-file search.
- Project-wide search.
- Plugin marketplace.
- Plugin runtime.
- Vim-style modal editing.
- Code folding.
- Markdown preview.
- File comparison or diff viewer.
- Minimap.
- Integrated development environment functionality.
- Embedded browser runtimes (for example Electron or similar web-view shells). See 7.0.

The absence of these features is intentional.

Do not suggest implementing them as part of ordinary development tasks.

------------------------------------------------------------

## 5. SYSTEM ARCHITECTURE

The application is divided into ten primary systems.

Each system has a clearly defined responsibility.

Systems communicate through explicit interfaces rather than directly accessing each other's internal implementation.

### SYSTEM 01: APPLICATION CORE

PURPOSE:
Manage application startup, shutdown, bootstrap configuration and shared application state.

COMPONENTS:

1. Application Entry
   - Application entry point.
   - Initialisation.
   - Main application creation.
   - Startup error handling.

2. Lifecycle Manager
   - Startup sequence.
   - Shutdown sequence.
   - Resource cleanup.
   - System initialisation coordination.

3. Application State
   - Active document reference.
   - Application-wide state.
   - Shared service references.
   - Window state.

4. Configuration Loader (bootstrap only)
   - Locate configuration, theme and keybinding files.
   - Hand each file's contents to its owning manager (Settings Manager, Theme Manager, Keybinding Manager).
   - Supply defaults when files are missing.
   - Handle invalid configuration without crashing.
   - It does not persist, own or interpret settings. Ownership and persistence belong to the Customisation system.

5. Resource Manager
   - Manage loaded resources.
   - Release resources when no longer required.

BOUNDARIES:
The Application Core coordinates the application but does not implement document editing, rendering, settings ownership or filesystem operations.

### SYSTEM 02: USER INTERFACE

PURPOSE:
Provide the visual interface.

COMPONENTS:

1. Main Window
2. Menu Bar
3. Tab Bar
4. Editor View
5. Status Bar
6. Settings Interface
7. Context Menus
8. Drawing Surface Implementation (the GUI-framework-specific implementation of the drawing interface consumed by the Editor Renderer)

RESPONSIBILITIES:

- Render the main application window.
- Display open document tabs.
- Display the active editor.
- Display document information.
- Provide access to menus and settings.
- Support configurable interface visibility.
- Reflect the current theme.

BOUNDARIES:
The UI must not contain document storage, editing algorithms, file operations or syntax-highlighting logic.

It delegates those operations to their respective systems.

### SYSTEM 03: EDITOR ENGINE

PURPOSE:
Handle all text editing operations.

COMPONENTS:

1. Text Buffer
2. Cursor Manager
3. Selection Manager
4. Editing Controller
5. Indentation Manager
6. Bracket Manager
7. Undo/Redo Manager
8. Editor Renderer

RESPONSIBILITIES:

TEXT BUFFER:
- Store document contents.
- Insert text.
- Delete text.
- Replace text.
- Retrieve document content.
- Apply editing operations efficiently.

CURSOR MANAGER:
- Track cursor positions.
- Handle cursor navigation.
- Handle multiple cursors.
- Maintain cursor positions after edits.

SELECTION MANAGER:
- Character selection.
- Word selection.
- Line selection.
- Mouse selection.
- Keyboard selection.
- Multiple selections.

EDITING CONTROLLER:
- Coordinate editing operations.
- Process input.
- Coordinate indentation.
- Coordinate bracket pairing.
- Apply changes to the text buffer.

INDENTATION MANAGER:
- Automatic indentation.
- Brace-aware indentation.
- Tabs and spaces.
- Configurable width.
- Selected-line indentation.
- Selected-line outdentation.

BRACKET MANAGER:
- Automatic pairing.
- Matching bracket detection.
- Skip existing closing characters.
- Automatic pair deletion.
- Individual pairing configuration.

UNDO/REDO MANAGER:
- Record editing operations.
- Undo.
- Redo.
- Group related modifications.
- Support coherent multi-cursor undo operations.

EDITOR RENDERER:
- A thin layer that decides what to draw for the visible region: text, syntax information, line numbers, relative line numbers, current-line highlighting, indentation guides, matching occurrences, whitespace, selections and cursors.
- It produces drawing instructions for the visible viewport only, and issues them through a framework-agnostic Drawing Surface interface.
- The GUI-framework-specific implementation of that interface lives in the User Interface system (System 02), so the Editor Engine stays independent of the chosen GUI framework.

BOUNDARIES:
The Editor Engine does not read or write files directly.

It receives document buffers and editor preferences through defined interfaces.

Rendering must never modify the underlying document contents.

### SYSTEM 04: DOCUMENT MANAGEMENT

PURPOSE:
Manage open documents and file persistence.

COMPONENTS:

1. Document Model
2. Document Manager
3. Tab Manager
4. File Service (facade over File Reader, File Writer, Encoding Handler and Line Ending Handler; uses the System 10 Filesystem Adapter)
5. File Reader
6. File Writer
7. Encoding Handler
8. Line Ending Handler
9. External Modification Detector
10. Drag-and-Drop Handler

DOCUMENT MODEL:
Each document maintains:
- File path.
- File name.
- A post-V1 optional language identifier when the Language System is implemented.
- File encoding.
- Line-ending format.
- Unsaved-change state.
- A reference to its Text Buffer.
- A reference to its cursor and selection state (owned by the Cursor Manager and Selection Manager).
- A reference to its editing history (owned by the Undo/Redo Manager).

The Document Model holds references only. It never owns text, cursor or history data itself (see section 6).

DOCUMENT MANAGER:
- Create documents.
- Open documents.
- Close documents.
- Maintain document instances.
- Track active document.
- Coordinate saving.

TAB MANAGER:
- Track open tabs.
- Switch active tabs.
- Close tabs.
- Maintain tab order.
- Display modification indicators.

FILE SERVICE:
- The single entry point for all file persistence in the application.
- Delegates to File Reader and File Writer.
- Performs saves atomically: write to a temporary file in the same directory, then rename over the original. A failed or interrupted save must never corrupt or truncate the existing file.
- Reports failures to callers so that a failed save never marks a document as saved.

FILE READER:
- Read file contents.
- Obtain file metadata.
- Handle loading failures.

FILE WRITER:
- Save document contents.
- Preserve encoding.
- Preserve line endings.
- Handle save failures.

ENCODING HANDLER:
- Detect and preserve file encoding where possible.

LINE ENDING HANDLER:
- Detect LF and CRLF.
- Preserve the original convention.
- Apply defaults to new files.

EXTERNAL MODIFICATION DETECTOR:
- Detect changes made outside the editor.
- Use operating-system file events where available, otherwise check when the window regains focus and before saving. It must not poll continuously.
- Notify the user.
- Avoid silently overwriting external modifications.

DRAG-AND-DROP HANDLER:
- Accept dropped files.
- Open dropped files through the Document Manager.

BOUNDARIES:
Document Management owns document instances and file persistence.

It does not implement editing logic or syntax highlighting.

### SYSTEM 05: LANGUAGE SYSTEM

**DEFERRED UNTIL AFTER V1**

PURPOSE:
Identify document languages and provide syntax highlighting after the V1 scope is complete.

COMPONENTS:

1. Language Registry
2. Language Detector
3. Syntax Definitions
4. Syntax Highlighter
5. Highlighting Cache

LANGUAGE REGISTRY:
- Maintain supported language definitions.
- Provide language identifiers.
- Resolve language definitions.

LANGUAGE DETECTOR:
- Detect languages from file extensions using a data-driven mapping.
- Support manual language selection.
- Fall back to plain text.

SYNTAX DEFINITIONS:
Define supported syntax categories for each language as data.

SYNTAX HIGHLIGHTER:
- Identify syntax elements.
- Generate highlighting information.
- Highlight the visible viewport first, then process the remainder at idle priority.
- Process changed regions incrementally.
- Degrade or disable itself on files or lines that exceed the thresholds in 7.6.

HIGHLIGHTING CACHE:
- Maintain processed syntax information.
- Avoid unnecessary repeated processing.

BOUNDARIES:
The post-V1 Language System may handle language identification and visual syntax classification only.

It does not provide autocomplete, intelligent analysis, diagnostics, compilation or language-server functionality.

### SYSTEM 06: CUSTOMISATION

PURPOSE:
Manage application appearance and user preferences.

COMPONENTS:

1. Theme Manager
2. Theme Editor
3. Theme Storage
4. Settings Manager
5. Typography Manager (a settings domain managed by the Settings Manager)
6. Layout Manager (a settings domain managed by the Settings Manager)
7. Keybinding Manager

THEME MANAGER:
- Load themes.
- Apply themes.
- Maintain active theme.
- Provide visual properties to UI and renderer.

THEME EDITOR:
- Create themes.
- Modify individual colours.
- Provide live preview.
- Save modified themes.
- Reset values.

THEME STORAGE:
- Store built-in themes.
- Store custom themes.
- Import themes.
- Export themes.
- Validate theme data.

SETTINGS MANAGER:
- Maintain editor preferences.
- Maintain general preferences.
- Provide settings to other systems.
- Persist configuration.

TYPOGRAPHY MANAGER:
- Font family.
- Font size.
- Font weight.
- Line spacing.
- Zoom.

LAYOUT MANAGER:
- Status bar visibility.
- Optional toolbar (hidden by default) and its configuration.
- Menu configuration.
- Layout preferences.

KEYBINDING MANAGER:
- Default keyboard shortcuts.
- Custom shortcuts.
- Conflict detection.
- Default restoration.

BOUNDARIES:
Customisation provides configuration and visual properties.

It must not directly modify document contents or implement editing behaviour.

Themes (colours) and settings (typography, layout, editing, keybindings) remain separate configuration categories.

### SYSTEM 07: COMMAND AND INPUT

PURPOSE:
Provide a unified interaction system.

COMPONENTS:

1. Input Handler
2. Command Registry
3. Command Executor
4. Command Palette
5. Shortcut Dispatcher
6. Context Menu Handler

INPUT HANDLER:
- Process keyboard input.
- Process mouse input.
- Distinguish text input and commands.
- Forward editing events appropriately.

COMMAND REGISTRY:
Maintain available application commands.

COMMAND EXECUTOR:
Execute registered commands through the appropriate system interfaces.

COMMAND PALETTE:
- Search commands.
- Filter results.
- Display matching commands.
- Execute selected commands.

SHORTCUT DISPATCHER:
- Resolve keyboard shortcuts.
- Use configured keybindings.
- Invoke registered commands.

CONTEXT MENU HANDLER:
- Provide contextual mouse-accessible actions.

EXAMPLES OF COMMANDS:

FILE:
- New File.
- Open File.
- Save.
- Save As.
- Close Tab.

EDIT:
- Undo.
- Redo.
- Find.
- Replace.
- Select All.

VIEW:
- Toggle Line Numbers.
- Toggle Whitespace.
- Toggle Indentation Guides.

SETTINGS:
- Open Settings.
- Change Theme.
- Configure Shortcuts.

EXTERNAL:
- Open Terminal Here.
- Open in External Application.

BOUNDARIES:
The Command System coordinates actions but does not implement the underlying editing, file or platform functionality.

All interaction methods (menus, shortcuts, context menus, palette) invoke the same underlying commands. Because of this, the Command Registry is established before any menu item is added (see section 9).

### SYSTEM 08: SEARCH

PURPOSE:
Provide document-level search and replacement.

COMPONENTS:

1. Search Interface
2. Search Engine
3. Match Manager
4. Replace Engine
5. Search Options

SEARCH INTERFACE:
- Search input.
- Replacement input.
- Search controls.
- Match navigation.

SEARCH ENGINE:
- Find text.
- Case-sensitive matching.
- Whole-word matching.
- Regular expression matching.
- Work in bounded chunks so the interface never freezes.
- Guard regular expressions against catastrophic backtracking, for example with a match time or step limit, and report a clear message when a pattern is aborted.

MATCH MANAGER:
- Track match positions.
- Navigate matches.
- Communicate match positions to the renderer.

REPLACE ENGINE:
- Replace individual matches.
- Replace applicable matches.
- Apply modifications through the Text Buffer.
- Preserve undo/redo functionality.

SEARCH OPTIONS:
- Case sensitivity.
- Whole-word matching.
- Regular expression mode.

BOUNDARIES:
Search operates on the active document only.

No cross-file or project-wide search.

### SYSTEM 09: SESSION AND RECOVERY

PURPOSE:
Preserve application sessions and unsaved document contents.

COMPONENTS:

1. Session Manager
2. Recovery Manager
3. Snapshot Manager
4. Recovery Storage

SESSION MANAGER:
- Save open document information.
- Save tab order.
- Save active document.
- Save cursor positions (read from the Cursor Manager, not stored independently as live state).
- Restore previous sessions.

RECOVERY MANAGER:
- Identify unsaved documents.
- Recover unsaved content.
- Restore documents after crashes.
- Avoid automatically overwriting originals.

SNAPSHOT MANAGER:
- Create periodic snapshots on a single shared timer.
- Debounce snapshots and write only documents that have changed since their last snapshot.
- Avoid unnecessary repeated full-document writes.

RECOVERY STORAGE:
- Store recovery data separately.
- Maintain recoverable document states.
- Support retrieval during startup.

BOUNDARIES:
Session restoration and content recovery are separate responsibilities.

Normal file saving remains the responsibility of Document Management.

### SYSTEM 10: PLATFORM INTEGRATION

PURPOSE:
Isolate operating-system-specific functionality.

COMPONENTS:

1. Platform Adapter
2. File Dialog Adapter
3. Filesystem Adapter
4. External Application Launcher
5. Terminal Launcher
6. Path Resolver

PLATFORM ADAPTER:
Provide common interfaces for Windows, Linux and macOS.

FILE DIALOG ADAPTER:
- Native open-file dialogs.
- Native save-file dialogs.

FILESYSTEM ADAPTER:
- Filesystem access.
- Path operations.
- File metadata.
- Directory resolution.

EXTERNAL APPLICATION LAUNCHER:
- Open files through external applications.
- Use operating-system application associations.

TERMINAL LAUNCHER:
- Identify the active file directory.
- Resolve the directory.
- Launch the system terminal there.
- Handle operating-system differences.

PATH RESOLVER:
- Resolve file paths.
- Resolve parent directories.
- Handle platform-specific path conventions.

BOUNDARIES:
Platform-specific code must not leak into the Editor Engine, Language System or Customisation System.

------------------------------------------------------------

## 6. SYSTEM COMMUNICATION AND DATA OWNERSHIP

Every major type of application data has one authoritative owner.

The following ownership rules are mandatory.

DOCUMENT TEXT:
Owned by the Text Buffer.

CURSOR POSITIONS:
Owned by the Cursor Manager.

SELECTION STATE:
Owned by the Selection Manager.

UNDO/REDO HISTORY:
Owned by the Undo/Redo Manager.

OPEN DOCUMENTS (metadata and references):
Owned by the Document Manager. A document holds references to its buffer, cursor state and history. It does not own them.

OPEN TABS:
Owned by the Tab Manager.

LANGUAGE IDENTIFICATION:
Owned by the Language System.

SYNTAX INFORMATION:
Post-V1 ownership: the Syntax Highlighter and its cache.

THEME DEFINITIONS:
Owned by the Theme Manager.

USER PREFERENCES (including typography, layout and editing preferences):
Owned by the Settings Manager.

KEYBOARD SHORTCUTS:
Owned by the Keybinding Manager.

APPLICATION COMMANDS:
Owned by the Command Registry.

SESSION INFORMATION:
Owned by the Session Manager.

RECOVERY SNAPSHOTS:
Owned by the Recovery Manager.

FILE PERSISTENCE:
Owned by Document Management through the File Service.

BOOTSTRAP CONFIGURATION DISCOVERY:
Owned by the Configuration Loader. It only locates and forwards configuration. It does not own the resulting settings, themes or keybindings.

The same state must not be independently maintained by multiple systems.

Other systems consume state through defined interfaces.

------------------------------------------------------------

## 7. PERFORMANCE REQUIREMENTS

Performance is considered during implementation rather than treated as a final optimisation stage. A benchmark harness exists from Phase 02 onward and is run throughout development.

### 7.0 Performance Budget **[PROVISIONAL]**

Targets are measured on a **reference low-end device** to be named by the project owner (for example: a dual-core CPU of roughly 2015 vintage, 4 GB RAM, a spinning or slow SSD, and a standard-DPI display).

The numbers below are proposed placeholders and must be confirmed by the project owner:

| Metric | Target |
|---|---|
| Cold start to a window the user can type into | under 300 ms |
| Idle memory with one small file open | under 50 MB |
| Keystroke to screen update | under 16 ms (60 fps) |
| Scrolling | no dropped frames in a 50 MB file |
| Opening a 50 MB file | window remains responsive; content usable progressively |
| Theme or settings change | applied with no document-buffer recreation and no visible stall |

Budget rules:

- **Performance wins conflicts.** A feature that cannot meet the budget is simplified, made optional, or not shipped.
- **No embedded browser runtime.** The application must not be built on Electron or similar web-view shells. This is a constraint on the stack, not a stack choice.
- **Graceful degradation.** Above defined thresholds (file size and line length, values to be set during Phase 02 benchmarking), highlighting, matching-occurrence highlighting and similar costly features degrade or turn off automatically rather than stalling the editor. The user is told when this happens.
- **Idle means idle.** No timers, polling or background work when nothing has changed.
- **Regressions fail.** Benchmarks that exceed budget are treated as failing tests once the budget is confirmed.

### 7.1 Startup

The application initialises only the resources required to become operational.

Avoid unnecessary loading of:
- Unused resources.
- Unrelated themes.
- Optional UI components.
- Unnecessary background services.

Language definitions, built-in themes other than the active one, and settings screens are loaded lazily.

### 7.2 Editing

Typing remains responsive.

A small text modification must not trigger unnecessary full-document processing.

### 7.3 Rendering

Only update the necessary visual regions.

Only the visible viewport is laid out and drawn.

Avoid unnecessary rendering work.

### 7.4 Syntax Highlighting

Use incremental, line-based processing.

Highlight the viewport first and the remainder at idle priority.

Avoid repeatedly processing unchanged document regions.

### 7.5 Memory

Avoid:
- Unnecessary document-buffer duplication.
- Redundant full-document copies.
- Unnecessary resource retention.
- Unnecessary background processes.
- Excessive temporary allocations.

### 7.6 Large Files

Provide optimisation for larger documents.

Maintain responsive:
- Editing.
- Scrolling.
- Text retrieval.
- Search.
- Post-V1 syntax highlighting.

The application avoids expensive operations when document size or line length makes them impractical (see 7.0, graceful degradation).

### 7.7 Customisation

Theme changes must not require recreating document buffers.

Settings changes only update the components affected by them.

Theme and preference loading must not introduce unnecessary runtime overhead.

Any customisation option that can only be delivered at a cost to the budget must be optional and off by default.

------------------------------------------------------------

## 8. SOURCE CODE ORGANISATION

The following is the proposed logical project structure.

The exact directory structure may be adjusted to suit the selected language and framework, but system boundaries must remain intact.

```
project/
|
|-- AGENTS.md
|
|-- docs/
|   |-- PROJECT_SPEC.md
|   |-- ARCHITECTURE.md
|   |-- SYSTEMS.md
|   |-- DATA_MODELS.md
|   |-- INTERFACES.md
|   |-- DESIGN_GUIDELINES.md
|   |-- DEVELOPMENT_PLAN.md
|
|-- src/
|   |
|   |-- application/
|   |   |-- app
|   |   |-- application_state
|   |   |-- lifecycle
|   |   |-- config_loader
|   |
|   |-- ui/
|   |   |-- main_window
|   |   |-- menu_bar
|   |   |-- tab_bar
|   |   |-- editor_view
|   |   |-- status_bar
|   |   |-- settings_view
|   |   |-- drawing_surface
|   |
|   |-- editor/
|   |   |-- text_buffer
|   |   |-- cursor_manager
|   |   |-- selection_manager
|   |   |-- editing_controller
|   |   |-- indentation_manager
|   |   |-- bracket_manager
|   |   |-- undo_manager
|   |   |-- renderer
|   |
|   |-- documents/
|   |   |-- document
|   |   |-- document_manager
|   |   |-- tab_manager
|   |   |-- file_service
|   |
|   |-- language/
|   |   |-- language_registry
|   |   |-- language_detector
|   |   |-- syntax_highlighter
|   |   |-- language_definitions
|   |
|   |-- customisation/
|   |   |-- theme_manager
|   |   |-- theme_editor
|   |   |-- theme_storage
|   |   |-- settings_manager
|   |   |-- typography_manager
|   |   |-- layout_manager
|   |   |-- keybinding_manager
|   |
|   |-- commands/
|   |   |-- input_handler
|   |   |-- command_registry
|   |   |-- command_executor
|   |   |-- command_palette
|   |   |-- shortcut_dispatcher
|   |
|   |-- search/
|   |   |-- search_interface
|   |   |-- search_engine
|   |   |-- match_manager
|   |   |-- replace_engine
|   |
|   |-- persistence/
|   |   |-- session_manager
|   |   |-- recovery_manager
|   |   |-- snapshot_manager
|   |
|   |-- platform/
|       |-- platform_adapter
|       |-- filesystem
|       |-- file_dialogs
|       |-- external_app_launcher
|       |-- terminal_launcher
|
|-- resources/
|   |-- themes/
|   |-- language_definitions/
|
|-- benchmarks/
|
|-- tests/
    |-- application/
    |-- ui/
    |-- editor/
    |-- documents/
    |-- language/
    |-- customisation/
    |-- commands/
    |-- search/
    |-- persistence/
    |-- platform/
```

This is a logical structure rather than a requirement to create every file immediately.

Avoid empty modules and unnecessary abstractions.

Components may initially share files where appropriate, provided their responsibilities and interfaces remain clear.

------------------------------------------------------------

## 9. DEVELOPMENT ORDER

The remaining development is reorganised into integrated work packages. Performance work is contained within implementation as justified by measured needs; comprehensive profiling and optimisation belong to integrated V1 validation. Phase 05 and System 05 remain deferred until after V1.

WORK PACKAGE 1: Commands, Customisation and Search

- Integrate Systems 06, 07 and 08.
- Complete settings and theme ownership and persistence.
- Implement theme editing and live preview.
- Complete typography, layout and configurable keybindings.
- Complete command registration, execution, input routing, shortcuts and the command palette.
- Implement active-document find and replacement.

Dependencies: stable application lifecycle and document/editor state.

WORK PACKAGE 2: Session and Recovery

- Implement System 09 and integrate it with application lifecycle.
- Implement session restoration and dirty-only, debounced recovery snapshots.
- Test recovery publication, crash continuity and lifecycle edge cases.

Dependencies: complete document lifecycle, close/save behavior and application bootstrap state.

WORK PACKAGE 3: Core Completion and Platform Integration

- Complete outstanding implementation and validation work in Systems 01–04.
- Complete platform-facing work in System 10.
- Finish native lifecycle behavior and cross-platform abstraction without Windows-only assumptions.
- Complete representative document/editor behavior and file handling.

Windows is the initial V1 distribution target. Platform-independent behavior remains a requirement.

WORK PACKAGE 4: V1 Integration, Validation and Final Performance Optimisation

- Integrate all agreed V1 systems.
- Test application workflows, document integrity, recovery behavior and customisation persistence.
- Perform comprehensive profiling and final optimisation against the reference-device budget.
- Validate startup, idle resource use, editing, rendering, large files and long lines.
- Complete cross-platform builds and user/developer documentation.

WORK PACKAGE 5: Windows Packaging and Release Preparation

- Prepare Windows release artifacts and packaging.
- Validate installation, first-run behavior, resources and the packaged application.
- Complete release verification and publishable documentation.

The original phased systems remain useful historical labels, but the development sequence above supersedes their old post-foundation order. Performance is measured throughout implementation, but no dedicated optimisation phase precedes the integrated V1.

The development order may be adjusted when dependencies justify it, but system boundaries and product scope must remain unchanged.

------------------------------------------------------------

## 10. CODING AND ARCHITECTURAL RULES

The following rules apply throughout development.

### 10.1 Scope Control

Do not introduce new product features without explicit approval.

Do not implement excluded IDE functionality.

Do not expand the project into a general-purpose development environment.

### 10.2 Separation of Concerns

Each system has a defined responsibility.

Avoid placing unrelated functionality in the same component.

Do not allow the UI to own editing or file-management logic.

Do not allow the Editor Engine to perform filesystem operations.

Do not allow the Editor Engine to depend on the GUI framework except through the Drawing Surface interface.

Do not allow theme code to modify document contents.

### 10.3 State Ownership

Every major data category has one authoritative owner (see section 6).

Do not maintain unnecessary duplicate application state.

### 10.4 Dependencies

Prefer small, established dependencies.

Do not introduce large frameworks or libraries without a clear technical justification.

Do not introduce dependencies merely to avoid implementing a small piece of functionality.

Every dependency must be justified against the Performance Budget (startup time, memory and distribution size).

### 10.5 Performance

Avoid unnecessary background processing.

Avoid unnecessary full-document processing.

Avoid unnecessary memory duplication.

Prefer incremental operations where appropriate.

Changes that regress the Performance Budget are not merged without owner approval.

### 10.6 Error Handling

File operations, configuration loading, recovery and platform integration must handle failures gracefully.

A failed file save must not silently mark a document as saved.

A failed or interrupted save must not corrupt the existing file (atomic saves).

Invalid theme data must not crash the application.

Missing configuration files must result in sensible defaults.

### 10.7 Testing

Each system has tests for its core responsibilities.

Prioritise tests for:
- Text-buffer integrity.
- Cursor and selection correctness.
- Indentation.
- Bracket pairing.
- Undo and redo.
- File persistence, including interrupted saves.
- Encoding and line endings.
- Theme loading, including invalid and partial themes.
- Settings persistence.
- Search and replacement, including pathological regular expressions.
- Session restoration.
- Crash recovery.
- Performance benchmarks against the budget.

### 10.8 Implementation Discipline

Implement systems incrementally.

Do not scaffold the entire application with empty classes merely to match the proposed directory structure.

Define interfaces before integrating dependent systems.

Do not rewrite working components without a specific reason.

Do not introduce unrelated refactoring into feature implementation tasks.

------------------------------------------------------------

## 11. CODEX DEVELOPMENT INSTRUCTIONS

Codex is used as the primary implementation assistant.

It must treat this document as the authoritative product specification.

Before implementing any system, Codex should:

1. Read the project specification.
2. Inspect the existing project structure.
3. Identify the system being implemented.
4. Identify its dependencies.
5. Review existing interfaces.
6. Determine the smallest appropriate implementation scope.

For each implementation task, Codex should:

1. Explain the intended changes briefly.
2. Identify affected files.
3. Implement the requested functionality.
4. Add or update relevant tests.
5. Run the relevant tests and, where applicable, the relevant benchmarks.
6. Report changed files.
7. Report test and benchmark results.
8. Identify unresolved issues.

Codex must not silently change architectural decisions.

If an implementation requires a change to the agreed architecture, it should explain the reason and request approval before making a scope-changing decision.

If a required technical decision has not yet been made, Codex should identify the decision rather than inventing a permanent choice.

Codex should prefer modifying existing components over creating duplicate implementations.

Items in Appendix A are not approved. Codex must not implement them unless the project owner moves them into the main specification.

------------------------------------------------------------

## 12. TECHNICAL DECISIONS NOT YET FINALIZED

The following decisions remain open and must be evaluated before application scaffolding.

1. Programming language.
2. Cross-platform GUI framework.
3. Text-editing implementation.
4. Text-rendering approach.
5. Text-buffer data structure.
6. Post-V1 Language System implementation.
7. Configuration file format.
8. Theme file format.
9. Recovery storage format.
10. Build and packaging system.

These decisions are evaluated against:

- Startup speed.
- Memory consumption.
- Editing responsiveness.
- Cross-platform support.
- Text-rendering capabilities.
- Customisation capabilities.
- Dependency footprint.
- Build and distribution complexity.
- Long-term maintainability.

Fixed constraint: the stack must not be an embedded browser runtime (see 4 and 7.0).

Beyond that constraint, do not assume a particular technology stack before this evaluation.

The evaluation should include a small prototype benchmark (typing latency, startup time and memory on the reference device) for each serious candidate.

The architecture remains as independent of the chosen stack as reasonably possible.

------------------------------------------------------------

## 13. PROJECT SUCCESS CRITERIA

The application is successful when it satisfies the following.

### FUNCTIONALITY

- Users can open and edit text and source-code files without a syntax-highlighting runtime.
- Indentation behaves correctly.
- Bracket pairing behaves correctly.
- Multiple cursors work.
- Search and replacement work.
- Documents can be saved reliably.
- Sessions can be restored.
- Unsaved changes can be recovered.

### CUSTOMISATION

- Users can create custom themes.
- Individual interface and currently supported editor colours can be changed.
- Theme data may retain future syntax-colour category structures, but V1 does not apply them.
- Theme changes appear immediately in previews.
- Themes can be imported and exported.
- Fonts and editor appearance can be customised.
- Keyboard shortcuts can be changed.
- Editing behaviour can be configured.

### USABILITY

- The default interface is minimal.
- The application is usable through a mouse.
- The application is usable through a keyboard alone.
- Common commands are easy to access.
- The interface does not feel like an IDE.

### PERFORMANCE

- All targets in the Performance Budget (7.0) are met on the reference device.
- Startup is fast.
- Idle resource usage is low.
- Typing remains responsive.
- Rendering avoids unnecessary work.
- Larger files remain usable.
- Customisation does not introduce unnecessary overhead.

### CROSS-PLATFORM

- Windows is the initial V1 release target.
- The application architecture remains platform-independent.
- Linux and macOS behavior is preserved and validated where supported.
- Filesystem behaviour is correct.
- Platform-specific functionality works correctly.

------------------------------------------------------------

## 14. FINAL PRODUCT DEFINITION

Note is a lightweight, minimal, reliable and highly customisable text editor that prioritises fast startup, low resource consumption, responsive editing and configurable interaction.

It provides essential editing conveniences such as smart indentation, bracket completion, multiple cursors and document-level search.

Its second major strength is customisation, particularly a complete visual theme editor, configurable fonts, keyboard shortcuts, commands, interface layout and editing behaviour, so that every user can make it their own.

The application is intentionally file-oriented and does not attempt to replace a full IDE.

It should be a comfortable, fast and customisable environment for writing code, without unnecessary development tooling.

The defining principle is:

A small, fast and unobtrusive code editor with powerful personalisation underneath.

------------------------------------------------------------

## APPENDIX A: PROPOSED ADDITIONS (NOT IN SCOPE UNTIL APPROVED)

Nothing in this appendix may be implemented until the project owner explicitly moves it into the main specification. Each item must also fit within the Performance Budget.

### A.1 Deeper personalisation

- Cursor shape, width, blink and animation settings.
- A separate interface font and interface scale or density, independent of the editor font.
- Editor padding, an optional centred maximum-width column, letter spacing and optional font ligatures.
- A single accent colour that tints the interface.
- Configurable status bar items and tab styles.
- Per-language overrides (indentation width, pairing, colours).
- Themes that inherit from a base theme, so a theme file may contain only a few overrides.
- Toggles for smooth scrolling and all animations, doubling as performance options.
- A first-run "make it yours" step: choose theme, font and size in about ten seconds.
- Simple and Advanced settings views with search.
- Human-readable settings files for power users (never required).

### A.2 Editing basics not currently listed

- Cut, copy and paste.
- Go to line.
- Select next occurrence.
- Duplicate line, move line and delete line.
- Save All, recent files and reload from disk.
- Byte-order-mark handling and manual encoding override.
- Binary-file detection.
- IME and full Unicode input, HiDPI displays and accessibility support.

### A.3 Language coverage and detection

- Additional data-only definitions: Markdown, YAML, XML, shell, Go, Rust and C#.
- Detection by special filenames (for example `Makefile`) and by shebang lines.

------------------------------------------------------------

## APPENDIX B: CHANGE LOG FROM PREVIOUS DRAFT

1. Removed the shell heredoc lines (`cat > ... <<'EOF'` and `EOF`) and set the file location to `docs/PROJECT_SPEC.md`.
2. Document Model now holds references to cursor and history state rather than owning it, resolving a conflict with section 6.
3. Defined the File Service in System 04 as the facade over File Reader and File Writer, including atomic saves.
4. Configuration Loader is now bootstrap-only; the Settings Manager is the sole owner of settings.
5. Clarified the Editor Renderer as a thin layer using a framework-agnostic Drawing Surface, implemented in the UI system.
6. Resolved themes versus typography: typography is a setting; a theme may optionally bundle a font preset.
7. Defined the toolbar as optional and hidden by default.
8. Reordered development phases so that settings and theme tokens, the command registry, file dialogs, a minimal renderer and benchmarking come early.
9. Added the Performance Budget (7.0) with provisional targets and rules, including the ban on embedded browser runtimes.
10. Added safeguards: viewport-first highlighting, debounced dirty-only snapshots, guarded and chunked regex search, event-based or focus-based external-modification detection, and atomic saves.
11. Added the apostrophe rule to bracket and quote completion, and made language detection data-driven.
12. Moved all new-scope proposals into Appendix A, pending approval.
