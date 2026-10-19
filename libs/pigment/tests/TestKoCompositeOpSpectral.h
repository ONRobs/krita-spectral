/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TESTKOCOMPOSITEOPSPECTRAL_H
#define TESTKOCOMPOSITEOPSPECTRAL_H

#include <QObject>

class TestKoCompositeOpSpectral : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAvailableOnlyInRgb();
    void testMissingOpFallsBack();

    void testOver_data();
    void testOver();
    void testOverOpacityAndMask_data();
    void testOverOpacityAndMask();
    void testOverOnTransparent_data();
    void testOverOnTransparent();
    void testAlphaLocked_data();
    void testAlphaLocked();
    void testChannelFlags_data();
    void testChannelFlags();
    void testSameColorIsUnchanged_data();
    void testSameColorIsUnchanged();
    void testConstantSource_data();
    void testConstantSource();
    void testCopy_data();
    void testCopy();
    void testHdrValuesFallBack();
};

#endif // TESTKOCOMPOSITEOPSPECTRAL_H
