/*
 * Copyright (c) 2026 Heathen Engineering Limited
 * Irish Registered Company #556277
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#if !defined(Q_MOC_RUN)
#include <QList>
#include <QStringList>
#include <QWidget>
#include <functional>
#endif

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QFileSystemWatcher;

namespace FoundationOgham
{
    // -------------------------------------------------------------------------
    // In-memory editing structures
    // -------------------------------------------------------------------------

    /// One conditional check applied before showing an entry / option or executing an op.
    struct OghamCondition
    {
        QString tag;
        QString comparison = QStringLiteral("Exists"); // "Exists","NotExists","Equal","NotEqual","Less","LessEqual","Greater","GreaterEqual"
        int     value      = 0;
        QString logicOp    = QStringLiteral("And");      // "And" / "Or" / "Xor" — empty on last row
    };

    /// One tag-mutation operation (set/add/subtract/…) with optional per-op conditions.
    struct OghamOperation
    {
        QString                tag;
        QString                arithmetic = QStringLiteral("Set"); // "Set","Add","Sub","Mul","Div","Min","Max"
        int                    value      = 0;
        QList<OghamCondition>  conditions;
    };

    struct OghamSourceOption
    {
        QString                tag;
        QString                textKey;
        QString                targetEntry;
        QList<OghamCondition>  conditions;
        QList<OghamOperation>  operations;
    };

    struct OghamSourceEntry
    {
        QString                  tag;
        QStringList              textKeys;
        QList<OghamSourceOption> options;
        QList<OghamOperation>    entryOperations;
    };

    /// One loaded .ogmcon file with its entries and dirty flag.
    struct LoadedFile
    {
        QString                  path;
        QList<OghamSourceEntry>  entries;
        bool                     dirty = false;
    };

    // =========================================================================
    // OghamStoryteller
    //
    // Multi-file editor for .ogmcon conversation source files.
    //
    // Layout:
    //   Toolbar  — [New File] [Save All] [Open Source…]
    //   ┌──────────────────────────┬────────────────────────────────────────┐
    //   │  Act1.ogmcon        [+][…]│  Entry editor (hidden until selected) │
    //   │    Act1             [+][↓]│  Tag / Localisation Keys / Operations │
    //   │      Scene1         [+][↓]│  / Options                            │
    //   │        Line1    [+][↓][X] │                                       │
    //   │  Act2.ogmcon        [+][…]│                                       │
    //   └──────────────────────────┴────────────────────────────────────────┘
    //   Status bar
    // =========================================================================
    class OghamStoryteller : public QWidget
    {
        Q_OBJECT

    public:
        explicit OghamStoryteller(QWidget* parent = nullptr);

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void OnNewFile();
        void OnSaveAll();
        void OnOpenSource();
        void OnTagEdited();
        void OnAddTextKey();
        void OnAddOption();

    private:
        // ── File operations ──────────────────────────────────────────────────
        void    ScanAndLoadAll();
        bool    ParseFile(const QString& path, QList<OghamSourceEntry>& out);
        bool    SaveFile(int fileIdx);

        // ── Tree building ────────────────────────────────────────────────────
        void     RebuildTree();
        QWidget* MakeFileButtons(int fileIdx);
        QWidget* MakeEntryButtons(int fileIdx, int entryIdx,
                                   bool isReal, const QString& nodeTag);
        void     SelectEntry(int fileIdx, int entryIdx);

        // ── Entry CRUD ───────────────────────────────────────────────────────
        void AddRootEntry   (int fileIdx);
        void AddSiblingEntry(int fileIdx, const QString& siblingTag);
        void AddChildEntry  (int fileIdx, const QString& parentTag);
        void RemoveEntry    (int fileIdx, int entryIdx);

        // ── Form ─────────────────────────────────────────────────────────────
        void PopulateForm(int fileIdx, int entryIdx);
        void ClearForm();
        void RebuildTextKeysArea();
        void RebuildOptionsArea();
        void RebuildEntryOpsArea();
        void SetFileDirty(int fileIdx, bool dirty);
        void UpdateStatusBar();

        // ── Condition/Operation row builders (shared) ─────────────────────────
        /// Append one condition row into the given VBoxLayout. condList is the live
        /// list of conditions that backs the row (shared ref path).
        using CondList = QList<OghamCondition>;
        using OpList   = QList<OghamOperation>;
        QWidget* MakeConditionRow(int ci,
                                  std::function<CondList&()> getList,
                                  std::function<void()>      onChanged,
                                  const QStringList&         knownTags,
                                  bool                       isLast = false);
        QWidget* MakeOperationRow(int oi,
                                  std::function<OpList&()>  getList,
                                  std::function<void()>     onChanged,
                                  const QStringList&        knownTags);

        // ── Lexicon helpers ──────────────────────────────────────────────────
        QStringList FetchKnownLexiconKeys() const;
        QString     FetchLexiconValueForKey(const QString& key) const;
        void        ShowAddKeyDialog(const QString& key, const QString& value);

        // ── Ogham tag helpers ─────────────────────────────────────────────────
        void        EnsureOghamTagsFile();
        QStringList FetchKnownOghamTags() const;
        QStringList FetchAllEntryTags()   const;
        QStringList FetchAllGptagsFiles() const;
        QStringList FetchAllTagsFromAllFiles() const;
        void        AddTagToGptagsFile(const QString& tag);
        void        AddTagToFile(const QString& tag, const QString& filePath);
        void        ShowAddTagDialog(const QString& tag, QToolButton* btn);

        // ── Toolbar ──────────────────────────────────────────────────────────
        QPushButton* m_newFileBtn  = nullptr;
        QPushButton* m_saveAllBtn  = nullptr;
        QPushButton* m_openSrcBtn  = nullptr;

        // ── Tree (2 columns: name | buttons) ─────────────────────────────────
        QTreeWidget* m_tree = nullptr;

        // ── Right panel ──────────────────────────────────────────────────────
        QStackedWidget* m_formStack  = nullptr;  // page 0=empty, page 1=form

        // Form fields (page 1)
        QComboBox*   m_tagCombo      = nullptr;
        QToolButton* m_tagStatus     = nullptr;
        QWidget*     m_keysWidget   = nullptr;
        QVBoxLayout* m_keysLayout   = nullptr;
        QWidget*     m_entOpsWidget = nullptr;
        QVBoxLayout* m_entOpsLayout = nullptr;
        QWidget*     m_optsWidget   = nullptr;
        QVBoxLayout* m_optsLayout   = nullptr;

        // ── Status ───────────────────────────────────────────────────────────
        QLabel*    m_statusLabel = nullptr;
        QSplitter* m_splitter    = nullptr;

        // ── File watcher ─────────────────────────────────────────────────────
        QFileSystemWatcher* m_fileWatcher    = nullptr;
        QTimer*             m_watchDebounce  = nullptr;

        // ── Ogham tags file ───────────────────────────────────────────────────
        QString m_oghamTagsPath;

        // ── Data ─────────────────────────────────────────────────────────────
        QList<LoadedFile> m_loadedFiles;
        int               m_selectedFileIdx  = -1;
        int               m_selectedEntryIdx = -1;
    };

} // namespace FoundationOgham
