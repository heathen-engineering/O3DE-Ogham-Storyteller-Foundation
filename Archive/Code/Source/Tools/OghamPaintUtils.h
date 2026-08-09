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
#include <QColor>
#include <QtMath>
#endif

namespace FoundationOgham
{
    // W3C relative luminance → black or white text for maximum contrast.
    // Safe to include in unity builds: inline prevents multiple-definition errors.
    inline QColor contrastTextColor(const QColor& bg)
    {
        auto ch = [](int v) -> qreal {
            const qreal s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : qPow((s + 0.055) / 1.055, 2.4);
        };
        const qreal L = 0.2126 * ch(bg.red())
                      + 0.7152 * ch(bg.green())
                      + 0.0722 * ch(bg.blue());
        return L > 0.4 ? QColor(Qt::black) : QColor(Qt::white);
    }

} // namespace FoundationOgham
