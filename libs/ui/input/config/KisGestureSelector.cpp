/*
 * SPDX-FileCopyrightText: 2022 Sharaf Zaman <shzam@sdf.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisGestureSelector.h"

KisGestureSelector::KisGestureSelector(QWidget *parent)
    : KComboBox(parent)
{
    QStringList gestures;
    for (int i = 0; i <= KisShortcutConfiguration::FiveFingerTouch; i++) {
        gestures << KisShortcutConfiguration::gestureToText(static_cast<KisShortcutConfiguration::GestureAction>(i));
    }
    addItems(gestures);
}

void KisGestureSelector::setGesture(KisShortcutConfiguration::GestureAction gestureAction)
{
    setCurrentIndex(gestureAction);
}

KisShortcutConfiguration::GestureAction KisGestureSelector::gesture()
{
    return static_cast<KisShortcutConfiguration::GestureAction>(currentIndex());
}
