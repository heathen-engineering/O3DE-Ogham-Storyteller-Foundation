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

#include <QApplication>
#include <QCheckBox>
#include <QScreen>
#include <QCloseEvent>
#include <QColorDialog>
#include <QRegularExpression>
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
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QKeyEvent>
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
#include <QTextEdit>
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
            if (auto* ok = buttons->button(QDialogButtonBox::Ok))
                { ok->setAutoDefault(false); ok->setDefault(false); }
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
    // Modal shared helpers — forward-declared so modals can use them.
    // Definitions are further down in this file.
    // =========================================================================

    static bool     IsValidTagStructure(const QString& tag);
    static void     ApplyTagStatus(QToolButton* btn, const QString& tag,
                                   const QStringList& knownTags, bool allowEmpty = false);
    static QPoint   ModalPos(QPoint screenPos, const QDialog& dlg);

    // Node-style + / × buttons used in all inline modals
    static QPushButton* makeAddBtn(QWidget* p)
    {
        auto* b = new QPushButton("+", p);
        b->setFixedSize(16, 16);
        b->setStyleSheet(
            "QPushButton{background:#286828;color:#e8e8e8;border-radius:2px;"
            "font-weight:bold;border:none;padding:0}"
            "QPushButton:hover{background:#38a838}");
        return b;
    }
    static QPushButton* makeRemoveBtn(QWidget* p)
    {
        auto* b = new QPushButton("\xc3\x97", p);
        b->setFixedSize(14, 14);
        b->setStyleSheet(
            "QPushButton{background:#782828;color:#e8e8e8;border-radius:2px;"
            "font-weight:bold;border:none;padding:0}"
            "QPushButton:hover{background:#a83838}");
        return b;
    }

    static const QStringList kModalComparisons = {
        "Exists","NotExists","Equal","NotEqual","Less","LessEqual","Greater","GreaterEqual"};
    static const QStringList kModalArithmetics = {
        "Set","Add","Subtract","Multiply","Divide","Min","Max"};
    static const QStringList kModalLogicOps = {"And","Or","Xor"};
    static bool kModalIsNumeric(const QString& cmp)
    { return cmp != "Exists" && cmp != "NotExists"; }

    // =========================================================================
    // OghamLexiconKeyEditPopup
    //
    // Matches Unity's OghamKeyEditWindow:
    //
    //   [Type ▼][Mode ▼]                         [Save][✕]
    //   [B][I][U] [■][A][✕A] [─pt─▼] [🔗] [Formatted]   ← Text type only
    //   [ ──── content text area (rendered preview / source) ──────────── ]
    //   [ link panel ─────────────────────────── ]   ← collapsible
    //   [ lexicon key field ──────────── ][▾]         ← Localised mode only
    //
    // Default mode: Formatted (markdown rendered as HTML, read-only).
    // "Formatted" / "Source" button toggles between preview and editable source.
    //
    // Data model:
    //   Localised: m_editKey = lexicon lookup key, m_editVal = resolved/edited text
    //   Literal:   m_editKey = "", m_editVal = the text content (stored as sourceKey.key)
    //   If fetchValue(key) is empty the key IS the content (legacy/no-lexicon data).
    //
    // Commits on Save or focus-loss; Escape cancels.
    // =========================================================================

    // Inline helper — converts Ogham markdown+TMPro subset to Qt-compatible HTML.
    static QString oghamToHtml(const QString& md)
    {
        QString s = md;
        s.replace("\n", "<br>");
        // Bold **text**
        s.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"), "<b>\\1</b>");
        // Italic *text* (don't match **)
        s.replace(QRegularExpression(R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*))"), "<i>\\1</i>");
        // Underline <u>text</u> passes through as valid HTML — no transform needed
        // TMPro color <color=#hex>text</color>
        s.replace(QRegularExpression(R"(<color=#([0-9A-Fa-f]{3,8})>(.*?)</color>)",
                                     QRegularExpression::DotMatchesEverythingOption),
                  "<span style='color:#\\1'>\\2</span>");
        // TMPro size <size=N>text</size>
        s.replace(QRegularExpression(R"(<size=(\d+)>(.*?)</size>)",
                                     QRegularExpression::DotMatchesEverythingOption),
                  "<span style='font-size:\\1pt'>\\2</span>");
        // Links [display](target) → styled underline
        s.replace(QRegularExpression(R"(\[([^\]]+)\]\([^)]*\))"),
                  "<a style='color:#6ab0d0;text-decoration:underline'>\\1</a>");
        return "<div style='white-space:pre-wrap;font-family:sans-serif;font-size:10pt;color:#e0e0e0;'>"
               + s + "</div>";
    }

    class OghamLexiconKeyEditPopup : public QFrame
    {
        Q_OBJECT
        using FetchValFn = std::function<QString(const QString&)>;
    public:
        enum class LexiconWrite { None, Write };
        struct Result
        {
            LexiconWrite lexiconWrite = LexiconWrite::None;
            QString      diskKey;
            QString      lexValue;
            QString      type;
            QString      mode;
        };

        OghamLexiconKeyEditPopup(
            const OghamSourceKey& sourceKey,
            const QStringList&    knownKeys,
            FetchValFn            fetchValue,
            QWidget*              parent = nullptr)
            : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
            , m_knownKeys(knownKeys)
            , m_fetchValue(std::move(fetchValue))
        {
            setAttribute(Qt::WA_DeleteOnClose);
            setFrameShape(QFrame::StyledPanel);
            setFixedWidth(620);

            auto* vl = new QVBoxLayout(this);
            vl->setSpacing(4);
            vl->setContentsMargins(8, 8, 8, 8);

            // ── Row 1: [Type][Mode]              [Save][✕] ────────────────────
            {
                auto* row = new QHBoxLayout();
                row->setSpacing(4);

                m_typeCombo = new QComboBox(this);
                m_typeCombo->addItems({ "Text", "Image", "Audio", "Prefab" });
                m_typeCombo->setCurrentText(sourceKey.type);
                m_typeCombo->setFixedWidth(80);
                row->addWidget(m_typeCombo);

                m_modeCombo = new QComboBox(this);
                m_modeCombo->addItems({ "Localised", "Literal", "Invariant" });
                m_modeCombo->setCurrentText(sourceKey.mode);
                m_modeCombo->setFixedWidth(100);
                row->addWidget(m_modeCombo);

                row->addStretch();

                auto* saveBtn = new QPushButton("Save", this);
                saveBtn->setFixedWidth(48);
                saveBtn->setDefault(false); saveBtn->setAutoDefault(false);
                row->addWidget(saveBtn);

                auto* cancelBtn = new QPushButton("\xe2\x9c\x95", this);
                cancelBtn->setFixedWidth(26);
                cancelBtn->setDefault(false); cancelBtn->setAutoDefault(false);
                row->addWidget(cancelBtn);

                connect(saveBtn,   &QPushButton::clicked, this, [this]{ hide(); });
                connect(cancelBtn, &QPushButton::clicked, this, [this]{ m_cancelled = true; hide(); });
                vl->addLayout(row);
            }

            // ── Row 2: Formatting toolbar (Text type only) ────────────────────
            m_fmtRow = new QWidget(this);
            {
                auto* fmtL = new QHBoxLayout(m_fmtRow);
                fmtL->setContentsMargins(0, 2, 0, 2);
                fmtL->setSpacing(2);

                auto mkBtn = [&](const QString& label, const QString& tip, const QString& css = {}) {
                    auto* b = new QPushButton(label, m_fmtRow);
                    b->setFixedSize(26, 22);
                    b->setToolTip(tip);
                    b->setDefault(false); b->setAutoDefault(false);
                    b->setFocusPolicy(Qt::NoFocus);
                    if (!css.isEmpty()) b->setStyleSheet(css);
                    fmtL->addWidget(b);
                    return b;
                };

                auto* boldBtn   = mkBtn("B",    "Bold (**text**)",         "QPushButton{font-weight:bold;}");
                auto* italicBtn = mkBtn("I",    "Italic (*text*)",          "QPushButton{font-style:italic;}");
                auto* underBtn  = mkBtn("U",    "Underline (<u>text</u>)", "QPushButton{text-decoration:underline;}");

                fmtL->addSpacing(6);

                // Color swatch — shows active color, click to pick
                m_colorBtn = new QPushButton(m_fmtRow);
                m_colorBtn->setFixedSize(26, 22);
                m_colorBtn->setToolTip("Text color");
                m_colorBtn->setDefault(false); m_colorBtn->setAutoDefault(false);
                m_colorBtn->setFocusPolicy(Qt::NoFocus);
                updateColorBtn();
                fmtL->addWidget(m_colorBtn);

                auto* applyColorBtn = mkBtn("A",   "Apply color to selection");
                auto* stripColorBtn = mkBtn("\xe2\x9c\x95""A", "Remove color from selection");

                fmtL->addSpacing(6);

                // Size dropdown — "─pt─" sentinel resets after applying
                m_sizeCombo = new QComboBox(m_fmtRow);
                m_sizeCombo->setFixedWidth(62);
                m_sizeCombo->setFocusPolicy(Qt::NoFocus);
                m_sizeCombo->addItems({
                    "\xe2\x94\x80pt\xe2\x94\x80", // ─pt─
                    "8","10","12","14","16","18","20","24","28","32"
                });
                fmtL->addWidget(m_sizeCombo);

                fmtL->addSpacing(6);

                // Link button — toggles link panel
                auto* linkBtn = mkBtn("\xf0\x9f\x94\x97", "Insert / edit link");  // 🔗

                fmtL->addSpacing(6);

                // Source/Formatted toggle — shows what clicking it will switch TO
                m_sourceToggleBtn = new QPushButton("Source", m_fmtRow);
                m_sourceToggleBtn->setFixedHeight(22);
                m_sourceToggleBtn->setDefault(false); m_sourceToggleBtn->setAutoDefault(false);
                m_sourceToggleBtn->setFocusPolicy(Qt::NoFocus);
                m_sourceToggleBtn->setToolTip("Toggle between formatted preview and raw markdown source");
                fmtL->addWidget(m_sourceToggleBtn);

                fmtL->addStretch();

                // ── Connections for toolbar buttons ─────────────────────────
                connect(boldBtn,   &QPushButton::clicked, this, [this]{ ensureSource(); wrapSelection("**",   "**");   });
                connect(italicBtn, &QPushButton::clicked, this, [this]{ ensureSource(); wrapSelection("*",    "*");    });
                connect(underBtn,  &QPushButton::clicked, this, [this]{ ensureSource(); wrapSelection("<u>",  "</u>"); });

                connect(m_colorBtn, &QPushButton::clicked, this,
                    [this]
                    {
                        QColor c = QColorDialog::getColor(m_activeColor, this, "Text Color");
                        if (c.isValid()) { m_activeColor = c; updateColorBtn(); }
                    });
                connect(applyColorBtn, &QPushButton::clicked, this,
                    [this]
                    {
                        ensureSource();
                        wrapSelection(QString("<color=%1>").arg(m_activeColor.name()), "</color>");
                    });
                connect(stripColorBtn, &QPushButton::clicked, this, [this]{ ensureSource(); stripTagsFromSelection("color"); });

                connect(m_sizeCombo, QOverload<int>::of(&QComboBox::activated), this,
                    [this](int idx)
                    {
                        if (idx <= 0) return;
                        const QString pt = m_sizeCombo->itemText(idx);
                        ensureSource();
                        wrapSelection(QString("<size=%1>").arg(pt), "</size>");
                        m_sizeCombo->setCurrentIndex(0);
                    });

                connect(linkBtn, &QPushButton::clicked, this,
                    [this]{ m_linkPanel->setVisible(!m_linkPanel->isVisible()); adjustSize(); });

                connect(m_sourceToggleBtn, &QPushButton::clicked, this,
                    [this]{ setSourceMode(!m_sourceMode); });
            }
            vl->addWidget(m_fmtRow);

            // ── Row 3: Content text area ──────────────────────────────────────
            m_valueEdit = new QTextEdit(this);
            m_valueEdit->setPlaceholderText("Content\xe2\x80\xa6");
            m_valueEdit->setMinimumHeight(220);
            m_valueEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_valueEdit->setStyleSheet(
                "QTextEdit { background-color: #1e1e1e; color: #e0e0e0; "
                "border: 1px solid #3a3a3a; border-radius: 2px; }");
            vl->addWidget(m_valueEdit, 1);

            // ── Row 4: Link panel (collapsed by default) ──────────────────────
            m_linkPanel = new QWidget(this);
            {
                auto* ll = new QVBoxLayout(m_linkPanel);
                ll->setContentsMargins(0, 4, 0, 0);
                ll->setSpacing(3);

                auto* sepLine = new QFrame(m_linkPanel);
                sepLine->setFrameShape(QFrame::HLine);
                sepLine->setFrameShadow(QFrame::Sunken);
                ll->addWidget(sepLine);

                auto* dispRow = new QHBoxLayout();
                dispRow->addWidget(new QLabel("Display:", m_linkPanel));
                m_linkDisplayEdit = new QLineEdit(m_linkPanel);
                m_linkDisplayEdit->setPlaceholderText("Link text\xe2\x80\xa6");
                dispRow->addWidget(m_linkDisplayEdit, 1);
                ll->addLayout(dispRow);

                auto* targetRow = new QHBoxLayout();
                targetRow->addWidget(new QLabel("Target:", m_linkPanel));
                m_linkTargetEdit = new QLineEdit(m_linkPanel);
                m_linkTargetEdit->setPlaceholderText("Ogham://Tag.Path or URL\xe2\x80\xa6");
                targetRow->addWidget(m_linkTargetEdit, 1);

                auto* targetPickBtn = new QPushButton("\xe2\x96\xbe", m_linkPanel);
                targetPickBtn->setFixedWidth(26);
                targetPickBtn->setFocusPolicy(Qt::NoFocus);
                targetPickBtn->setToolTip("Pick an Ogham entry tag");
                targetRow->addWidget(targetPickBtn);
                ll->addLayout(targetRow);

                auto* btnRow = new QHBoxLayout();
                auto* insertBtn = new QPushButton("Insert Link", m_linkPanel);
                insertBtn->setDefault(false); insertBtn->setAutoDefault(false);
                btnRow->addWidget(insertBtn);
                btnRow->addStretch();
                ll->addLayout(btnRow);

                connect(insertBtn, &QPushButton::clicked, this, [this]{ insertLink(); });
                connect(targetPickBtn, &QPushButton::clicked, this,
                    [this]
                    {
                        // Build a menu of all known Ogham entry tags from the known-keys list
                        QMenu menu(this);
                        if (m_knownKeys.isEmpty())
                            menu.addAction("(no tags)")->setEnabled(false);
                        else
                            for (const QString& k : m_knownKeys)
                                connect(menu.addAction(k), &QAction::triggered, this,
                                    [this, k]{ m_linkTargetEdit->setText("Ogham://" + k); });
                        menu.exec(m_linkPanel->mapToGlobal(m_linkPanel->rect().bottomLeft()));
                    });
            }
            m_linkPanel->setVisible(false);
            vl->addWidget(m_linkPanel);

            // ── Row 5: Lexicon key row (Localised only) ───────────────────────
            m_keyRow = new QWidget(this);
            {
                auto* keyL = new QHBoxLayout(m_keyRow);
                keyL->setContentsMargins(0, 4, 0, 0);
                keyL->setSpacing(4);
                keyL->addWidget(new QLabel("Key:", m_keyRow));

                m_keyEdit = new QLineEdit(m_keyRow);
                m_keyEdit->setPlaceholderText("Localisation key\xe2\x80\xa6");
                auto* cpl = new QCompleter(knownKeys, m_keyEdit);
                cpl->setCaseSensitivity(Qt::CaseInsensitive);
                cpl->setFilterMode(Qt::MatchContains);
                m_keyEdit->setCompleter(cpl);
                keyL->addWidget(m_keyEdit, 1);

                auto* keyPickBtn = new QPushButton("\xe2\x96\xbe", m_keyRow);
                keyPickBtn->setFixedWidth(26);
                keyPickBtn->setToolTip("Pick an existing localisation key");
                keyPickBtn->setDefault(false); keyPickBtn->setAutoDefault(false);
                keyPickBtn->setFocusPolicy(Qt::NoFocus);
                keyL->addWidget(keyPickBtn);

                connect(keyPickBtn, &QPushButton::clicked, this,
                    [this, keyPickBtn]
                    {
                        QMenu menu(this);
                        if (m_knownKeys.isEmpty())
                            menu.addAction("(No keys found)")->setEnabled(false);
                        else
                            for (const QString& k : m_knownKeys)
                                connect(menu.addAction(k), &QAction::triggered, this,
                                    [this, k]
                                    {
                                        m_keyEdit->setText(k);
                                        const QString val = m_fetchValue(k);
                                        if (!val.isEmpty()) setEditorText(val);
                                    });
                        menu.exec(keyPickBtn->mapToGlobal(keyPickBtn->rect().bottomLeft()));
                    });

                connect(m_keyEdit, &QLineEdit::returnPressed, this,
                    [this]
                    {
                        const QString val = m_fetchValue(m_keyEdit->text().trimmed());
                        if (!val.isEmpty()) setEditorText(val);
                    });
                m_keyEdit->installEventFilter(this);
            }
            vl->addWidget(m_keyRow);

            // ── Populate ──────────────────────────────────────────────────────
            const bool isLocalised = (sourceKey.mode == QLatin1String("Localised"));
            if (isLocalised)
            {
                m_editKey     = sourceKey.key;
                m_originalKey = sourceKey.key;
                const QString fetched = m_fetchValue(sourceKey.key);
                m_editVal     = fetched.isEmpty() ? sourceKey.key : fetched;
                m_originalVal = m_editVal;
                m_keyEdit->setText(m_editKey);
            }
            else
            {
                m_editKey     = QString();
                m_originalKey = QString();
                m_editVal     = sourceKey.key;
                m_originalVal = m_editVal;
            }

            // Pre-populate link display from selection (if any text was selected in the graph)
            if (m_linkDisplayEdit && m_linkDisplayEdit->text().isEmpty())
                m_linkDisplayEdit->setPlaceholderText("Selected text\xe2\x80\xa6");

            updateKeyRowVisibility();
            updateFmtRowVisibility();

            // Prime the text edit before setSourceMode reads from it
            m_valueEdit->setPlainText(m_editVal);

            // Start in Formatted mode (preview) — same default as Unity
            m_sourceMode = true;   // force setSourceMode to run the switch
            setSourceMode(false);  // → switches to Formatted (preview)

            // Track text changes in Source mode to keep m_editVal in sync
            connect(m_valueEdit, &QTextEdit::textChanged, this,
                [this]
                {
                    if (m_sourceMode && !m_suppressSync)
                        m_editVal = m_valueEdit->toPlainText();
                });

            // Mode / type change signals
            connect(m_modeCombo, &QComboBox::currentTextChanged, this,
                [this](const QString& mode)
                {
                    if (mode == QLatin1String("Localised"))
                    {
                        const QString val = m_fetchValue(m_keyEdit->text().trimmed());
                        if (!val.isEmpty()) setEditorText(val);
                    }
                    updateKeyRowVisibility();
                });
            connect(m_typeCombo, &QComboBox::currentTextChanged, this,
                [this](const QString&){ updateFmtRowVisibility(); });

            adjustSize();
        }

    signals:
        void committed(OghamLexiconKeyEditPopup::Result result);

    protected:
        void hideEvent(QHideEvent* event) override
        {
            // Capture final source text before emitting
            if (m_sourceMode) m_editVal = m_valueEdit->toPlainText();
            if (!m_cancelled) emit committed(buildResult());
            QFrame::hideEvent(event);
        }
        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() == Qt::Key_Escape)
                { m_cancelled = true; hide(); return; }
            QFrame::keyPressEvent(event);
        }
        bool eventFilter(QObject* obj, QEvent* ev) override
        {
            if (obj == m_keyEdit && ev->type() == QEvent::KeyPress)
            {
                const auto* ke = static_cast<QKeyEvent*>(ev);
                if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                {
                    const QString val = m_fetchValue(m_keyEdit->text().trimmed());
                    if (!val.isEmpty()) setEditorText(val);
                    return true;
                }
            }
            return QFrame::eventFilter(obj, ev);
        }

    private:
        // ── Source / Formatted toggle ─────────────────────────────────────────
        void setSourceMode(bool source)
        {
            if (m_sourceMode == source) return;

            if (m_sourceMode && !m_suppressSync)
                m_editVal = m_valueEdit->toPlainText();   // save before switching away

            m_sourceMode = source;

            if (source)
            {
                // Source: editable plain-text markdown
                m_suppressSync = true;
                m_valueEdit->setAcceptRichText(false);
                m_valueEdit->setReadOnly(false);
                m_valueEdit->setPlainText(m_editVal);
                m_suppressSync = false;
                if (m_sourceToggleBtn) m_sourceToggleBtn->setText("Formatted");
            }
            else
            {
                // Formatted: rendered HTML preview (read-only)
                m_valueEdit->setReadOnly(true);
                m_valueEdit->setHtml(oghamToHtml(m_editVal));
                if (m_sourceToggleBtn) m_sourceToggleBtn->setText("Source");
            }
        }

        void ensureSource()
        {
            if (!m_sourceMode) setSourceMode(true);
        }

        void setEditorText(const QString& text)
        {
            m_editVal = text;
            m_suppressSync = true;
            if (m_sourceMode)
                m_valueEdit->setPlainText(text);
            else
                m_valueEdit->setHtml(oghamToHtml(text));
            m_suppressSync = false;
        }

        // ── Helpers ───────────────────────────────────────────────────────────
        void updateColorBtn()
        {
            if (!m_colorBtn) return;
            const QString hex = m_activeColor.name();
            // Invert text colour for readability on the swatch
            const bool dark = (m_activeColor.red()*299 + m_activeColor.green()*587
                                + m_activeColor.blue()*114) < 128000;
            m_colorBtn->setStyleSheet(
                QString("QPushButton { background-color: %1; color: %2; }")
                .arg(hex, dark ? "#ffffff" : "#000000"));
        }

        void updateKeyRowVisibility()
        {
            if (m_keyRow) m_keyRow->setVisible(
                m_modeCombo && m_modeCombo->currentText() == QLatin1String("Localised"));
            adjustSize();
        }

        void updateFmtRowVisibility()
        {
            if (m_fmtRow) m_fmtRow->setVisible(
                m_typeCombo && m_typeCombo->currentText() == QLatin1String("Text"));
            adjustSize();
        }

        void wrapSelection(const QString& open, const QString& close)
        {
            QTextCursor cur = m_valueEdit->textCursor();
            const QString sel = cur.selectedText();
            cur.insertText(open + sel + close);
            if (sel.isEmpty())
                cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, close.length());
            m_valueEdit->setTextCursor(cur);
            m_valueEdit->setFocus();
        }

        void stripTagsFromSelection(const QString& tagName)
        {
            QTextCursor cur = m_valueEdit->textCursor();
            const QString sel = cur.selectedText();
            if (sel.isEmpty()) return;
            // Remove <tagName=...>...</tagName> or <tagName>...</tagName>
            QString stripped = sel;
            QRegularExpression rx(
                QString(R"(<\s*%1[^>]*>(.*?)<\s*/%1\s*>)").arg(QRegularExpression::escape(tagName)),
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
            stripped.replace(rx, "\\1");
            cur.insertText(stripped);
            m_valueEdit->setFocus();
        }

        void insertLink()
        {
            ensureSource();
            const QString display = m_linkDisplayEdit ? m_linkDisplayEdit->text() : QString{};
            const QString target  = m_linkTargetEdit  ? m_linkTargetEdit->text()  : QString{};
            if (target.isEmpty()) return;

            QTextCursor cur = m_valueEdit->textCursor();
            const QString sel = cur.selectedText();
            const QString disp = display.isEmpty() ? (sel.isEmpty() ? "link" : sel) : display;
            cur.insertText(QString("[%1](%2)").arg(disp, target));
            m_valueEdit->setFocus();
            if (m_linkPanel) { m_linkPanel->setVisible(false); adjustSize(); }
        }

        Result buildResult() const
        {
            const QString mode = m_modeCombo ? m_modeCombo->currentText() : "Literal";
            const QString type = m_typeCombo ? m_typeCombo->currentText() : "Text";

            if (mode == QLatin1String("Localised"))
            {
                const QString key = m_keyEdit ? m_keyEdit->text().trimmed() : QString{};
                const bool textChanged = (m_editVal != m_originalVal);
                const bool keyChanged  = (key       != m_originalKey);
                const LexiconWrite lw  = (textChanged || (keyChanged && !key.isEmpty()))
                                         ? LexiconWrite::Write : LexiconWrite::None;
                return { lw, key, m_editVal, type, mode };
            }
            // Literal / Invariant: the text IS the disk key
            return { LexiconWrite::None, m_editVal, QString(), type, mode };
        }

        bool        m_cancelled    = false;
        bool        m_sourceMode   = true;
        bool        m_suppressSync = false;
        QColor      m_activeColor  = Qt::white;
        QString     m_editKey;
        QString     m_editVal;
        QString     m_originalKey;
        QString     m_originalVal;
        QStringList m_knownKeys;
        FetchValFn  m_fetchValue;

        QComboBox*   m_typeCombo        = nullptr;
        QComboBox*   m_modeCombo        = nullptr;
        QTextEdit*   m_valueEdit        = nullptr;
        QLineEdit*   m_keyEdit          = nullptr;
        QWidget*     m_keyRow           = nullptr;
        QWidget*     m_fmtRow           = nullptr;
        QWidget*     m_linkPanel        = nullptr;
        QLineEdit*   m_linkDisplayEdit  = nullptr;
        QLineEdit*   m_linkTargetEdit   = nullptr;
        QPushButton* m_colorBtn         = nullptr;
        QPushButton* m_sourceToggleBtn  = nullptr;
        QComboBox*   m_sizeCombo        = nullptr;
    };

    // =========================================================================
    // OghamOperationEditPopup — non-modal frameless popup for editing one OghamOperation.
    // Commits on Enter or close; cancels on Escape. Auto-deletes on close.
    // =========================================================================

    class OghamOperationEditPopup : public QFrame
    {
        Q_OBJECT
        using AddTagFn = std::function<void(const QString&, QToolButton*)>;
    public:
        OghamOperationEditPopup(const OghamOperation& op,
                                const QStringList&    knownTags,
                                AddTagFn              addTagFn,
                                QWidget*              parent = nullptr)
            : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
            , m_op(op)
            , m_knownTags(knownTags)
            , m_addTagFn(std::move(addTagFn))
        {
            setAttribute(Qt::WA_DeleteOnClose);
            setFrameShape(QFrame::StyledPanel);
            setMinimumWidth(460);
            auto* vl = new QVBoxLayout(this);
            vl->setSpacing(8);

            // ── Top form: tag + arithmetic + value ────────────────────────
            auto* topRow = new QWidget(this);
            auto* hl     = new QHBoxLayout(topRow);
            hl->setContentsMargins(0,0,0,0);
            hl->setSpacing(4);

            m_tagStatus = new QToolButton(topRow);
            m_tagStatus->setFixedWidth(26);
            m_tagStatus->setAutoRaise(true);

            m_tagCombo = new QComboBox(topRow);
            m_tagCombo->setEditable(true);
            m_tagCombo->setInsertPolicy(QComboBox::NoInsert);
            m_tagCombo->addItems(knownTags);
            m_tagCombo->setCurrentText(op.tag);
            m_tagCombo->setPlaceholderText("tag");
            auto* cpl = new QCompleter(knownTags, m_tagCombo);
            cpl->setCaseSensitivity(Qt::CaseInsensitive);
            cpl->setFilterMode(Qt::MatchContains);
            m_tagCombo->setCompleter(cpl);
            ApplyTagStatus(m_tagStatus, op.tag, knownTags);
            refreshTagStatusConn();

            m_arithCombo = new QComboBox(topRow);
            m_arithCombo->addItems(kModalArithmetics);
            m_arithCombo->setCurrentText(op.arithmetic);

            m_valueBox = new QSpinBox(topRow);
            m_valueBox->setRange(-99999, 99999);
            m_valueBox->setValue(op.value);

            hl->addWidget(m_tagStatus);
            hl->addWidget(m_tagCombo, 3);
            hl->addWidget(m_arithCombo, 2);
            hl->addWidget(m_valueBox, 1);
            vl->addWidget(topRow);

            connect(m_tagCombo, &QComboBox::editTextChanged, this,
                [this](const QString& v)
                {
                    m_op.tag = v.trimmed();
                    ApplyTagStatus(m_tagStatus, v.trimmed(), m_knownTags);
                    refreshTagStatusConn();
                });
            connect(m_arithCombo, &QComboBox::currentTextChanged, this,
                [this](const QString& v){ m_op.arithmetic = v; });
            connect(m_valueBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this](int v){ m_op.value = v; });

            // ── Conditions ────────────────────────────────────────────────
            auto* condHdr = new QHBoxLayout();
            condHdr->addWidget(new QLabel("<b>Conditions</b>", this));
            condHdr->addStretch();
            auto* addCondBtn = makeAddBtn(this);
            condHdr->addWidget(addCondBtn);
            vl->addLayout(condHdr);

            auto* condScroll = new QScrollArea(this);
            condScroll->setWidgetResizable(true);
            condScroll->setMaximumHeight(180);
            condScroll->setFrameShape(QFrame::NoFrame);
            condScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_condArea   = new QWidget(this);
            m_condLayout = new QVBoxLayout(m_condArea);
            m_condLayout->setContentsMargins(0,0,0,0);
            m_condLayout->setSpacing(2);
            condScroll->setWidget(m_condArea);
            vl->addWidget(condScroll);

            connect(addCondBtn, &QPushButton::clicked, this, [this]
            {
                m_op.conditions.append(OghamCondition{});
                rebuildConditions();
            });
            rebuildConditions();
        }

    signals:
        void committed(OghamOperation op);

    protected:
        void hideEvent(QHideEvent* event) override
        {
            if (!m_cancelled)
                emit committed(m_op);
            QFrame::hideEvent(event);
        }
        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
                { hide(); return; }
            if (event->key() == Qt::Key_Escape)
                { m_cancelled = true; hide(); return; }
            QFrame::keyPressEvent(event);
        }

    private:
        bool m_cancelled = false;

        void refreshTagStatusConn()
        {
            m_tagStatus->disconnect();
            if (m_tagStatus->isEnabled())
                connect(m_tagStatus, &QToolButton::clicked, this,
                    [this]{ m_addTagFn(m_op.tag, m_tagStatus); });
        }

        void rebuildConditions()
        {
            while (QLayoutItem* it = m_condLayout->takeAt(0))
            { if (QWidget* w = it->widget()) w->deleteLater(); delete it; }

            const int total = m_op.conditions.size();
            for (int ci = 0; ci < total; ++ci)
            {
                const OghamCondition& cond = m_op.conditions[ci];
                auto* row  = new QWidget(m_condArea);
                auto* hl   = new QHBoxLayout(row);
                hl->setContentsMargins(0,0,0,0);
                hl->setSpacing(3);

                // Tag status icon
                auto* st = new QToolButton(row);
                st->setFixedWidth(24);
                st->setAutoRaise(true);
                ApplyTagStatus(st, cond.tag, m_knownTags);
                if (st->isEnabled())
                    connect(st, &QToolButton::clicked, this,
                        [this, ci]{ m_addTagFn(ci < m_op.conditions.size() ? m_op.conditions[ci].tag : QString{}, nullptr); });

                // Tag combo
                auto* tagCb = new QComboBox(row);
                tagCb->setEditable(true);
                tagCb->setInsertPolicy(QComboBox::NoInsert);
                tagCb->addItems(m_knownTags);
                tagCb->setCurrentText(cond.tag);
                tagCb->setPlaceholderText("tag");
                auto* cp = new QCompleter(m_knownTags, tagCb);
                cp->setCaseSensitivity(Qt::CaseInsensitive);
                cp->setFilterMode(Qt::MatchContains);
                tagCb->setCompleter(cp);
                connect(tagCb, &QComboBox::editTextChanged, this,
                    [this, ci, st](const QString& t)
                    {
                        if (ci < m_op.conditions.size()) m_op.conditions[ci].tag = t.trimmed();
                        ApplyTagStatus(st, t.trimmed(), m_knownTags);
                        st->disconnect();
                        if (st->isEnabled())
                            connect(st, &QToolButton::clicked, this,
                                [this, t]{ m_addTagFn(t.trimmed(), nullptr); });
                    });

                // Comparison combo
                auto* cmpCb = new QComboBox(row);
                cmpCb->addItems(kModalComparisons);
                cmpCb->setCurrentText(cond.comparison);

                // Value spin (hidden for Exists/NotExists)
                auto* valSp = new QSpinBox(row);
                valSp->setRange(-99999, 99999);
                valSp->setValue(cond.value);
                valSp->setVisible(kModalIsNumeric(cond.comparison));

                connect(cmpCb, &QComboBox::currentTextChanged, this,
                    [this, ci, valSp](const QString& t)
                    {
                        if (ci < m_op.conditions.size()) m_op.conditions[ci].comparison = t;
                        valSp->setVisible(kModalIsNumeric(t));
                    });
                connect(valSp, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, ci](int v)
                    { if (ci < m_op.conditions.size()) m_op.conditions[ci].value = v; });

                // Logic-op combo (hidden on last row)
                auto* logCb = new QComboBox(row);
                logCb->addItems(kModalLogicOps);
                if (!cond.logicOp.isEmpty()) logCb->setCurrentText(cond.logicOp);
                logCb->setVisible(ci < total - 1);
                connect(logCb, &QComboBox::currentTextChanged, this,
                    [this, ci](const QString& t)
                    { if (ci < m_op.conditions.size()) m_op.conditions[ci].logicOp = t; });

                // Remove button
                auto* remBtn = makeRemoveBtn(row);
                connect(remBtn, &QPushButton::clicked, this, [this, ci]
                {
                    if (ci < m_op.conditions.size()) m_op.conditions.removeAt(ci);
                    rebuildConditions();
                });

                hl->addWidget(st);
                hl->addWidget(tagCb, 3);
                hl->addWidget(cmpCb, 2);
                hl->addWidget(valSp, 1);
                hl->addWidget(logCb, 1);
                hl->addWidget(remBtn);
                m_condLayout->addWidget(row);
            }
            if (total == 0)
                m_condLayout->addWidget(new QLabel("(none)", m_condArea));
            m_condLayout->addStretch();
        }

        OghamOperation m_op;
        QStringList    m_knownTags;
        AddTagFn       m_addTagFn;
        QToolButton*   m_tagStatus  = nullptr;
        QComboBox*     m_tagCombo   = nullptr;
        QComboBox*     m_arithCombo = nullptr;
        QSpinBox*      m_valueBox   = nullptr;
        QWidget*       m_condArea   = nullptr;
        QVBoxLayout*   m_condLayout = nullptr;
    };

    // =========================================================================
    // OptionEditorModal — edit one OghamSourceOption (Choice ID / Localise ID / Conditions / Operations)
    // =========================================================================

    class OptionEditorModal : public QDialog
    {
        using AddTagFn = std::function<void(const QString&, QToolButton*)>;
        using FetchValFn = std::function<QString(const QString&)>;
    public:
        OptionEditorModal(const OghamSourceOption& opt,
                          const QStringList&       lexiconKeys,
                          const QStringList&       oghamTags,
                          AddTagFn                 addTagFn,
                          FetchValFn               fetchLexVal,
                          QWidget*                 parent = nullptr)
            : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
            , m_opt(opt)
            , m_oghamTags(oghamTags)
            , m_lexiconKeys(lexiconKeys)
            , m_addTagFn(std::move(addTagFn))
            , m_fetchLexVal(std::move(fetchLexVal))
        {
            setMinimumWidth(520);
            auto* vl = new QVBoxLayout(this);
            vl->setSpacing(6);
            vl->setContentsMargins(12, 12, 12, 12);

            auto* form = new QFormLayout();
            form->setSpacing(6);

            // ── Choice ID (tag) ───────────────────────────────────────────
            auto* tagRow = new QWidget(this);
            auto* tagHL  = new QHBoxLayout(tagRow);
            tagHL->setContentsMargins(0,0,0,0);
            tagHL->setSpacing(4);

            m_tagStatus = new QToolButton(tagRow);
            m_tagStatus->setFixedWidth(26);
            m_tagStatus->setAutoRaise(true);

            m_tagCombo = new QComboBox(tagRow);
            m_tagCombo->setEditable(true);
            m_tagCombo->setInsertPolicy(QComboBox::NoInsert);
            m_tagCombo->addItems(oghamTags);
            m_tagCombo->setCurrentText(opt.tag);
            m_tagCombo->setPlaceholderText("choice tag\xe2\x80\xa6");
            auto* tagCpl = new QCompleter(oghamTags, m_tagCombo);
            tagCpl->setCaseSensitivity(Qt::CaseInsensitive);
            tagCpl->setFilterMode(Qt::MatchContains);
            m_tagCombo->setCompleter(tagCpl);
            ApplyTagStatus(m_tagStatus, opt.tag, oghamTags);
            refreshTagStatusConn();
            tagHL->addWidget(m_tagStatus);
            tagHL->addWidget(m_tagCombo);
            form->addRow("Choice ID:", tagRow);
            connect(m_tagCombo, &QComboBox::editTextChanged, this,
                [this](const QString& v)
                {
                    m_opt.tag = v.trimmed();
                    ApplyTagStatus(m_tagStatus, v.trimmed(), m_oghamTags);
                    refreshTagStatusConn();
                });

            // ── Localise ID (textKey) ─────────────────────────────────────
            m_keyCombo = new QComboBox(this);
            m_keyCombo->setEditable(true);
            m_keyCombo->setInsertPolicy(QComboBox::NoInsert);
            m_keyCombo->addItems(lexiconKeys);
            m_keyCombo->setCurrentText(opt.textKey);
            m_keyCombo->setPlaceholderText("localisation key\xe2\x80\xa6");
            auto* keyCpl = new QCompleter(lexiconKeys, m_keyCombo);
            keyCpl->setCaseSensitivity(Qt::CaseInsensitive);
            keyCpl->setFilterMode(Qt::MatchContains);
            m_keyCombo->setCompleter(keyCpl);
            form->addRow("Localise ID:", m_keyCombo);

            // ── Localised Value (read-only preview) ───────────────────────
            m_valueEdit = new QLineEdit(m_fetchLexVal(opt.textKey), this);
            m_valueEdit->setPlaceholderText("localised text\xe2\x80\xa6");
            m_valueEdit->setReadOnly(true);
            form->addRow("Localised Value:", m_valueEdit);
            connect(m_keyCombo, &QComboBox::editTextChanged, this,
                [this](const QString& v)
                {
                    m_opt.textKey = v.trimmed();
                    m_valueEdit->setText(m_fetchLexVal(v.trimmed()));
                });

            vl->addLayout(form);

            // ── Collapsible Conditions ────────────────────────────────────
            {
                auto* hdr = new QHBoxLayout();
                m_condToggle = new QToolButton(this);
                m_condToggle->setText("\xe2\x96\xb6"); // ▶
                m_condToggle->setCheckable(true);
                m_condToggle->setAutoRaise(true);
                m_condToggle->setChecked(!opt.conditions.isEmpty());
                hdr->addWidget(m_condToggle);
                hdr->addWidget(new QLabel("<b>Conditions</b>", this));
                hdr->addStretch();
                auto* addCondBtn = makeAddBtn(this);
                hdr->addWidget(addCondBtn);
                vl->addLayout(hdr);

                m_condScroll = new QScrollArea(this);
                m_condScroll->setWidgetResizable(true);
                m_condScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                m_condArea   = new QWidget(this);
                m_condLayout = new QVBoxLayout(m_condArea);
                m_condLayout->setContentsMargins(0,0,0,0);
                m_condLayout->setSpacing(2);
                m_condScroll->setWidget(m_condArea);
                vl->addWidget(m_condScroll);

                connect(m_condToggle, &QToolButton::toggled, this, [this](bool on)
                {
                    m_condToggle->setText(on ? "\xe2\x96\xbc" : "\xe2\x96\xb6");
                    m_condScroll->setVisible(on);
                    if (on) updateCondScrollHeight(); else adjustSize();
                });
                connect(addCondBtn, &QPushButton::clicked, this, [this]
                {
                    m_opt.conditions.append(OghamCondition{});
                    if (!m_condToggle->isChecked()) m_condToggle->setChecked(true);
                    rebuildConditions();
                    updateCondScrollHeight();
                });
                rebuildConditions();
                m_condScroll->setVisible(!opt.conditions.isEmpty());
            }

            // ── Collapsible Operations ────────────────────────────────────
            {
                auto* hdr = new QHBoxLayout();
                m_opsToggle = new QToolButton(this);
                m_opsToggle->setText("\xe2\x96\xb6"); // ▶
                m_opsToggle->setCheckable(true);
                m_opsToggle->setAutoRaise(true);
                m_opsToggle->setChecked(!opt.operations.isEmpty());
                hdr->addWidget(m_opsToggle);
                hdr->addWidget(new QLabel("<b>Operations</b>", this));
                hdr->addStretch();
                auto* addOpBtn = makeAddBtn(this);
                hdr->addWidget(addOpBtn);
                vl->addLayout(hdr);

                m_opsScroll = new QScrollArea(this);
                m_opsScroll->setWidgetResizable(true);
                m_opsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                m_opsScroll->setMinimumHeight(80);
                m_opsArea   = new QWidget(this);
                m_opsLayout = new QVBoxLayout(m_opsArea);
                m_opsLayout->setContentsMargins(0,0,0,0);
                m_opsLayout->setSpacing(4);
                m_opsScroll->setWidget(m_opsArea);
                vl->addWidget(m_opsScroll);

                connect(m_opsToggle, &QToolButton::toggled, this, [this](bool on)
                {
                    m_opsToggle->setText(on ? "\xe2\x96\xbc" : "\xe2\x96\xb6");
                    m_opsScroll->setVisible(on);
                    adjustSize();
                });
                connect(addOpBtn, &QPushButton::clicked, this, [this]
                {
                    m_opt.operations.append(OghamOperation{});
                    if (!m_opsToggle->isChecked()) m_opsToggle->setChecked(true);
                    rebuildOperations();
                });
                rebuildOperations();
                m_opsScroll->setVisible(!opt.operations.isEmpty());
            }

            // Mutual exclusion: expanding one section collapses the other
            connect(m_condToggle, &QToolButton::toggled, this, [this](bool on){ if (on) m_opsToggle->setChecked(false); });
            connect(m_opsToggle,  &QToolButton::toggled, this, [this](bool on){ if (on) m_condToggle->setChecked(false); });

            auto* btns = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            if (auto* ok = btns->button(QDialogButtonBox::Ok))
                { ok->setAutoDefault(false); ok->setDefault(false); }
            vl->addWidget(btns);
            connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

            // Update conditions scroll area max height after layout is built
            QTimer::singleShot(0, this, [this]{ updateCondScrollHeight(); });
        }

        OghamSourceOption result() const { return m_opt; }

    private:
        void refreshTagStatusConn()
        {
            m_tagStatus->disconnect();
            if (m_tagStatus->isEnabled())
                connect(m_tagStatus, &QToolButton::clicked, this,
                    [this]{ m_addTagFn(m_opt.tag, m_tagStatus); });
        }

        // Build one condition row inside a parent widget/layout
        QWidget* makeCondRow(int ci, QWidget* parent)
        {
            const OghamCondition& cond = m_opt.conditions[ci];
            auto* row = new QWidget(parent);
            auto* hl  = new QHBoxLayout(row);
            hl->setContentsMargins(0,0,0,0);
            hl->setSpacing(3);

            auto* st = new QToolButton(row);
            st->setFixedWidth(24);
            st->setAutoRaise(true);
            ApplyTagStatus(st, cond.tag, m_oghamTags);
            if (st->isEnabled())
                connect(st, &QToolButton::clicked, this,
                    [this, ci]{ m_addTagFn(ci < m_opt.conditions.size() ? m_opt.conditions[ci].tag : QString{}, nullptr); });

            auto* tagCb = new QComboBox(row);
            tagCb->setEditable(true);
            tagCb->setInsertPolicy(QComboBox::NoInsert);
            tagCb->addItems(m_oghamTags);
            tagCb->setCurrentText(cond.tag);
            tagCb->setPlaceholderText("tag");
            auto* cp = new QCompleter(m_oghamTags, tagCb);
            cp->setCaseSensitivity(Qt::CaseInsensitive);
            cp->setFilterMode(Qt::MatchContains);
            tagCb->setCompleter(cp);
            connect(tagCb, &QComboBox::editTextChanged, this,
                [this, ci, st](const QString& t)
                {
                    if (ci < m_opt.conditions.size()) m_opt.conditions[ci].tag = t.trimmed();
                    ApplyTagStatus(st, t.trimmed(), m_oghamTags);
                    st->disconnect();
                    if (st->isEnabled())
                        connect(st, &QToolButton::clicked, this,
                            [this, t]{ m_addTagFn(t.trimmed(), nullptr); });
                });

            auto* cmpCb = new QComboBox(row);
            cmpCb->addItems(kModalComparisons);
            cmpCb->setCurrentText(cond.comparison);

            auto* valSp = new QSpinBox(row);
            valSp->setRange(-99999, 99999);
            valSp->setValue(cond.value);
            valSp->setVisible(kModalIsNumeric(cond.comparison));

            connect(cmpCb, &QComboBox::currentTextChanged, this,
                [this, ci, valSp](const QString& t)
                {
                    if (ci < m_opt.conditions.size()) m_opt.conditions[ci].comparison = t;
                    valSp->setVisible(kModalIsNumeric(t));
                });
            connect(valSp, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, ci](int v)
                { if (ci < m_opt.conditions.size()) m_opt.conditions[ci].value = v; });

            const int total = m_opt.conditions.size();
            auto* logCb = new QComboBox(row);
            logCb->addItems(kModalLogicOps);
            if (!cond.logicOp.isEmpty()) logCb->setCurrentText(cond.logicOp);
            logCb->setVisible(ci < total - 1);
            connect(logCb, &QComboBox::currentTextChanged, this,
                [this, ci](const QString& t)
                { if (ci < m_opt.conditions.size()) m_opt.conditions[ci].logicOp = t; });

            auto* remBtn = makeRemoveBtn(row);
            connect(remBtn, &QPushButton::clicked, this, [this, ci]
            {
                if (ci < m_opt.conditions.size()) m_opt.conditions.removeAt(ci);
                rebuildConditions();
                updateCondScrollHeight();
            });

            hl->addWidget(st);
            hl->addWidget(tagCb, 3);
            hl->addWidget(cmpCb, 2);
            hl->addWidget(valSp, 1);
            hl->addWidget(logCb, 1);
            hl->addWidget(remBtn);
            return row;
        }

        void rebuildConditions()
        {
            while (QLayoutItem* it = m_condLayout->takeAt(0))
            { if (QWidget* w = it->widget()) w->deleteLater(); delete it; }
            const int total = m_opt.conditions.size();
            for (int ci = 0; ci < total; ++ci)
                m_condLayout->addWidget(makeCondRow(ci, m_condArea));
            if (total == 0)
                m_condLayout->addWidget(new QLabel("(none)", m_condArea));
            m_condLayout->addStretch();
            if (m_condScroll) updateCondScrollHeight();
        }

        void rebuildOperations()
        {
            while (QLayoutItem* it = m_opsLayout->takeAt(0))
            { if (QWidget* w = it->widget()) w->deleteLater(); delete it; }

            const int total = m_opt.operations.size();
            for (int oi = 0; oi < total; ++oi)
            {
                const OghamOperation& op = m_opt.operations[oi];
                auto* frame = new QFrame(m_opsArea);
                frame->setFrameShape(QFrame::StyledPanel);
                auto* fl = new QVBoxLayout(frame);
                fl->setContentsMargins(6,4,6,4);
                fl->setSpacing(4);

                // Top row: status icon + tag combo + arithmetic + value + X
                auto* topRow = new QWidget(frame);
                auto* hl     = new QHBoxLayout(topRow);
                hl->setContentsMargins(0,0,0,0);
                hl->setSpacing(4);

                auto* st = new QToolButton(topRow);
                st->setFixedWidth(26);
                st->setAutoRaise(true);
                ApplyTagStatus(st, op.tag, m_oghamTags);
                if (st->isEnabled())
                    connect(st, &QToolButton::clicked, this,
                        [this, oi]{ m_addTagFn(oi < m_opt.operations.size() ? m_opt.operations[oi].tag : QString{}, nullptr); });

                auto* tagCb = new QComboBox(topRow);
                tagCb->setEditable(true);
                tagCb->setInsertPolicy(QComboBox::NoInsert);
                tagCb->addItems(m_oghamTags);
                tagCb->setCurrentText(op.tag);
                tagCb->setPlaceholderText("tag");
                auto* cp = new QCompleter(m_oghamTags, tagCb);
                cp->setCaseSensitivity(Qt::CaseInsensitive);
                cp->setFilterMode(Qt::MatchContains);
                tagCb->setCompleter(cp);
                connect(tagCb, &QComboBox::editTextChanged, this,
                    [this, oi, st](const QString& v)
                    {
                        if (oi < m_opt.operations.size()) m_opt.operations[oi].tag = v.trimmed();
                        ApplyTagStatus(st, v.trimmed(), m_oghamTags);
                        st->disconnect();
                        if (st->isEnabled())
                            connect(st, &QToolButton::clicked, this,
                                [this, v]{ m_addTagFn(v.trimmed(), nullptr); });
                    });

                auto* arithCb = new QComboBox(topRow);
                arithCb->addItems(kModalArithmetics);
                arithCb->setCurrentText(op.arithmetic);
                connect(arithCb, &QComboBox::currentTextChanged, this,
                    [this, oi](const QString& v)
                    { if (oi < m_opt.operations.size()) m_opt.operations[oi].arithmetic = v; });

                auto* valSp = new QSpinBox(topRow);
                valSp->setRange(-99999, 99999);
                valSp->setValue(op.value);
                connect(valSp, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, oi](int v)
                    { if (oi < m_opt.operations.size()) m_opt.operations[oi].value = v; });

                auto* remBtn = makeRemoveBtn(topRow);
                connect(remBtn, &QPushButton::clicked, this, [this, oi]
                { if (oi < m_opt.operations.size()) m_opt.operations.removeAt(oi); rebuildOperations(); });

                hl->addWidget(st);
                hl->addWidget(tagCb, 3);
                hl->addWidget(arithCb, 2);
                hl->addWidget(valSp, 1);
                hl->addWidget(remBtn);
                fl->addWidget(topRow);

                // Nested conditions
                auto* condHdr = new QHBoxLayout();
                condHdr->addWidget(new QLabel("  Conditions:", frame));
                condHdr->addStretch();
                auto* addCondBtn = makeAddBtn(frame);
                condHdr->addWidget(addCondBtn);
                fl->addLayout(condHdr);

                auto* condContainer = new QWidget(frame);
                auto* condVL        = new QVBoxLayout(condContainer);
                condVL->setContentsMargins(12,0,0,0);
                condVL->setSpacing(2);

                // Populate op conditions
                const int nConds = op.conditions.size();
                for (int k = 0; k < nConds; ++k)
                {
                    // Build an inline condition row for op[oi].conditions[k]
                    const OghamCondition& c = op.conditions[k];
                    auto* crow = new QWidget(condContainer);
                    auto* chl  = new QHBoxLayout(crow);
                    chl->setContentsMargins(0,0,0,0);
                    chl->setSpacing(3);

                    auto* cst = new QToolButton(crow);
                    cst->setFixedWidth(24);
                    cst->setAutoRaise(true);
                    ApplyTagStatus(cst, c.tag, m_oghamTags);

                    auto* cTagCb = new QComboBox(crow);
                    cTagCb->setEditable(true);
                    cTagCb->setInsertPolicy(QComboBox::NoInsert);
                    cTagCb->addItems(m_oghamTags);
                    cTagCb->setCurrentText(c.tag);
                    cTagCb->setPlaceholderText("tag");
                    auto* ccp = new QCompleter(m_oghamTags, cTagCb);
                    ccp->setCaseSensitivity(Qt::CaseInsensitive);
                    ccp->setFilterMode(Qt::MatchContains);
                    cTagCb->setCompleter(ccp);
                    connect(cTagCb, &QComboBox::editTextChanged, this,
                        [this, oi, k, cst](const QString& t)
                        {
                            if (oi < m_opt.operations.size() && k < m_opt.operations[oi].conditions.size())
                                m_opt.operations[oi].conditions[k].tag = t.trimmed();
                            ApplyTagStatus(cst, t.trimmed(), m_oghamTags);
                        });

                    auto* cCmpCb = new QComboBox(crow);
                    cCmpCb->addItems(kModalComparisons);
                    cCmpCb->setCurrentText(c.comparison);

                    auto* cValSp = new QSpinBox(crow);
                    cValSp->setRange(-99999, 99999);
                    cValSp->setValue(c.value);
                    cValSp->setVisible(kModalIsNumeric(c.comparison));

                    connect(cCmpCb, &QComboBox::currentTextChanged, this,
                        [this, oi, k, cValSp](const QString& t)
                        {
                            if (oi < m_opt.operations.size() && k < m_opt.operations[oi].conditions.size())
                                m_opt.operations[oi].conditions[k].comparison = t;
                            cValSp->setVisible(kModalIsNumeric(t));
                        });
                    connect(cValSp, QOverload<int>::of(&QSpinBox::valueChanged), this,
                        [this, oi, k](int v)
                        {
                            if (oi < m_opt.operations.size() && k < m_opt.operations[oi].conditions.size())
                                m_opt.operations[oi].conditions[k].value = v;
                        });

                    auto* cLogCb = new QComboBox(crow);
                    cLogCb->addItems(kModalLogicOps);
                    if (!c.logicOp.isEmpty()) cLogCb->setCurrentText(c.logicOp);
                    cLogCb->setVisible(k < nConds - 1);
                    connect(cLogCb, &QComboBox::currentTextChanged, this,
                        [this, oi, k](const QString& t)
                        {
                            if (oi < m_opt.operations.size() && k < m_opt.operations[oi].conditions.size())
                                m_opt.operations[oi].conditions[k].logicOp = t;
                        });

                    auto* cRemBtn = makeRemoveBtn(crow);
                    connect(cRemBtn, &QPushButton::clicked, this, [this, oi, k]
                    {
                        if (oi < m_opt.operations.size() && k < m_opt.operations[oi].conditions.size())
                            m_opt.operations[oi].conditions.removeAt(k);
                        rebuildOperations();
                    });

                    chl->addWidget(cst);
                    chl->addWidget(cTagCb, 3);
                    chl->addWidget(cCmpCb, 2);
                    chl->addWidget(cValSp, 1);
                    chl->addWidget(cLogCb, 1);
                    chl->addWidget(cRemBtn);
                    condVL->addWidget(crow);
                }
                if (nConds == 0)
                    condVL->addWidget(new QLabel("(none)", condContainer));

                connect(addCondBtn, &QPushButton::clicked, this, [this, oi]
                {
                    if (oi < m_opt.operations.size())
                        m_opt.operations[oi].conditions.append(OghamCondition{});
                    rebuildOperations();
                });

                fl->addWidget(condContainer);
                m_opsLayout->addWidget(frame);
            }
            if (total == 0)
                m_opsLayout->addWidget(new QLabel("(none)", m_opsArea));
            m_opsLayout->addStretch();
        }

        OghamSourceOption m_opt;
        QStringList       m_oghamTags;
        QStringList       m_lexiconKeys;
        AddTagFn          m_addTagFn;
        FetchValFn        m_fetchLexVal;

        void updateCondScrollHeight()
        {
            const int n    = m_opt.conditions.size();
            const int rowH = 28; // approximate height of one condition row
            const int maxH = rowH * 4 + 8;
            // Fit exactly n rows (min 1 for "(none)" label), cap at 4 rows
            const int h = (n == 0) ? 26 : qMin(n * rowH + 8, maxH);
            m_condScroll->setMaximumHeight(h);
            adjustSize();
        }

        QToolButton*  m_tagStatus   = nullptr;
        QComboBox*    m_tagCombo    = nullptr;
        QComboBox*    m_keyCombo    = nullptr;
        QLineEdit*    m_valueEdit   = nullptr;
        QWidget*      m_condArea    = nullptr;
        QVBoxLayout*  m_condLayout  = nullptr;
        QWidget*      m_opsArea     = nullptr;
        QVBoxLayout*  m_opsLayout   = nullptr;
        QToolButton*  m_condToggle  = nullptr;
        QToolButton*  m_opsToggle   = nullptr;
        QScrollArea*  m_condScroll  = nullptr;
        QScrollArea*  m_opsScroll   = nullptr;
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
                                bool allowEmpty)
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
        // Left group: [Support][Docs][Save .ogmcon][Snap]
        // Right group: [Layout][▶ Play][Import]
        auto* toolbar  = new QWidget(this);
        auto* tbLayout = new QHBoxLayout(toolbar);
        tbLayout->setContentsMargins(4, 4, 4, 4);
        tbLayout->setSpacing(6);

        m_supportBtn = new QPushButton("Support", toolbar);
        m_supportBtn->setToolTip("Open Heathen Discord support channel");
        tbLayout->addWidget(m_supportBtn);

        m_docsBtn = new QPushButton("Docs", toolbar);
        m_docsBtn->setToolTip("Open Ogham Storyteller documentation");
        tbLayout->addWidget(m_docsBtn);

        tbLayout->addSpacing(8);

        m_saveAllBtn = new QPushButton("Save .ogmcon", toolbar);
        m_saveAllBtn->setEnabled(false);
        m_saveAllBtn->setToolTip("Save all modified conversation files");
        tbLayout->addWidget(m_saveAllBtn);

        m_snapBtn = new QPushButton("\xe2\x8a\x9e Snap", toolbar);
        m_snapBtn->setCheckable(true);
        m_snapBtn->setChecked(false);
        m_snapBtn->setToolTip("Snap node positions to 20px grid");
        tbLayout->addWidget(m_snapBtn);

        tbLayout->addStretch();

        m_layoutBtn = new QPushButton("Layout", toolbar);
        m_layoutBtn->setToolTip("Auto-arrange all visible nodes (BFS tree layout)");
        tbLayout->addWidget(m_layoutBtn);

        m_playFromNodeBtn = new QPushButton("\xe2\x96\xb6 Play", toolbar);
        m_playFromNodeBtn->setEnabled(false);
        m_playFromNodeBtn->setToolTip("Play from selected node");
        m_playFromNodeBtn->setStyleSheet(
            "QPushButton { background-color: #2a5c2a; color: white; "
            "font-weight: bold; padding: 5px; border-radius: 3px; }"
            "QPushButton:hover { background-color: #3a7c3a; }"
            "QPushButton:disabled { background-color: #333333; color: #666666; }");
        tbLayout->addWidget(m_playFromNodeBtn);

        m_importBtn = new QPushButton("Import", toolbar);
        m_importBtn->setToolTip("Open Twee/Twine importer (requires Ogham Toolkit)");
        tbLayout->addWidget(m_importBtn);

        // ── Left: 2-column entry tree ────────────────────────────────────────
        m_tree = new QTreeWidget(this);
        m_tree->setColumnCount(2);
        m_tree->setHeaderHidden(true);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_tree->setColumnWidth(1, 28);
        m_tree->setMinimumWidth(150);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

        // ── Graph viewport ────────────────────────────────────────────────────
        m_graphView = new OghamGraphView(this);

        // ── Outer splitter: [search+tree panel] | graph ───────────────────────
        {
            auto* treePanel  = new QWidget(this);
            auto* treePanelL = new QVBoxLayout(treePanel);
            treePanelL->setContentsMargins(0, 0, 0, 0);
            treePanelL->setSpacing(2);

            m_treeSearch = new QLineEdit(treePanel);
            m_treeSearch->setPlaceholderText("Filter entries\xe2\x80\xa6");
            m_treeSearch->setClearButtonEnabled(true);
            treePanelL->addWidget(m_treeSearch);
            treePanelL->addWidget(m_tree);

            m_splitter = new QSplitter(Qt::Horizontal, this);
            m_splitter->addWidget(treePanel);
            m_splitter->addWidget(m_graphView);
            m_splitter->setStretchFactor(0, 2);
            m_splitter->setStretchFactor(1, 8);
            treePanel->setMinimumWidth(150);
        }

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
        connect(m_supportBtn,      &QPushButton::clicked, this, &OghamStoryteller::OnSupport);
        connect(m_docsBtn,         &QPushButton::clicked, this, &OghamStoryteller::OnDocs);
        connect(m_saveAllBtn,      &QPushButton::clicked, this, &OghamStoryteller::OnSaveAll);
        connect(m_snapBtn,         &QPushButton::toggled,  this, &OghamStoryteller::OnSnapToggle);
        connect(m_layoutBtn,       &QPushButton::clicked, this, &OghamStoryteller::OnLayoutGraph);
        connect(m_playFromNodeBtn, &QPushButton::clicked, this, &OghamStoryteller::OnPlayFromNode);
        connect(m_importBtn,       &QPushButton::clicked, this, &OghamStoryteller::OnImport);
        connect(m_treeSearch,      &QLineEdit::textChanged, this, &OghamStoryteller::OnTreeSearch);

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
        connect(m_graphView, &OghamGraphView::deleteNodesRequested,
            this, &OghamStoryteller::OnDeleteNodesFromGraph);
        connect(m_graphView, &OghamGraphView::deleteAliasPinRequested,
            this, &OghamStoryteller::OnDeleteAliasPin);
        connect(m_graphView, &OghamGraphView::deleteAliasPinsRequested,
            this, [this](const QList<QPair<QPair<int,int>,int>>& pins)
            {
                for (const auto& p : pins)
                    OnDeleteAliasPin(p.first.first, p.first.second, p.second);
            });
        connect(m_graphView, &OghamGraphView::rubberBandStarted,
            [this]() { m_suppressFormOnSelect = true; });
        connect(m_graphView, &OghamGraphView::rubberBandEnded,
            [this]()
            {
                m_suppressFormOnSelect = false;
                const auto sel = m_graphView->graphScene()->selectedItems();
                if (sel.size() == 1)
                {
                    if (auto* node = dynamic_cast<OghamNodeItem*>(sel.first()))
                    {
                        PopulateForm(node->fileIdx(), node->entryIdx());
                        SelectEntry(node->fileIdx(), node->entryIdx());
                    }
                }
            });
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
                QMenu menu(this);

                if (!item)
                {
                    // Background click — file-level operations
                    menu.addAction("New File\xe2\x80\xa6", [this]{ OnNewFile(); });
                    menu.exec(m_tree->viewport()->mapToGlobal(pos));
                    return;
                }

                const int fi = item->data(0, kRoleFileIdx).toInt();
                const int ei = item->data(0, kRoleEntryIdx).toInt();

                if (fi >= 0 && ei < 0)
                {
                    // File header row
                    menu.addAction("Add Entry", [this, fi]{ AddRootEntry(fi); });
                    menu.addSeparator();
                    menu.addAction("Open Source File", [this, fi]{
                        if (fi >= 0 && fi < m_loadedFiles.size())
                            QDesktopServices::openUrl(QUrl::fromLocalFile(m_loadedFiles[fi].path));
                    });
                }
                else if (fi >= 0 && ei >= 0)
                {
                    // Entry row
                    const QString nodeTag = item->data(0, kRoleNodeTag).toString();
                    const bool isReal = !nodeTag.isEmpty();
                    menu.addAction("Add Sibling", [this, fi, nodeTag]{ AddSiblingEntry(fi, nodeTag); });
                    menu.addAction("Add Child",   [this, fi, nodeTag]{ AddChildEntry(fi, nodeTag); });
                    if (isReal)
                    {
                        menu.addSeparator();
                        menu.addAction("Remove Entry", [this, fi, ei]{ RemoveEntry(fi, ei); });
                    }
                    if (m_loadedFiles.size() > 1)
                    {
                        menu.addSeparator();
                        QMenu* moveMenu = menu.addMenu("Move to file\xe2\x80\xa6");
                        for (int dfi = 0; dfi < m_loadedFiles.size(); ++dfi)
                        {
                            if (dfi == fi) continue;
                            const QString label = QFileInfo(m_loadedFiles[dfi].path).fileName();
                            QAction* act = moveMenu->addAction(label);
                            connect(act, &QAction::triggered,
                                [this, fi, ei, dfi]() { MoveEntryToFile(fi, ei, dfi); });
                        }
                    }
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
            m_splitter->setSizes({ 200, 800 });

        EnsureOghamTagsFile();
        ScanAndLoadAll();
        LoadGraphMeta();
        RebuildTree();
        UpdateStatusBar();
    }

    void OghamStoryteller::closeEvent(QCloseEvent* event)
    {
        // Warn about unsaved changes
        int dirtyCount = 0;
        for (const auto& lf : m_loadedFiles)
            if (lf.dirty) ++dirtyCount;

        if (dirtyCount > 0)
        {
            const auto reply = QMessageBox::question(this,
                "Unsaved Changes",
                QString("%1 file(s) have unsaved changes.\n"
                        "Save before closing?").arg(dirtyCount),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (reply == QMessageBox::Cancel)
            {
                event->ignore();
                return;
            }
            if (reply == QMessageBox::Save)
                OnSaveAll();
        }

        QSettings cfg("HeathenEngineering", "OghamStoryteller");
        cfg.setValue("splitterState", m_splitter->saveState());
        QWidget::closeEvent(event);
    }

    // -------------------------------------------------------------------------
    // Toolbar slots
    // -------------------------------------------------------------------------

    void OghamStoryteller::OnSupport()
    {
        QDesktopServices::openUrl(QUrl("https://discord.gg/UsNkQWEyww"));
    }

    void OghamStoryteller::OnDocs()
    {
        QDesktopServices::openUrl(QUrl("https://heathen.group/kb/ogham-welcome/"));
    }

    void OghamStoryteller::OnImport()
    {
        // Requires the Ogham Storyteller Toolkit (sponsor gem).
        // The toolkit registers an import handler via EBus; if none is present, inform the user.
        QMessageBox::information(this, "Ogham Importer",
            "The Twee/Twine importer requires the Ogham Storyteller Toolkit.\n\n"
            "Available to GitHub Sponsors at heathen.group/kb/do-more/");
    }

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


    int OghamStoryteller::PropagateTagRename(const QString& oldTag, const QString& newTag,
                                              int srcFi, int srcEi)
    {
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
            if (fi == srcFi && srcEi >= 0 && srcEi < lf.entries.size())
            {
                for (auto& ap : lf.entries[srcEi].aliasPins)
                {
                    if (ap.tag == oldTag)
                    { ap.tag = newTag; ++refCount; changed = true; }
                    else if (ap.tag.startsWith(oldTag + "."))
                    { ap.tag = newTag + ap.tag.mid(oldTag.size()); ++refCount; changed = true; }
                }
            }
            if (changed) SetFileDirty(fi, true);
        }
        return refCount;
    }

    int OghamStoryteller::CascadeDescendantRename(const QString& oldPrefix,
                                                   const QString& newPrefix)
    {
        const QString prefixDot = oldPrefix + ".";
        QMap<QString, QString> renames;
        for (const auto& lf : m_loadedFiles)
            for (const auto& e : lf.entries)
                if (e.tag.startsWith(prefixDot))
                    renames[e.tag] = newPrefix + e.tag.mid(oldPrefix.length());
        if (renames.isEmpty()) return 0;

        int refCount = 0;
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            auto& lf = m_loadedFiles[fi];
            bool changed = false;
            for (auto& e : lf.entries)
            {
                if (renames.contains(e.tag))
                { e.tag = renames[e.tag]; ++refCount; changed = true; }
                for (auto& opt : e.options)
                    if (renames.contains(opt.targetTag))
                    { opt.targetTag = renames[opt.targetTag]; ++refCount; changed = true; }
            }
            if (changed) SetFileDirty(fi, true);
        }
        return refCount;
    }

    // -------------------------------------------------------------------------
    // File operations
    // -------------------------------------------------------------------------

    void OghamStoryteller::ScanAndLoadAll()
    {
        AZ::IO::FixedMaxPath projectPath = AZ::Utils::GetProjectPath();
        if (projectPath.empty()) return;

        // Derive the graph meta path from the project root (editor-only metadata)
        if (m_graphMetaPath.isEmpty())
        {
            const QString projectDir = QString::fromUtf8(projectPath.c_str());
            m_graphMetaPath = projectDir + "/Assets/Storyteller/graph.ogmgraph";
        }

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

            // contentKeys (typed objects) takes precedence; dataKeys/textKeys are legacy fallback
            if (eo.contains("contentKeys") && eo["contentKeys"].isArray())
            {
                for (const QJsonValue ckv : eo["contentKeys"].toArray())
                {
                    const QJsonObject cko = ckv.toObject();
                    OghamSourceKey sk;
                    sk.type = cko.value("type").toString(QStringLiteral("Text"));
                    sk.mode = cko.value("mode").toString(QStringLiteral("Localised"));
                    sk.key  = cko.value("key").toString();
                    entry.dataKeys.append(sk);
                }
            }
            else
            {
                const QString keysField = eo.contains("dataKeys") ? "dataKeys" : "textKeys";
                for (const QJsonValue tk : eo[keysField].toArray())
                {
                    OghamSourceKey sk;
                    sk.key = tk.toString();
                    entry.dataKeys.append(sk);
                }
            }

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
                opt.displayAsTab     = oo["displayAsTab"].toBool(false);
                const QJsonArray rds = oo["redirects"].toArray();
                for (const QJsonValue& rv : rds)
                    if (rv.isObject()) opt.redirects.append(ParsePoint(rv.toObject()));
                opt.conditions  = ParseConditions(oo["conditions"].toArray());
                opt.operations  = ParseOperations(oo["operations"].toArray());
                entry.options.append(opt);
            }

            // Label IDs and highlight color (editor-only, absent in older files)
            if (eo.contains("labelIds"))
            {
                for (const QJsonValue lv : eo["labelIds"].toArray())
                    entry.labelIds.append(lv.toInt());
            }
            if (eo.contains("highlightColor"))
            {
                const QString hc = eo["highlightColor"].toString();
                if (!hc.isEmpty())
                    entry.highlightColor = QColor(hc);
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
            for (const OghamSourceKey& sk : entry.dataKeys)
            {
                QJsonObject cko;
                cko["type"] = sk.type;
                cko["mode"] = sk.mode;
                cko["key"]  = sk.key;
                tks.append(cko);
            }
            eo["contentKeys"] = tks;
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
                if (opt.displayAsTab) oo["displayAsTab"] = true;
                QJsonArray rdsArr;
                for (const QPointF& rd : opt.redirects) rdsArr.append(SerialisePoint(rd));
                oo["redirects"]   = rdsArr;
                oo["conditions"]  = SerialiseConditions(opt.conditions);
                oo["operations"]  = SerialiseOperations(opt.operations);
                optsArr.append(oo);
            }
            eo["options"] = optsArr;

            // Editor-only: label IDs and highlight color
            if (!entry.labelIds.isEmpty())
            {
                QJsonArray lidsArr;
                for (int lid : entry.labelIds) lidsArr.append(lid);
                eo["labelIds"] = lidsArr;
            }
            if (entry.highlightColor.isValid())
                eo["highlightColor"] = entry.highlightColor.name();

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

    QString OghamStoryteller::FindPrimaryLexiconPath() const
    {
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);
        if (azPaths.empty()) return {};

        auto* fileIO = AZ::IO::FileIOBase::GetInstance();
        QString chosen;
        for (const auto& p : azPaths)
        {
            AZ::IO::FixedMaxPath resolved;
            const QString qp = (fileIO && fileIO->ResolvePath(resolved, p.c_str()))
                ? QString::fromUtf8(resolved.c_str())
                : QString::fromUtf8(p.c_str());
            if (QFileInfo(qp).baseName().compare("default", Qt::CaseInsensitive) == 0)
                return qp;
            if (chosen.isEmpty()) chosen = qp;
        }
        return chosen;
    }

    bool OghamStoryteller::WriteLexiconEntry(const QString& key, const QString& value)
    {
        const QString path = FindPrimaryLexiconPath();
        if (path.isEmpty()) return false;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError) return false;

        QJsonObject root    = doc.object();
        QJsonObject entries = root["entries"].toObject();
        entries[key]        = value;
        root["entries"]     = entries;

        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

        FoundationLocalisation::LexiconEditorRequestBus::Broadcast(
            &FoundationLocalisation::LexiconEditorRequests::RefreshKeyTree);
        return true;
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
    }

    // -------------------------------------------------------------------------
    // Inline data-key editing (node hover-reveal buttons)
    // -------------------------------------------------------------------------

    void OghamStoryteller::ShowLexiconFieldModal(int fi, int ei, int rowIdx, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (rowIdx < 0 || rowIdx >= entries[ei].dataKeys.size()) return;

        const OghamSourceKey currentSk = entries[ei].dataKeys[rowIdx];
        const QStringList    known     = FetchKnownLexiconKeys();
        auto fetchVal = [this](const QString& k){ return FetchLexiconValueForKey(k); };

        // Qt::Popup closes any previous popup automatically.
        auto* popup = new OghamLexiconKeyEditPopup(currentSk, known, fetchVal);

        connect(popup, &OghamLexiconKeyEditPopup::committed, this,
            [this, fi, ei, rowIdx](const OghamLexiconKeyEditPopup::Result& res)
            {
                if (fi >= m_loadedFiles.size()) return;
                auto& ents = m_loadedFiles[fi].entries;
                if (ei >= ents.size() || rowIdx >= ents[ei].dataKeys.size()) return;

                // Always update the data model
                ents[ei].dataKeys[rowIdx] = OghamSourceKey{ res.type, res.mode, res.diskKey };
                SetFileDirty(fi, true);

                // Write to lexicon only if the popup determined it's needed
                if (res.lexiconWrite == OghamLexiconKeyEditPopup::LexiconWrite::Write
                        && !res.diskKey.isEmpty())
                    WriteLexiconEntry(res.diskKey, res.lexValue);

                RebuildGraph();
            });

        popup->adjustSize();
        popup->move(screenPos);
        popup->show();
        popup->raise();
        popup->activateWindow();
    }

    void OghamStoryteller::AddDataKey(int fi, int ei, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;

        entries[ei].dataKeys.append(OghamSourceKey{});
        SetFileDirty(fi, true);
        RebuildGraph();

        // Open the modal immediately on the newly created (last) row.
        const int newRowIdx = entries[ei].dataKeys.size() - 1;
        ShowLexiconFieldModal(fi, ei, newRowIdx, screenPos);
    }

    void OghamStoryteller::RemoveDataKey(int fi, int ei, int rowIdx)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (rowIdx < 0 || rowIdx >= entries[ei].dataKeys.size()) return;

        entries[ei].dataKeys.removeAt(rowIdx);
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    // ── Operations (section 0) ───────────────────────────────────────────────

    static QPoint ModalPos(QPoint screenPos, const QDialog& dlg)
    {
        const QRect screen = QApplication::primaryScreen()
            ? QApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, 1920, 1080);
        QPoint pos = screenPos - QPoint(dlg.width() / 2, 0);
        pos.setX(qBound(screen.left(), pos.x(), screen.right()  - dlg.width()));
        pos.setY(qBound(screen.top(),  pos.y(), screen.bottom() - dlg.height()));
        return pos;
    }

    void OghamStoryteller::ShowOperationModal(int fi, int ei, int row, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        auto& entOps = entries[ei].entryOperations;
        if (row < 0 || row >= entOps.size()) return;

        auto addTagFn = [this](const QString& tag, QToolButton* btn)
        { ShowAddTagDialog(tag, btn); };

        // Qt::Popup closes any previous popup automatically when a new one opens.
        auto* popup = new OghamOperationEditPopup(
            entOps[row], FetchAllTagsFromAllFiles(), addTagFn);

        connect(popup, &OghamOperationEditPopup::committed, this,
            [this, fi, ei, row](const OghamOperation& result)
            {
                if (fi >= m_loadedFiles.size()) return;
                auto& ents = m_loadedFiles[fi].entries;
                if (ei >= ents.size() || row >= ents[ei].entryOperations.size()) return;
                ents[ei].entryOperations[row] = result;
                SetFileDirty(fi, true);
                RebuildGraph();
            });

        popup->adjustSize();
        popup->move(screenPos);
        popup->show();
        popup->raise();
        popup->activateWindow();
    }

    void OghamStoryteller::AddEntryOperation(int fi, int ei, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;

        entries[ei].entryOperations.append(OghamOperation{});
        SetFileDirty(fi, true);
        RebuildGraph();
        ShowOperationModal(fi, ei, entries[ei].entryOperations.size() - 1, screenPos);
    }

    void OghamStoryteller::RemoveEntryOperation(int fi, int ei, int row)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (row < 0 || row >= entries[ei].entryOperations.size()) return;

        entries[ei].entryOperations.removeAt(row);
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    // ── Options / Choices (section 2) ────────────────────────────────────────

    void OghamStoryteller::ShowOptionModal(int fi, int ei, int row, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        auto& opts = entries[ei].options;
        if (row < 0 || row >= opts.size()) return;

        auto addTagFn   = [this](const QString& tag, QToolButton* btn)
        { ShowAddTagDialog(tag, btn); };
        auto fetchLexFn = [this](const QString& key){ return FetchLexiconValueForKey(key); };

        OptionEditorModal dlg(
            opts[row],
            FetchKnownLexiconKeys(),
            FetchAllTagsFromAllFiles(),
            addTagFn,
            fetchLexFn,
            this);
        dlg.adjustSize();
        dlg.move(ModalPos(screenPos, dlg));
        if (dlg.exec() != QDialog::Accepted) return;

        OghamSourceOption updated = dlg.result();
        updated.redirects        = opts[row].redirects;   // preserve graph reroute points
        // Preserve fields not shown in this modal
        updated.targetTag        = opts[row].targetTag;
        updated.targetAliasIndex = opts[row].targetAliasIndex;
        updated.displayAsTab     = opts[row].displayAsTab;
        opts[row] = updated;
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    void OghamStoryteller::AddEntryOption(int fi, int ei, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;

        entries[ei].options.append(OghamSourceOption{});
        SetFileDirty(fi, true);
        RebuildGraph();
        ShowOptionModal(fi, ei, entries[ei].options.size() - 1, screenPos);
    }

    void OghamStoryteller::RemoveEntryOption(int fi, int ei, int row)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        if (row < 0 || row >= entries[ei].options.size()) return;

        entries[ei].options.removeAt(row);
        SetFileDirty(fi, true);
        RebuildGraph();
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
        hl->setSpacing(0);

        // Eye toggle only — show/hide this file's nodes in the graph
        const bool initVis = (fileIdx < m_loadedFiles.size()) && m_loadedFiles[fileIdx].visible;
        auto* eyeBtn = new QPushButton(initVis ? "\xe2\x97\x8f" : "\xe2\x97\x8b", w);  // ● / ○
        eyeBtn->setFixedSize(22, 22);
        eyeBtn->setStyleSheet(initVis ? "color: #6ab0d0;" : "color: #555555;");
        eyeBtn->setToolTip(initVis ? "Hide nodes in graph" : "Show nodes in graph");
        hl->addWidget(eyeBtn);

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
        return w;
    }

    QWidget* OghamStoryteller::MakeEntryButtons(int /*fileIdx*/, int /*entryIdx*/,
                                                  bool /*isReal*/, const QString& /*nodeTag*/)
    {
        return nullptr;  // all entry-level operations are via right-click context menu
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

        // Preserve selection across rebuild
        QSet<QString> previouslySelected;
        for (const QGraphicsItem* item : scene->selectedItems())
            if (const auto* node = dynamic_cast<const OghamNodeItem*>(item))
                previouslySelected.insert(node->tag());

        scene->clear();

        // ── Phase 1: create nodes and build tag lookup ────────────────────────
        QMap<QString, OghamNodeItem*> tagToNode;
        const QStringList knownOghamTags = FetchKnownOghamTags();

        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            const LoadedFile& lf = m_loadedFiles[fi];
            for (int ei = 0; ei < lf.entries.size(); ++ei)
            {
                const OghamSourceEntry& entry = lf.entries[ei];

                QStringList optLabels;
                for (const OghamSourceOption& opt : entry.options)
                {
                    const QString lexVal = opt.textKey.isEmpty() ? QString()
                                        : FetchLexiconValueForKey(opt.textKey);
                    // Display the resolved localised string; fall back to key then tag
                    optLabels.append(!lexVal.isEmpty()      ? lexVal
                                   : !opt.textKey.isEmpty() ? opt.textKey
                                   : opt.tag);
                }

                QStringList opLabels;
                for (const OghamOperation& op : entry.entryOperations)
                    opLabels.append(op.tag + QStringLiteral(" ") + op.arithmetic
                                    + QStringLiteral(" ") + QString::number(op.value));

                QStringList dataKeyIds;
                QStringList dataLabels;
                for (const OghamSourceKey& sk : entry.dataKeys)
                {
                    dataKeyIds.append(sk.key);
                    QString val;
                    if (sk.mode == QLatin1String("Localised"))
                        val = sk.key.isEmpty() ? QString() : FetchLexiconValueForKey(sk.key);
                    else
                        val = sk.key;
                    if (sk.type != QLatin1String("Text") && !sk.type.isEmpty())
                        val = QString("[%1] %2").arg(sk.type, val.isEmpty() ? sk.key : val);
                    dataLabels.append(val.isEmpty() ? sk.key : val);
                }

                const bool tagInReg = knownOghamTags.contains(entry.tag);

                // Resolve label IDs → display data for this entry
                OghamNodeItem::LabelData assignedLabels;
                for (int lid : entry.labelIds)
                {
                    for (const OghamLabel& lbl : m_graphLabels)
                    {
                        if (lbl.id == lid)
                        {
                            assignedLabels.append({lbl.color, lbl.name});
                            break;
                        }
                    }
                }

                const QColor hdrColor = m_loadedFiles[fi].fileColor.isValid()
                    ? m_loadedFiles[fi].fileColor
                    : kGraphFileColors[fi % kGraphFileColorCount];
                auto* node = new OghamNodeItem(fi, ei, entry.tag, tagInReg,
                    opLabels, dataKeyIds, dataLabels, optLabels,
                    assignedLabels, entry.highlightColor, hdrColor);

                node->setPos(entry.position);

                node->setSnapToGrid(m_snapToGrid);
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
                        if (!m_suppressFormOnSelect)
                        {
                            PopulateForm(nfi, nei);
                            SelectEntry(nfi, nei);
                        }
                    });

                connect(node, &OghamNodeItem::grabStarted,
                    [this](int, int) { m_isInteracting = true; });
                connect(node, &OghamNodeItem::grabEnded,
                    [this](int, int) { m_isInteracting = false; });

                connect(node, &OghamNodeItem::createAliasPinRequested,
                    this, &OghamStoryteller::OnCreateAliasPin);
                connect(node, &OghamNodeItem::cascadeFromNodeRequested,
                    this, &OghamStoryteller::OnCascadeFromNode);
                connect(node, &OghamNodeItem::duplicateNodeRequested,
                    this, &OghamStoryteller::OnDuplicateNode);
                connect(node, &OghamNodeItem::deleteNodeRequested,
                    this, &OghamStoryteller::OnDeleteNodeFromGraph);

                connect(node, &OghamNodeItem::tagStatusClicked,
                    [this](int nfi, int nei, QPoint)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        if (nei < 0 || nei >= m_loadedFiles[nfi].entries.size()) return;
                        ShowAddTagDialog(m_loadedFiles[nfi].entries[nei].tag, nullptr);
                        RebuildGraph();
                    });

                connect(node, &OghamNodeItem::tagRenameRequested,
                    [this](int nfi, int nei, QPoint)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        if (nei < 0 || nei >= m_loadedFiles[nfi].entries.size()) return;
                        const QString oldTag = m_loadedFiles[nfi].entries[nei].tag;

                        bool ok = false;
                        const QString input = QInputDialog::getText(
                            this, "Rename Entry", "New tag:", QLineEdit::Normal, oldTag, &ok);
                        if (!ok) return;
                        const QString newTag = input.trimmed();
                        if (newTag == oldTag || !IsValidTagStructure(newTag)) return;

                        m_loadedFiles[nfi].entries[nei].tag = newTag;
                        SetFileDirty(nfi, true);
                        const int refs = PropagateTagRename(oldTag, newTag, nfi, nei);
                        if (m_selectedFileIdx == nfi && m_selectedEntryIdx == nei)
                            m_renamedFromTag = newTag;

                        int cascadeRefs = 0;
                        {
                            const QString pfx = oldTag + ".";
                            int descCount = 0;
                            for (const auto& lf : m_loadedFiles)
                                for (const auto& e : lf.entries)
                                    if (e.tag.startsWith(pfx)) ++descCount;
                            if (descCount > 0)
                            {
                                const auto reply = QMessageBox::question(this, "Cascade Rename",
                                    QString("%1 child entr(ies) share the '%2.' prefix.\n"
                                            "Rename them to '%3.'?")
                                        .arg(descCount).arg(oldTag).arg(newTag),
                                    QMessageBox::Yes | QMessageBox::No);
                                if (reply == QMessageBox::Yes)
                                    cascadeRefs = CascadeDescendantRename(oldTag, newTag);
                            }
                        }

                        RebuildGraph();
                        RebuildTree();
                        QString msg = QString("Renamed '%1' \xe2\x86\x92 '%2' (%3 ref(s) updated).")
                            .arg(oldTag, newTag).arg(refs);
                        if (cascadeRefs > 0)
                            msg += QString(" Cascade: %1 item(s) renamed.").arg(cascadeRefs);
                        m_statusLabel->setText(msg);
                    });

                connect(node, &OghamNodeItem::fieldClicked,
                    [this](int nfi, int nei, int sec, int row, QPoint sp)
                    {
                        if      (sec == 0) ShowOperationModal(nfi, nei, row, sp);
                        else if (sec == 1) ShowLexiconFieldModal(nfi, nei, row, sp);
                        else if (sec == 2) ShowOptionModal(nfi, nei, row, sp);
                    });
                connect(node, &OghamNodeItem::sectionAddClicked,
                    [this](int nfi, int nei, int sec, QPoint sp)
                    {
                        if      (sec == 0) AddEntryOperation(nfi, nei, sp);
                        else if (sec == 1) AddDataKey(nfi, nei, sp);
                        else if (sec == 2) AddEntryOption(nfi, nei, sp);
                    });
                connect(node, &OghamNodeItem::rowRemoveClicked,
                    [this](int nfi, int nei, int sec, int row)
                    {
                        if      (sec == 0) RemoveEntryOperation(nfi, nei, row);
                        else if (sec == 1) RemoveDataKey(nfi, nei, row);
                        else if (sec == 2) RemoveEntryOption(nfi, nei, row);
                    });

                connect(node, &OghamNodeItem::addLabelRequested,
                    [this](int nfi, int nei, QPoint sp)
                    { ShowLabelModal(nfi, nei, sp); });
                connect(node, &OghamNodeItem::setHighlightColorRequested,
                    [this](int nfi, int nei)
                    { ShowHighlightColorPicker(nfi, nei); });
                connect(node, &OghamNodeItem::clearHighlightColorRequested,
                    [this](int nfi, int nei)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        auto& entries = m_loadedFiles[nfi].entries;
                        if (nei < 0 || nei >= entries.size()) return;
                        entries[nei].highlightColor = QColor();
                        SetFileDirty(nfi, true);
                        RebuildGraph();
                    });
                connect(node, &OghamNodeItem::rowMoveUpClicked,
                    [this](int nfi, int nei, int sec, int row)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        auto& entries = m_loadedFiles[nfi].entries;
                        if (nei < 0 || nei >= entries.size() || row <= 0) return;
                        auto& e = entries[nei];
                        if      (sec == 0 && row < e.entryOperations.size())
                            { e.entryOperations.move(row, row - 1); SetFileDirty(nfi, true); RebuildGraph(); }
                        else if (sec == 1 && row < e.dataKeys.size())
                            { e.dataKeys.move(row, row - 1);        SetFileDirty(nfi, true); RebuildGraph(); }
                        else if (sec == 2 && row < e.options.size())
                            { e.options.move(row, row - 1);         SetFileDirty(nfi, true); RebuildGraph(); }
                    });
                connect(node, &OghamNodeItem::rowMoveDownClicked,
                    [this](int nfi, int nei, int sec, int row)
                    {
                        if (nfi < 0 || nfi >= m_loadedFiles.size()) return;
                        auto& entries = m_loadedFiles[nfi].entries;
                        if (nei < 0 || nei >= entries.size() || row < 0) return;
                        auto& e = entries[nei];
                        if      (sec == 0 && row < e.entryOperations.size() - 1)
                            { e.entryOperations.move(row, row + 1); SetFileDirty(nfi, true); RebuildGraph(); }
                        else if (sec == 1 && row < e.dataKeys.size() - 1)
                            { e.dataKeys.move(row, row + 1);        SetFileDirty(nfi, true); RebuildGraph(); }
                        else if (sec == 2 && row < e.options.size() - 1)
                            { e.options.move(row, row + 1);         SetFileDirty(nfi, true); RebuildGraph(); }
                    });
                connect(node, &OghamNodeItem::labelPillClicked,
                    [this]()
                    {
                        OghamNodeItem::s_labelsExpanded = !OghamNodeItem::s_labelsExpanded;
                        if (m_graphView && m_graphView->graphScene())
                            m_graphView->graphScene()->update();
                    });
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

                        // Look up destination highlight color from data model
                        const int dfi = dstNode->fileIdx();
                        const int dei = dstNode->entryIdx();
                        const QColor dstHL = (dfi >= 0 && dfi < m_loadedFiles.size() &&
                                              dei >= 0 && dei < m_loadedFiles[dfi].entries.size())
                            ? m_loadedFiles[dfi].entries[dei].highlightColor
                            : QColor();

                        conn = new OghamConnectionItem(srcNode, oi,
                            [dstNode]() { return dstNode->inputPinScenePos(); },
                            opt.redirects, opt.targetTag, opt.displayAsTab,
                            dstNode->fileIdx(), dstHL);

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
                            opt.redirects, opt.targetTag, opt.displayAsTab,
                            ap->fileIdx());

                        connect(ap, &OghamAliasPinItem::positionChanged, conn,
                            [conn](QPointF) { conn->refreshPath(); });
                    }

                    // When the source node's sections expand/collapse, output pin positions change.
                    connect(srcNode, &OghamNodeItem::layoutChanged, conn,
                        [conn](int, int) { conn->refreshPath(); });

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

                    // Persist wire↔tab mode toggle
                    connect(conn, &OghamConnectionItem::displayModeChanged,
                        this, [this, fi, ei, oi](bool asTab)
                        {
                            if (fi < 0 || fi >= m_loadedFiles.size()) return;
                            auto& entries = m_loadedFiles[fi].entries;
                            if (ei < 0 || ei >= entries.size()) return;
                            if (oi < 0 || oi >= entries[ei].options.size()) return;
                            entries[ei].options[oi].displayAsTab = asTab;
                            SetFileDirty(fi, true);
                        });

                    // Click on tab → select target node in the graph
                    connect(conn, &OghamConnectionItem::tabClicked,
                        this, [this](const QString& targetTag)
                        {
                            for (int nfi = 0; nfi < m_loadedFiles.size(); ++nfi)
                            {
                                const auto& entries = m_loadedFiles[nfi].entries;
                                for (int nei = 0; nei < entries.size(); ++nei)
                                {
                                    if (entries[nei].tag != targetTag) continue;
                                    PopulateForm(nfi, nei);
                                    SelectEntry(nfi, nei);
                                    for (QGraphicsItem* gi : m_graphView->graphScene()->items())
                                    {
                                        if (auto* n = dynamic_cast<OghamNodeItem*>(gi))
                                        {
                                            if (n->fileIdx() == nfi && n->entryIdx() == nei)
                                            {
                                                m_graphView->graphScene()->clearSelection();
                                                n->setSelected(true);
                                                m_graphView->centerOn(n);
                                                break;
                                            }
                                        }
                                    }
                                    return;
                                }
                            }
                        });
                }
            }
        }

        // ── Phase 3: restore graph selection ──────────────────────────────────────
        if (!previouslySelected.isEmpty())
        {
            for (QGraphicsItem* item : scene->items())
            {
                if (auto* node = dynamic_cast<OghamNodeItem*>(item))
                {
                    if (previouslySelected.contains(node->tag()))
                        node->setSelected(true);
                }
            }
        }

        // ── Phase 4: mark pin connection states (open vs filled triangle) ────────
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            const LoadedFile& lf = m_loadedFiles[fi];
            for (int ei = 0; ei < lf.entries.size(); ++ei)
            {
                OghamNodeItem* srcNode = tagToNode.value(lf.entries[ei].tag);
                if (!srcNode) continue;

                for (int oi = 0; oi < lf.entries[ei].options.size(); ++oi)
                {
                    const QString& dstTag = lf.entries[ei].options[oi].targetTag;
                    if (dstTag.isEmpty()) continue;

                    srcNode->setOutputConnected(oi, true);

                    OghamNodeItem* dstNode = tagToNode.value(dstTag);
                    if (dstNode)
                        dstNode->setInputConnected(true);
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

    void OghamStoryteller::OnCascadeFromNode(int fi, int ei, QPoint /*screenPos*/)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& lf = m_loadedFiles[fi];
        if (ei < 0 || ei >= lf.entries.size()) return;
        const QString srcTag = lf.entries[ei].tag;

        // ── Dialog ─────────────────────────────────────────────────────────────
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Create Cascade"));
        dlg.setFixedWidth(400);
        auto* vlay = new QVBoxLayout(&dlg);

        vlay->addWidget(new QLabel(
            "Split this node into a chain of N copies.\n"
            "The original is renamed to <tag>.<suffix>.<N>; N-1 new nodes are\n"
            "inserted before it, each with the same data fields.\n"
            "You then manually reduce the text in each copy.", &dlg));

        auto* formLay = new QFormLayout;

        // Suggest the last dot-segment of the original tag as base, then add suffix
        const int lastDot = srcTag.lastIndexOf(QLatin1Char('.'));
        const QString defaultBase = (lastDot >= 0) ? srcTag.mid(lastDot + 1) : srcTag;

        auto* suffixEdit = new QLineEdit(QStringLiteral("part"), &dlg);
        suffixEdit->setPlaceholderText("e.g. part, line, beat");
        auto* countSpin  = new QSpinBox(&dlg);
        countSpin->setRange(2, 20);
        countSpin->setValue(3);

        formLay->addRow("Suffix:", suffixEdit);
        formLay->addRow("Number of nodes (N):", countSpin);

        // Preview label
        auto* previewLabel = new QLabel(&dlg);
        previewLabel->setStyleSheet("color: #888;");
        auto updatePreview = [&]()
        {
            const QString suf = suffixEdit->text().trimmed();
            const int     n   = countSpin->value();
            if (!suf.isEmpty() && n >= 2)
            {
                previewLabel->setText(
                    QString("e.g. %1.%2.001 \xe2\x86\x92 \xe2\x80\xa6 \xe2\x86\x92 %1.%2.%3")
                        .arg(srcTag, suf).arg(n, 3, 10, QChar('0')));
            }
            else
            {
                previewLabel->setText("(enter suffix above)");
            }
        };
        connect(suffixEdit, &QLineEdit::textChanged, [&](const QString&) { updatePreview(); });
        connect(countSpin,  QOverload<int>::of(&QSpinBox::valueChanged), [&](int) { updatePreview(); });
        updatePreview();

        vlay->addLayout(formLay);
        vlay->addWidget(previewLabel);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vlay->addWidget(btns);

        if (dlg.exec() != QDialog::Accepted) return;

        const QString suffix = suffixEdit->text().trimmed();
        const int     N      = countSpin->value();
        if (suffix.isEmpty() || N < 2) return;

        // ── Generate all N tags ─────────────────────────────────────────────────
        // allTags[0..N-2] = new nodes; allTags[N-1] = renamed original
        QStringList allTags;
        allTags.reserve(N);
        for (int i = 1; i <= N; ++i)
            allTags.append(QString("%1.%2.%3").arg(srcTag, suffix).arg(i, 3, 10, QChar('0')));

        // Collision check
        for (const QString& t : allTags)
        {
            for (const auto& loadedFile : m_loadedFiles)
                for (const auto& e : loadedFile.entries)
                    if (e.tag == t)
                    {
                        QMessageBox::warning(this, "Tag Collision",
                            QString("Tag '%1' already exists. Choose a different suffix.").arg(t));
                        return;
                    }
        }

        // ── Save original data fields before we mutate anything ─────────────────
        const QList<OghamSourceKey> origDataKeys = lf.entries[ei].dataKeys;
        const QPointF     srcPos       = lf.entries[ei].position;
        static constexpr qreal kXGap  = OghamNodeItem::kNodeWidth + 60.0;

        // ── Step 1: redirect all INCOMING connections to original → first new node ─
        // Any option that previously targeted srcTag should now target allTags[0]
        for (int f2 = 0; f2 < m_loadedFiles.size(); ++f2)
        {
            bool changed = false;
            for (auto& entry : m_loadedFiles[f2].entries)
            {
                for (auto& opt : entry.options)
                {
                    if (opt.targetTag == srcTag)
                    {
                        opt.targetTag = allTags[0];
                        changed = true;
                    }
                }
            }
            if (changed)
                SetFileDirty(f2, true);
        }

        // ── Step 2: create N-1 new nodes ──────────────────────────────────────────
        for (int i = 0; i < N - 1; ++i)
        {
            OghamSourceEntry entry;
            entry.tag      = allTags[i];
            entry.position = srcPos + QPointF(kXGap * (i + 1), 0.0);
            entry.dataKeys = origDataKeys;     // copy data fields from original

            // One choice pointing to the next in chain
            OghamSourceOption opt;
            opt.tag       = QStringLiteral("continue");
            opt.targetTag = allTags[i + 1];    // next = new node or renamed original
            entry.options.append(opt);

            lf.entries.append(entry);
        }

        // ── Step 3: rename original to allTags[N-1] and move it ───────────────────
        lf.entries[ei].tag      = allTags[N - 1];
        lf.entries[ei].position = srcPos + QPointF(kXGap * N, 0.0);

        // ── Step 4: finish ─────────────────────────────────────────────────────────
        SetFileDirty(fi, true);
        RebuildTree();
        RebuildGraph();
        m_statusLabel->setText(
            QString("Cascade: %1 nodes created, '%2' renamed to '%3'.")
                .arg(N - 1).arg(srcTag).arg(allTags[N - 1]));
    }

    void OghamStoryteller::OnDeleteNodesFromGraph(QList<QPair<int,int>> fileEntryPairs)
    {
        if (fileEntryPairs.isEmpty()) return;

        // Collect all tags that will be removed so we can clear cross-references
        QSet<QString> tagsToRemove;
        for (const auto& [fi, ei] : fileEntryPairs)
        {
            if (fi >= 0 && fi < m_loadedFiles.size() &&
                ei >= 0 && ei < m_loadedFiles[fi].entries.size())
            {
                tagsToRemove.insert(m_loadedFiles[fi].entries[ei].tag);
            }
        }

        // Clear option targetTags pointing to any deleted entry across all files
        for (int fi = 0; fi < m_loadedFiles.size(); ++fi)
        {
            bool changed = false;
            for (auto& e : m_loadedFiles[fi].entries)
            {
                for (auto& opt : e.options)
                {
                    if (tagsToRemove.contains(opt.targetTag))
                    {
                        opt.targetTag        = QString();
                        opt.targetAliasIndex = 0;
                        changed = true;
                    }
                }
            }
            if (changed)
                SetFileDirty(fi, true);
        }

        // Group by file, sort entry indices descending so removals don't shift subsequent indices
        QMap<int, QList<int>> byFile;
        for (const auto& [fi, ei] : fileEntryPairs)
        {
            if (fi >= 0 && fi < m_loadedFiles.size() &&
                ei >= 0 && ei < m_loadedFiles[fi].entries.size())
            {
                byFile[fi].append(ei);
            }
        }
        for (auto& indices : byFile)
            std::sort(indices.begin(), indices.end(), std::greater<int>());

        for (auto it = byFile.constBegin(); it != byFile.constEnd(); ++it)
        {
            const int fi = it.key();
            auto& entries = m_loadedFiles[fi].entries;
            for (int ei : it.value())
            {
                entries.removeAt(ei);

                // Keep selected entry pointer consistent
                if (m_selectedFileIdx == fi)
                {
                    if (m_selectedEntryIdx == ei)
                    {
                        m_selectedFileIdx  = -1;
                        m_selectedEntryIdx = -1;
                        ClearForm();
                    }
                    else if (m_selectedEntryIdx > ei)
                    {
                        --m_selectedEntryIdx;
                    }
                }
            }
            SetFileDirty(fi, true);
        }

        const int n = fileEntryPairs.size();
        RebuildTree();
        RebuildGraph();
        m_statusLabel->setText(
            n == 1 ? QString("Deleted 1 node.") : QString("Deleted %1 nodes.").arg(n));
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
        m_playFromNodeBtn->setEnabled(true);
    }

    void OghamStoryteller::ClearForm()
    {
        m_selectedFileIdx  = -1;
        m_selectedEntryIdx = -1;
        m_playFromNodeBtn->setEnabled(false);
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
        m_saveAllBtn->setStyleSheet(dirtyFiles > 0
            ? "background-color: #4a7c2f; color: white; font-weight: bold;"
            : "");

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

    // -------------------------------------------------------------------------
    // Label management
    // -------------------------------------------------------------------------

    void OghamStoryteller::LoadGraphMeta()
    {
        m_graphLabels.clear();
        if (m_graphMetaPath.isEmpty()) return;

        QFile f(m_graphMetaPath);
        if (!f.open(QIODevice::ReadOnly)) return;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

        const QJsonArray labelsArr = doc.object()["labels"].toArray();
        for (const QJsonValue& v : labelsArr)
        {
            const QJsonObject o = v.toObject();
            OghamLabel lbl;
            lbl.id    = o["id"].toInt();
            lbl.name  = o["name"].toString();
            lbl.color = QColor(o["color"].toString());
            m_graphLabels.append(lbl);
        }
    }

    void OghamStoryteller::SaveGraphMeta()
    {
        if (m_graphMetaPath.isEmpty()) return;

        QJsonArray labelsArr;
        for (const OghamLabel& lbl : m_graphLabels)
        {
            QJsonObject o;
            o["id"]    = lbl.id;
            o["name"]  = lbl.name;
            o["color"] = lbl.color.name();
            labelsArr.append(o);
        }

        QJsonObject root;
        root["labels"] = labelsArr;

        QFile f(m_graphMetaPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    void OghamStoryteller::ShowLabelModal(int fi, int ei, QPoint screenPos)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;
        OghamSourceEntry& entry = entries[ei];

        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Labels");
        dlg->setMinimumWidth(300);
        auto* vl = new QVBoxLayout(dlg);

        // Existing labels as checkboxes
        QMap<int, QCheckBox*> cbMap;
        for (const OghamLabel& lbl : m_graphLabels)
        {
            auto* cb = new QCheckBox(lbl.name, dlg);
            cb->setChecked(entry.labelIds.contains(lbl.id));
            // Color swatch in checkbox via stylesheet
            const QString bg = lbl.color.name();
            cb->setStyleSheet(QString("QCheckBox { color: %1; font-weight: bold; }").arg(bg));
            cbMap[lbl.id] = cb;
            vl->addWidget(cb);
        }

        vl->addSpacing(6);

        // New label row
        auto* newRow = new QWidget(dlg);
        auto* newHL  = new QHBoxLayout(newRow);
        newHL->setContentsMargins(0, 0, 0, 0);
        auto* newName  = new QLineEdit(newRow);
        newName->setPlaceholderText("New label name...");
        auto* newColor = new QPushButton(newRow);
        newColor->setFixedSize(24, 24);
        QColor pickedColor(0x4a, 0x90, 0xd9);
        auto updateColorBtn = [&pickedColor, newColor]()
        {
            newColor->setStyleSheet(
                QString("background-color:%1; border:1px solid #888;").arg(pickedColor.name()));
        };
        updateColorBtn();
        connect(newColor, &QPushButton::clicked, dlg,
            [dlg, &pickedColor, updateColorBtn]()
            {
                QColor c = QColorDialog::getColor(pickedColor, dlg, "Label Color");
                if (c.isValid()) { pickedColor = c; updateColorBtn(); }
            });
        newHL->addWidget(newName);
        newHL->addWidget(newColor);
        vl->addWidget(newRow);

        auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        vl->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

        dlg->adjustSize();
        const QRect screen = QApplication::primaryScreen()
            ? QApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, 1920, 1080);
        QPoint pos = screenPos - QPoint(dlg->width() / 2, 0);
        pos.setX(qBound(screen.left(), pos.x(), screen.right()  - dlg->width()));
        pos.setY(qBound(screen.top(),  pos.y(), screen.bottom() - dlg->height()));
        dlg->move(pos);

        if (dlg->exec() != QDialog::Accepted) { delete dlg; return; }

        // Create new label if name is non-empty
        int newLabelId = -1;
        const QString newLabelName = newName->text().trimmed();
        if (!newLabelName.isEmpty())
        {
            int maxId = 0;
            for (const OghamLabel& l : m_graphLabels) maxId = qMax(maxId, l.id);
            OghamLabel newLbl;
            newLbl.id    = maxId + 1;
            newLbl.name  = newLabelName;
            newLbl.color = pickedColor;
            newLabelId = newLbl.id;
            m_graphLabels.append(newLbl);
            SaveGraphMeta();
        }

        // Apply checkbox state for existing labels + always-on newly created one
        entry.labelIds.clear();
        for (auto it = cbMap.cbegin(); it != cbMap.cend(); ++it)
        {
            if (it.value()->isChecked())
                entry.labelIds.append(it.key());
        }
        if (newLabelId >= 0)
            entry.labelIds.append(newLabelId);

        delete dlg;
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    void OghamStoryteller::ShowHighlightColorPicker(int fi, int ei)
    {
        if (fi < 0 || fi >= m_loadedFiles.size()) return;
        auto& entries = m_loadedFiles[fi].entries;
        if (ei < 0 || ei >= entries.size()) return;

        const QColor current = entries[ei].highlightColor;
        const QColor chosen  = QColorDialog::getColor(
            current.isValid() ? current : Qt::white, this, "Node Highlight Color");
        if (!chosen.isValid()) return;

        entries[ei].highlightColor = chosen;
        SetFileDirty(fi, true);
        RebuildGraph();
    }

    // -------------------------------------------------------------------------
    // Snap to grid
    // -------------------------------------------------------------------------

    void OghamStoryteller::OnSnapToggle()
    {
        m_snapToGrid = m_snapBtn->isChecked();
        for (QGraphicsItem* gi : m_graphView->graphScene()->items())
            if (auto* ni = dynamic_cast<OghamNodeItem*>(gi))
                ni->setSnapToGrid(m_snapToGrid);
    }

    // -------------------------------------------------------------------------
    // Node alignment
    // -------------------------------------------------------------------------

    void OghamStoryteller::AlignSelected(int mode)
    {
        QList<OghamNodeItem*> nodes;
        for (QGraphicsItem* gi : m_graphView->graphScene()->selectedItems())
            if (auto* ni = dynamic_cast<OghamNodeItem*>(gi))
                nodes.append(ni);
        if (nodes.size() < 2) return;

        switch (mode)
        {
        case 0: // Align Left
        {
            qreal ref = nodes[0]->pos().x();
            for (auto* n : nodes) ref = qMin(ref, n->pos().x());
            for (auto* n : nodes) n->setPos(ref, n->pos().y());
            break;
        }
        case 1: // Align Right
        {
            qreal ref = nodes[0]->pos().x() + OghamNodeItem::kNodeWidth;
            for (auto* n : nodes) ref = qMax(ref, n->pos().x() + OghamNodeItem::kNodeWidth);
            for (auto* n : nodes) n->setPos(ref - OghamNodeItem::kNodeWidth, n->pos().y());
            break;
        }
        case 2: // Center H
        {
            qreal sum = 0.0;
            for (auto* n : nodes) sum += n->pos().x() + OghamNodeItem::kNodeWidth * 0.5;
            const qreal avg = sum / nodes.size();
            for (auto* n : nodes)
                n->setPos(avg - OghamNodeItem::kNodeWidth * 0.5, n->pos().y());
            break;
        }
        case 3: // Align Top
        {
            qreal ref = nodes[0]->pos().y();
            for (auto* n : nodes) ref = qMin(ref, n->pos().y());
            for (auto* n : nodes) n->setPos(n->pos().x(), ref);
            break;
        }
        case 4: // Align Bottom
        {
            qreal ref = nodes[0]->pos().y() + nodes[0]->boundingRect().height();
            for (auto* n : nodes)
                ref = qMax(ref, n->pos().y() + n->boundingRect().height());
            for (auto* n : nodes)
                n->setPos(n->pos().x(), ref - n->boundingRect().height());
            break;
        }
        case 5: // Center V
        {
            qreal sum = 0.0;
            for (auto* n : nodes)
                sum += n->pos().y() + n->boundingRect().height() * 0.5;
            const qreal avg = sum / nodes.size();
            for (auto* n : nodes)
                n->setPos(n->pos().x(), avg - n->boundingRect().height() * 0.5);
            break;
        }
        case 6: // Distribute H (distribute left-edge X positions evenly)
        {
            if (nodes.size() < 3) break;
            std::sort(nodes.begin(), nodes.end(),
                [](OghamNodeItem* a, OghamNodeItem* b){ return a->pos().x() < b->pos().x(); });
            const qreal x0 = nodes.front()->pos().x();
            const qreal xN = nodes.back()->pos().x();
            const int   N  = nodes.size();
            for (int i = 1; i < N - 1; ++i)
                nodes[i]->setPos(x0 + i * (xN - x0) / (N - 1), nodes[i]->pos().y());
            break;
        }
        case 7: // Distribute V (distribute top-edge Y positions evenly)
        {
            if (nodes.size() < 3) break;
            std::sort(nodes.begin(), nodes.end(),
                [](OghamNodeItem* a, OghamNodeItem* b){ return a->pos().y() < b->pos().y(); });
            const qreal y0 = nodes.front()->pos().y();
            const qreal yN = nodes.back()->pos().y();
            const int   N  = nodes.size();
            for (int i = 1; i < N - 1; ++i)
                nodes[i]->setPos(nodes[i]->pos().x(), y0 + i * (yN - y0) / (N - 1));
            break;
        }
        default: break;
        }

        UpdateStatusBar();
    }

    // -------------------------------------------------------------------------
    // Tree search / filter
    // -------------------------------------------------------------------------

    static bool ApplyTreeFilter(QTreeWidgetItem* item, const QString& text)
    {
        bool anyChildMatch = false;
        for (int i = 0; i < item->childCount(); ++i)
            if (ApplyTreeFilter(item->child(i), text))
                anyChildMatch = true;

        const QString tag = item->data(0, kRoleNodeTag).toString();
        const int     ei  = item->data(0, kRoleEntryIdx).toInt();

        bool visible;
        if (text.isEmpty())
            visible = true;
        else if (ei < 0)
            visible = anyChildMatch;
        else
            visible = tag.contains(text, Qt::CaseInsensitive);

        item->setHidden(!visible && !anyChildMatch);
        return visible || anyChildMatch;
    }

    void OghamStoryteller::OnTreeSearch(const QString& text)
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            ApplyTreeFilter(m_tree->topLevelItem(i), text);
        if (!text.isEmpty())
            m_tree->expandAll();
    }

} // namespace FoundationOgham

#include "OghamStoryteller.moc"
#include <moc_OghamStoryteller.cpp>
