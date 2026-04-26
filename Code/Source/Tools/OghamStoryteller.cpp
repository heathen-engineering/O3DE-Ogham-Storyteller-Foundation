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

#include "OghamAliasPinItem.h"
#include "OghamConnectionItem.h"
#include "OghamGraphView.h"
#include "OghamNodeItem.h"
#include "OghamPlayPanel.h"
#include "OghamStoryteller.h"

#include <AzCore/IO/FileIO.h>
#include <AzCore/Utils/Utils.h>

#include <FoundationLocalisation/LexiconEditorRequestBus.h>

#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QGraphicsScene>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QSettings>
#include <QSpinBox>

#include <algorithm>
#include <climits>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QInputDialog>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace FoundationOgham
{
    // UserRole assignments for tree items
    static constexpr int kRoleFileIdx  = Qt::UserRole + 0;  // int  (file index in m_loadedFiles)
    static constexpr int kRoleEntryIdx = Qt::UserRole + 1;  // int  (-1 = file header or virtual folder)
    static constexpr int kRoleNodeTag  = Qt::UserRole + 2;  // QString (full dot-path for this node)

    // Per-file node header colours (cycles when more files than palette entries)
    static const QColor kGraphFileColors[] = {
        { 0x2d, 0x5a, 0x8e },   // blue
        { 0x5a, 0x2d, 0x5a },   // violet
        { 0x2d, 0x5a, 0x3d },   // teal-green
        { 0x5a, 0x4a, 0x2d },   // amber
        { 0x5a, 0x2d, 0x2d },   // burgundy
        { 0x2d, 0x45, 0x5a },   // steel blue
    };
    static constexpr int kGraphFileColorCount =
        static_cast<int>(sizeof(kGraphFileColors) / sizeof(kGraphFileColors[0]));

    // =========================================================================
    // AddKeyDialog — modal for adding a missing localisation key to all Lexicons
    // =========================================================================

    class AddKeyDialog : public QDialog
    {
    public:
        AddKeyDialog(
            const QString&     suggestedKey,
            const QString&     suggestedValue,
            const QStringList& filePaths,
            QWidget*           parent = nullptr)
            : QDialog(parent, Qt::Dialog)
        {
            setWindowTitle("Add Localisation Key");
            setMinimumWidth(480);

            auto* layout = new QVBoxLayout(this);
            layout->setSpacing(10);

            layout->addWidget(new QLabel(
                "This key does not exist in any Lexicon.\n"
                "It will be created in <b>all</b> Lexicons. Select the primary Lexicon "
                "to receive the real value; the others will get a placeholder to update later.",
                this));

            auto* form = new QFormLayout();
            form->setSpacing(6);

            m_keyEdit = new QLineEdit(suggestedKey, this);
            form->addRow("Key:", m_keyEdit);

            m_valueEdit = new QLineEdit(suggestedValue, this);
            m_valueEdit->setPlaceholderText("Value for the primary Lexicon");
            form->addRow("Value:", m_valueEdit);

            m_primaryCombo = new QComboBox(this);
            for (const QString& fp : filePaths)
                m_primaryCombo->addItem(QFileInfo(fp).fileName(), fp);
            form->addRow("Primary Lexicon:", m_primaryCombo);

            layout->addLayout(form);

            if (filePaths.size() > 1)
            {
                QStringList others;
                for (int i = 1; i < filePaths.size(); ++i)
                    others.append("  \xe2\x80\xa2 " + QFileInfo(filePaths[i]).fileName());
                m_otherLabel = new QLabel(
                    "Other Lexicons (will receive placeholder):\n" + others.join("\n"), this);
                m_otherLabel->setWordWrap(true);
                layout->addWidget(m_otherLabel);
            }

            auto* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            layout->addWidget(buttons);

            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

            connect(m_primaryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [this, filePaths](int idx)
                {
                    if (!m_otherLabel) return;
                    QStringList others;
                    for (int i = 0; i < filePaths.size(); ++i)
                        if (i != idx)
                            others.append("  \xe2\x80\xa2 " + QFileInfo(filePaths[i]).fileName());
                    m_otherLabel->setText(
                        others.isEmpty()
                            ? "(no other Lexicons)"
                            : "Other Lexicons (will receive placeholder):\n" + others.join("\n"));
                });
        }

        QString key()             const { return m_keyEdit->text().trimmed(); }
        QString value()           const { return m_valueEdit->text(); }
        QString primaryFilePath() const { return m_primaryCombo->currentData().toString(); }

    private:
        QLineEdit* m_keyEdit      = nullptr;
        QLineEdit* m_valueEdit    = nullptr;
        QComboBox* m_primaryCombo = nullptr;
        QLabel*    m_otherLabel   = nullptr;
    };

    // =========================================================================
    // Tag validation helpers (free functions)
    // =========================================================================

    static bool IsValidTagStructure(const QString& tag)
    {
        if (tag.isEmpty()) return false;
        if (tag.startsWith('.') || tag.endsWith('.')) return false;
        if (tag.contains("..")) return false;
        for (const QChar& c : tag)
        {
            if (!c.isLetterOrNumber() && c != '_' && c != '.') return false;
        }
        return true;
    }

    /// Apply tristate icon to a status button for a gameplay tag field.
    /// knownTags is the set against which the tag is validated (e.g. .gptags contents).
    /// allowEmpty: if true, an empty tag is neutral (⚫ disabled) rather than invalid.
    static void ApplyTagStatus(QToolButton* btn, const QString& tag,
                                const QStringList& knownTags,
                                bool allowEmpty = false)
    {
        if (tag.isEmpty())
        {
            btn->setText("\xe2\x97\x8f");   // ⚫
            btn->setStyleSheet("color: #555555;");
            btn->setToolTip(allowEmpty ? "No tag set (optional)." : "No tag set.");
            btn->setEnabled(false);
        }
        else if (!IsValidTagStructure(tag))
        {
            btn->setText(QString(QChar(0x2717)));   // ✗  (U+2717)
            btn->setStyleSheet("color: #cc3333; font-weight: bold;");
            btn->setToolTip("Invalid tag structure.");
            btn->setEnabled(false);
        }
        else if (knownTags.contains(tag))
        {
            btn->setText(QString(QChar(0x2713)));   // ✓
            btn->setStyleSheet("color: #44aa44; font-weight: bold;");
            btn->setToolTip(QString("Tag '%1' found.").arg(tag));
            btn->setEnabled(false);
        }
        else
        {
            btn->setText("\xe2\x9a\xa0");   // ⚠
            btn->setStyleSheet("color: #cc7700; font-weight: bold;");
            btn->setToolTip("Tag not in OghamStoryteller.gptags. Click to add.");
            btn->setEnabled(true);
        }
    }

    // =========================================================================
    // OghamStoryteller
    // =========================================================================

    OghamStoryteller::OghamStoryteller(QWidget* parent)
        : QWidget(parent)
    {
        setWindowTitle("Ogham Storyteller");

        // ── Toolbar ─────────────────────────────────────────────────────────
        auto* toolbar  = new QWidget(this);
        auto* tbLayout = new QHBoxLayout(toolbar);
        tbLayout->setContentsMargins(4, 4, 4, 4);
        tbLayout->setSpacing(6);

        m_newFileBtn = new QPushButton("New File...", toolbar);
        tbLayout->addWidget(m_newFileBtn);

        tbLayout->addSpacing(8);

        m_saveAllBtn = new QPushButton("Save All", toolbar);
        m_saveAllBtn->setEnabled(false);
        tbLayout->addWidget(m_saveAllBtn);

        m_openSrcBtn = new QPushButton("Open Source...", toolbar);
        tbLayout->addWidget(m_openSrcBtn);

        tbLayout->addSpacing(16);

        m_layoutBtn = new QPushButton("Layout", toolbar);
        m_layoutBtn->setToolTip("Auto-arrange all visible nodes (BFS tree layout)");
        tbLayout->addWidget(m_layoutBtn);

        tbLayout->addStretch();

        // ── Left: 2-column entry tree ────────────────────────────────────────
        m_tree = new QTreeWidget(this);
        m_tree->setColumnCount(2);
        m_tree->setHeaderHidden(true);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_tree->setColumnWidth(1, 100);
        m_tree->setMinimumWidth(150);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

        // ── Right: stacked panel ─────────────────────────────────────────────
        auto* noSelLabel = new QLabel("Select an entry from the tree to edit.", this);
        noSelLabel->setAlignment(Qt::AlignCenter);

        auto* formContent = new QWidget(this);
        auto* formLayout  = new QVBoxLayout(formContent);
        formLayout->setContentsMargins(8, 8, 8, 8);
        formLayout->setSpacing(6);

        {
            m_playFromNodeBtn = new QPushButton(
                "\xe2\x96\xb6 Play from Node", formContent);
            m_playFromNodeBtn->setEnabled(false);
            m_playFromNodeBtn->setStyleSheet(
                "QPushButton { background-color: #2a5c2a; color: white; "
                "font-weight: bold; padding: 5px; border-radius: 3px; }"
                "QPushButton:hover { background-color: #3a7c3a; }"
                "QPushButton:disabled { background-color: #333333; color: #666666; }");
            formLayout->addWidget(m_playFromNodeBtn);
        }
        {
            auto* row = new QHBoxLayout();
            row->addWidget(new QLabel("Tag:", formContent));

            m_tagStatus = new QToolButton(formContent);
            m_tagStatus->setFixedWidth(26);
            m_tagStatus->setAutoRaise(true);
            m_tagStatus->setText("\xe2\x97\x8f");   // ⚫ default until an entry is selected
            m_tagStatus->setStyleSheet("color: #555555;");
            m_tagStatus->setEnabled(false);
            row->addWidget(m_tagStatus);

            m_tagCombo = new QComboBox(formContent);
            m_tagCombo->setEditable(true);
            m_tagCombo->setInsertPolicy(QComboBox::NoInsert);
            m_tagCombo->setPlaceholderText("e.g. Act1.Scene1.Line1");
            auto* tagCpl = new QCompleter(m_tagCombo);
            tagCpl->setCaseSensitivity(Qt::CaseInsensitive);
            tagCpl->setFilterMode(Qt::MatchContains);
            m_tagCombo->setCompleter(tagCpl);
            row->addWidget(m_tagCombo, 1);
            formLayout->addLayout(row);
        }
        {
            auto* row = new QHBoxLayout();
            row->addWidget(new QLabel("Localisation Keys:", formContent));
            row->addStretch();
            auto* addKeyBtn = new QPushButton("+", formContent);
            addKeyBtn->setFixedSize(22, 22);
            addKeyBtn->setToolTip("Add localisation key");
            row->addWidget(addKeyBtn);
            formLayout->addLayout(row);
            connect(addKeyBtn, &QPushButton::clicked, this, &OghamStoryteller::OnAddTextKey);
        }

        m_keysWidget = new QWidget(formContent);
        m_keysLayout = new QVBoxLayout(m_keysWidget);
        m_keysLayout->setContentsMargins(0, 0, 0, 0);
        m_keysLayout->setSpacing(2);
        formLayout->addWidget(m_keysWidget);

        {
            static const char* kEntryOpsTooltip =
                "A collection of Gameplay Tag operations executed when this Dialogue Entry "
                "is entered. Each operation can optionally have conditions that must be true "
                "for it to be applied, such as only setting a tag if another already exists.";
            auto* row = new QHBoxLayout();
            auto* opsLabel = new QLabel("Operations:", formContent);
            opsLabel->setToolTip(kEntryOpsTooltip);
            row->addWidget(opsLabel);
            row->addStretch();
            auto* addOpBtn = new QPushButton("+", formContent);
            addOpBtn->setFixedSize(22, 22);
            addOpBtn->setToolTip(kEntryOpsTooltip);
            row->addWidget(addOpBtn);
            formLayout->addLayout(row);
            connect(addOpBtn, &QPushButton::clicked,
                [this]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    e.entryOperations.append(OghamOperation{});
                    SetFileDirty(m_selectedFileIdx, true);
                    RebuildEntryOpsArea();
                });
        }

        m_entOpsWidget = new QWidget(formContent);
        m_entOpsLayout = new QVBoxLayout(m_entOpsWidget);
        m_entOpsLayout->setContentsMargins(0, 0, 0, 0);
        m_entOpsLayout->setSpacing(2);
        formLayout->addWidget(m_entOpsWidget);
        {
            static const char* kOptionsTooltip =
                "A collection of dialogue options associated with this entry. Options are "
                "typically displayed as choices the player can interact with — buttons, "
                "keywords, or actions that send signals over the Ogham Dialogue system "
                "to trigger specific responses in other systems.";
            auto* row = new QHBoxLayout();
            auto* optsLabel = new QLabel("Options:", formContent);
            optsLabel->setToolTip(kOptionsTooltip);
            row->addWidget(optsLabel);
            row->addStretch();
            auto* addOptBtn = new QPushButton("+", formContent);
            addOptBtn->setFixedSize(22, 22);
            addOptBtn->setToolTip(kOptionsTooltip);
            row->addWidget(addOptBtn);
            formLayout->addLayout(row);
            connect(addOptBtn, &QPushButton::clicked, this, &OghamStoryteller::OnAddOption);
        }

        m_optsWidget = new QWidget(formContent);
        m_optsLayout = new QVBoxLayout(m_optsWidget);
        m_optsLayout->setContentsMargins(0, 0, 0, 0);
        m_optsLayout->setSpacing(4);
        formLayout->addWidget(m_optsWidget);

        formLayout->addStretch();

        auto* formScroll = new QScrollArea(this);
        formScroll->setWidget(formContent);
        formScroll->setWidgetResizable(true);
        formScroll->setMinimumWidth(150);

        m_formStack = new QStackedWidget(this);
        m_formStack->addWidget(noSelLabel);
        m_formStack->addWidget(formScroll);
        m_formStack->setCurrentIndex(0);

        // ── Graph viewport ────────────────────────────────────────────────────
        m_graphView = new OghamGraphView(this);

        // ── Inner splitter: graph | form ──────────────────────────────────────
        m_innerSplitter = new QSplitter(Qt::Horizontal, this);
        m_innerSplitter->addWidget(m_graphView);
        m_innerSplitter->addWidget(m_formStack);
        m_innerSplitter->setStretchFactor(0, 6);
        m_innerSplitter->setStretchFactor(1, 3);
        m_formStack->setMinimumWidth(150);

        // ── Outer splitter: tree | inner ──────────────────────────────────────
        m_splitter = new QSplitter(Qt::Horizontal, this);
        m_splitter->addWidget(m_tree);
        m_splitter->addWidget(m_innerSplitter);
        m_splitter->setStretchFactor(0, 2);
        m_splitter->setStretchFactor(1, 8);
        m_tree->setMinimumWidth(150);

        // ── Status bar ───────────────────────────────────────────────────────
        m_statusLabel = new QLabel("No files loaded.", this);

        // ── Main layout ──────────────────────────────────────────────────────
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(4);
        m_mainStack = new QStackedWidget(this);
        m_mainStack->addWidget(m_splitter);   // page 0: editor

        mainLayout->addWidget(toolbar);
        mainLayout->addWidget(m_mainStack, 1);
        mainLayout->addWidget(m_statusLabel);

        // ── Connections ──────────────────────────────────────────────────────
        connect(m_newFileBtn,  &QPushButton::clicked, this, &OghamStoryteller::OnNewFile);
        connect(m_saveAllBtn,  &QPushButton::clicked, this, &OghamStoryteller::OnSaveAll);
        connect(m_openSrcBtn,  &QPushButton::clicked, this, &OghamStoryteller::OnOpenSource);
        connect(m_layoutBtn,   &QPushButton::clicked, this, &OghamStoryteller::OnLayoutGraph);
        connect(m_playFromNodeBtn, &QPushButton::clicked, this, &OghamStoryteller::OnPlayFromNode);
        connect(m_tagCombo,    &QComboBox::editTextChanged, this, &OghamStoryteller::OnTagEdited);

        // ── Tag rename propagation: fires when user commits the tag field ─────
        connect(m_tagCombo->lineEdit(), &QLineEdit::editingFinished, this,
            [this]()
            {
                if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                const QString newTag = m_tagCombo->currentText();
                if (!IsValidTagStructure(newTag) || newTag == m_renamedFromTag) return;
                const QString oldTag = m_renamedFromTag;
                int refCount = 0;
                for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
                {
                    auto& lf = m_loadedFiles[fi];
                    bool changed = false;
                    for (auto& e : lf.entries)
                    {
                        for (auto& opt : e.options)
                        {
                            if (opt.targetTag == oldTag)
                            {
                                opt.targetTag = newTag;
                                ++refCount;
                                changed = true;
                            }
                        }
                    }
                    // Rename alias pins on the renamed entry that carry the old tag as prefix
                    if (fi == m_selectedFileIdx)
                    {
                        auto& entry = lf.entries[m_selectedEntryIdx];
                        for (auto& ap : entry.aliasPins)
                        {
                            if (ap.tag == oldTag)
                            {
                                ap.tag = newTag;
                                ++refCount;
                                changed = true;
                            }
                            else if (ap.tag.startsWith(oldTag + "."))
                            {
                                ap.tag = newTag + ap.tag.mid(oldTag.size());
                                ++refCount;
                                changed = true;
                            }
                        }
                    }
                    if (changed)
                        SetFileDirty(fi, true);
                }
                m_renamedFromTag = newTag;
                if (refCount > 0)
                {
                    RebuildGraph();
                    m_statusLabel->setText(
                        QString("Updated %1 reference(s) to '%2'.").arg(refCount).arg(newTag));
                }
            });

        connect(m_tree, &QTreeWidget::currentItemChanged,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*)
            {
                if (!current) { ClearForm(); return; }
                int fi = current->data(0, kRoleFileIdx).toInt();
                int ei = current->data(0, kRoleEntryIdx).toInt();
                if (fi >= 0 && fi < m_loadedFiles.size() && ei >= 0)
                    PopulateForm(fi, ei);
                else
                    ClearForm();
            });

        // ── File watcher (reload .ogmcon on external changes) ─────────────────
        m_watchDebounce = new QTimer(this);
        m_watchDebounce->setSingleShot(true);
        m_watchDebounce->setInterval(300);
        connect(m_watchDebounce, &QTimer::timeout, this,
            [this]()
            {
                ScanAndLoadAll();
                RebuildTree();
                if (m_selectedFileIdx >= 0 && m_selectedEntryIdx >= 0)
                    PopulateForm(m_selectedFileIdx, m_selectedEntryIdx);
                UpdateStatusBar();
            });

        m_fileWatcher = new QFileSystemWatcher(this);
        connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            [this](const QString&)
            {
                if (!m_suppressWatcher)
                    m_watchDebounce->start();
            });

        // ── Graph weld signals ────────────────────────────────────────────────
        connect(m_graphView, &OghamGraphView::pinDroppedOnNode,
            this, &OghamStoryteller::OnPinDroppedOnNode);
        connect(m_graphView, &OghamGraphView::pinDroppedOnAlias,
            this, &OghamStoryteller::OnPinDroppedOnAlias);
        connect(m_graphView, &OghamGraphView::pinDroppedOnCanvas,
            this, &OghamStoryteller::OnPinDroppedOnCanvas);
        connect(m_graphView, &OghamGraphView::deleteNodeRequested,
            this, &OghamStoryteller::OnDeleteNodeFromGraph);
        connect(m_graphView, &OghamGraphView::deleteAliasPinRequested,
            this, &OghamStoryteller::OnDeleteAliasPin);
        connect(m_graphView, &OghamGraphView::createNodeRequested,
            this, &OghamStoryteller::OnCreateNodeFromCanvas);
        connect(m_graphView, &OghamGraphView::pinDragStarted,
            [this]() { m_isInteracting = true; });
        connect(m_graphView, &OghamGraphView::pinDragEnded,
            [this]() { m_isInteracting = false; });

        // ── Tree context menu — "Move to file…" ──────────────────────────────
        connect(m_tree, &QTreeWidget::customContextMenuRequested,
            [this](const QPoint& pos)
            {
                QTreeWidgetItem* item = m_tree->itemAt(pos);
                if (!item) return;
                const int fi = item->data(0, kRoleFileIdx).toInt();
                const int ei = item->data(0, kRoleEntryIdx).toInt();
                if (fi < 0 || ei < 0 || m_loadedFiles.size() <= 1) return;

                QMenu menu(this);
                QMenu* moveMenu = menu.addMenu("Move to file\xe2\x80\xa6");
                for (int dfi = 0; dfi < m_loadedFiles.size(); ++dfi)
                {
                    if (dfi == fi) continue;
                    const QString label = QFileInfo(m_loadedFiles[dfi].path).fileName();
                    QAction* act = moveMenu->addAction(label);
                    connect(act, &QAction::triggered,
                        [this, fi, ei, dfi]() { MoveEntryToFile(fi, ei, dfi); });
                }
                menu.exec(m_tree->viewport()->mapToGlobal(pos));
            });

        ClearForm();
    }

    // -------------------------------------------------------------------------
    // showEvent
    // -------------------------------------------------------------------------

    void OghamStoryteller::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);

        QSettings cfg("HeathenEngineering", "OghamStoryteller");
        if (cfg.contains("splitterState"))
            m_splitter->restoreState(cfg.value("splitterState").toByteArray());
        else
            m_splitter->setSizes({ 200, 800 });   // first launch: tree 200px
        if (cfg.contains("innerSplitterState"))
            m_innerSplitter->restoreState(cfg.value("innerSplitterState").toByteArray());
        else
            m_innerSplitter->setSizes({ 500, 500 }); // first launch: graph 1:1 form

        EnsureOghamTagsFile();
        ScanAndLoadAll();
        RebuildTree();
        UpdateStatusBar();
    }

    void OghamStoryteller::closeEvent(QCloseEvent* event)
    {
        QSettings cfg("HeathenEngineering", "OghamStoryteller");
        cfg.setValue("splitterState",      m_splitter->saveState());
        cfg.setValue("innerSplitterState", m_innerSplitter->saveState());
        QWidget::closeEvent(event);
    }

    // -------------------------------------------------------------------------
    // Toolbar slots
    // -------------------------------------------------------------------------

    void OghamStoryteller::OnNewFile()
    {
        QString defaultDir;
        if (m_selectedFileIdx >= 0 && m_selectedFileIdx < m_loadedFiles.size())
        {
            defaultDir = QFileInfo(m_loadedFiles[m_selectedFileIdx].path).absolutePath();
        }
        else if (m_loadedFiles.isEmpty())
        {
            AZ::IO::FixedMaxPath pp = AZ::Utils::GetProjectPath();
            if (!pp.empty())
            {
                QDir storytellerDir(
                    QString::fromUtf8(pp.c_str()) + "/Assets/Storyteller");
                if (!storytellerDir.exists())
                    storytellerDir.mkpath(".");
                defaultDir = storytellerDir.absolutePath();
            }
            else
            {
                defaultDir = QDir::homePath();
            }
        }
        else
        {
            AZ::IO::FixedMaxPath pp = AZ::Utils::GetProjectPath();
            defaultDir = pp.empty()
                ? QDir::homePath()
                : QString::fromUtf8(pp.c_str());
        }

        QString path = QFileDialog::getSaveFileName(this,
            "New Ogham Source File", defaultDir,
            "Ogham Source (*.ogmcon)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".ogmcon")) path += ".ogmcon";

        {
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly))
            {
                QMessageBox::critical(this, "Error",
                    QString("Cannot create: %1").arg(path));
                return;
            }
            QJsonObject root; root["entries"] = QJsonArray();
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }

        LoadedFile lf;
        lf.path  = path;
        lf.dirty = false;
        m_loadedFiles.append(lf);
        RebuildTree();
        UpdateStatusBar();
    }

    void OghamStoryteller::OnSaveAll()
    {
        int saved = 0;
        for (int i = 0; i < m_loadedFiles.size(); ++i)
        {
            if (m_loadedFiles[i].dirty && SaveFile(i))
            {
                m_loadedFiles[i].dirty = false;
                ++saved;
            }
        }
        if (saved > 0)
        {
            RebuildTree();
            m_statusLabel->setText(QString("Saved %1 file(s).").arg(saved));
        }
        UpdateStatusBar();
    }

    void OghamStoryteller::OnOpenSource()
    {
        if (m_selectedFileIdx >= 0 && m_selectedFileIdx < m_loadedFiles.size())
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(m_loadedFiles[m_selectedFileIdx].path));
    }

    // ── OghamInitStateDialog ─────────────────────────────────────────────────
    // Local-only dialog for choosing initial tag state before entering play mode.
    // Loads/saves named states from OghamTestStates.json in the Storyteller folder.

    class OghamInitStateDialog : public QDialog
    {
    public:
        explicit OghamInitStateDialog(QWidget* parent)
            : QDialog(parent)
        {
            setWindowTitle("Set Initial State");
            setMinimumSize(440, 380);
            resize(480, 420);

            resolveStatesPath();
            loadStates();

            auto* mainLayout = new QVBoxLayout(this);
            mainLayout->setSpacing(8);

            // Saved-state bar
            auto* stateRow = new QHBoxLayout();
            stateRow->addWidget(new QLabel("Saved State:", this));
            m_statesCombo = new QComboBox(this);
            m_statesCombo->addItem("(blank)");
            for (const QString& name : m_states.keys())
                m_statesCombo->addItem(name);
            stateRow->addWidget(m_statesCombo, 1);

            auto* saveBtn = new QPushButton("Save\xe2\x80\xa6", this);
            saveBtn->setFixedWidth(64);
            stateRow->addWidget(saveBtn);

            auto* delBtn = new QPushButton("Delete", this);
            delBtn->setFixedWidth(64);
            stateRow->addWidget(delBtn);
            mainLayout->addLayout(stateRow);

            // Tag/value table
            mainLayout->addWidget(new QLabel("Tag State:", this));
            m_table = new QTableWidget(0, 2, this);
            m_table->setHorizontalHeaderLabels({ "Tag", "Value" });
            m_table->horizontalHeader()->setStretchLastSection(false);
            m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
            m_table->setColumnWidth(1, 80);
            mainLayout->addWidget(m_table, 1);

            // Add/Remove row buttons
            auto* rowBtns = new QHBoxLayout();
            auto* addRowBtn = new QPushButton("+ Add Tag", this);
            auto* remRowBtn = new QPushButton("- Remove", this);
            rowBtns->addWidget(addRowBtn);
            rowBtns->addWidget(remRowBtn);
            rowBtns->addStretch();
            mainLayout->addLayout(rowBtns);

            // Dialog buttons
            auto* buttons = new QDialogButtonBox(this);
            auto* playBtn  = buttons->addButton(
                "\xe2\x96\xb6 Play", QDialogButtonBox::AcceptRole);
            auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);
            Q_UNUSED(playBtn); Q_UNUSED(cancelBtn);
            mainLayout->addWidget(buttons);

            // Connections
            connect(m_statesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applySelectedState(); });

            connect(saveBtn, &QPushButton::clicked, this, [this]()
            {
                bool ok = false;
                const QString name = QInputDialog::getText(
                    this, "Save State", "Name:", QLineEdit::Normal, {}, &ok);
                if (!ok || name.isEmpty()) return;
                m_states[name] = currentTableState();
                saveStates();
                // Refresh combo preserving position
                m_statesCombo->blockSignals(true);
                while (m_statesCombo->count() > 1) m_statesCombo->removeItem(1);
                for (const QString& n : m_states.keys()) m_statesCombo->addItem(n);
                const int idx = m_statesCombo->findText(name);
                m_statesCombo->setCurrentIndex(idx >= 0 ? idx : 0);
                m_statesCombo->blockSignals(false);
            });

            connect(delBtn, &QPushButton::clicked, this, [this]()
            {
                const QString name = m_statesCombo->currentText();
                if (name == "(blank)" || !m_states.contains(name)) return;
                m_states.remove(name);
                saveStates();
                m_statesCombo->removeItem(m_statesCombo->currentIndex());
            });

            connect(addRowBtn, &QPushButton::clicked, this, [this]()
            {
                const int row = m_table->rowCount();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(""));
                m_table->setItem(row, 1, new QTableWidgetItem("0"));
                m_table->editItem(m_table->item(row, 0));
            });

            connect(remRowBtn, &QPushButton::clicked, this, [this]()
            {
                const int row = m_table->currentRow();
                if (row >= 0) m_table->removeRow(row);
            });

            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        }

        QHash<QString, int> initialState() const { return currentTableState(); }

    private:
        void resolveStatesPath()
        {
            AZ::IO::FixedMaxPath pp = AZ::Utils::GetProjectPath();
            if (pp.empty()) return;
            m_statesPath = QString::fromUtf8(pp.c_str())
                         + "/Assets/Storyteller/OghamTestStates.json";
        }

        void loadStates()
        {
            QFile f(m_statesPath);
            if (!f.open(QIODevice::ReadOnly)) return;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error != QJsonParseError::NoError) return;

            const QJsonObject allStates = doc.object().value("states").toObject();
            for (auto it = allStates.begin(); it != allStates.end(); ++it)
            {
                QHash<QString, int> tagMap;
                const QJsonObject tagObj = it.value().toObject();
                for (auto jt = tagObj.begin(); jt != tagObj.end(); ++jt)
                    tagMap[jt.key()] = jt.value().toInt();
                m_states[it.key()] = tagMap;
            }
        }

        void saveStates()
        {
            QJsonObject allStates;
            for (auto it = m_states.begin(); it != m_states.end(); ++it)
            {
                QJsonObject tagObj;
                for (auto jt = it.value().begin(); jt != it.value().end(); ++jt)
                    tagObj[jt.key()] = jt.value();
                allStates[it.key()] = tagObj;
            }
            QJsonObject root;
            root["states"] = allStates;

            QFileInfo fi(m_statesPath);
            fi.dir().mkpath(".");

            QFile f(m_statesPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }

        void applySelectedState()
        {
            const QString name = m_statesCombo->currentText();
            m_table->setRowCount(0);
            if (name == "(blank)" || !m_states.contains(name)) return;
            const QHash<QString, int>& state = m_states[name];
            QStringList keys = state.keys();
            keys.sort();
            for (const QString& k : keys)
            {
                const int row = m_table->rowCount();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(k));
                m_table->setItem(row, 1,
                    new QTableWidgetItem(QString::number(state[k])));
            }
        }

        QHash<QString, int> currentTableState() const
        {
            QHash<QString, int> result;
            for (int r = 0; r < m_table->rowCount(); ++r)
            {
                const QTableWidgetItem* tagItem = m_table->item(r, 0);
                const QTableWidgetItem* valItem = m_table->item(r, 1);
                if (!tagItem || tagItem->text().trimmed().isEmpty()) continue;
                result[tagItem->text().trimmed()] =
                    valItem ? valItem->text().toInt() : 0;
            }
            return result;
        }

        QString                            m_statesPath;
        QHash<QString, QHash<QString,int>> m_states;
        QComboBox*    m_statesCombo = nullptr;
        QTableWidget* m_table       = nullptr;
    };

    // ── OnPlayFromNode ────────────────────────────────────────────────────────

    void OghamStoryteller::OnPlayFromNode()
    {
        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;

        OghamInitStateDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;

        // Close any existing play panel
        if (m_playPanel)
        {
            m_mainStack->setCurrentIndex(0);
            m_mainStack->removeWidget(m_playPanel);
            m_playPanel->deleteLater();
            m_playPanel = nullptr;
        }

        m_playPanel = new OghamPlayPanel(
            m_loadedFiles,
            m_selectedFileIdx,
            m_selectedEntryIdx,
            dlg.initialState(),
            this);

        m_mainStack->addWidget(m_playPanel);   // page 1
        m_mainStack->setCurrentIndex(1);

        connect(m_playPanel, &OghamPlayPanel::stopRequested, this, [this]()
        {
            m_mainStack->setCurrentIndex(0);
            m_mainStack->removeWidget(m_playPanel);
            m_playPanel->deleteLater();
            m_playPanel = nullptr;
        });
    }

    void OghamStoryteller::OnTagEdited()
    {
        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
        auto& entry = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
        entry.tag = m_tagCombo->currentText();
        SetFileDirty(m_selectedFileIdx, true);

        // Update tristate status
        ApplyTagStatus(m_tagStatus, entry.tag, FetchKnownOghamTags());

        // Connect status button click to add-to-gptags
        disconnect(m_tagStatus, nullptr, nullptr, nullptr);
        if (m_tagStatus->isEnabled())
        {
            connect(m_tagStatus, &QToolButton::clicked,
                [this]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    const QString tag =
                        m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].tag;
                    AddTagToGptagsFile(tag);
                    ApplyTagStatus(m_tagStatus, tag, FetchKnownOghamTags());
                    disconnect(m_tagStatus, nullptr, nullptr, nullptr);
                });
        }

        // Refresh the tree item label without full rebuild
        QTreeWidgetItem* cur = m_tree->currentItem();
        if (cur && cur->data(0, kRoleEntryIdx).toInt() == m_selectedEntryIdx)
        {
            int dot = entry.tag.lastIndexOf('.');
            cur->setText(0, dot < 0 ? entry.tag : entry.tag.mid(dot + 1));
            cur->setData(0, kRoleNodeTag, entry.tag);
        }
    }

    void OghamStoryteller::OnAddTextKey()
    {
        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
        m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].dataKeys.append("");
        SetFileDirty(m_selectedFileIdx, true);
        RebuildTextKeysArea();
    }

    void OghamStoryteller::OnAddOption()
    {
        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
        auto& entry = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
        OghamSourceOption opt;
        opt.tag = entry.tag + ".Option" +
                  QString::number(entry.options.size() + 1);
        entry.options.append(opt);
        SetFileDirty(m_selectedFileIdx, true);
        RebuildOptionsArea();
    }

    // -------------------------------------------------------------------------
    // File operations
    // -------------------------------------------------------------------------

    void OghamStoryteller::ScanAndLoadAll()
    {
        AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
        if (projectPath.empty()) return;

        // Use QDirIterator directly on the real filesystem path so stored paths
        // are always plain OS paths, never @projectroot@ aliases.
        QStringList found;
        QDirIterator it(QString::fromUtf8(projectPath.c_str()),
            QStringList() << "*.ogmcon", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            found.append(it.next());

        // Merge: keep already-loaded (preserving dirty state), add new
        QList<LoadedFile> merged;
        for (const QString& fp : found)
        {
            bool alreadyLoaded = false;
            for (auto& lf : m_loadedFiles)
            {
                if (lf.path == fp)
                {
                    merged.append(lf);
                    alreadyLoaded = true;
                    break;
                }
            }
            if (!alreadyLoaded)
            {
                LoadedFile lf;
                lf.path  = fp;
                lf.dirty = false;
                ParseFile(fp, lf);
                merged.append(lf);
            }
        }
        m_loadedFiles = merged;

        // Update watcher — remove all, then add current set + .gptags + .helex files (C)
        if (m_fileWatcher)
        {
            if (!m_fileWatcher->files().isEmpty())
                m_fileWatcher->removePaths(m_fileWatcher->files());
            m_fileWatcher->addPaths(found);

            // Also watch the .gptags file so tag combos refresh when it changes
            if (!m_oghamTagsPath.isEmpty() && QFile::exists(m_oghamTagsPath))
                m_fileWatcher->addPath(m_oghamTagsPath);

            // Also watch .helex files so lexicon key combos refresh when they change
            AZStd::vector<AZStd::string> azLexPaths;
            FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
                azLexPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);
            auto* fileIO = AZ::IO::FileIOBase::GetInstance();
            for (const auto& p : azLexPaths)
            {
                AZ::IO::FixedMaxPath resolved;
                QString qp;
                if (fileIO && fileIO->ResolvePath(resolved, p.c_str()))
                    qp = QString::fromUtf8(resolved.c_str());
                else
                    qp = QString::fromUtf8(p.c_str());
                if (!qp.isEmpty() && QFile::exists(qp))
                    m_fileWatcher->addPath(qp);
            }
        }

        // Clamp selection
        if (m_selectedFileIdx >= m_loadedFiles.size())
        {
            m_selectedFileIdx  = -1;
            m_selectedEntryIdx = -1;
        }
    }

    // ── JSON <-> typed struct helpers ────────────────────────────────────────

    static OghamCondition ParseCondition(const QJsonObject& co)
    {
        OghamCondition c;
        c.tag        = co["tag"].toString();
        c.comparison = co["comparison"].toString(QStringLiteral("Exists"));
        c.value      = co["value"].toInt(0);
        c.logicOp    = co["logicOp"].toString(QStringLiteral("AND"));
        return c;
    }

    static QJsonObject SerialiseCondition(const OghamCondition& c)
    {
        QJsonObject o;
        o["tag"]        = c.tag;
        o["comparison"] = c.comparison;
        o["value"]      = c.value;
        o["logicOp"]    = c.logicOp;
        return o;
    }

    static QPointF ParsePoint(const QJsonObject& o)
    {
        return { o["x"].toDouble(0.0), o["y"].toDouble(0.0) };
    }

    static QJsonObject SerialisePoint(QPointF p)
    {
        QJsonObject o;
        o["x"] = p.x();
        o["y"] = p.y();
        return o;
    }

    static QList<OghamCondition> ParseConditions(const QJsonArray& arr)
    {
        QList<OghamCondition> out;
        for (const QJsonValue v : arr)
            if (v.isObject()) out.append(ParseCondition(v.toObject()));
        return out;
    }

    static QJsonArray SerialiseConditions(const QList<OghamCondition>& list)
    {
        QJsonArray arr;
        for (const OghamCondition& c : list) arr.append(SerialiseCondition(c));
        return arr;
    }

    static OghamOperation ParseOperation(const QJsonObject& oo)
    {
        OghamOperation op;
        op.tag        = oo["tag"].toString();
        op.arithmetic = oo["arithmetic"].toString(QStringLiteral("Set"));
        op.value      = oo["value"].toInt(0);
        op.conditions = ParseConditions(oo["conditions"].toArray());
        return op;
    }

    static QJsonObject SerialiseOperation(const OghamOperation& op)
    {
        QJsonObject o;
        o["tag"]        = op.tag;
        o["arithmetic"] = op.arithmetic;
        o["value"]      = op.value;
        o["conditions"] = SerialiseConditions(op.conditions);
        return o;
    }

    static QList<OghamOperation> ParseOperations(const QJsonArray& arr)
    {
        QList<OghamOperation> out;
        for (const QJsonValue v : arr)
            if (v.isObject()) out.append(ParseOperation(v.toObject()));
        return out;
    }

    static QJsonArray SerialiseOperations(const QList<OghamOperation>& list)
    {
        QJsonArray arr;
        for (const OghamOperation& op : list) arr.append(SerialiseOperation(op));
        return arr;
    }

    // ─────────────────────────────────────────────────────────────────────────

    bool OghamStoryteller::ParseFile(const QString& path, LoadedFile& lf)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (doc.isNull()) return false;

        const QJsonObject root = doc.object();

        // Read per-file color (stored as RGB integer; absent = invalid/palette default)
        if (root.contains("fileColor"))
        {
            const QRgb rgb = static_cast<QRgb>(root["fileColor"].toInt(-1));
            lf.fileColor = QColor::fromRgb(rgb);
        }

        const QJsonArray allEntries = root["entries"].toArray();
        for (const QJsonValue& ev : allEntries)
        {
            QJsonObject eo = ev.toObject();
            OghamSourceEntry entry;
            entry.tag             = eo["tag"].toString();
            entry.entryOperations = ParseOperations(eo["entryOperations"].toArray());

            if (eo.contains("position"))
                entry.position = ParsePoint(eo["position"].toObject());

            // dataKeys is the current name; textKeys is the legacy fallback
            const QString keysField = eo.contains("dataKeys") ? "dataKeys" : "textKeys";
            const QJsonArray tks = eo[keysField].toArray();
            for (const QJsonValue& tk : tks) entry.dataKeys.append(tk.toString());

            const QJsonArray pins = eo["aliasPins"].toArray();
            for (const QJsonValue& pv : pins)
            {
                QJsonObject po = pv.toObject();
                OghamAliasPin pin;
                pin.tag      = po["tag"].toString();
                pin.pinId    = po["pinId"].toInt(0);
                if (po.contains("position"))
                    pin.position = ParsePoint(po["position"].toObject());
                entry.aliasPins.append(pin);
            }

            const QJsonArray opts = eo["options"].toArray();
            for (const QJsonValue& ov : opts)
            {
                QJsonObject oo = ov.toObject();
                OghamSourceOption opt;
                opt.tag              = oo["tag"].toString();
                opt.textKey          = oo["textKey"].toString();
                // targetTag is the current name; targetEntry is the legacy fallback
                opt.targetTag        = oo.contains("targetTag")
                                         ? oo["targetTag"].toString()
                                         : oo["targetEntry"].toString();
                opt.targetAliasIndex = oo["targetAliasIndex"].toInt(0);
                const QJsonArray rds = oo["redirects"].toArray();
                for (const QJsonValue& rv : rds)
                    if (rv.isObject()) opt.redirects.append(ParsePoint(rv.toObject()));
                opt.conditions  = ParseConditions(oo["conditions"].toArray());
                opt.operations  = ParseOperations(oo["operations"].toArray());
                entry.options.append(opt);
            }
            lf.entries.append(entry);
        }
        return true;
    }

    bool OghamStoryteller::SaveFile(int fileIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return false;
        const LoadedFile& lf = m_loadedFiles[fileIdx];

        QJsonArray entriesArr;
        for (const OghamSourceEntry& entry : lf.entries)
        {
            QJsonObject eo;
            eo["tag"]             = entry.tag;
            eo["position"]        = SerialisePoint(entry.position);
            eo["entryOperations"] = SerialiseOperations(entry.entryOperations);
            QJsonArray tks;
            for (const QString& k : entry.dataKeys) tks.append(k);
            eo["dataKeys"] = tks;
            QJsonArray pinsArr;
            for (const OghamAliasPin& pin : entry.aliasPins)
            {
                QJsonObject po;
                po["tag"]      = pin.tag;
                po["position"] = SerialisePoint(pin.position);
                po["pinId"]    = pin.pinId;
                pinsArr.append(po);
            }
            eo["aliasPins"] = pinsArr;
            QJsonArray optsArr;
            for (const OghamSourceOption& opt : entry.options)
            {
                QJsonObject oo;
                oo["tag"]              = opt.tag;
                oo["textKey"]          = opt.textKey;
                oo["targetTag"]        = opt.targetTag;
                oo["targetAliasIndex"] = opt.targetAliasIndex;
                QJsonArray rdsArr;
                for (const QPointF& rd : opt.redirects) rdsArr.append(SerialisePoint(rd));
                oo["redirects"]   = rdsArr;
                oo["conditions"]  = SerialiseConditions(opt.conditions);
                oo["operations"]  = SerialiseOperations(opt.operations);
                optsArr.append(oo);
            }
            eo["options"] = optsArr;
            entriesArr.append(eo);
        }

        QJsonObject root;
        root["entries"] = entriesArr;
        if (lf.fileColor.isValid())
            root["fileColor"] = static_cast<int>(lf.fileColor.rgb());
        QFile f(lf.path);
        if (!f.open(QIODevice::WriteOnly))
        {
            QMessageBox::critical(this, "Save Error",
                QString("Cannot write: %1").arg(lf.path));
            return false;
        }
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }

    // -------------------------------------------------------------------------
    // Lexicon key helpers
    // -------------------------------------------------------------------------

    QStringList OghamStoryteller::FetchKnownLexiconKeys() const
    {
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);

        QSet<QString> keysSet;
        auto* fileIO = AZ::IO::FileIOBase::GetInstance();
        for (const auto& p : azPaths)
        {
            AZ::IO::FixedMaxPath resolved;
            QString qp;
            if (fileIO && fileIO->ResolvePath(resolved, p.c_str()))
                qp = QString::fromUtf8(resolved.c_str());
            else
                qp = QString::fromUtf8(p.c_str());

            QFile f(qp);
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error != QJsonParseError::NoError || !doc.isObject()) continue;
            const QJsonObject entries = doc.object()[QStringLiteral("entries")].toObject();
            for (auto it = entries.constBegin(); it != entries.constEnd(); ++it)
                keysSet.insert(it.key());
        }

        QStringList result = QStringList(keysSet.begin(), keysSet.end());
        result.sort(Qt::CaseInsensitive);
        return result;
    }

    QString OghamStoryteller::FetchLexiconValueForKey(const QString& key) const
    {
        if (key.isEmpty()) return {};

        // Get the discovered .helex file paths from the editor bus
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);
        if (azPaths.empty()) return {};

        // Prefer a file named "default.helex"; fall back to the first file
        QString chosenPath;
        for (const auto& p : azPaths)
        {
            QString qp = QString::fromUtf8(p.c_str());
            if (QFileInfo(qp).baseName().compare("default", Qt::CaseInsensitive) == 0)
            {
                chosenPath = qp;
                break;
            }
        }
        if (chosenPath.isEmpty())
            chosenPath = QString::fromUtf8(azPaths.front().c_str());

        QFile f(chosenPath);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError) return {};

        QJsonObject root = doc.object();
        QJsonObject entries = root.value("entries").toObject();
        QJsonValue val = entries.value(key);
        if (val.isUndefined()) return {};
        if (val.isString()) return val.toString();
        if (val.isObject())
        {
            QString uuid = val.toObject().value("uuid").toString();
            return uuid.isEmpty() ? QString() : QString("[asset] %1").arg(uuid);
        }
        return {};
    }

    void OghamStoryteller::ShowAddKeyDialog(const QString& suggestedKey,
                                             const QString& suggestedValue)
    {
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);

        // Resolve @projectroot@ and other engine aliases to real filesystem paths
        // before passing to QFile — QFile does not understand AZ IO aliases.
        QStringList filePaths;
        auto* fileIO = AZ::IO::FileIOBase::GetInstance();
        for (const auto& p : azPaths)
        {
            AZ::IO::FixedMaxPath resolved;
            if (fileIO && fileIO->ResolvePath(resolved, p.c_str()))
                filePaths.append(QString::fromUtf8(resolved.c_str()));
            else
                filePaths.append(QString::fromUtf8(p.c_str()));
        }

        if (filePaths.isEmpty())
        {
            QMessageBox::information(this, "No Lexicons Found",
                "No .helex files found in the project.\nCreate a Lexicon with the Lexicon tool first.");
            return;
        }

        AddKeyDialog dlg(suggestedKey, suggestedValue, filePaths, this);
        if (dlg.exec() != QDialog::Accepted) return;

        const QString key         = dlg.key();
        const QString value       = dlg.value();
        const QString primaryPath = dlg.primaryFilePath();
        const QString placeholder = QString("[%1 \xe2\x80\x94 update me]").arg(key);

        if (key.isEmpty()) { QMessageBox::warning(this, "Empty Key", "Key cannot be empty."); return; }

        int written = 0; int skipped = 0; QStringList errors;
        for (const QString& fp : filePaths)
        {
            QFile f(fp);
            if (!f.open(QIODevice::ReadOnly)) { ++skipped; errors.append(fp + ": read error"); continue; }
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
            f.close();
            if (doc.isNull()) { ++skipped; errors.append(fp + ": invalid JSON"); continue; }

            QJsonObject root    = doc.object();
            QJsonObject entries = root["entries"].toObject();
            if (entries.contains(key)) { ++skipped; continue; }

            entries[key]    = (fp == primaryPath) ? value : placeholder;
            root["entries"] = entries;

            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            { ++skipped; errors.append(fp + ": write error"); continue; }
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            ++written;
        }

        FoundationLocalisation::LexiconEditorRequestBus::Broadcast(
            &FoundationLocalisation::LexiconEditorRequests::RefreshKeyTree);

        QString msg = QString("Key '%1' added to %2 Lexicon(s).").arg(key).arg(written);
        if (skipped > 0) msg += QString("\n%1 skipped (already present or error).").arg(skipped);
        if (!errors.isEmpty()) msg += "\n\nErrors:\n" + errors.join("\n");
        if (written > 1) msg += "\n\nRemember to update non-primary Lexicons.";

        QMessageBox::information(this, "Key Added", msg);
        RebuildTextKeysArea();
    }

    // -------------------------------------------------------------------------
    // Ogham tag file helpers
    // -------------------------------------------------------------------------

    void OghamStoryteller::EnsureOghamTagsFile()
    {
        AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
        if (projectPath.empty()) return;

        QDir dir(QString::fromUtf8(projectPath.c_str()) + "/Assets/Storyteller");
        if (!dir.exists())
            dir.mkpath(".");

        const QString path = dir.absoluteFilePath("OghamStoryteller.gptags");
        if (!QFile::exists(path))
        {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly))
            {
                QJsonObject root;
                root["registered"] = false;
                root["tags"]       = QJsonArray();
                f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            }
        }

        m_oghamTagsPath = path;
    }

    QStringList OghamStoryteller::FetchKnownOghamTags() const
    {
        if (m_oghamTagsPath.isEmpty()) return {};

        QFile f(m_oghamTagsPath);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError) return {};

        const QJsonArray arr = doc.object().value("tags").toArray();
        QStringList result;
        result.reserve(arr.size());
        for (const QJsonValue v : arr)
            if (v.isString() && !v.toString().isEmpty())
                result.append(v.toString());
        return result;
    }

    QStringList OghamStoryteller::FetchAllEntryTags() const
    {
        QStringList result;
        for (const LoadedFile& lf : m_loadedFiles)
            for (const OghamSourceEntry& entry : lf.entries)
                if (!entry.tag.isEmpty() && !result.contains(entry.tag))
                    result.append(entry.tag);
        return result;
    }

    void OghamStoryteller::AddTagToGptagsFile(const QString& tag)
    {
        AddTagToFile(tag, m_oghamTagsPath);
    }

    void OghamStoryteller::AddTagToFile(const QString& tag, const QString& filePath)
    {
        if (filePath.isEmpty() || tag.isEmpty()) return;

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError) return;

        QJsonObject root = doc.object();
        QJsonArray arr   = root.value("tags").toArray();

        for (const QJsonValue v : arr)
            if (v.isString() && v.toString() == tag) return;

        arr.append(tag);
        root["tags"] = arr;

        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    QStringList OghamStoryteller::FetchAllGptagsFiles() const
    {
        AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
        if (projectPath.empty()) return {};

        const QString root = QString::fromUtf8(projectPath.c_str());
        QStringList result;
        QDirIterator it(root,
                        QStringList() << QStringLiteral("*.gptags"),
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            result.append(it.next());
        result.sort(Qt::CaseInsensitive);
        return result;
    }

    QStringList OghamStoryteller::FetchAllTagsFromAllFiles() const
    {
        QSet<QString> seen;
        for (const QString& filePath : FetchAllGptagsFiles())
        {
            QFile f(filePath);
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error != QJsonParseError::NoError) continue;
            for (const QJsonValue v : doc.object().value("tags").toArray())
                if (v.isString() && !v.toString().isEmpty())
                    seen.insert(v.toString());
        }
        QStringList result(seen.begin(), seen.end());
        result.sort(Qt::CaseInsensitive);
        return result;
    }

    void OghamStoryteller::ShowAddTagDialog(const QString& tag, QToolButton* btn)
    {
        const QStringList files = FetchAllGptagsFiles();
        if (files.isEmpty())
        {
            QMessageBox::information(this, "No .gptags Files",
                "No .gptags files found in the project.\n"
                "Create a .gptags source file first (e.g. Assets/MyTags.gptags).");
            return;
        }

        auto* dlg    = new QDialog(this, Qt::Dialog);
        dlg->setWindowTitle(QStringLiteral("Add GameplayTag"));
        dlg->setMinimumWidth(440);
        auto* vl = new QVBoxLayout(dlg);

        vl->addWidget(new QLabel(
            QString("Tag <b>%1</b> is not in any .gptags file.<br>"
                    "Choose which file to add it to.<br>"
                    "Each .gptags file may be marked as <i>registered</i> (always active) "
                    "or not — story entries are typically unregistered; "
                    "inventory/NPC groupings typically registered.")
                .arg(tag.toHtmlEscaped()), dlg));

        auto* fileCombo = new QComboBox(dlg);
        for (const QString& fp : files)
            fileCombo->addItem(QFileInfo(fp).fileName(), fp);
        // Pre-select OghamStoryteller.gptags if present, else first
        for (int i = 0; i < fileCombo->count(); ++i)
            if (QFileInfo(fileCombo->itemData(i).toString()).baseName()
                    .compare("OghamStoryteller", Qt::CaseInsensitive) == 0)
            { fileCombo->setCurrentIndex(i); break; }

        vl->addWidget(fileCombo);

        // Show the registered state of the selected file
        auto* regLabel = new QLabel(dlg);
        regLabel->setTextFormat(Qt::RichText);
        auto updateRegLabel = [regLabel, fileCombo]()
        {
            const QString fp = fileCombo->currentData().toString();
            QFile f(fp);
            if (!f.open(QIODevice::ReadOnly)) { regLabel->clear(); return; }
            QJsonParseError e;
            auto doc = QJsonDocument::fromJson(f.readAll(), &e);
            f.close();
            const bool reg = e.error == QJsonParseError::NoError
                ? doc.object()["registered"].toBool()
                : false;
            regLabel->setText(reg
                ? "<span style='color:#44aa44;'>This file is <b>registered</b> (tags always active).</span>"
                : "<span style='color:#888888;'>This file is <b>not registered</b> (tags require runtime registration).</span>");
        };
        updateRegLabel();
        connect(fileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [updateRegLabel](int) { updateRegLabel(); });
        vl->addWidget(regLabel);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        vl->addWidget(btns);

        if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

        const QString chosen = fileCombo->currentData().toString();
        dlg->deleteLater();

        AddTagToFile(tag, chosen);

        // Refresh the status icon
        const QStringList allTags = FetchKnownOghamTags() + FetchAllEntryTags();
        if (btn)
            ApplyTagStatus(btn, tag, allTags);
    }

    // -------------------------------------------------------------------------
    // Tree building
    // -------------------------------------------------------------------------

    QWidget* OghamStoryteller::MakeFileButtons(int fileIdx)
    {
        auto* w  = new QWidget();
        auto* hl = new QHBoxLayout(w);
        hl->setContentsMargins(2, 1, 2, 1);
        hl->setSpacing(2);

        // Stretch first so buttons sit on the right edge of the column
        hl->addStretch();

        // Eye toggle — show/hide this file's nodes in the graph
        const bool initVis = (fileIdx < m_loadedFiles.size()) && m_loadedFiles[fileIdx].visible;
        auto* eyeBtn = new QPushButton(initVis ? "\xe2\x97\x8f" : "\xe2\x97\x8b", w);  // ● / ○
        eyeBtn->setFixedSize(22, 22);
        eyeBtn->setStyleSheet(initVis ? "color: #6ab0d0;" : "color: #555555;");
        eyeBtn->setToolTip(initVis ? "Hide nodes in graph" : "Show nodes in graph");
        hl->addWidget(eyeBtn);

        auto* addRoot = new QPushButton("+", w);
        addRoot->setFixedSize(22, 22);
        addRoot->setToolTip("Add root entry to this file");
        hl->addWidget(addRoot);

        auto* openBtn = new QPushButton("\xe2\x80\xa6", w);  // …
        openBtn->setFixedSize(22, 22);
        openBtn->setToolTip("Open source file");
        hl->addWidget(openBtn);

        connect(eyeBtn, &QPushButton::clicked,
            [this, fileIdx, eyeBtn]()
            {
                if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
                m_loadedFiles[fileIdx].visible = !m_loadedFiles[fileIdx].visible;
                const bool v = m_loadedFiles[fileIdx].visible;
                eyeBtn->setText(v ? "\xe2\x97\x8f" : "\xe2\x97\x8b");
                eyeBtn->setStyleSheet(v ? "color: #6ab0d0;" : "color: #555555;");
                eyeBtn->setToolTip(v ? "Hide nodes in graph" : "Show nodes in graph");
                ApplyGraphVisibility();
            });
        connect(addRoot, &QPushButton::clicked,
            [this, fileIdx]() { AddRootEntry(fileIdx); });
        connect(openBtn, &QPushButton::clicked,
            [this, fileIdx]()
            {
                if (fileIdx >= 0 && fileIdx < m_loadedFiles.size())
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(m_loadedFiles[fileIdx].path));
            });
        return w;
    }

    QWidget* OghamStoryteller::MakeEntryButtons(int fileIdx, int entryIdx,
                                                  bool isReal, const QString& nodeTag)
    {
        auto* w  = new QWidget();
        auto* hl = new QHBoxLayout(w);
        hl->setContentsMargins(2, 1, 2, 1);
        hl->setSpacing(2);

        // Stretch first so buttons sit on the right edge of the column
        hl->addStretch();

        auto* addSibling = new QPushButton("+", w);
        addSibling->setFixedSize(22, 22);
        addSibling->setToolTip("Add sibling entry");
        hl->addWidget(addSibling);

        auto* addChild = new QPushButton("\xe2\x86\x93", w);  // ↓
        addChild->setFixedSize(22, 22);
        addChild->setToolTip("Add child entry");
        hl->addWidget(addChild);

        if (isReal)
        {
            auto* removeBtn = new QPushButton("X", w);
            removeBtn->setFixedSize(22, 22);
            removeBtn->setToolTip("Remove this entry (children remain)");
            removeBtn->setStyleSheet("color: #cc3333; font-weight: bold;");
            hl->addWidget(removeBtn);
            connect(removeBtn, &QPushButton::clicked,
                [this, fileIdx, entryIdx]() { RemoveEntry(fileIdx, entryIdx); });
        }
        else
        {
            // Keep column width uniform
            auto* spacer = new QWidget(w);
            spacer->setFixedSize(22, 22);
            hl->addWidget(spacer);
        }

        connect(addSibling, &QPushButton::clicked,
            [this, fileIdx, nodeTag]() { AddSiblingEntry(fileIdx, nodeTag); });
        connect(addChild, &QPushButton::clicked,
            [this, fileIdx, nodeTag]() { AddChildEntry(fileIdx, nodeTag); });

        return w;
    }

    void OghamStoryteller::RebuildTree()
    {
        const int prevFi = m_selectedFileIdx;
        const int prevEi = m_selectedEntryIdx;

        m_tree->blockSignals(true);
        m_tree->clear();

        // Build a display order sorted by filename without mutating m_loadedFiles
        // (indices are used everywhere, so we only sort the view).
        QList<int> displayOrder;
        displayOrder.reserve(m_loadedFiles.size());
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
            displayOrder.append(fi);
        std::sort(displayOrder.begin(), displayOrder.end(),
            [this](int a, int b)
            {
                return QFileInfo(m_loadedFiles[a].path).fileName().toLower() <
                       QFileInfo(m_loadedFiles[b].path).fileName().toLower();
            });

        for (int fi : displayOrder)
        {
            const LoadedFile& lf = m_loadedFiles[fi];

            // ── File header ──────────────────────────────────────────────
            auto* fileItem = new QTreeWidgetItem(m_tree);
            fileItem->setData(0, kRoleFileIdx,  fi);
            fileItem->setData(0, kRoleEntryIdx, -1);
            fileItem->setData(0, kRoleNodeTag,  QString());

            // Column 0: [color swatch] [filename label]
            {
                const QString filename = QFileInfo(lf.path).fileName();
                const QColor  swatchColor = lf.fileColor.isValid()
                    ? lf.fileColor
                    : kGraphFileColors[fi % kGraphFileColorCount];

                auto* headerWidget = new QWidget();
                auto* hLayout = new QHBoxLayout(headerWidget);
                hLayout->setContentsMargins(2, 1, 2, 1);
                hLayout->setSpacing(4);

                auto* colorBtn = new QPushButton(headerWidget);
                colorBtn->setFixedSize(12, 12);
                colorBtn->setFlat(true);
                colorBtn->setToolTip("Click to change file color");
                colorBtn->setStyleSheet(
                    QString("QPushButton { background-color: %1; border: 1px solid #666; "
                            "border-radius: 2px; } "
                            "QPushButton:hover { border-color: #aaa; }")
                        .arg(swatchColor.name()));
                connect(colorBtn, &QPushButton::clicked, [this, fi]()
                {
                    QColor cur = m_loadedFiles[fi].fileColor.isValid()
                        ? m_loadedFiles[fi].fileColor
                        : kGraphFileColors[fi % kGraphFileColorCount];
                    QColor chosen = QColorDialog::getColor(cur, this, "File Color");
                    if (!chosen.isValid()) return;
                    m_loadedFiles[fi].fileColor = chosen;
                    SetFileDirty(fi, true);
                    RebuildTree();
                    RebuildGraph();
                });
                hLayout->addWidget(colorBtn);

                auto* nameLabel = new QLabel(
                    QString("%1%2").arg(filename).arg(lf.dirty ? "  \u25CF" : ""),
                    headerWidget);
                nameLabel->setObjectName("fileNameLabel");
                QFont lf2 = nameLabel->font();
                lf2.setBold(true);
                nameLabel->setFont(lf2);
                hLayout->addWidget(nameLabel, 1);

                m_tree->setItemWidget(fileItem, 0, headerWidget);
            }

            m_tree->setItemWidget(fileItem, 1, MakeFileButtons(fi));

            // ── Entry hierarchy ──────────────────────────────────────────
            QMap<QString, QTreeWidgetItem*> nodeMap;

            std::function<QTreeWidgetItem*(const QString&)> getOrCreate =
                [&](const QString& prefix) -> QTreeWidgetItem*
                {
                    if (nodeMap.contains(prefix)) return nodeMap[prefix];

                    int dot = prefix.lastIndexOf('.');
                    QString label = (dot < 0) ? prefix : prefix.mid(dot + 1);

                    QTreeWidgetItem* parentItem = (dot < 0)
                        ? fileItem
                        : getOrCreate(prefix.left(dot));

                    auto* node = new QTreeWidgetItem(parentItem, QStringList(label));
                    node->setData(0, kRoleFileIdx,  fi);
                    node->setData(0, kRoleEntryIdx, -1);       // virtual for now
                    node->setData(0, kRoleNodeTag,  prefix);
                    QFont nf = node->font(0); nf.setItalic(true); node->setFont(0, nf);
                    nodeMap.insert(prefix, node);
                    return node;
                };

            // Sort entries by tag for consistent display
            QList<QPair<QString, int>> sorted;
            for (int i = 0; i < lf.entries.size(); ++i)
                sorted.append({ lf.entries[i].tag, i });
            std::sort(sorted.begin(), sorted.end());

            for (const auto& pair : sorted)
            {
                const QString& tag = pair.first;
                int            ei  = pair.second;
                if (tag.isEmpty()) continue;

                int     dot   = tag.lastIndexOf('.');
                QString label = (dot < 0) ? tag : tag.mid(dot + 1);

                QTreeWidgetItem* item = nodeMap.contains(tag)
                    ? nodeMap[tag]
                    : nullptr;

                if (!item)
                {
                    QTreeWidgetItem* parent = (dot < 0)
                        ? fileItem
                        : getOrCreate(tag.left(dot));
                    item = new QTreeWidgetItem(parent, QStringList(label));
                    nodeMap.insert(tag, item);
                }

                // Upgrade to real entry
                QFont ef = item->font(0); ef.setItalic(false); item->setFont(0, ef);
                item->setData(0, kRoleFileIdx,  fi);
                item->setData(0, kRoleEntryIdx, ei);
                item->setData(0, kRoleNodeTag,  tag);

                m_tree->setItemWidget(item, 1,
                    MakeEntryButtons(fi, ei, true, tag));
            }

            // Button widgets for remaining virtual folder nodes
            for (auto it = nodeMap.begin(); it != nodeMap.end(); ++it)
            {
                QTreeWidgetItem* node = it.value();
                if (node->data(0, kRoleEntryIdx).toInt() == -1)
                    m_tree->setItemWidget(node, 1,
                        MakeEntryButtons(fi, -1, false, it.key()));
            }
        }

        m_tree->expandAll();
        m_tree->blockSignals(false);

        // Restore selection
        if (prevFi >= 0 && prevEi >= 0)
            SelectEntry(prevFi, prevEi);

        RebuildGraph();
    }

    void OghamStoryteller::RebuildGraph()
    {
        QGraphicsScene* scene = m_graphView->graphScene();
        scene->clear();

        // ── Phase 1: create nodes and build tag lookup ────────────────────────
        QMap<QString, OghamNodeItem*> tagToNode;

        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            const LoadedFile& lf = m_loadedFiles[fi];
            for (int ei = 0; ei < lf.entries.size(); ++ei)
            {
                const OghamSourceEntry& entry = lf.entries[ei];

                QStringList optTags;
                for (const OghamSourceOption& opt : entry.options)
                {
                    const QString lexVal = opt.textKey.isEmpty() ? QString()
                                        : FetchLexiconValueForKey(opt.textKey);
                    optTags.append(opt.tag.isEmpty()
                        ? (lexVal.isEmpty() ? opt.textKey : lexVal)
                        : opt.tag);
                }

                const QColor hdrColor = m_loadedFiles[fi].fileColor.isValid()
                    ? m_loadedFiles[fi].fileColor
                    : kGraphFileColors[fi % kGraphFileColorCount];
                auto* node = new OghamNodeItem(fi, ei, entry.tag, optTags, hdrColor);

                node->setPos(entry.position);

                scene->addItem(node);
                if (!entry.tag.isEmpty())
                    tagToNode[entry.tag] = node;

                connect(node, &OghamNodeItem::positionChanged,
                    [this](int nfi, int nei, QPointF p)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        auto& entries = m_loadedFiles[nfi].entries;
                        if (nei < 0 || nei >= entries.size()) return;
                        entries[nei].position = p;
                        m_loadedFiles[nfi].dirty = true;
                    });

                connect(node, &OghamNodeItem::entrySelected,
                    [this](int nfi, int nei)
                    {
                        PopulateForm(nfi, nei);
                        SelectEntry(nfi, nei);
                    });

                connect(node, &OghamNodeItem::grabStarted,
                    [this](int, int) { m_isInteracting = true; });
                connect(node, &OghamNodeItem::grabEnded,
                    [this](int, int) { m_isInteracting = false; TrySave(); });

                connect(node, &OghamNodeItem::createAliasPinRequested,
                    this, &OghamStoryteller::OnCreateAliasPin);
                connect(node, &OghamNodeItem::duplicateNodeRequested,
                    this, &OghamStoryteller::OnDuplicateNode);
                connect(node, &OghamNodeItem::deleteNodeRequested,
                    this, &OghamStoryteller::OnDeleteNodeFromGraph);
            }
        }

        // ── Phase 1b: create alias pins ───────────────────────────────────────
        // Key: (entryTag, pinId) → alias pin item
        QMap<QPair<QString,int>, OghamAliasPinItem*> aliasPinMap;

        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            const LoadedFile& lf = m_loadedFiles[fi];
            for (int ei = 0; ei < lf.entries.size(); ++ei)
            {
                const OghamSourceEntry& entry = lf.entries[ei];
                OghamNodeItem* parentNode = tagToNode.value(entry.tag);

                for (int pi = 0; pi < entry.aliasPins.size(); ++pi)
                {
                    const OghamAliasPin& ap = entry.aliasPins[pi];

                    auto* apItem = new OghamAliasPinItem(fi, ei, ap.pinId, entry.tag);

                    // Auto-layout near parent node if position not set
                    QPointF apPos = ap.position;
                    if (apPos.isNull() && parentNode)
                        apPos = parentNode->pos() + QPointF(OghamNodeItem::kNodeWidth + 30.0,
                                                            pi * 50.0);
                    apItem->setPos(apPos);
                    scene->addItem(apItem);
                    aliasPinMap[{entry.tag, ap.pinId}] = apItem;

                    // Update alias pin position in data model on drag-release
                    connect(apItem, &OghamAliasPinItem::positionInDataChanged,
                        [this](int nfi, int nei, int pid, QPointF p)
                        {
                            if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                            auto& entries = m_loadedFiles[nfi].entries;
                            if (nei < 0 || nei >= entries.size()) return;
                            for (OghamAliasPin& pin : entries[nei].aliasPins)
                                if (pin.pinId == pid) { pin.position = p; break; }
                            SetFileDirty(nfi, true);
                        });

                    // Double-click: select and focus owning node (live scene lookup avoids stale pointers)
                    connect(apItem, &OghamAliasPinItem::nodeRequested,
                        [this](int nfi, int nei)
                        {
                            if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                            if (nei < 0 || nei >= m_loadedFiles[nfi].entries.size()) return;
                            OghamNodeItem* n = nullptr;
                            for (QGraphicsItem* gi : m_graphView->graphScene()->items())
                            {
                                if (auto* ni = dynamic_cast<OghamNodeItem*>(gi))
                                {
                                    if (ni->fileIdx() == nfi && ni->entryIdx() == nei)
                                        { n = ni; break; }
                                }
                            }
                            if (n)
                            {
                                m_graphView->graphScene()->clearSelection();
                                n->setSelected(true);
                                m_graphView->centerOn(n);
                            }
                            PopulateForm(nfi, nei);
                            SelectEntry(nfi, nei);
                        });
                }
            }
        }

        // ── Phase 2: create connections ───────────────────────────────────────
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            const LoadedFile& lf = m_loadedFiles[fi];
            for (int ei = 0; ei < lf.entries.size(); ++ei)
            {
                OghamNodeItem* srcNode = tagToNode.value(lf.entries[ei].tag);
                if (!srcNode) continue;

                for (int oi = 0; oi < lf.entries[ei].options.size(); ++oi)
                {
                    const OghamSourceOption& opt = lf.entries[ei].options[oi];
                    if (opt.targetTag.isEmpty()) continue;

                    OghamConnectionItem* conn = nullptr;

                    if (opt.targetAliasIndex == 0)
                    {
                        // Target is the node's own input pin
                        OghamNodeItem* dstNode = tagToNode.value(opt.targetTag);
                        if (!dstNode) continue;

                        conn = new OghamConnectionItem(srcNode, oi,
                            [dstNode]() { return dstNode->inputPinScenePos(); },
                            opt.redirects, dstNode->fileIdx());

                        connect(dstNode, &OghamNodeItem::positionChanged, conn,
                            [conn](int, int, QPointF) { conn->refreshPath(); });
                    }
                    else
                    {
                        // Target is an alias pin
                        OghamAliasPinItem* ap = aliasPinMap.value({opt.targetTag, opt.targetAliasIndex});
                        if (!ap) continue;

                        conn = new OghamConnectionItem(srcNode, oi,
                            [ap]() { return ap->inputPinScenePos(); },
                            opt.redirects, ap->fileIdx());

                        connect(ap, &OghamAliasPinItem::positionChanged, conn,
                            [conn](QPointF) { conn->refreshPath(); });
                    }

                    scene->addItem(conn);

                    // Persist redirect waypoint edits back to the data model
                    connect(conn, &OghamConnectionItem::redirectsChanged,
                        this, [this, fi, ei, oi](QVector<QPointF> rds)
                        {
                            if (fi < 0 || fi >= m_loadedFiles.size()) return;
                            auto& entries = m_loadedFiles[fi].entries;
                            if (ei < 0 || ei >= entries.size()) return;
                            if (oi < 0 || oi >= entries[ei].options.size()) return;
                            entries[ei].options[oi].redirects = rds;
                            SetFileDirty(fi, true);
                        });
                }
            }
        }

        ApplyGraphVisibility();
    }

    void OghamStoryteller::ApplyGraphVisibility()
    {
        QGraphicsScene* scene = m_graphView->graphScene();
        for (QGraphicsItem* item : scene->items())
        {
            if (auto* node = dynamic_cast<OghamNodeItem*>(item))
            {
                const int nfi = node->fileIdx();
                const bool vis = (nfi >= 0 && nfi < m_loadedFiles.size()) &&
                                  m_loadedFiles[nfi].visible;
                node->setVisible(vis);
            }
            else if (auto* ap = dynamic_cast<OghamAliasPinItem*>(item))
            {
                const int nfi = ap->fileIdx();
                const bool vis = (nfi >= 0 && nfi < m_loadedFiles.size()) &&
                                  m_loadedFiles[nfi].visible;
                ap->setVisible(vis);
            }
            else if (auto* conn = dynamic_cast<OghamConnectionItem*>(item))
            {
                const int sfi = conn->srcFileIdx();
                const int dfi = conn->dstFileIdx();
                const bool srcVis = (sfi >= 0 && sfi < m_loadedFiles.size()) &&
                                     m_loadedFiles[sfi].visible;
                const bool dstVis = (dfi < 0 || dfi >= m_loadedFiles.size()) ||
                                     m_loadedFiles[dfi].visible;
                if (!srcVis)
                {
                    conn->setVisible(false);
                }
                else
                {
                    conn->setVisible(true);
                    conn->setOpacity(dstVis ? 1.0 : 0.3);
                }
            }
        }
    }

    void OghamStoryteller::SelectEntry(int fileIdx, int entryIdx)
    {
        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find =
            [&](QTreeWidgetItem* item) -> QTreeWidgetItem*
            {
                if (item->data(0, kRoleFileIdx).toInt()  == fileIdx &&
                    item->data(0, kRoleEntryIdx).toInt() == entryIdx)
                    return item;
                for (int i = 0; i < item->childCount(); ++i)
                    if (auto* f = find(item->child(i))) return f;
                return nullptr;
            };

        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            if (auto* found = find(m_tree->topLevelItem(i)))
            {
                m_tree->setCurrentItem(found);
                return;
            }
    }

    // -------------------------------------------------------------------------
    // Entry CRUD
    // -------------------------------------------------------------------------

    // Returns a tag unique across ALL loaded files. Tries base (no suffix) first,
    // then base1, base2, … to find the next available slot.
    static QString UniqueTagGlobal(const QList<LoadedFile>& files, const QString& base)
    {
        auto isTaken = [&](const QString& candidate) -> bool
        {
            for (const auto& lf : files)
                for (const auto& e : lf.entries)
                    if (e.tag == candidate) return true;
            return false;
        };
        if (!isTaken(base)) return base;
        for (int n = 1; ; ++n)
        {
            const QString candidate = base + QString::number(n);
            if (!isTaken(candidate)) return candidate;
        }
    }

    void OghamStoryteller::AddRootEntry(int fileIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];

        OghamSourceEntry entry;
        entry.tag = UniqueTagGlobal(m_loadedFiles, "Dialogue.NewNode");
        lf.entries.append(entry);

        SetFileDirty(fileIdx, true);
        int newEi = lf.entries.size() - 1;
        RebuildTree();
        SelectEntry(fileIdx, newEi);
    }

    void OghamStoryteller::AddSiblingEntry(int fileIdx, const QString& siblingTag)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];

        const int dot = siblingTag.lastIndexOf('.');
        const QString base = (dot < 0) ? "Dialogue.NewNode"
                                       : siblingTag.left(dot) + ".NewNode";
        OghamSourceEntry entry;
        entry.tag = UniqueTagGlobal(m_loadedFiles, base);
        lf.entries.append(entry);

        SetFileDirty(fileIdx, true);
        int newEi = lf.entries.size() - 1;
        RebuildTree();
        SelectEntry(fileIdx, newEi);
    }

    void OghamStoryteller::AddChildEntry(int fileIdx, const QString& parentTag)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];

        OghamSourceEntry entry;
        entry.tag = UniqueTagGlobal(m_loadedFiles, parentTag + ".NewNode");
        lf.entries.append(entry);

        SetFileDirty(fileIdx, true);
        int newEi = lf.entries.size() - 1;
        RebuildTree();
        SelectEntry(fileIdx, newEi);
    }

    void OghamStoryteller::RemoveEntry(int fileIdx, int entryIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];
        if (entryIdx < 0 || entryIdx >= lf.entries.size()) return;

        auto r = QMessageBox::question(this, "Remove Entry",
            QString("Remove entry '%1'?\n"
                    "Child entries (if any) remain as independent entries.")
                .arg(lf.entries[entryIdx].tag),
            QMessageBox::Yes | QMessageBox::No);
        if (r == QMessageBox::No) return;

        lf.entries.removeAt(entryIdx);
        SetFileDirty(fileIdx, true);

        if (m_selectedFileIdx == fileIdx)
        {
            if (m_selectedEntryIdx == entryIdx)
            {
                m_selectedFileIdx  = -1;
                m_selectedEntryIdx = -1;
                ClearForm();
            }
            else if (m_selectedEntryIdx > entryIdx)
            {
                --m_selectedEntryIdx;
            }
        }

        RebuildTree();
    }

    void OghamStoryteller::MoveEntryToFile(int srcFi, int srcEi, int dstFi)
    {
        if (srcFi < 0 || srcFi >= m_loadedFiles.size()) return;
        if (dstFi < 0 || dstFi >= m_loadedFiles.size()) return;
        auto& srcEntries = m_loadedFiles[srcFi].entries;
        if (srcEi < 0 || srcEi >= srcEntries.size()) return;

        OghamSourceEntry moved = srcEntries.takeAt(srcEi);
        m_loadedFiles[dstFi].entries.append(moved);
        const int newEi = m_loadedFiles[dstFi].entries.size() - 1;

        SetFileDirty(srcFi, true);
        SetFileDirty(dstFi, true);

        if (m_selectedFileIdx == srcFi && m_selectedEntryIdx == srcEi)
        {
            m_selectedFileIdx  = dstFi;
            m_selectedEntryIdx = newEi;
        }
        else if (m_selectedFileIdx == srcFi && m_selectedEntryIdx > srcEi)
        {
            --m_selectedEntryIdx;
        }

        RebuildTree();
        SelectEntry(dstFi, newEi);
    }

    // ── Graph weld handlers ───────────────────────────────────────────────────

    void OghamStoryteller::OnPinDroppedOnNode(OghamNodeItem* src, int optIdx, OghamNodeItem* dst)
    {
        const int fi = src->fileIdx();
        const int ei = src->entryIdx();
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (optIdx < 0 || optIdx >= entries[ei].options.size()) return;

        entries[ei].options[optIdx].targetTag        = dst->tag();
        entries[ei].options[optIdx].targetAliasIndex = 0;
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    void OghamStoryteller::OnPinDroppedOnAlias(OghamNodeItem* src, int optIdx, OghamAliasPinItem* dst)
    {
        const int fi = src->fileIdx();
        const int ei = src->entryIdx();
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (optIdx < 0 || optIdx >= entries[ei].options.size()) return;

        const int dfi = dst->fileIdx();
        const int dei = dst->entryIdx();
        if (dfi < 0 || dfi >= m_loadedFiles.size()) return;
        if (dei < 0 || dei >= m_loadedFiles[dfi].entries.size()) return;

        // Use the parent ENTRY's tag (not the alias's own label) so aliasPinMap lookup works.
        entries[ei].options[optIdx].targetTag        = m_loadedFiles[dfi].entries[dei].tag;
        entries[ei].options[optIdx].targetAliasIndex = dst->pinId();
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    void OghamStoryteller::OnPinDroppedOnCanvas(OghamNodeItem* src, int optIdx, QPointF scenePos)
    {
        const int fi = src->fileIdx();
        const int ei = src->entryIdx();
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fi];
        if (ei < 0 || ei >= lf.entries.size()) return;
        if (optIdx < 0 || optIdx >= lf.entries[ei].options.size()) return;

        const QString newTag = UniqueTagGlobal(m_loadedFiles, "Dialogue.NewNode");
        OghamSourceEntry newEntry;
        newEntry.tag      = newTag;
        newEntry.position = scenePos;
        lf.entries.append(newEntry);
        const int newEi = lf.entries.size() - 1;

        lf.entries[ei].options[optIdx].targetTag        = newTag;
        lf.entries[ei].options[optIdx].targetAliasIndex = 0;
        SetFileDirty(fi, true);
        RebuildTree();
        SelectEntry(fi, newEi);
    }

    void OghamStoryteller::OnCreateAliasPin(int fileIdx, int entryIdx, QPointF scenePos)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];
        if (entryIdx < 0 || entryIdx >= lf.entries.size()) return;
        auto& entry = lf.entries[entryIdx];

        // Generate a unique pinId
        int maxId = 0;
        for (const auto& ap : entry.aliasPins)
            maxId = qMax(maxId, ap.pinId);

        OghamAliasPin pin;
        pin.pinId    = maxId + 1;
        pin.tag      = entry.tag + ".Alias" + QString::number(pin.pinId);
        pin.position = scenePos;
        entry.aliasPins.append(pin);

        SetFileDirty(fileIdx, true);
        RebuildGraph();
    }

    void OghamStoryteller::OnDuplicateNode(int fileIdx, int entryIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];
        if (entryIdx < 0 || entryIdx >= lf.entries.size()) return;

        OghamSourceEntry copy = lf.entries[entryIdx];
        copy.tag      = UniqueTagGlobal(m_loadedFiles, copy.tag);
        copy.position = copy.position + QPointF(40.0, 40.0);
        copy.aliasPins.clear();   // alias pins are pinId-keyed; don't duplicate them

        lf.entries.append(copy);
        const int newEi = lf.entries.size() - 1;

        SetFileDirty(fileIdx, true);
        RebuildTree();
        RebuildGraph();
        SelectEntry(fileIdx, newEi);
    }

    void OghamStoryteller::OnDeleteNodeFromGraph(int fileIdx, int entryIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];
        if (entryIdx < 0 || entryIdx >= lf.entries.size()) return;

        const QString deletedTag = lf.entries[entryIdx].tag;

        // Clear all option targetTags pointing to the deleted entry across all files
        int cleared = 0;
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            bool changed = false;
            for (auto& e : m_loadedFiles[fi].entries)
            {
                for (auto& opt : e.options)
                {
                    if (opt.targetTag == deletedTag)
                    {
                        opt.targetTag        = QString();
                        opt.targetAliasIndex = 0;
                        ++cleared;
                        changed = true;
                    }
                }
            }
            if (changed)
                SetFileDirty(fi, true);
        }

        lf.entries.removeAt(entryIdx);
        SetFileDirty(fileIdx, true);

        if (m_selectedFileIdx == fileIdx)
        {
            if (m_selectedEntryIdx == entryIdx)
            {
                m_selectedFileIdx  = -1;
                m_selectedEntryIdx = -1;
                ClearForm();
            }
            else if (m_selectedEntryIdx > entryIdx)
            {
                --m_selectedEntryIdx;
            }
        }

        RebuildTree();
        RebuildGraph();

        const QString msg = cleared > 0
            ? QString("Deleted '%1' — %2 reference(s) cleared.").arg(deletedTag).arg(cleared)
            : QString("Deleted '%1'.").arg(deletedTag);
        m_statusLabel->setText(msg);
    }

    void OghamStoryteller::OnDeleteAliasPin(int fileIdx, int entryIdx, int pinId)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fileIdx];
        if (entryIdx < 0 || entryIdx >= lf.entries.size()) return;
        auto& pins = lf.entries[entryIdx].aliasPins;

        const int before = pins.size();
        pins.erase(std::remove_if(pins.begin(), pins.end(),
            [pinId](const OghamAliasPin& p) { return p.pinId == pinId; }), pins.end());

        if (pins.size() == before) return;

        // Clear any options that pointed to this alias pin
        const QString entryTag = lf.entries[entryIdx].tag;
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            bool changed = false;
            for (auto& e : m_loadedFiles[fi].entries)
            {
                for (auto& opt : e.options)
                {
                    if (opt.targetTag == entryTag && opt.targetAliasIndex == pinId)
                    {
                        opt.targetAliasIndex = 0;
                        changed = true;
                    }
                }
            }
            if (changed) SetFileDirty(fi, true);
        }

        SetFileDirty(fileIdx, true);
        RebuildGraph();
    }

    void OghamStoryteller::OnCreateNodeFromCanvas(QPointF scenePos)
    {
        if (m_loadedFiles.isEmpty()) return;

        // Pick the first visible file as the target
        int targetFi = -1;
        for (int i = 0; i < m_loadedFiles.size(); ++i)
            if (m_loadedFiles[i].visible) { targetFi = i; break; }
        if (targetFi < 0) targetFi = 0;

        const QString baseTag = QStringLiteral("Dialogue.NewNode");
        const QString newTag  = UniqueTagGlobal(m_loadedFiles, baseTag);

        bool ok = false;
        const QString tag = QInputDialog::getText(
            this, QStringLiteral("Create Node"),
            QStringLiteral("Tag:"), QLineEdit::Normal, newTag, &ok);
        if (!ok || tag.trimmed().isEmpty()) return;

        OghamSourceEntry entry;
        entry.tag      = tag.trimmed();
        entry.position = scenePos;
        m_loadedFiles[targetFi].entries.append(entry);
        const int newEi = m_loadedFiles[targetFi].entries.size() - 1;

        SetFileDirty(targetFi, true);
        RebuildTree();
        RebuildGraph();
        SelectEntry(targetFi, newEi);
    }

    // -------------------------------------------------------------------------
    // Graph layout
    // -------------------------------------------------------------------------

    void OghamStoryteller::OnLayoutGraph()
    {
        // Build tag -> (fi, ei) lookup for all files
        QMap<QString, QPair<int,int>> tagIndex;
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
            for (int ei = 0; ei < m_loadedFiles[fi].entries.size(); ++ei)
            {
                const QString& tag = m_loadedFiles[fi].entries[ei].tag;
                if (!tag.isEmpty())
                    tagIndex[tag] = {fi, ei};
            }

        // Build set of tags that are referenced by any option (have incoming edges)
        QSet<QString> hasIncoming;
        for (const auto& lf : m_loadedFiles)
            for (const auto& entry : lf.entries)
                for (const auto& opt : entry.options)
                    if (!opt.targetTag.isEmpty())
                        hasIncoming.insert(opt.targetTag);

        // Collect start nodes (visible entries with no incoming edges), in file order
        QList<QPair<int,int>> startNodes;
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            if (!m_loadedFiles[fi].visible) continue;
            for (int ei = 0; ei < m_loadedFiles[fi].entries.size(); ++ei)
            {
                const QString& tag = m_loadedFiles[fi].entries[ei].tag;
                if (!tag.isEmpty() && !hasIncoming.contains(tag))
                    startNodes.append({fi, ei});
            }
        }

        // Fallback: all nodes are in cycles — treat every visible node as a start node
        if (startNodes.isEmpty())
        {
            for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
                if (m_loadedFiles[fi].visible)
                    for (int ei = 0; ei < m_loadedFiles[fi].entries.size(); ++ei)
                        startNodes.append({fi, ei});
        }

        const qreal colW    = OghamNodeItem::kNodeWidth + 80.0;
        const qreal rowH    = 160.0;
        const qreal treeGap = rowH * 0.5;

        QSet<QString> visited;
        qreal baseY = 0.0;

        struct QueueEntry { int fi; int ei; int col; };

        for (const auto& [sfi, sei] : startNodes)
        {
            const QString& startTag = m_loadedFiles[sfi].entries[sei].tag;
            if (startTag.isEmpty() || visited.contains(startTag))
                continue;

            // BFS from this start node; assign columns (depth) and rows (stacking)
            QList<QueueEntry> bfsQueue;
            QSet<QString>     queued;
            bfsQueue.append({sfi, sei, 0});
            queued.insert(startTag);

            QMap<int, int> nextRowPerCol;  // col -> next row index to assign
            qreal treeMaxY = baseY;

            while (!bfsQueue.isEmpty())
            {
                auto [fi, ei, col] = bfsQueue.takeFirst();
                const QString& tag = m_loadedFiles[fi].entries[ei].tag;
                if (tag.isEmpty() || visited.contains(tag)) continue;
                visited.insert(tag);

                const int  row = nextRowPerCol.value(col, 0);
                nextRowPerCol[col] = row + 1;

                const QPointF pos(col * colW, baseY + row * rowH);
                m_loadedFiles[fi].entries[ei].position = pos;
                treeMaxY = qMax(treeMaxY, pos.y() + rowH);

                // Enqueue children (option targets not yet queued)
                for (const auto& opt : m_loadedFiles[fi].entries[ei].options)
                {
                    if (opt.targetTag.isEmpty() || queued.contains(opt.targetTag)) continue;
                    if (!tagIndex.contains(opt.targetTag)) continue;
                    const auto& [cfi, cei] = tagIndex[opt.targetTag];
                    queued.insert(opt.targetTag);
                    bfsQueue.append({cfi, cei, col + 1});
                }
            }

            baseY = treeMaxY + treeGap;
        }

        // Append any isolated / unreachable visible nodes below the laid-out trees
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            if (!m_loadedFiles[fi].visible) continue;
            for (int ei = 0; ei < m_loadedFiles[fi].entries.size(); ++ei)
            {
                const QString& tag = m_loadedFiles[fi].entries[ei].tag;
                if (tag.isEmpty() || visited.contains(tag)) continue;
                m_loadedFiles[fi].entries[ei].position = QPointF(0.0, baseY);
                baseY += rowH;
            }
        }

        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
            if (!m_loadedFiles[fi].entries.isEmpty())
                SetFileDirty(fi, true);

        RebuildGraph();
        m_graphView->fitAll();
    }

    // -------------------------------------------------------------------------
    // Form
    // -------------------------------------------------------------------------

    void OghamStoryteller::PopulateForm(int fileIdx, int entryIdx)
    {
        m_selectedFileIdx  = fileIdx;
        m_selectedEntryIdx = entryIdx;

        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size() ||
            entryIdx < 0 || entryIdx >= m_loadedFiles[fileIdx].entries.size())
        {
            ClearForm();
            return;
        }

        m_formStack->setCurrentIndex(1);
        m_playFromNodeBtn->setEnabled(true);
        const OghamSourceEntry& entry = m_loadedFiles[fileIdx].entries[entryIdx];

        const QStringList oghamTags  = FetchKnownOghamTags();

        m_tagCombo->blockSignals(true);
        m_tagCombo->clear();
        m_tagCombo->addItems(oghamTags);
        if (auto* cpl = m_tagCombo->completer())
            cpl->setModel(m_tagCombo->model());
        m_tagCombo->setCurrentText(entry.tag);
        m_tagCombo->blockSignals(false);
        m_renamedFromTag = entry.tag;
        ApplyTagStatus(m_tagStatus, entry.tag, oghamTags);

        // Wire status button to add-to-gptags if yellow
        disconnect(m_tagStatus, nullptr, nullptr, nullptr);
        if (m_tagStatus->isEnabled())
        {
            connect(m_tagStatus, &QToolButton::clicked,
                [this]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    const QString tag =
                        m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].tag;
                    AddTagToGptagsFile(tag);
                    ApplyTagStatus(m_tagStatus, tag, FetchKnownOghamTags());
                    disconnect(m_tagStatus, nullptr, nullptr, nullptr);
                });
        }

        RebuildTextKeysArea();
        RebuildEntryOpsArea();
        RebuildOptionsArea();
    }

    void OghamStoryteller::ClearForm()
    {
        m_selectedFileIdx  = -1;
        m_selectedEntryIdx = -1;
        m_renamedFromTag.clear();
        m_formStack->setCurrentIndex(0);
        m_playFromNodeBtn->setEnabled(false);

        m_tagCombo->blockSignals(true);
        m_tagCombo->clear();
        m_tagCombo->blockSignals(false);

        m_tagStatus->setText("\xe2\x97\x8f");
        m_tagStatus->setStyleSheet("color: #555555;");
        m_tagStatus->setEnabled(false);
        disconnect(m_tagStatus, nullptr, nullptr, nullptr);

        QLayoutItem* child;
        while ((child = m_keysLayout->takeAt(0)))    { if (child->widget()) child->widget()->deleteLater(); delete child; }
        while ((child = m_entOpsLayout->takeAt(0)))  { if (child->widget()) child->widget()->deleteLater(); delete child; }
        while ((child = m_optsLayout->takeAt(0)))    { if (child->widget()) child->widget()->deleteLater(); delete child; }
    }

    // Shared helper: write a key/value to the default lexicon immediately.
    static void WriteToDefaultLexicon(const QString& key, const QString& value,
                                       const QStringList& lexFilePaths)
    {
        if (key.isEmpty() || lexFilePaths.isEmpty()) return;

        QString defaultPath;
        for (const QString& fp : lexFilePaths)
            if (QFileInfo(fp).baseName().compare("default", Qt::CaseInsensitive) == 0)
            { defaultPath = fp; break; }
        if (defaultPath.isEmpty()) defaultPath = lexFilePaths.first();

        QFile f(defaultPath);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

        QJsonObject root    = doc.object();
        QJsonObject entries = root[QStringLiteral("entries")].toObject();
        entries[key]        = value;
        root[QStringLiteral("entries")] = entries;

        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    // Shared helper: build a text-key row widget
    // [status][key combo, stretch][value edit (single-line collapsed / multiline)][▼][X or nullptr]
    static QWidget* MakeTextKeyRow(
        const QString&     currentKey,
        const QString&     currentValue,
        const QStringList& knownKeys,
        QWidget*           parent,
        std::function<void(const QString& key)>   onKeyChanged,
        std::function<void(const QString& val)>   onValueChanged,
        std::function<void()>                     onStatusClicked,
        std::function<void()>                     onRemove)
    {
        auto* outer  = new QWidget(parent);
        auto* outerV = new QVBoxLayout(outer);
        outerV->setContentsMargins(0, 0, 0, 0);
        outerV->setSpacing(2);

        // ── Top row: [status][key combo][value (elided)][▼][X] ──────────────
        auto* topRow = new QWidget(outer);
        auto* hl     = new QHBoxLayout(topRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* statusBtn = new QToolButton(topRow);
        statusBtn->setFixedWidth(26);
        statusBtn->setAutoRaise(true);

        auto* keyCombo = new QComboBox(topRow);
        keyCombo->setEditable(true);
        keyCombo->setInsertPolicy(QComboBox::NoInsert);
        keyCombo->addItems(knownKeys);
        keyCombo->setCurrentText(currentKey);
        keyCombo->setPlaceholderText("localisation key");
        auto* cpl = new QCompleter(knownKeys, keyCombo);
        cpl->setCaseSensitivity(Qt::CaseInsensitive);
        cpl->setFilterMode(Qt::MatchContains);
        keyCombo->setCompleter(cpl);

        auto* toggleBtn = new QToolButton(topRow);
        toggleBtn->setFixedWidth(22);
        toggleBtn->setText("\xe2\x96\xbc"); // ▼

        hl->addWidget(statusBtn);
        hl->addWidget(keyCombo, 1);
        hl->addWidget(toggleBtn);

        if (onRemove)
        {
            auto* removeBtn = new QPushButton("X", topRow);
            removeBtn->setFixedWidth(26);
            removeBtn->setStyleSheet("color: #cc3333; font-weight: bold;");
            hl->addWidget(removeBtn);
            QObject::connect(removeBtn, &QPushButton::clicked, [onRemove]() { onRemove(); });
        }
        outerV->addWidget(topRow);

        // ── Expanded row: full-width value edit (shown when ▼ is toggled) ────
        auto* expandRow  = new QWidget(outer);
        auto* expandHL   = new QHBoxLayout(expandRow);
        expandHL->setContentsMargins(26 + 4, 0, 0, 0);
        expandHL->setSpacing(0);
        auto* fullEdit = new QLineEdit(currentValue, expandRow);
        fullEdit->setPlaceholderText("value");
        expandHL->addWidget(fullEdit, 1);
        expandRow->setVisible(false);
        outerV->addWidget(expandRow);

        // ── Apply initial status ──────────────────────────────────────────────
        auto applyStatus = [statusBtn, knownKeys, onStatusClicked](const QString& key)
        {
            statusBtn->disconnect();
            const bool emp = key.isEmpty();
            const bool hit = !emp && knownKeys.contains(key);
            if (emp)
            {
                statusBtn->setText("\xe2\x97\x8f");
                statusBtn->setStyleSheet("color: #555555;");
                statusBtn->setToolTip("No key set.");
                statusBtn->setEnabled(false);
            }
            else if (hit)
            {
                statusBtn->setText(QString(QChar(0x2713)));
                statusBtn->setStyleSheet("color: #44aa44; font-weight: bold;");
                statusBtn->setToolTip(QString("Key '%1' found in Lexicon.").arg(key));
                statusBtn->setEnabled(false);
            }
            else
            {
                statusBtn->setText("\xe2\x9a\xa0");
                statusBtn->setStyleSheet("color: #cc7700; font-weight: bold;");
                statusBtn->setToolTip("Key not in any Lexicon. Click to add.");
                statusBtn->setEnabled(true);
                if (onStatusClicked)
                    QObject::connect(statusBtn, &QToolButton::clicked,
                                     [onStatusClicked]() { onStatusClicked(); });
            }
        };
        applyStatus(currentKey);

        // ── Wire toggle ───────────────────────────────────────────────────────
        QObject::connect(toggleBtn, &QToolButton::clicked,
            [expandRow, toggleBtn]()
            {
                const bool show = !expandRow->isVisible();
                expandRow->setVisible(show);
                toggleBtn->setText(show ? "\xe2\x96\xb2" : "\xe2\x96\xbc");
            });

        // ── Wire key combo ─────────────────────────────────────────────────────
        // Every keystroke: update status indicator only (no save — prevents focus stealing)
        QObject::connect(keyCombo, &QComboBox::editTextChanged,
            [applyStatus](const QString& v) { applyStatus(v.trimmed()); });
        // Commit (Enter / focus-loss / dropdown pick): update data model + save
        QObject::connect(keyCombo->lineEdit(), &QLineEdit::editingFinished,
            [keyCombo, onKeyChanged]()
            {
                if (onKeyChanged) onKeyChanged(keyCombo->currentText().trimmed());
            });
        QObject::connect(keyCombo, QOverload<int>::of(&QComboBox::activated),
            [keyCombo, onKeyChanged](int)
            {
                if (onKeyChanged) onKeyChanged(keyCombo->currentText().trimmed());
            });

        // ── Wire value edit ────────────────────────────────────────────────────
        QObject::connect(fullEdit, &QLineEdit::editingFinished,
            [fullEdit, onValueChanged]()
            {
                if (onValueChanged) onValueChanged(fullEdit->text());
            });

        return outer;
    }

    void OghamStoryteller::RebuildTextKeysArea()
    {
        QLayoutItem* child;
        while ((child = m_keysLayout->takeAt(0))) { if (child->widget()) child->widget()->deleteLater(); delete child; }

        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;

        const QStringList knownKeys = FetchKnownLexiconKeys();

        // Resolve lexicon file paths for live value writing
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);
        QStringList lexPaths;
        auto* fileIO = AZ::IO::FileIOBase::GetInstance();
        for (const auto& p : azPaths)
        {
            AZ::IO::FixedMaxPath resolved;
            if (fileIO && fileIO->ResolvePath(resolved, p.c_str()))
                lexPaths.append(QString::fromUtf8(resolved.c_str()));
            else
                lexPaths.append(QString::fromUtf8(p.c_str()));
        }

        const QStringList& keys =
            m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].dataKeys;

        for (int i = 0; i < keys.size(); ++i)
        {
            const QString& currentKey = keys[i];
            const QString  currentVal = FetchLexiconValueForKey(currentKey);
            int ci = i;

            auto onKeyChanged = [this, ci](const QString& key)
            {
                if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                if (ci < e.dataKeys.size()) e.dataKeys[ci] = key;
                SetFileDirty(m_selectedFileIdx, true);
            };

            auto onValueChanged = [this, ci, lexPaths](const QString& val)
            {
                if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                if (ci >= e.dataKeys.size()) return;
                const QString& key = e.dataKeys[ci];
                if (key.isEmpty()) return;
                WriteToDefaultLexicon(key, val, lexPaths);
                FoundationLocalisation::LexiconEditorRequestBus::Broadcast(
                    &FoundationLocalisation::LexiconEditorRequests::RefreshKeyTree);
            };

            auto onStatusClicked = [this, ci]()
            {
                if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                if (ci >= e.dataKeys.size()) return;
                const QString& typed = e.dataKeys[ci];
                const QString sugKey = IsValidTagStructure(typed) ? typed : QString{};
                const QString sugVal = IsValidTagStructure(typed)
                    ? QString("[%1-TextKey%2]").arg(e.tag).arg(ci) : typed;
                ShowAddKeyDialog(sugKey, sugVal);
            };

            auto onRemove = [this, ci]()
            {
                if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                if (ci < e.dataKeys.size())
                {
                    e.dataKeys.removeAt(ci);
                    SetFileDirty(m_selectedFileIdx, true);
                    RebuildTextKeysArea();
                }
            };

            m_keysLayout->addWidget(
                MakeTextKeyRow(currentKey, currentVal, knownKeys, m_keysWidget,
                               onKeyChanged, onValueChanged, onStatusClicked, onRemove));
        }
    }

    void OghamStoryteller::RebuildOptionsArea()
    {
        QLayoutItem* child;
        while ((child = m_optsLayout->takeAt(0))) { if (child->widget()) child->widget()->deleteLater(); delete child; }

        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
        const QStringList knownKeys  = FetchKnownLexiconKeys();
        const QStringList oghamTags  = FetchKnownOghamTags();

        // Resolve lexicon file paths for live value writing
        AZStd::vector<AZStd::string> azOptPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azOptPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);
        QStringList optLexPaths;
        auto* optFileIO = AZ::IO::FileIOBase::GetInstance();
        for (const auto& p : azOptPaths)
        {
            AZ::IO::FixedMaxPath resolved;
            if (optFileIO && optFileIO->ResolvePath(resolved, p.c_str()))
                optLexPaths.append(QString::fromUtf8(resolved.c_str()));
            else
                optLexPaths.append(QString::fromUtf8(p.c_str()));
        }
        // allTags: option tag combo completer (entry tags + OghamStoryteller tags)
        QStringList allTags = oghamTags;
        for (const QString& t : FetchAllEntryTags())
            if (!allTags.contains(t)) allTags.append(t);
        allTags.sort(Qt::CaseInsensitive);
        // condOpTags: cond/op tag validation — any tag in any .gptags file is valid
        const QStringList condOpTags = FetchAllTagsFromAllFiles();

        const QString currentEntryTag =
            m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].tag;
        const auto& opts = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].options;

        for (int i = 0; i < opts.size(); ++i)
        {
            const OghamSourceOption& opt = opts[i];

            auto* frame = new QFrame(m_optsWidget);
            frame->setFrameShape(QFrame::StyledPanel);
            frame->setFrameShadow(QFrame::Raised);
            auto* fl = new QFormLayout(frame);
            fl->setContentsMargins(6, 6, 6, 6); fl->setSpacing(4);

            // ── Option tag row: tristate status + combo + X ───────────────────
            auto* tagRow    = new QHBoxLayout();
            auto* optTagSt  = new QToolButton(frame);
            optTagSt->setFixedWidth(26);
            optTagSt->setAutoRaise(true);
            ApplyTagStatus(optTagSt, opt.tag, oghamTags);
            // D: wire ⚠ click to add tag to gptags file
            if (optTagSt->isEnabled())
            {
                const QString capturedTag = opt.tag;
                connect(optTagSt, &QToolButton::clicked,
                    [this, optTagSt, capturedTag]()
                    {
                        AddTagToGptagsFile(capturedTag);
                        ApplyTagStatus(optTagSt, capturedTag, FetchKnownOghamTags());
                        optTagSt->disconnect();
                    });
            }
            tagRow->addWidget(optTagSt);

            auto* optTagCombo = new QComboBox(frame);
            optTagCombo->setEditable(true);
            optTagCombo->setInsertPolicy(QComboBox::NoInsert);
            optTagCombo->addItems(allTags);
            optTagCombo->setCurrentText(opt.tag);
            optTagCombo->setPlaceholderText("Option tag");
            auto* otCpl = new QCompleter(allTags, optTagCombo);
            otCpl->setCaseSensitivity(Qt::CaseInsensitive);
            otCpl->setFilterMode(Qt::MatchContains);
            optTagCombo->setCompleter(otCpl);
            tagRow->addWidget(optTagCombo, 1);

            auto* removeBtn = new QPushButton("X", frame);
            removeBtn->setFixedWidth(26);
            removeBtn->setStyleSheet("color: #cc3333; font-weight: bold;");
            tagRow->addWidget(removeBtn);
            fl->addRow("Tag:", tagRow);

            {
                const int    oci      = i;
                const QString initKey = opt.textKey;
                const QString initVal = FetchLexiconValueForKey(initKey);

                auto tkOnKey = [this, oci](const QString& key)
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (oci < e.options.size()) { e.options[oci].textKey = key; SetFileDirty(m_selectedFileIdx, true); }
                };
                auto tkOnVal = [this, oci, optLexPaths](const QString& val)
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (oci >= e.options.size()) return;
                    const QString& key = e.options[oci].textKey;
                    if (key.isEmpty()) return;
                    WriteToDefaultLexicon(key, val, optLexPaths);
                    FoundationLocalisation::LexiconEditorRequestBus::Broadcast(
                        &FoundationLocalisation::LexiconEditorRequests::RefreshKeyTree);
                };
                auto tkOnStatus = [this, oci]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (oci >= e.options.size()) return;
                    const QString& typed = e.options[oci].textKey;
                    const QString sugKey = IsValidTagStructure(typed) ? typed : QString{};
                    const QString sugVal = IsValidTagStructure(typed)
                        ? QString("[%1-Option%2]").arg(e.tag).arg(oci) : typed;
                    ShowAddKeyDialog(sugKey, sugVal);
                };
                fl->addRow("Text Key:",
                    MakeTextKeyRow(initKey, initVal, knownKeys, frame,
                                   tkOnKey, tkOnVal, tkOnStatus, nullptr));
            }

            // ── Target entry row: tristate status + combo ─────────────────────
            // M: build target list excluding the current entry (circular reference prevention)
            QStringList targetEntries;
            for (const QString& t : FetchAllEntryTags())
                if (t != currentEntryTag) targetEntries.append(t);

            auto* targetRow = new QHBoxLayout();
            auto* targetSt  = new QToolButton(frame);
            targetSt->setFixedWidth(26);
            targetSt->setAutoRaise(true);
            // M: flag circular reference
            if (!opt.targetTag.isEmpty() && opt.targetTag == currentEntryTag)
            {
                targetSt->setText(QString(QChar(0x2717)));   // ✗
                targetSt->setStyleSheet("color: #cc3333; font-weight: bold;");
                targetSt->setToolTip("Cannot target own entry (circular reference).");
                targetSt->setEnabled(false);
            }
            else
            {
                ApplyTagStatus(targetSt, opt.targetTag, targetEntries, /*allowEmpty=*/true);
            }
            targetRow->addWidget(targetSt);

            auto* targetCombo = new QComboBox(frame);
            targetCombo->setEditable(true);
            targetCombo->setInsertPolicy(QComboBox::NoInsert);
            targetCombo->addItems(targetEntries);
            targetCombo->setCurrentText(opt.targetTag);
            targetCombo->setPlaceholderText("target entry tag  (empty = end conversation)");
            auto* teCpl = new QCompleter(targetEntries, targetCombo);
            teCpl->setCaseSensitivity(Qt::CaseInsensitive);
            teCpl->setFilterMode(Qt::MatchContains);
            targetCombo->setCompleter(teCpl);
            targetRow->addWidget(targetCombo, 1);
            fl->addRow("Target Entry:", targetRow);

            // ── Per-option Conditions ─────────────────────────────────────────
            static const char* kOptCondTooltip =
                "Gameplay Tag conditions tested against the Dialogue State to determine "
                "whether this option should be displayed. If any condition fails this "
                "option is omitted from the Entry Signal entirely.";
            auto* addOptCond = new QPushButton("+", frame);
            addOptCond->setFixedSize(22, 22);
            addOptCond->setToolTip(kOptCondTooltip);
            {
                auto* optCondHdr = new QHBoxLayout();
                auto* optCondLabel = new QLabel("  Conditions:", frame);
                optCondLabel->setToolTip(kOptCondTooltip);
                optCondHdr->addWidget(optCondLabel);
                optCondHdr->addStretch();
                optCondHdr->addWidget(addOptCond);
                fl->addRow("", optCondHdr);
            }
            auto* optCondContainer = new QWidget(frame);
            auto* optCondVL        = new QVBoxLayout(optCondContainer);
            optCondVL->setContentsMargins(12, 0, 0, 0);
            optCondVL->setSpacing(2);
            fl->addRow("", optCondContainer);

            // ── Per-option Operations ─────────────────────────────────────────
            static const char* kOptOpsTooltip =
                "Gameplay Tag operations executed on the Dialogue State when this option "
                "is selected. Each operation can optionally have a condition that must be "
                "true for it to be applied.";
            auto* addOptOp = new QPushButton("+", frame);
            addOptOp->setFixedSize(22, 22);
            addOptOp->setToolTip(kOptOpsTooltip);
            {
                auto* optOpsHdr = new QHBoxLayout();
                auto* optOpsLabel = new QLabel("  Operations:", frame);
                optOpsLabel->setToolTip(kOptOpsTooltip);
                optOpsHdr->addWidget(optOpsLabel);
                optOpsHdr->addStretch();
                optOpsHdr->addWidget(addOptOp);
                fl->addRow("", optOpsHdr);
            }
            auto* optOpsContainer = new QWidget(frame);
            auto* optOpsVL        = new QVBoxLayout(optOpsContainer);
            optOpsVL->setContentsMargins(12, 0, 0, 0);
            optOpsVL->setSpacing(2);
            fl->addRow("", optOpsContainer);

            m_optsLayout->addWidget(frame);
            int ci = i;

            // Populate conditions rows for this option.
            // MakeConditionRow only calls onChanged() on [X] (not on text edits), so
            // a full RebuildOptionsArea() here is safe — no focus stealing from typing.
            {
                auto onCondRemoved = [this]() { RebuildOptionsArea(); };

                const auto& conds = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].options[i].conditions;
                const int total = conds.size();
                for (int k = 0; k < total; ++k)
                {
                    auto getCondList = [this, i]() -> CondList&
                    { return m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].options[i].conditions; };
                    optCondVL->addWidget(MakeConditionRow(k, getCondList, onCondRemoved,
                        condOpTags, /*isLast=*/ k == total - 1));
                }
            }

            // Populate operations rows for this option (B: use allTags)
            {
                const auto& ops = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].options[i].operations;
                for (int k = 0; k < ops.size(); ++k)
                {
                    auto getOpList = [this, i]() -> OpList&
                    { return m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].options[i].operations; };
                    auto onOpChanged = [this]()
                    { SetFileDirty(m_selectedFileIdx, true); RebuildOptionsArea(); };
                    optOpsVL->addWidget(MakeOperationRow(k, getOpList, onOpChanged, condOpTags));
                }
            }

            // Wire add-condition button
            connect(addOptCond, &QPushButton::clicked,
                [this, i]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (i < e.options.size())
                    {
                        e.options[i].conditions.append(OghamCondition{});
                        RebuildOptionsArea();
                    }
                });

            // Wire add-operation button
            connect(addOptOp, &QPushButton::clicked,
                [this, i]()
                {
                    if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (i < e.options.size())
                    {
                        e.options[i].operations.append(OghamOperation{});
                        SetFileDirty(m_selectedFileIdx, true);
                        RebuildOptionsArea();
                    }
                });

            connect(optTagCombo, &QComboBox::editTextChanged,
                [this, optTagSt, oghamTags, ci](const QString& v)
                {
                    if (m_selectedFileIdx >= 0 && m_selectedEntryIdx >= 0) {
                        auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                        if (ci < e.options.size()) { e.options[ci].tag = v; SetFileDirty(m_selectedFileIdx, true); } }
                    // Option tags validate against gptags only (not entry tags)
                    ApplyTagStatus(optTagSt, v, oghamTags);
                    // Rewire click for new value
                    optTagSt->disconnect();
                    if (optTagSt->isEnabled())
                    {
                        connect(optTagSt, &QToolButton::clicked,
                            [this, optTagSt, v]()
                            {
                                AddTagToGptagsFile(v);
                                ApplyTagStatus(optTagSt, v, FetchKnownOghamTags());
                                optTagSt->disconnect();
                            });
                    }
                });

            connect(targetCombo, &QComboBox::editTextChanged,
                [this, targetSt, targetEntries, currentEntryTag, ci](const QString& v)
                {
                    if (m_selectedFileIdx >= 0 && m_selectedEntryIdx >= 0) {
                        auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                        if (ci < e.options.size()) { e.options[ci].targetTag = v; SetFileDirty(m_selectedFileIdx, true); } }
                    // M: detect circular reference
                    if (!v.isEmpty() && v == currentEntryTag)
                    {
                        targetSt->setText(QString(QChar(0x2717)));
                        targetSt->setStyleSheet("color: #cc3333; font-weight: bold;");
                        targetSt->setToolTip("Cannot target own entry (circular reference).");
                        targetSt->setEnabled(false);
                    }
                    else
                    {
                        ApplyTagStatus(targetSt, v, targetEntries, /*allowEmpty=*/true);
                    }
                });

            connect(removeBtn, &QPushButton::clicked,
                [this, ci]()
                { if (m_selectedFileIdx >= 0 && m_selectedEntryIdx >= 0) {
                    auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];
                    if (ci < e.options.size()) { e.options.removeAt(ci); SetFileDirty(m_selectedFileIdx, true); RebuildOptionsArea(); } } });
        }
    }

    // -------------------------------------------------------------------------
    // Condition / Operation row builders
    // -------------------------------------------------------------------------

    static const QStringList kComparisons = {
        "Exists", "NotExists", "Equal", "NotEqual", "Less", "LessEqual", "Greater", "GreaterEqual"
    };
    static const QStringList kArithmetics = {
        "Set", "Add", "Sub", "Mul", "Div", "Min", "Max"
    };
    static const QStringList kLogicOps = { "And", "Or", "Xor" };
    static bool kNumericComparison(const QString& cmp)
    {
        return cmp == "Equal"   || cmp == "NotEqual" ||
               cmp == "Less"    || cmp == "LessEqual" ||
               cmp == "Greater" || cmp == "GreaterEqual";
    }

    QWidget* OghamStoryteller::MakeConditionRow(
        int ci,
        std::function<CondList&()> getList,
        std::function<void()>      onChanged,
        const QStringList&         knownTags,
        bool                       isLast)
    {
        auto* row = new QWidget();
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        // Tag status icon (left of combo)
        auto* tagStatus = new QToolButton(row);
        tagStatus->setFixedWidth(26);
        tagStatus->setAutoRaise(true);

        // Tag combo
        auto* tagCombo = new QComboBox(row);
        tagCombo->setEditable(true);
        tagCombo->setInsertPolicy(QComboBox::NoInsert);
        tagCombo->addItems(knownTags);
        CondList& clist = getList();
        const QString initCondTag = ci < clist.size() ? clist[ci].tag : QString{};
        tagCombo->setCurrentText(initCondTag);
        tagCombo->setPlaceholderText("tag");
        auto* cpl = new QCompleter(knownTags, tagCombo);
        cpl->setCaseSensitivity(Qt::CaseInsensitive);
        cpl->setFilterMode(Qt::MatchContains);
        tagCombo->setCompleter(cpl);
        ApplyTagStatus(tagStatus, initCondTag, knownTags);
        if (tagStatus->isEnabled())
            connect(tagStatus, &QToolButton::clicked,
                [this, tagStatus, initCondTag]() { ShowAddTagDialog(initCondTag, tagStatus); });
        hl->addWidget(tagStatus);
        hl->addWidget(tagCombo, 2);

        // Comparison dropdown
        auto* cmpCombo = new QComboBox(row);
        cmpCombo->addItems(kComparisons);
        cmpCombo->setCurrentText(ci < clist.size() ? clist[ci].comparison : kComparisons[0]);
        hl->addWidget(cmpCombo, 1);

        // Unsigned whole-number value — hidden unless comparison is numeric
        auto* valSpin = new QSpinBox(row);
        valSpin->setRange(0, INT_MAX);
        valSpin->setValue(ci < clist.size() ? clist[ci].value : 0);
        valSpin->setVisible(kNumericComparison(cmpCombo->currentText()));
        hl->addWidget(valSpin, 1);

        // Logic-op dropdown — hidden on the last row (E)
        auto* logicCombo = new QComboBox(row);
        logicCombo->addItems(kLogicOps);
        logicCombo->setCurrentText(ci < clist.size() ? clist[ci].logicOp : kLogicOps[0]);
        logicCombo->setToolTip("Logic operator connecting this condition to the next");
        logicCombo->setVisible(!isLast);
        hl->addWidget(logicCombo, 1);

        // X button
        auto* removeBtn = new QPushButton("X", row);
        removeBtn->setFixedWidth(26);
        removeBtn->setStyleSheet("color: #cc3333; font-weight: bold;");
        hl->addWidget(removeBtn);

        // ── Connections ──────────────────────────────────────────────────────
        // Data-only changes write to model and mark dirty WITHOUT triggering a
        // rebuild (which would steal focus on every keystroke).
        // Only the remove button triggers a structural rebuild via onChanged().
        connect(tagCombo, &QComboBox::editTextChanged,
            [this, getList, ci, tagStatus, knownTags](const QString& v)
            {
                CondList& cl = getList();
                if (ci < cl.size()) { cl[ci].tag = v; SetFileDirty(m_selectedFileIdx, true); }
                ApplyTagStatus(tagStatus, v.trimmed(), knownTags);
                tagStatus->disconnect();
                if (tagStatus->isEnabled())
                    connect(tagStatus, &QToolButton::clicked,
                        [this, tagStatus, v]() { ShowAddTagDialog(v.trimmed(), tagStatus); });
            });

        connect(cmpCombo, &QComboBox::currentTextChanged,
            [this, getList, valSpin, ci](const QString& v)
            {
                CondList& cl = getList();
                if (ci < cl.size()) { cl[ci].comparison = v; SetFileDirty(m_selectedFileIdx, true); }
                valSpin->setVisible(kNumericComparison(v));
            });

        connect(valSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this, getList, ci](int v)
            {
                CondList& cl = getList();
                if (ci < cl.size()) { cl[ci].value = v; SetFileDirty(m_selectedFileIdx, true); }
            });

        connect(logicCombo, &QComboBox::currentTextChanged,
            [this, getList, ci](const QString& v)
            {
                CondList& cl = getList();
                if (ci < cl.size()) { cl[ci].logicOp = v; SetFileDirty(m_selectedFileIdx, true); }
            });

        connect(removeBtn, &QPushButton::clicked,
            [getList, onChanged, ci]()
            {
                CondList& cl = getList();
                if (ci < cl.size()) { cl.removeAt(ci); onChanged(); }
            });

        return row;
    }

    QWidget* OghamStoryteller::MakeOperationRow(
        int oi,
        std::function<OpList&()>  getList,
        std::function<void()>     onChanged,
        const QStringList&        knownTags)
    {
        auto* frame = new QFrame();
        frame->setFrameShape(QFrame::StyledPanel);
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(6, 4, 6, 4);
        fl->setSpacing(4);

        OpList& oplist = getList();
        const OghamOperation& op = oi < oplist.size() ? oplist[oi] : OghamOperation{};

        // ── Top row: tag + arithmetic + value + X ────────────────────────────
        auto* topRow = new QWidget(frame);
        auto* hl     = new QHBoxLayout(topRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* opTagStatus = new QToolButton(topRow);
        opTagStatus->setFixedWidth(26);
        opTagStatus->setAutoRaise(true);

        auto* tagCombo = new QComboBox(topRow);
        tagCombo->setEditable(true);
        tagCombo->setInsertPolicy(QComboBox::NoInsert);
        tagCombo->addItems(knownTags);
        tagCombo->setCurrentText(op.tag);
        tagCombo->setPlaceholderText("tag");
        auto* cpl = new QCompleter(knownTags, tagCombo);
        cpl->setCaseSensitivity(Qt::CaseInsensitive);
        cpl->setFilterMode(Qt::MatchContains);
        tagCombo->setCompleter(cpl);
        ApplyTagStatus(opTagStatus, op.tag, knownTags);
        if (opTagStatus->isEnabled())
            connect(opTagStatus, &QToolButton::clicked,
                [this, opTagStatus, capturedTag = op.tag]()
                { ShowAddTagDialog(capturedTag, opTagStatus); });
        hl->addWidget(opTagStatus);
        hl->addWidget(tagCombo, 2);

        auto* arithCombo = new QComboBox(topRow);
        arithCombo->addItems(kArithmetics);
        arithCombo->setCurrentText(op.arithmetic);
        hl->addWidget(arithCombo, 1);

        auto* valSpin = new QSpinBox(topRow);
        valSpin->setRange(0, INT_MAX);
        valSpin->setValue(op.value);
        hl->addWidget(valSpin, 1);

        auto* removeBtn = new QPushButton("X", topRow);
        removeBtn->setFixedWidth(26);
        removeBtn->setStyleSheet("color: #cc3333; font-weight: bold;");
        hl->addWidget(removeBtn);

        fl->addWidget(topRow);

        // ── Nested conditions section ─────────────────────────────────────────
        static const char* kOpCondTooltip =
            "Conditions that must be true for this operation to be applied. "
            "Leave empty to always apply the operation.";
        auto* condHeader = new QHBoxLayout();
        auto* opCondLabel = new QLabel("  Conditions:", frame);
        opCondLabel->setToolTip(kOpCondTooltip);
        condHeader->addWidget(opCondLabel);
        condHeader->addStretch();
        auto* addCondBtn = new QPushButton("+", frame);
        addCondBtn->setFixedSize(22, 22);
        addCondBtn->setToolTip(kOpCondTooltip);
        condHeader->addWidget(addCondBtn);
        fl->addLayout(condHeader);

        auto* condContainer = new QWidget(frame);
        auto* condVL        = new QVBoxLayout(condContainer);
        condVL->setContentsMargins(12, 0, 0, 0);
        condVL->setSpacing(2);

        // Populate existing op conditions.
        // MakeConditionRow only calls onChanged() on [X] (not on text edits), so
        // a full area rebuild here is safe — no focus stealing from typing.
        {
            OpList& ol = getList();
            if (oi < ol.size())
            {
                const int total = ol[oi].conditions.size();
                for (int k = 0; k < total; ++k)
                {
                    auto gcl = [getList, oi]() -> CondList& { return getList()[oi].conditions; };
                    condVL->addWidget(MakeConditionRow(k, gcl, onChanged,
                        knownTags, /*isLast=*/ k == total - 1));
                }
            }
        }
        fl->addWidget(condContainer);

        // ── Connections for op row ────────────────────────────────────────────
        // Data-only changes (tag/arith/value) write to the model and mark dirty
        // WITHOUT triggering a full area rebuild — that would steal focus on every
        // keystroke. Only structural changes (remove) call onChanged().
        connect(tagCombo, &QComboBox::editTextChanged,
            [this, getList, oi, opTagStatus, knownTags](const QString& v)
            {
                OpList& ol = getList();
                if (oi < ol.size()) { ol[oi].tag = v; SetFileDirty(m_selectedFileIdx, true); }
                ApplyTagStatus(opTagStatus, v.trimmed(), knownTags);
                opTagStatus->disconnect();
                if (opTagStatus->isEnabled())
                    connect(opTagStatus, &QToolButton::clicked,
                        [this, opTagStatus, v]() { ShowAddTagDialog(v.trimmed(), opTagStatus); });
            });

        connect(arithCombo, &QComboBox::currentTextChanged,
            [this, getList, oi](const QString& v)
            { OpList& ol = getList(); if (oi < ol.size()) { ol[oi].arithmetic = v; SetFileDirty(m_selectedFileIdx, true); } });

        connect(valSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this, getList, oi](int v)
            { OpList& ol = getList(); if (oi < ol.size()) { ol[oi].value = v; SetFileDirty(m_selectedFileIdx, true); } });

        connect(removeBtn, &QPushButton::clicked,
            [getList, onChanged, oi]()
            { OpList& ol = getList(); if (oi < ol.size()) { ol.removeAt(oi); onChanged(); } });

        connect(addCondBtn, &QPushButton::clicked,
            [getList, onChanged, oi]()
            {
                OpList& ol = getList();
                if (oi < ol.size())
                {
                    ol[oi].conditions.append(OghamCondition{});
                    onChanged();
                }
            });

        return frame;
    }

    void OghamStoryteller::RebuildEntryOpsArea()
    {
        QLayoutItem* child;
        while ((child = m_entOpsLayout->takeAt(0)))
        { if (child->widget()) child->widget()->deleteLater(); delete child; }

        if (m_selectedFileIdx < 0 || m_selectedEntryIdx < 0) return;

        // Conditions/ops tag validation: any tag in any .gptags file is valid
        const QStringList allTags = FetchAllTagsFromAllFiles();

        auto& e = m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx];

        for (int oi = 0; oi < e.entryOperations.size(); ++oi)
        {
            auto getList = [this]() -> OpList&
            { return m_loadedFiles[m_selectedFileIdx].entries[m_selectedEntryIdx].entryOperations; };
            auto onChanged = [this]()
            { SetFileDirty(m_selectedFileIdx, true); RebuildEntryOpsArea(); };
            m_entOpsLayout->addWidget(MakeOperationRow(oi, getList, onChanged, allTags));
        }
    }

    void OghamStoryteller::TrySave()
    {
        if (m_isInteracting) return;
        m_suppressWatcher = true;
        for (int i = 0; i < m_loadedFiles.size(); ++i)
        {
            if (m_loadedFiles[i].dirty && SaveFile(i))
                SetFileDirty(i, false);
        }
        m_suppressWatcher = false;
    }

    void OghamStoryteller::SetFileDirty(int fileIdx, bool dirty)
    {
        if (fileIdx < 0 || fileIdx >= m_loadedFiles.size()) return;
        m_loadedFiles[fileIdx].dirty = dirty;

        // Update header text in tree
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        {
            auto* item = m_tree->topLevelItem(i);
            if (item->data(0, kRoleFileIdx).toInt() == fileIdx)
            {
                const QString fn = QFileInfo(m_loadedFiles[fileIdx].path).fileName();
                const QString label = QString("%1%2").arg(fn).arg(dirty ? "  \u25CF" : "");
                if (QWidget* w = m_tree->itemWidget(item, 0))
                {
                    if (auto* lbl = w->findChild<QLabel*>("fileNameLabel"))
                        lbl->setText(label);
                }
                else
                {
                    item->setText(0, label);
                }
                break;
            }
        }

        if (dirty)
            TrySave();

        UpdateStatusBar();
    }

    void OghamStoryteller::UpdateStatusBar()
    {
        int totalEntries = 0;
        int dirtyFiles   = 0;
        for (const auto& lf : m_loadedFiles)
        {
            totalEntries += lf.entries.size();
            if (lf.dirty) ++dirtyFiles;
        }

        m_saveAllBtn->setEnabled(dirtyFiles > 0);

        if (m_loadedFiles.isEmpty())
            m_statusLabel->setText("No files loaded.");
        else if (dirtyFiles > 0)
            m_statusLabel->setText(
                QString("%1 entries across %2 file(s)  \u2014  %3 file(s) with unsaved changes.")
                    .arg(totalEntries).arg(m_loadedFiles.size()).arg(dirtyFiles));
        else
            m_statusLabel->setText(
                QString("%1 entries across %2 file(s)  \u2014  All saved.")
                    .arg(totalEntries).arg(m_loadedFiles.size()));
    }

} // namespace FoundationOgham

#include <moc_OghamStoryteller.cpp>
