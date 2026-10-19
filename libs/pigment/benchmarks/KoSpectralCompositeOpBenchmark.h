/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSPECTRALCOMPOSITEOPBENCHMARK_H
#define KOSPECTRALCOMPOSITEOPBENCHMARK_H

#include <QObject>

class KoSpectralCompositeOpBenchmark : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void benchmark_data();
    void benchmark();
};

#endif // KOSPECTRALCOMPOSITEOPBENCHMARK_H
