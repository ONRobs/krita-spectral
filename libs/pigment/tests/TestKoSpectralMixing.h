/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TESTKOSPECTRALMIXING_H
#define TESTKOSPECTRALMIXING_H

#include <QObject>

class TestKoSpectralMixing : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testBlueAndYellowMakeGreen();
    void testMatchesSpectralJs();
    void testEndpointsAreExact();
    void testMixingWithItselfIsIdentity();
    void testResultsStayInRange();
    void testBlackAndWhite();
    void testMonotonic();
    void testRepeatedMixingDoesNotDrift();

    void testTransferRoundTrip_data();
    void testTransferRoundTrip();
    void testTransferMatchesLcms_data();
    void testTransferMatchesLcms();
};

#endif // TESTKOSPECTRALMIXING_H
