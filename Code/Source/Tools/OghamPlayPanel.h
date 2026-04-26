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
class QListWidget;
class QPushButton;
class QTextEdit;
class QVBoxLayout;

namespace FoundationOgham
{
    // =========================================================================
    // OghamPlayPanel
    //
    // Full-window play-test panel that replaces OghamStoryteller's content area.
    // Left: VN-style dialogue text + option buttons (✓/✗ by state).
    // Right: state monitor + clickable history log.
    // Top bar: ■ Stop button.
    //
    // Emits stopRequested() when the user presses Stop; the host should then
    // remove this widget and restore the editor layout.
    // =========================================================================
    class OghamPlayPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit OghamPlayPanel(QList<LoadedFile>  files,
                                int                startFileIdx,
                                int                startEntryIdx,
                                QHash<QString,int> initialState,
                                QWidget*           parent = nullptr);

    signals:
        void stopRequested();

    private:
        // ── Navigation ───────────────────────────────────────────────────────
        void showEntry(int fileIdx, int entryIdx);
        void rebuildOptions(const OghamSourceEntry& entry);
        void replayFrom(int historyIndex);

        // ── Logic ─────────────────────────────────────────────────────────────
        void applyOperations(const QList<OghamOperation>& ops);

        static bool evalConditions(const QList<OghamCondition>& conds,
                                   const QHash<QString, int>&   state);

        // ── Helpers ───────────────────────────────────────────────────────────
        bool    findEntry(const QString& tag, int* outFi, int* outEi) const;
        QString resolveText(const QString& key) const;
        void    updateStateMonitor();
        void    loadLexiconCache();
        void    pushHistory(int fileIdx, int entryIdx);
        void    syncHistoryList();

        // ── Data ──────────────────────────────────────────────────────────────
        QList<LoadedFile>        m_files;
        const int                m_startFi;
        const int                m_startEi;
        int                      m_curFi    = -1;
        int                      m_curEi    = -1;
        QHash<QString, int>      m_tagState;
        QHash<QString, QString>  m_lexiconCache;

        struct HistoryEntry
        {
            int                 fi;
            int                 ei;
            QHash<QString, int> tagState; ///< state on entry (before entry ops)
            QString             label;    ///< entry tag for display
        };
        QVector<HistoryEntry>    m_history;

        // ── UI ────────────────────────────────────────────────────────────────
        QLabel*      m_headerLabel    = nullptr;
        QLabel*      m_entryTagLabel  = nullptr;
        QLabel*      m_textLabel      = nullptr;
        QWidget*     m_optsWidget     = nullptr;
        QVBoxLayout* m_optsLayout     = nullptr;
        QTextEdit*   m_stateMonitor   = nullptr;
        QListWidget* m_historyList    = nullptr;
    };

} // namespace FoundationOgham
