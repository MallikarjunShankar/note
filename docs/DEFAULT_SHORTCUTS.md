# Default keyboard shortcuts

These are the current application defaults. Standard shortcuts are supplied by
Qt's `QKeySequence` and may resolve differently on each supported platform.
The Windows column is the initial V1 distribution mapping.

| Command | Windows default | Source |
|---|---|---|
| New document (`file.new`) | `Ctrl+N` | `QKeySequence::New` |
| Open file (`file.open`) | `Ctrl+O` | `QKeySequence::Open` |
| Save (`file.save`) | `Ctrl+S` | `QKeySequence::Save` |
| Save As (`file.saveAs`) | `Ctrl+Shift+S` | `QKeySequence::SaveAs` |
| Close tab (`file.close`) | `Ctrl+F4` | `QKeySequence::Close` |
| Undo (`edit.undo`) | `Ctrl+Z` | `QKeySequence::Undo` |
| Redo (`edit.redo`) | `Ctrl+Y` | `QKeySequence::Redo` |
| Find (`search.find`) | `Ctrl+F` | `QKeySequence::Find` |
| Replace (`search.replace`) | `Ctrl+H` | `QKeySequence::Replace` |
| Preferences (`settings.open`) | `Ctrl+,` | Explicit Qt key sequence |
| Command palette (`commands.palette`) | `Ctrl+P` | Explicit Qt key sequence |

`file.external`, `file.terminal`, and the View toggles have no default shortcut;
they can be assigned from Settings > Keybindings. Shortcut overrides are
persisted separately from settings and themes.

The editor currently also handles `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, and `Ctrl+A`
directly for copy, cut, paste, and select all. These remain widget-level
compatibility behavior and are not registered or configurable through the
command system. They are listed for completeness only; those editing basics
remain unapproved Appendix A.2 scope and are not promoted to V1 requirements by
this document.
