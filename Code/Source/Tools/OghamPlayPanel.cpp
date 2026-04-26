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

#include "OghamPlayPanel.h"

#include <FoundationLocalisation/LexiconEditorRequestBus.h>

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

namespace FoundationOgham
{
    OghamPlayPanel::OghamPlayPanel(QList<LoadedFile>  files,
                                   int                startFileIdx,
                                   int                startEntryIdx,
                                   QHash<QString,int> initialState,
                                   QWidget*           parent)
        : QWidget(parent)
        , m_files(std::move(files))
        , m_startFi(startFileIdx)
        , m_startEi(startEntryIdx)
        , m_tagState(std::move(initialState))
    {
        loadLexiconCache();

        // ── Top bar: stop button ───────────────────────────────────────────
        auto* topBar = new QWidget(this);
        topBar->setStyleSheet("background-color: #1e1e2e;");
        auto* topLayout = new QHBoxLayout(topBar);
        topLayout->setContentsMargins(6, 4, 6, 4);
        topLayout->setSpacing(8);

        auto* stopBtn = new QPushButton("\xe2\x96\xa0 Stop", topBar);
        stopBtn->setStyleSheet(
            "QPushButton { background-color: #6b2020; color: white; "
            "font-weight: bold; padding: 4px 12px; border-radius: 3px; }"
            "QPushButton:hover { background-color: #8b3030; }");
        topLayout->addWidget(stopBtn);

        m_headerLabel = new QLabel(topBar);
        m_headerLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");
        topLayout->addWidget(m_headerLabel, 1);

        // ── Content splitter ──────────────────────────────────────────────
        auto* splitter = new QSplitter(Qt::Horizontal, this);

        // ── Left panel: dialogue ──────────────────────────────────────────
        auto* leftPanel  = new QWidget(splitter);
        auto* leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(12, 12, 12, 12);
        leftLayout->setSpacing(8);

        m_entryTagLabel = new QLabel(leftPanel);
        m_entryTagLabel->setStyleSheet("color: #888888; font-size: 11px;");
        leftLayout->addWidget(m_entryTagLabel);

        m_textLabel = new QLabel(leftPanel);
        m_textLabel->setWordWrap(true);
        m_textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_textLabel->setStyleSheet("font-size: 16px; padding: 8px 0px;");
        m_textLabel->setMinimumHeight(100);
        leftLayout->addWidget(m_textLabel);

        auto* divider = new QFrame(leftPanel);
        divider->setFrameShape(QFrame::HLine);
        divider->setFrameShadow(QFrame::Sunken);
        leftLayout->addWidget(divider);

        auto* scroll = new QScrollArea(leftPanel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        m_optsWidget = new QWidget();
        m_optsLayout = new QVBoxLayout(m_optsWidget);
        m_optsLayout->setContentsMargins(0, 0, 0, 0);
        m_optsLayout->setSpacing(6);
        m_optsLayout->addStretch();
        scroll->setWidget(m_optsWidget);
        leftLayout->addWidget(scroll, 1);

        // ── Right panel: state + history ──────────────────────────────────
        auto* rightPanel  = new QWidget(splitter);
        auto* rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(8, 12, 12, 12);
        rightLayout->setSpacing(6);

        auto* stateHeader = new QLabel("State Monitor", rightPanel);
        stateHeader->setStyleSheet(
            "color: #aaaaaa; font-size: 11px; font-weight: bold; "
            "border-bottom: 1px solid #444; padding-bottom: 2px;");
        rightLayout->addWidget(stateHeader);

        m_stateMonitor = new QTextEdit(rightPanel);
        m_stateMonitor->setReadOnly(true);
        m_stateMonitor->setStyleSheet(
            "font-family: monospace; font-size: 11px; color: #cccccc; "
            "background-color: #1a1a2a; border: none;");
        m_stateMonitor->setFixedHeight(160);
        rightLayout->addWidget(m_stateMonitor);

        auto* histHeader = new QLabel("History", rightPanel);
        histHeader->setStyleSheet(
            "color: #aaaaaa; font-size: 11px; font-weight: bold; "
            "border-bottom: 1px solid #444; padding-bottom: 2px;");
        rightLayout->addWidget(histHeader);

        m_historyList = new QListWidget(rightPanel);
        m_historyList->setStyleSheet(
            "font-size: 11px; color: #bbbbbb; background-color: #1a1a2a; "
            "border: none;");
        m_historyList->setSelectionMode(QAbstractItemView::SingleSelection);
        rightLayout->addWidget(m_historyList, 1);

        // ── Splitter proportions ──────────────────────────────────────────
        splitter->addWidget(leftPanel);
        splitter->addWidget(rightPanel);
        splitter->setStretchFactor(0, 6);
        splitter->setStretchFactor(1, 3);

        // ── Main layout ───────────────────────────────────────────────────
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        mainLayout->addWidget(topBar);
        mainLayout->addWidget(splitter, 1);

        // ── Connections ───────────────────────────────────────────────────
        connect(stopBtn, &QPushButton::clicked, this, &OghamPlayPanel::stopRequested);

        connect(m_historyList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item)
            {
                if (!item) return;
                const int idx = m_historyList->row(item);
                if (idx >= 0 && idx < m_history.size())
                    replayFrom(idx);
            });

        // ── Start ─────────────────────────────────────────────────────────
        updateStateMonitor();
        showEntry(m_startFi, m_startEi);
    }

