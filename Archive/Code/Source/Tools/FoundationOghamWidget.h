
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QWidget>
#endif

namespace FoundationOgham
{
    class FoundationOghamWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        explicit FoundationOghamWidget(QWidget* parent = nullptr);
    };
} 
