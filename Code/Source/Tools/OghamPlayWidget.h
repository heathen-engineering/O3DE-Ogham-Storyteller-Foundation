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
#include "OghamStoryteller.h"
#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>
#endif

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace FoundationOgham
{
    // =========================================================================
    // OghamPlayWidget
    //
    // Floating play-test window launched from OghamStoryteller.
    // Navigates the dialogue tree from a starting entry, evaluating conditions
    // and applying tag operations, with full back-navigation history.
    //
    // Receives a value-copy of the loaded files so edits made after launch
    // do not affect the in-progress playthrough.
    //
    // Usage:
    //   auto* w = new OghamPlayWidget(files, startFi, startEi, parent);
    //   w->show();   // WA_DeleteOnClose is set in constructor
    // =========================================================================
    class OghamPlayWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit OghamPlayWidget(QList<LoadedFile> files,
                                 int               startFileIdx,
                                 int               startEntryIdx,
                                 QWidget*          parent = nullptr);

    private:
        // ── Navigation ───────────────────────────────────────────────────────
        void showEntry(int fileIdx, int entryIdx);
        void rebuildOptions(const OghamSourceEntry& entry);

        // ── Logic ─────────────────────────────────────────────────────────────
        void applyOperations(const QList<OghamOperation>& ops);

        static bool evalConditions(const QList<OghamCondition>& conds,
                                   const QHash<QString, int>&   state);

        // ── Helpers ───────────────────────────────────────────────────────────
        bool    findEntry(const QString& tag, int* outFi, int* outEi) const;
        QString resolveText(const QString& key) const;
        void    updateTagStateLabel();
        void    loadLexiconCache();

        // ── Data ──────────────────────────────────────────────────────────────
        QList<LoadedFile>        m_files;
        const int                m_startFi;
        const int                m_startEi;
        int                      m_curFi    = -1;
        int                      m_curEi    = -1;
        QHash<QString, int>      m_tagState;
        QHash<QString, QString>  m_lexiconCache;

        struct HistoryEntry { int fi; int ei; QHash<QString, int> tagState; };
        QVector<HistoryEntry>    m_history;

        // ── UI ────────────────────────────────────────────────────────────────
        QLabel*      m_entryTagLabel = nullptr;
        QLabel*      m_textLabel     = nullptr;
        QWidget*     m_optsWidget    = nullptr;
        QVBoxLayout* m_optsLayout    = nullptr;
        QLabel*      m_tagStateLabel = nullptr;
        QPushButton* m_backBtn       = nullptr;
        QPushButton* m_restartBtn    = nullptr;
    };

} // namespace FoundationOgham