    // ── Navigation ────────────────────────────────────────────────────────────

    void OghamPlayPanel::showEntry(int fileIdx, int entryIdx)
    {
        m_curFi = fileIdx;
        m_curEi = entryIdx;

        if (fileIdx < 0 || fileIdx >= m_files.size() ||
            entryIdx < 0 || entryIdx >= m_files[fileIdx].entries.size())
        {
            m_headerLabel->setText("Conversation Closed");
            m_entryTagLabel->setText({});
            m_textLabel->setText(
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "  Conversation Closed  "
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
            m_textLabel->setStyleSheet(
                "font-size: 14px; color: #888888; font-style: italic;");
            rebuildOptions(OghamSourceEntry{});
            return;
        }

        const OghamSourceEntry& entry = m_files[fileIdx].entries[entryIdx];

        applyOperations(entry.entryOperations);
        updateStateMonitor();

        m_headerLabel->setText(
            QStringLiteral("Playing: %1").arg(entry.tag));
        m_entryTagLabel->setText(entry.tag);
        m_textLabel->setStyleSheet("font-size: 16px; padding: 8px 0px;");

        QStringList textLines;
        for (const QString& key : entry.dataKeys)
        {
            const QString resolved = resolveText(key);
            textLines.append(resolved.isEmpty()
                ? QStringLiteral("[%1]").arg(key)
                : resolved);
        }
        m_textLabel->setText(textLines.isEmpty()
            ? QStringLiteral("(no content)")
            : textLines.join("\n\n"));

        rebuildOptions(entry);
    }

    void OghamPlayPanel::rebuildOptions(const OghamSourceEntry& entry)
    {
        while (m_optsLayout->count() > 1)
        {
            QLayoutItem* item = m_optsLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        if (entry.options.isEmpty())
        {
            auto* lbl = new QLabel(
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "  Conversation Closed  "
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80",
                m_optsWidget);
            lbl->setStyleSheet("color: #888888; font-style: italic;");
            lbl->setAlignment(Qt::AlignCenter);
            m_optsLayout->insertWidget(0, lbl);
            return;
        }

        const int insertBefore = m_optsLayout->count() - 1;

        for (int oi = 0; oi < entry.options.size(); ++oi)
        {
            const OghamSourceOption& opt = entry.options[oi];
            const bool condOk = evalConditions(opt.conditions, m_tagState);

            QString label = resolveText(opt.textKey);
            if (label.isEmpty()) label = opt.textKey;
            if (label.isEmpty()) label = opt.tag;
            if (label.isEmpty()) label = QStringLiteral("Option %1").arg(oi + 1);

            // Use QChar::fromUtf32 for reliable cross-platform rendering;
            // prefer emoji fonts that carry glyphs for U+2713/U+2717.
            static const QString kCheck = QString(QChar(0x2713)) + ' ';
            static const QString kCross = QString(QChar(0x2717)) + ' ';
            const QString prefix = condOk ? kCheck : kCross;

            auto* btn = new QPushButton(prefix + label, m_optsWidget);
            {
                QFont f = btn->font();
                f.setFamilies({ "Segoe UI Emoji", "Noto Color Emoji", f.family() });
                btn->setFont(f);
            }
            btn->setEnabled(condOk);
            btn->setStyleSheet(condOk
                ? "QPushButton { color: #44cc44; text-align: left; padding: 6px 10px; }"
                  "QPushButton:hover { background-color: #1a3a1a; }"
                : "QPushButton { color: #cc4444; font-style: italic; "
                  "text-align: left; padding: 6px 10px; }");

            const int cFi = m_curFi, cEi = m_curEi;
            connect(btn, &QPushButton::clicked, this, [this, cFi, cEi, oi]()
            {
                if (cFi < 0 || cFi >= m_files.size()) return;
                if (cEi < 0 || cEi >= m_files[cFi].entries.size()) return;
                const OghamSourceEntry& cur = m_files[cFi].entries[cEi];
                if (oi >= cur.options.size()) return;
                const OghamSourceOption& opt = cur.options[oi];

                // Push current entry to history before navigating
                pushHistory(cFi, cEi);
                applyOperations(opt.operations);
                updateStateMonitor();

                if (opt.targetTag.isEmpty())
                {
                    m_curFi = -1;
                    m_curEi = -1;
                    m_headerLabel->setText("Conversation Closed");
                    m_entryTagLabel->setText({});
                    m_textLabel->setText(
                        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                        "  Conversation Closed  "
                        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
                    m_textLabel->setStyleSheet(
                        "font-size: 14px; color: #888888; font-style: italic;");
                    rebuildOptions(OghamSourceEntry{});
                    return;
                }

                int dstFi = -1, dstEi = -1;
                if (!findEntry(opt.targetTag, &dstFi, &dstEi))
                {
                    m_headerLabel->setText(
                        QStringLiteral("Entry not found: %1").arg(opt.targetTag));
                    m_textLabel->setText(
                        QStringLiteral("(Entry not found: %1)").arg(opt.targetTag));
                    rebuildOptions(OghamSourceEntry{});
                    return;
                }

                showEntry(dstFi, dstEi);
            });

            m_optsLayout->insertWidget(insertBefore + oi, btn);
        }
    }

    void OghamPlayPanel::replayFrom(int index)
    {
        if (index < 0 || index >= m_history.size()) return;

        HistoryEntry target = m_history[index];

        // Truncate history to everything before this point
        m_history.resize(index);
        syncHistoryList();

        // Restore tag state as it was when entering that entry
        m_tagState = std::move(target.tagState);

        showEntry(target.fi, target.ei);
    }

    // ── Logic ─────────────────────────────────────────────────────────────────

    void OghamPlayPanel::applyOperations(const QList<OghamOperation>& ops)
    {
        for (const OghamOperation& op : ops)
        {
            if (!evalConditions(op.conditions, m_tagState)) continue;

            int val = m_tagState.value(op.tag, 0);
            if      (op.arithmetic == "Set") val  = op.value;
            else if (op.arithmetic == "Add") val += op.value;
            else if (op.arithmetic == "Sub") val -= op.value;
            else if (op.arithmetic == "Mul") val *= op.value;
            else if (op.arithmetic == "Div") val  = (op.value != 0) ? val / op.value : val;
            else if (op.arithmetic == "Min") val  = qMin(val, op.value);
            else if (op.arithmetic == "Max") val  = qMax(val, op.value);
            m_tagState[op.tag] = val;
        }
    }

    bool OghamPlayPanel::evalConditions(const QList<OghamCondition>& conds,
                                        const QHash<QString, int>&   state)
    {
        if (conds.isEmpty()) return true;

        auto evalOne = [&](const OghamCondition& c) -> bool
        {
            const bool exists = state.contains(c.tag);
            const int  val    = state.value(c.tag, 0);
            if (c.comparison == "Exists")       return exists;
            if (c.comparison == "NotExists")    return !exists;
            if (c.comparison == "Equal")        return val == c.value;
            if (c.comparison == "NotEqual")     return val != c.value;
            if (c.comparison == "Less")         return val <  c.value;
            if (c.comparison == "LessEqual")    return val <= c.value;
            if (c.comparison == "Greater")      return val >  c.value;
            if (c.comparison == "GreaterEqual") return val >= c.value;
            return true;
        };

        bool result = evalOne(conds.first());
        for (int i = 1; i < conds.size(); ++i)
        {
            const bool   next = evalOne(conds[i]);
            const QString& op = conds[i - 1].logicOp;
            if      (op == "And") result = result && next;
            else if (op == "Or")  result = result || next;
            else if (op == "Xor") result = (result != next);
        }
        return result;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    bool OghamPlayPanel::findEntry(const QString& tag, int* outFi, int* outEi) const
    {
        for (int fi = 0; fi < m_files.size(); ++fi)
            for (int ei = 0; ei < m_files[fi].entries.size(); ++ei)
                if (m_files[fi].entries[ei].tag == tag)
                {
                    *outFi = fi;
                    *outEi = ei;
                    return true;
                }
        return false;
    }

    QString OghamPlayPanel::resolveText(const QString& key) const
    {
        if (key.isEmpty()) return {};
        return m_lexiconCache.value(key);
    }

    void OghamPlayPanel::updateStateMonitor()
    {
        if (m_tagState.isEmpty())
        {
            m_stateMonitor->setPlainText("(no tags set)");
            return;
        }
        QStringList lines;
        QStringList keys = m_tagState.keys();
        keys.sort();
        for (const QString& k : keys)
            lines.append(QStringLiteral("%1  =  %2").arg(k, -30).arg(m_tagState[k]));
        m_stateMonitor->setPlainText(lines.join("\n"));
    }

    void OghamPlayPanel::loadLexiconCache()
    {
        AZStd::vector<AZStd::string> azPaths;
        FoundationLocalisation::LexiconEditorRequestBus::BroadcastResult(
            azPaths, &FoundationLocalisation::LexiconEditorRequests::GetKnownFilePaths);

        for (const auto& azPath : azPaths)
        {
            QFile f(QString::fromUtf8(azPath.c_str()));
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error != QJsonParseError::NoError) continue;

            const QJsonObject entries = doc.object().value("entries").toObject();
            for (auto it = entries.begin(); it != entries.end(); ++it)
                if (it.value().isString())
                    m_lexiconCache.insert(it.key(), it.value().toString());
        }
    }

    void OghamPlayPanel::pushHistory(int fileIdx, int entryIdx)
    {
        if (fileIdx < 0 || fileIdx >= m_files.size()) return;
        if (entryIdx < 0 || entryIdx >= m_files[fileIdx].entries.size()) return;

        const QString label = m_files[fileIdx].entries[entryIdx].tag;
        m_history.append({ fileIdx, entryIdx, m_tagState, label });
        syncHistoryList();
    }

    void OghamPlayPanel::syncHistoryList()
    {
        m_historyList->clear();
        for (const HistoryEntry& he : m_history)
        {
            auto* item = new QListWidgetItem(
                QStringLiteral("\xe2\x96\xb8 %1").arg(he.label),  // ▸
                m_historyList);
            item->setToolTip("Click to replay from this point");
        }
        if (!m_history.isEmpty())
            m_historyList->scrollToBottom();
    }

} // namespace FoundationOgham
