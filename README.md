# Ogham Storyteller Foundaiton Gem
![License](https://img.shields.io/badge/License-Apache_2.0-blue?style=flat-square)
![Maintained](https://img.shields.io/badge/Maintained%3F-yes-green?style=flat-square)
![O3DE](https://img.shields.io/badge/O3DE-25.10%20%2B-%2300AEEF?style=flat-square&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0id2hpdGUiIGQ9Ik0xMiAxTDEgNy40djkuMkwxMiAyM2wxMS02LjRWNy40TDEyIDF6bTkuMSAxNC45TDExLjUgMjEuM2wtOC42LTYuNFY4LjFsOC42LTYuNCA5LjEgNi40djYuOHpNMTEuNSA0LjZMMi45IDkuNnY0LjhsOC42IDUuMSA4LjYtNS4xVjkuNmwtOC42LTUuMHoiLz48L3N2Zz4=)

An [Open 3D Engine (O3DE)](https://o3de.org) gem providing a data-driven narrative and dialogue management system. Decouples story logic from UI presentation using a tag-hash state machine — supporting branching dialogue, persistent world state, conditions, operations, and save/load snapshots.

- **License:** Apache 2.0
- **Origin:** Heathen Group
- **Platforms:** Windows, Linux, Android, iOS

> [!TIP]
> **Looking for the easiest way to install?**  
> You can add this gem—along with all of Heathen's free O3DE tools—by using the centralized [O3DE-Gems](https://github.com/heathen-engineering/O3DE-Gems) repository. Step-by-step setup instructions are available directly in its README.

-----

## 🛠 Also Available For
[![Unity](https://img.shields.io/badge/Unity-6%20%2B-%23313131?style=for-the-badge&logo=unity&logoColor=white)](https://github.com/heathen-engineering/Unity-Ogham-Storyteller-Foundation)

----

<img width="2591" height="1440" alt="image" src="https://github.com/user-attachments/assets/396ae4fc-7aa2-4305-bf36-9b823f1076a7" />

---

## Requirements

- O3DE engine **25.10.2** or compatible
- [Gameplay Tags Foundation](https://github.com/heathen-engineering/O3DE-Foundation-for-GameplayTags) Gem (provides `Heathen::GameplayTag`, conditions, and operations)
- [Lexicon Localisation Foundaiton](https://github.com/heathen-engineering/O3DE-Lexicon-Foundation) Gem (provides localisation key lookup for `textKey` fields)
- [xxHash](https://github.com/heathen-engineering/O3DE-xxHash) Gem (provides `xxHash::xxHashFunctions`)

---

## Become a GitHub Sponsor
[![Discord](https://img.shields.io/badge/Discord--1877F2?style=social&logo=discord)](https://discord.gg/6X3xrRc)
[![GitHub followers](https://img.shields.io/github/followers/heathen-engineering?style=social)](https://github.com/heathen-engineering?tab=followers)
Support Heathen by becoming a [GitHub Sponsor](https://github.com/sponsors/heathen-engineering). Sponsorship directly funds the development and maintenance of free tools like this, as well as our game development [Knowledge Base](https://heathen.group/) and community on [Discord](https://discord.gg/6X3xrRc).

Sponsors also get access to our private SourceRepo, which includes developer tools for O3DE, Unreal, Unity, and Godot.
Learn more or explore other ways to support @ [heathen.group/kb](https://heathen.group/kb/do-more/)

### Ogham Storyteller Toolkit
The **Ogham Storyteller Toolkit** is a sponsor-exclusive extension to this gem that adds professional narrative production features, including:

- **Twine / Twee 3 Importer** — import SugarCube-format `.twee` files directly into `.ogmcon`, with staged review of passage tags, localisation key assignment, variable-to-GameplayTag mapping, and condition/operation preview before committing
- Additional authoring and pipeline utilities for larger narrative projects

Available to [GitHub Sponsors](https://github.com/sponsors/heathen-engineering) via the private SourceRepo.

---

## Core Concept

Each conversation tree is stored as an **Ogham** file pair:

| Extension | Role | Contents |
|-----------|------|----------|
| `.ogmcon` | Source | Human-readable JSON; edited in the OghamStoryteller tool |
| `.ogmbin` | Product | Binary `OghamAsset`; produced by the Asset Processor |

At runtime the system component holds one or more registered `OghamAsset` instances. Tag lookups are O(log n) hash-map operations. Entry and option tags are hashed with **XXH3\_64bits** (seed 0) — the same algorithm used across the Heathen gem suite.

A single `.ogmcon` file can contain any number of entries. There is no enforced folder structure; place `.ogmcon` files anywhere in the project source tree. Options may reference entry tags in *other* `.ogmcon` files — the runtime builds a unified index across all registered assets.

Source files carry editor layout data (`position`, `aliasPins`, `fileColor`) alongside the narrative data. The current canonical field names are `dataKeys` (for entry text keys) and `targetTag` (for option destination); the earlier names `textKeys` and `targetEntry` are still accepted on load for backwards compatibility.

---

## .ogmcon File Format

A `.ogmcon` file is a UTF-8 JSON document with a single top-level field.

### Schema

```json
{
  "entries": [ <entry>, ... ]
}
```

### Entry object

```json
{
  "tag":             "Act1.Scene1.Line1",
  "position":        { "x": 100.0, "y": 200.0 },
  "dataKeys":        [ "Dialogue.Act1.Scene1.Line1.Body" ],
  "aliasPins":       [ <aliasPin>, ... ],
  "entryOperations": [ <operation>, ... ],
  "options":         [ <option>, ... ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tag` | string | Dot-path gameplay tag uniquely identifying this entry |
| `position` | object `{x, y}` | Graph canvas position in scene coordinates (editor layout only) |
| `dataKeys` | string[] | Localisation keys for body/narrator text (resolved via FoundationLocalisation); legacy name `textKeys` accepted on load |
| `aliasPins` | aliasPin[] | Named terminus pins attached to this entry; each has `tag`, `pinId`, and `position` |
| `entryOperations` | operation[] | State mutations applied when this entry is entered |
| `options` | option[] | Selectable choices shown to the player |

### Option object

```json
{
  "tag":              "Act1.Scene1.Line1.Option1",
  "textKey":          "Dialogue.Act1.Scene1.Line1.Option1.Label",
  "targetTag":        "Act1.Scene2.Line1",
  "targetAliasIndex": 0,
  "redirects":        [ <point>, ... ],
  "conditions":       [ <condition>, ... ],
  "operations":       [ <operation>, ... ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tag` | string | Dot-path gameplay tag for this option |
| `textKey` | string | Localisation key for the option's display label |
| `targetTag` | string | Tag of the entry to navigate to when selected (empty = close conversation); legacy name `targetEntry` accepted on load |
| `targetAliasIndex` | integer | Index of the alias pin on the target entry to connect to (0 = node input pin) |
| `redirects` | point[] | Ordered list of `{x, y}` waypoints for Bezier routing in the graph canvas |
| `conditions` | condition[] | All must pass for this option to appear in `GetAvailableOptions()` |
| `operations` | operation[] | State mutations applied when this option is selected |

### Condition object

```json
{
  "tag":        "World.PlayerReputation",
  "comparison": "GreaterEqual",
  "value":      10,
  "logicOp":    "And"
}
```

| Field | Values | Description |
|-------|--------|-------------|
| `tag` | string | Gameplay tag whose value to test |
| `comparison` | `"Exists"` `"NotExists"` `"Equal"` `"NotEqual"` `"Less"` `"LessEqual"` `"Greater"` `"GreaterEqual"` | Comparison operator |
| `value` | integer | Value to compare against |
| `logicOp` | `"And"` `"Or"` `"Xor"` | How this condition chains with the next (omit on last condition) |

### Operation object

```json
{
  "tag":        "World.PlayerReputation",
  "arithmetic": "Add",
  "value":      5,
  "conditions": [ <condition>, ... ]
}
```

| Field | Values | Description |
|-------|--------|-------------|
| `tag` | string | Gameplay tag to mutate |
| `arithmetic` | `"Set"` `"Add"` `"Sub"` `"Mul"` `"Div"` `"Min"` `"Max"` | Mutation operator; default `"Set"` |
| `value` | integer | Operand |
| `conditions` | condition[] | Optional per-operation guard conditions |

### Full example

```json
{
  "entries": [
    {
      "tag":             "Dialogue.Start",
      "position":        { "x": 0.0, "y": 0.0 },
      "dataKeys":        [ "Dialogue.Start.Body" ],
      "aliasPins":       [],
      "entryOperations": [],
      "options": [
        {
          "tag":              "Dialogue.Start.Option1",
          "textKey":          "Dialogue.Start.Option1.Label",
          "targetTag":        "Dialogue.Act2",
          "targetAliasIndex": 0,
          "redirects":        [],
          "conditions":       [],
          "operations":       [
            { "tag": "World.PlayerReputation", "arithmetic": "Add", "value": 2, "conditions": [] }
          ]
        },
        {
          "tag":              "Dialogue.Start.Option2",
          "textKey":          "Dialogue.Start.Option2.Label",
          "targetTag":        "",
          "targetAliasIndex": 0,
          "redirects":        [],
          "conditions":       [
            { "tag": "World.PlayerReputation", "comparison": "GreaterEqual", "value": 5, "logicOp": "And" }
          ],
          "operations":       []
        }
      ]
    }
  ]
}
```

---

## Hashing Rule

Entry and option tags are hashed at **build time** (by the Asset Processor) and at **runtime** (by the convenience string overloads on the bus). The algorithm must be identical in both places:

```
hash = XXH3_64bits_withSeed(tag.data(), tag.size(), /*seed=*/0)
```

Via the xxHash gem's C++ API:

```cpp
#include <xxHash/xxHashFunctions.h>
AZ::u64 hash = xxHash::xxHashFunctions::Hash64("Act1.Scene1.Line1", 0);
```

Never use `XXH64` or `XXH32` — they produce different values and lookups will silently miss.

---

## Runtime API

### C++ — via the EBus

```cpp
#include <FoundationOgham/FoundationOghamBus.h>

// Register a loaded OghamAsset with the system
FoundationOgham::FoundationOghamRequestBus::Broadcast(
    &FoundationOgham::FoundationOghamRequests::RegisterAsset, myAsset);

// Start a conversation at a specific entry
bool started;
FoundationOgham::FoundationOghamRequestBus::BroadcastResult(
    started, &FoundationOgham::FoundationOghamRequests::StartConversation, "Dialogue.Start");

// Select an option (applies operations + navigates to targetEntry)
bool ok;
FoundationOgham::FoundationOghamRequestBus::BroadcastResult(
    ok, &FoundationOgham::FoundationOghamRequests::SelectOption, "Dialogue.Start.Option1");

// Close the conversation
FoundationOgham::FoundationOghamRequestBus::Broadcast(
    &FoundationOgham::FoundationOghamRequests::CloseConversation, /*interrupted=*/false);

// Navigate back up to a parent entry (strips descendant state, re-enters parent)
FoundationOgham::FoundationOghamRequestBus::Broadcast(
    &FoundationOgham::FoundationOghamRequests::ReturnTo, "Act1.Scene1");
```

### C++ — via the AZ::Interface (lowest overhead)

```cpp
if (auto* ogham = FoundationOgham::FoundationOghamInterface::Get())
{
    const Heathen::DialogueEntry* entry = ogham->GetCurrentEntry();
    AZStd::vector<Heathen::DialogueOption> opts = ogham->GetAvailableOptions();
}
```

### Listening for narrative events

```cpp
class MyUIComponent
    : public AZ::Component
    , public FoundationOgham::OghamNotificationBus::Handler
{
    void Activate() override { OghamNotificationBus::Handler::BusConnect(); }
    void Deactivate() override { OghamNotificationBus::Handler::BusDisconnect(); }

    void OnDialogueEntered(
        const Heathen::DialogueEntry& entry,
        const AZStd::vector<Heathen::DialogueOption>& availableOptions) override
    {
        // Populate UI with entry.textKeys and availableOptions
    }

    void OnDialogueClosed(bool interrupted) override
    {
        // Hide dialogue UI
    }
};
```

---

## Data Types

Use these types as fields on your components. All are reflected to `SerializeContext`, `EditContext`, and `BehaviorContext`.

### `FoundationOgham::DialogueEntry`

Compiled runtime type. Include `<FoundationOgham/OghamTypes.h>`.

| Field | Type | Description |
|-------|------|-------------|
| `tag` | `GameplayTag` | Hashed dot-path identifier |
| `parentTag` | `GameplayTag` | Parent entry tag (zero if root) |
| `textKeys` | `AZStd::vector<AZStd::string>` | Localisation key strings for body text (mapped from `dataKeys` in `.ogmcon`) |
| `entryOperations` | `GameplayTagOperation[]` | Applied on entry |
| `options` | `DialogueOption[]` | Available choices |

### `FoundationOgham::DialogueOption`

A selectable choice within an entry.

| Field | Type | Description |
|-------|------|-------------|
| `tag` | `GameplayTag` | Hashed identifier |
| `textKey` | `AZStd::string` | Localisation key for the option label |
| `targetEntry` | `GameplayTag` | Entry to navigate to (zero = close conversation; mapped from `targetTag` in `.ogmcon`) |
| `conditions` | `GameplayTagCondition[]` | Must all pass for option to be visible |
| `operations` | `GameplayTagOperation[]` | Applied when selected |

### Editor source types

The OghamStoryteller editor works from `OghamSourceEntry` and `OghamSourceOption` (defined in `OghamStoryteller.h`), which carry graph layout fields not present in the compiled asset.

**`OghamSourceEntry`** — mirrors `DialogueEntry` plus:

| Field | Type | Description |
|-------|------|-------------|
| `position` | `QPointF` | Node position in the graph canvas |
| `dataKeys` | `QVector<QString>` | Localisation key strings (written as `dataKeys` in `.ogmcon`) |
| `aliasPins` | `QList<OghamAliasPin>` | Named terminus pins; each has `tag`, `pinId`, `position` |

**`OghamSourceOption`** — mirrors `DialogueOption` plus:

| Field | Type | Description |
|-------|------|-------------|
| `targetTag` | `QString` | Destination entry tag (written as `targetTag` in `.ogmcon`) |
| `targetAliasIndex` | `int` | Index of the alias pin to connect to (0 = node input pin) |
| `redirects` | `QVector<QPointF>` | Ordered Bezier waypoints for the connection curve |

### `Heathen::OghamSaveState`

A portable snapshot of the full narrative state.

| Field | Type | Description |
|-------|------|-------------|
| `uuid` | `AZ::Uuid` | Unique save ID |
| `name` | `AZStd::string` | Human-readable save name |
| `currentEntryId` | `AZ::u64` | Hashed tag of the active entry |
| `state` | `GameplayTagCollection` | Full tag value map |
| `history` | `HistoryEntry[]` | Append-only play history |

```cpp
// Create a save snapshot
Heathen::OghamSaveState save;
FoundationOgham::FoundationOghamRequestBus::BroadcastResult(
    save, &FoundationOgham::FoundationOghamRequests::CreateSaveState, "SlotA");

// Restore it (does not fire narrative events)
FoundationOgham::FoundationOghamRequestBus::Broadcast(
    &FoundationOgham::FoundationOghamRequests::LoadSaveState, save);
```

---

## Script Canvas

All request bus methods are exposed as Script Canvas events under the **"Ogham"** category via the `OghamProcessor` bus. `OghamNotificationBus` is exposed as a handler node, allowing graphs to react to `OnDialogueEntered` and `OnDialogueClosed` without any C++.

---

## OghamStoryteller Editor Tool

The **OghamStoryteller** is a dockable editor panel for authoring `.ogmcon` files directly within O3DE Editor.

**Layout:** three resizable panels — tree (left) | graph canvas (centre) | form editor (right). All splitter positions persist across sessions via QSettings.

- Scans the project source tree for all `.ogmcon` files and loads them automatically
- Tree view shows file → entry hierarchy with inline add/remove controls; each file header has a clickable colour swatch (stored as `fileColor` in the `.ogmcon` root) that tints node headers in the graph
- File visibility toggle: the eye-icon button in a file tree header hides that file's nodes from the graph; cross-file connections are shown ghosted
- Graph canvas displays all loaded entries as nodes with Bezier-curve connections (see Graph View below)
- Form editor for tag, localisation data keys, entry operations, and options; selecting a node in the graph selects the matching entry in the tree and form
- Tag status icons: ⚫ unset · ✓ registered · ⚠ valid-but-unknown (click to add to `.gptags`) · ✗ invalid
- Localisation key rows show `[status] [key] [value] [▼]` — the value field writes directly to the default lexicon
- Condition and operation tag fields validate against all `.gptags` files in the project; clicking ⚠ opens a file-picker dialog to choose which `.gptags` to add the tag to
- **Auto-save:** any field edit starts a 200 ms debounce timer; all dirty files are written automatically on expiry
- **Rename propagation:** changing a node's tag updates all `targetTag` references in options across all loaded files
- **Delete key:** removes the selected node from the graph and clears all option references to it; also removes a hovered redirect waypoint on a connection
- **Context menus:** right-click a node for *Create Alias Pin Here* / *Duplicate Node* / *Delete Node*; right-click a connection for *Add Redirect Point* / *Remove Waypoint*
- **Unique default tags:** new nodes are tagged `Dialogue.NewNode`, `Dialogue.NewNode1`, etc., auto-numbered across all loaded files
- File system watcher detects external changes with debounced reload (suppressed while auto-save writes)
- **Save All** writes all dirty `.ogmcon` files; the Asset Processor picks up changes automatically

Open via **Tools → Ogham Storyteller** in the O3DE Editor menu bar.

---

## Graph View

A `QGraphicsScene`-based canvas occupies the centre panel of OghamStoryteller. It provides a visual representation of all loaded dialogue entries and their connections.

**Navigation**
- Pan — left-drag on empty canvas, or middle-drag anywhere
- Zoom — mouse wheel (clamped 10 %–500 %)
- Fit all — **F** key with nothing selected; fit selected — **F** key with nodes selected

**Nodes**

Each dialogue entry is a node. The header shows the entry tag, tinted by the file's colour. The left edge carries a single input pin; the right edge has one output pin per option. Node header colour cycles through a palette of six built-in colours per file unless a custom `fileColor` is set.

**Connections**

Dragging an output pin and releasing on a node input pin wires the option's `targetTag`. Releasing on the canvas creates a new entry at that position. Connections are rendered as Bezier curves. Redirect waypoints can be added by double-clicking a curve, dragged to adjust routing, and removed with the **Delete** key while hovered or via right-click → *Remove Waypoint*.

**Alias Pins**

Right-click a node → *Create Alias Pin Here* to place a named terminus pin (`OghamAliasPin`) at that canvas position. An alias pin gives an entry a secondary connection target, allowing multiple incoming connections to reference it by index. Double-click an alias pin to navigate to its owning node. Alias pins are stored in `entry.aliasPins` in the `.ogmcon` file.

**File Colours**

The clickable colour swatch in each file's tree header sets `fileColor` (stored as an RGB integer in the `.ogmcon` root). This tints node headers for all entries in that file. An absent or invalid `fileColor` falls back to the built-in palette.

**File Visibility**

The eye-icon toggle in the file tree header hides all nodes belonging to that file without unloading them. Cross-file connections to hidden-file nodes remain visible as ghosted curves.

**Keyboard shortcuts**

| Key | Action |
|-----|--------|
| Delete | Remove the selected node (clears all option references to it) |
| Delete (hover) | Remove the hovered redirect waypoint on a connection |
| F | Fit all nodes in view, or fit the current selection |

---

## Play Mode

A **▶ Play from Node** button in the form editor opens the **OghamPlayPanel** — a full-window in-editor play-test mode that replaces the three-panel layout until stopped.

**Starting a play session**

1. Select any entry node in the tree or graph.
2. Click **▶ Play from Node** in the form panel.
3. The *Set Initial State* dialog appears. Pre-populate GameplayTag values before play begins. Named initial states can be saved to and loaded from `Assets/Storyteller/OghamTestStates.json` in the project directory.
4. Click **▶ Play** to start.

**Play panel layout**

- **Left:** dialogue text (resolved via FoundationLocalisation) for the current entry + option buttons. Options whose conditions are met show ✓; unmet options show ✗ and are disabled. All options are listed so locked paths remain visible.
- **Right:** real-time GameplayTagCollection state monitor showing all active tag values, plus a clickable history log. Click any history entry to replay the conversation from that point using the state snapshot captured at that moment.
- **Top bar:** entry tag breadcrumb + **■ Stop** button.

**Ending a session**

When the current entry has no options with a non-empty `targetTag`, the panel displays "Conversation Closed" and disables all option buttons. Press **■ Stop** at any time to return to the editor layout.
