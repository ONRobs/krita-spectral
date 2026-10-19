/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSpectralCompositeOpBenchmark.h"

#include <simpletest.h>
#include <testpigment.h>

#include <QRandomGenerator>

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOp.h>
#include <KoCompositeOpRegistry.h>

Q_DECLARE_METATYPE(const KoColorSpace *)

namespace
{

// a 300 px brush dab
static const int SIZE = 300;

enum Content {
    Flat,      ///< a single color, e.g. a flat area of the canvas
    Smooth,    ///< a gradient, e.g. a smudged area
    Noise,     ///< every pixel different, the worst case
};

QByteArray createPixels(const KoColorSpace *cs, Content content, quint32 seed)
{
    QByteArray result(int(cs->pixelSize()) * SIZE * SIZE, '\0');
    QRandomGenerator random(seed);

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            QColor color;
            switch (content) {
            case Flat:
                color = QColor(seed % 2 ? 252 : 0, seed % 2 ? 211 : 33, seed % 2 ? 0 : 133);
                break;
            case Smooth:
                color = QColor(x * 255 / SIZE, y * 255 / SIZE, (seed * 37) % 256);
                break;
            case Noise:
                color = QColor(random.bounded(256), random.bounded(256), random.bounded(256));
                break;
            }
            KoColor c(color, cs);
            memcpy(result.data() + (y * SIZE + x) * cs->pixelSize(), c.data(), cs->pixelSize());
        }
    }

    return result;
}

}

void KoSpectralCompositeOpBenchmark::benchmark_data()
{
    QTest::addColumn<const KoColorSpace *>("cs");
    QTest::addColumn<QString>("opId");
    QTest::addColumn<int>("content");
    QTest::addColumn<bool>("constantSource");

    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const QList<QPair<QString, const KoColorSpace *>> spaces = {
        {"u8", registry->rgb8()},
        {"u16", registry->rgb16()},
        {"f32", registry->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), QString())},
    };

    for (const auto &space : spaces) {
        for (const QString &op : {COMPOSITE_OVER, COMPOSITE_SPECTRAL}) {
            QTest::addRow("%s_%s_constant_src", qPrintable(space.first), qPrintable(op)) << space.second << op << int(Smooth) << true;
            QTest::addRow("%s_%s_flat", qPrintable(space.first), qPrintable(op)) << space.second << op << int(Flat) << false;
            QTest::addRow("%s_%s_smooth", qPrintable(space.first), qPrintable(op)) << space.second << op << int(Smooth) << false;
            QTest::addRow("%s_%s_noise", qPrintable(space.first), qPrintable(op)) << space.second << op << int(Noise) << false;
        }
    }
}

void KoSpectralCompositeOpBenchmark::benchmark()
{
    QFETCH(const KoColorSpace *, cs);
    QFETCH(QString, opId);
    QFETCH(int, content);
    QFETCH(bool, constantSource);

    const KoCompositeOp *op = cs->compositeOp(opId);
    QCOMPARE(op->id(), opId);

    const QByteArray src = createPixels(cs, Content(content), 1);
    const QByteArray originalDst = createPixels(cs, Content(content), 2);

    QByteArray mask(SIZE * SIZE, '\0');
    for (int i = 0; i < mask.size(); i++) {
        mask[i] = char(64 + i % 128);
    }

    QByteArray dst = originalDst;

    QBENCHMARK {
        dst = originalDst;
        op->composite(reinterpret_cast<quint8 *>(dst.data()), SIZE * cs->pixelSize(),
                      reinterpret_cast<const quint8 *>(src.constData()), constantSource ? 0 : SIZE * cs->pixelSize(),
                      reinterpret_cast<const quint8 *>(mask.constData()), SIZE,
                      SIZE, SIZE, 0.5f);
    }
}

KISTEST_MAIN(KoSpectralCompositeOpBenchmark)
