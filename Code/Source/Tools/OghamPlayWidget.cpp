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

#include "OghamPlayWidget.h"

#include <FoundationLocalisation/LexiconEditorRequestBus.h>

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace FoundationOgham
{
    OghamPlayWidget::OghamPlayWidget(QList<LoadedFile> files,
                                     int               startFileIdx,
                                     int               startEntryIdx,
                                     QWidget*          parent)
        : QWidget(parent, Qt::Tool | Qt::Window)
        , m_files(std::move(files))
        , m_startFi(startFileIdx)
        , m_startEi(startEntryIdx)
    {
        setWindowTitle("Play Mode");
        setMinimumSize(380, 480);
        resize(420, 560);
        setAttribute(Qt::WA_DeleteOnClose);

        loadLexiconCache();

        // ── Main layout ────────────────────────────────────────────────────
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 8, 8, 8);
        mainLayout->setSpacing(6);

        // ── Entry card ─────────────────────────────────────────────────────
        auto* card = new QFrame(this);
        card->setFrameShape(QFrame::StyledPanel);
        card->setFrameShadow(QFrame::Sunken);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 6, 8, 6);
        cardLayout->setSpacing(4);

        m_entryTagLabel = new QLabel(card);
        m_entryTagLabel->setStyleSheet("color: #888888; font-size: 11px;");
        cardLayout->addWidget(m_entryTagLabel);

        m_textLabel = new QLabel(card);
        m_textLabel->setWordWrap(true);
        m_textLabel->setStyleSheet("font-size: 13px;");
        m_textLabel->setMinimumHeight(60);
        m_textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        cardLayout->addWidget(m_textLabel);

        mainLayout->addWidget(card);

        // ── Options ────────────────────────────────────────────────────────
        auto* optHeader = new QLabel("Options", this);
        optHeader->setStyleSheet("color: #aaaaaa; font-size: 11px;");
        mainLayout->addWidget(optHeader);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        m_optsWidget = new QWidget();
        m_optsLayout = new QVBoxLayout(m_optsWidget);
        m_optsLayout->setContentsMargins(0, 0, 0, 0);
        m_optsLayout->setSpacing(4);
        m_optsLayout->addStretch();
        scroll->setWidget(m_optsWidget);
        mainLayout->addWidget(scroll, 1);

        // ── Tag state ──────────────────────────────────────────────────────
        auto* stateHeader = new QLabel("Tag State", this);
        stateHeader->setStyleSheet("color: #aaaaaa; font-size: 11px;");
        mainLayout->addWidget(stateHeader);

        m_tagStateLabel = new QLabel("(empty)", this);
        m_tagStateLabel->setStyleSheet(
            "font-family: monospace; font-size: 11px; color: #cccccc;");
        m_tagStateLabel->setWordWrap(true);
        mainLayout->addWidget(m_tagStateLabel);

        // ── Footer ─────────────────────────────────────────────────────────
        auto* footer = new QHBoxLayout();
        footer->setSpacing(6);

        m_backBtn = new QPushButton("\xe2\x97\x80 Back", this);
        m_backBtn->setEnabled(false);
        footer->addWidget(m_backBtn);

        footer->addStretch();

        m_restartBtn = new QPushButton("\xe2\x86\xba Restart", this);
        footer->addWidget(m_restartBtn);

        auto* closeBtn = new QPushButton("\xe2\x9c\x95 Close", this);
        footer->addWidget(closeBtn);

        mainLayout->addLayout(footer);

        // ── Connections ────────────────────────────────────────────────────
        connect(m_backBtn, &QPushButton::clicked, this, [this]()
        {
            if (m_history.isEmpty()) return;
            HistoryEntry prev = m_history.takeLast();
            m_tagState = std::move(prev.tagState);
            showEntry(prev.fi, prev.ei);
        });

        connect(m_restartBtn, &QPushButton::clicked, this, [this]()
        {
            m_history.clear();
            m_tagState.clear();
            showEntry(m_startFi, m_startEi);
        });

        connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

        // ── Start ──────────────────────────────────────────────────────────
        showEntry(m_startFi, m_startEi);
    }

    // ── Navigation ────────────────────────────────────────────────────────────

    void OghamPlayWidget::showEntry(int fileIdx, int entryIdx)
    {
        m_curFi = fileIdx;
        m_curEi = entryIdx;
        m_backBtn->setEnabled(!m_history.isEmpty());

        if (fileIdx < 0 || fileIdx >= m_files.size() ||
            entryIdx < 0 || entryIdx >= m_files[fileIdx].entries.size())
        {
            m_entryTagLabel->setText("(End of Conversation)");
            m_textLabel->clear();
            rebuildOptions(OghamSourceEntry{});
            return;
        }

        const OghamSourceEntry& entry = m_files[fileIdx].entries[entryIdx];

        // Apply entry operations before displaying
        applyOperations(entry.entryOperations);
        updateTagStateLabel();

        m_entryTagLabel->setText(entry.tag);

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

    void OghamPlayWidget::rebuildOptions(const OghamSourceEntry& entry)
    {
        // Remove all items except the trailing stretch
        while (m_optsLayout->count() > 1)
        {
            QLayoutItem* item = m_optsLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        if (entry.options.isEmpty())
        {
            auto* lbl = new QLabel(
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 End of Conversation \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80",
                m_optsWidget);
            lbl->setStyleSheet("color: #888888; font-style: italic;");
            lbl->setAlignment(Qt::AlignCenter);
            m_optsLayout->insertWidget(0, lbl);
            return;
        }

        const int insertBefore = m_optsLayout->count() - 1; // position of stretch

        for (int oi = 0; oi < entry.options.size(); ++oi)
        {
            const OghamSourceOption& opt = entry.options[oi];
            const bool condOk = evalConditions(opt.conditions, m_tagState);

            QString label = resolveText(opt.textKey);
            if (label.isEmpty()) label = opt.textKey;
            if (label.isEmpty()) label = opt.tag;
            if (label.isEmpty()) label = QStringLiteral("Option %1").arg(oi + 1);

            auto* btn = new QPushButton(label, m_optsWidget);
            btn->setEnabled(condOk);
            if (!condOk)
                btn->setStyleSheet("color: #555555; font-style: italic;");

            const int cFi = m_curFi, cEi = m_curEi;
            connect(btn, &QPushButton::clicked, this, [this, cFi, cEi, oi]()
            {
                if (cFi < 0 || cFi >= m_files.size()) return;
                if (cEi < 0 || cEi >= m_files[cFi].entries.size()) return;
                const OghamSourceEntry& cur = m_files[cFi].entries[cEi];
                if (oi >= cur.options.size()) return;
                const OghamSourceOption& opt = cur.options[oi];

                // Push history before mutating tag state
                m_history.append({ cFi, cEi, m_tagState });

                applyOperations(opt.operations);

                if (opt.targetTag.isEmpty())
                {
                    m_curFi = -1;
                    m_curEi = -1;
                    m_backBtn->setEnabled(!m_history.isEmpty());
                    m_entryTagLabel->setText("(End of Conversation)");
                    m_textLabel->clear();
                    rebuildOptions(OghamSourceEntry{});
                    updateTagStateLabel();
                    return;
                }

                int dstFi = -1, dstEi = -1;
                if (!findEntry(opt.targetTag, &dstFi, &dstEi))
                {
                    m_entryTagLabel->setText(
                        QStringLiteral("(Entry not found: %1)").arg(opt.targetTag));
                    m_textLabel->clear();
                    rebuildOptions(OghamSourceEntry{});
                    m_backBtn->setEnabled(!m_history.isEmpty());
                    updateTagStateLabel();
                    return;
                }

                showEntry(dstFi, dstEi);
            });

            m_optsLayout->insertWidget(insertBefore + oi, btn);
        }
    }

    // ── Logic ─────────────────────────────────────────────────────────────────

    void OghamPlayWidget::applyOperations(const QList<OghamOperation>& ops)
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

    bool OghamPlayWidget::evalConditions(const QList<OghamCondition>& conds,
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
            const bool next = evalOne(conds[i]);
            const QString& op = conds[i - 1].logicOp;
            if      (op == "And") result = result && next;
            else if (op == "Or")  result = result || next;
            else if (op == "Xor") result = (result != next);
        }
        return result;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    bool OghamPlayWidget::findEntry(const QString& tag, int* outFi, int* outEi) const
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

    QString OghamPlayWidget::resolveText(const QString& key) const
    {
        if (key.isEmpty()) return {};
        return m_lexiconCache.value(key);
    }

    void OghamPlayWidget::updateTagStateLabel()
    {
        if (m_tagState.isEmpty()) { m_tagStateLabel->setText("(empty)"); return; }
        QStringList parts;
        QStringList keys = m_tagState.keys();
        keys.sort();
        for (const QString& k : keys)
            parts.append(QStringLiteral("%1 = %2").arg(k).arg(m_tagState[k]));
        m_tagStateLabel->setText(parts.join("   "));
    }

    void OghamPlayWidget::loadLexiconCache()
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

} // namespace FoundationOgham
