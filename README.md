# Ogham Storyteller Foundaiton Gem
![License](https://img.shields.io/badge/License-Apache_2.0-blue?style=flat-square)
![Maintained](https://img.shields.io/badge/Maintained%3F-yes-green?style=flat-square)
![O3DE](https://img.shields.io/badge/O3DE-25.10%20%2B-%2300AEEF?style=flat-square&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0id2hpdGUiIGQ9Ik0xMiAxTDEgNy40djkuMkwxMiAyM2wxMS02LjRWNy40TDEyIDF6bTkuMSAxNC45TDExLjUgMjEuM2wtOC42LTYuNFY4LjFsOC42LTYuNCA5LjEgNi40djYuOHpNMTEuNSA0LjZMMi45IDkuNnY0LjhsOC42IDUuMSA4LjYtNS4xVjkuNmwtOC42LTUuMHoiLz48L3N2Zz4=)

An [Open 3D Engine (O3DE)](https://o3de.org) gem providing a data-driven narrative and dialogue management system. Decouples story logic from UI presentation using a tag-hash state machine — supporting branching dialogue, persistent world state, conditions, operations, and save/load snapshots.

- **License:** Apache 2.0
- **Origin:** Heathen Group
- **Platforms:** Windows, Linux, Android, iOS

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
  "parentTag":       "Act1.Scene1",
  "textKeys":        [ "Dialogue.Act1.Scene1.Line1.Body" ],
  "entryOperations": [ <operation>, ... ],
  "options":         [ <option>, ... ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tag` | string | Dot-path gameplay tag uniquely identifying this entry |
| `parentTag` | string | Tag of the parent entry (used to build the tree) |
| `textKeys` | string[] | Localisation keys for body/narrator text (resolved via FoundationLocalisation) |
| `entryOperations` | operation[] | State mutations applied when this entry is entered |
| `options` | option[] | Selectable choices shown to the player |

### Option object

```json
{
  "tag":         "Act1.Scene1.Line1.Option1",
  "textKey":     "Dialogue.Act1.Scene1.Line1.Option1.Label",
  "targetEntry": "Act1.Scene2.Line1",
  "conditions":  [ <condition>, ... ],
  "operations":  [ <operation>, ... ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `tag` | string | Dot-path gameplay tag for this option |
| `textKey` | string | Localisation key for the option's display label |
| `targetEntry` | string | Tag of the entry to navigate to when selected (empty = close conversation) |
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
      "tag":       "Dialogue.Start",
      "parentTag": "",
      "textKeys":  [ "Dialogue.Start.Body" ],
      "entryOperations": [],
      "options": [
        {
          "tag":         "Dialogue.Start.Option1",
          "textKey":     "Dialogue.Start.Option1.Label",
          "targetEntry": "Dialogue.Act2",
          "conditions":  [],
          "operations":  [
            { "tag": "World.PlayerReputation", "arithmetic": "Add", "value": 2, "conditions": [] }
          ]
        },
        {
          "tag":         "Dialogue.Start.Option2",
          "textKey":     "Dialogue.Start.Option2.Label",
          "targetEntry": "",
          "conditions":  [
            { "tag": "World.PlayerReputation", "comparison": "GreaterEqual", "value": 5, "logicOp": "And" }
          ],
          "operations":  []
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

### `Heathen::DialogueEntry`

A single narrative node. Include `<FoundationOgham/OghamTypes.h>`.

| Field | Type | Description |
|-------|------|-------------|
| `tag` | `GameplayTag` | Hashed dot-path identifier |
| `parentTag` | `GameplayTag` | Parent entry tag (zero if root) |
| `textKeys` | `AZStd::string[]` | Localisation key strings for body text |
| `entryOperations` | `GameplayTagOperation[]` | Applied on entry |
| `options` | `DialogueOption[]` | Available choices |

### `Heathen::DialogueOption`

A selectable choice within an entry.

| Field | Type | Description |
|-------|------|-------------|
| `tag` | `GameplayTag` | Hashed identifier |
| `textKey` | `AZStd::string` | Localisation key for the option label |
| `targetEntry` | `GameplayTag` | Entry to navigate to (zero = close conversation) |
| `conditions` | `GameplayTagCondition[]` | Must all pass for option to be visible |
| `operations` | `GameplayTagOperation[]` | Applied when selected |

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

- Scans the project source tree for all `.ogmcon` files and loads them automatically
- Tree view shows file → entry hierarchy with inline add/remove/reorder controls
- Right-panel form editor for tag, localisation text keys, entry operations, and options
- Tag status icons: ⚫ unset · ✓ registered · ⚠ valid-but-unknown (click to add to `.gptags`) · ✗ invalid
- Localisation key rows show `[status] [key] [value] [▼]` — the value field writes directly to the default lexicon
- Condition and operation tag fields validate against all `.gptags` files in the project; clicking ⚠ opens a file-picker dialog to choose which `.gptags` to add the tag to
- File system watcher detects external changes with debounced reload
- **Save All** writes all dirty `.ogmcon` files; the Asset Processor picks up changes automatically

Open via **Tools → Ogham Storyteller** in the O3DE Editor menu bar.

---

## Requirements

- O3DE engine **25.10.2** or compatible
- `FoundationGameplayTags` gem (included in this project; provides `Heathen::GameplayTag`, conditions, and operations)
- `FoundationLocalisation` gem (included in this project; provides localisation key lookup for `textKey` fields)
- `xxHash` gem (included in this project; provides `xxHash::xxHashFunctions`)
