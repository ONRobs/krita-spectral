/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCOLORSMUDGESPECTRALTEST_H
#define KISCOLORSMUDGESPECTRALTEST_H

#include <QTest>

class KisColorSmudgeSpectralTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMixesSpectrally_data();
    void testMixesSpectrally();
    void testNoDriftOnFlatColor_data();
    void testNoDriftOnFlatColor();
    void testNeutralsStayNeutral_data();
    void testNeutralsStayNeutral();
    void testAlphaLockedLayer_data();
    void testAlphaLockedLayer();
    void testNonRgbFallsBack();
};

#endif // KISCOLORSMUDGESPECTRALTEST_H
