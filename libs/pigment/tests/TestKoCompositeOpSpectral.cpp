/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoCompositeOpSpectral.h"

#include <simpletest.h>
#include <testpigment.h>

#include <QBitArray>
#include <QColor>
#include <QVector>

#include <cmath>

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOp.h>
#include <KoCompositeOpRegistry.h>

Q_DECLARE_METATYPE(const KoColorSpace *)

namespace
{

/// a row of pixels in a given color space
struct Pixels {
    Pixels(const KoColorSpace *cs, const QVector<QColor> &colors)
        : cs(cs)
        , data(int(cs->pixelSize()) * colors.size(), '\0')
    {
        for (int i = 0; i < colors.size(); i++) {
            KoColor color(colors[i], cs);
            memcpy(data.data() + i * cs->pixelSize(), color.data(), cs->pixelSize());
        }
    }

    QColor at(int i) const
    {
        QColor result;
        cs->toQColor(reinterpret_cast<const quint8 *>(data.constData()) + i * cs->pixelSize(), &result);
        return result;
    }

    int count() const
    {
        return data.size() / cs->pixelSize();
    }

    quint8 *bytes()
    {
        return reinterpret_cast<quint8 *>(data.data());
    }

    const KoColorSpace *cs;
    QByteArray data;
};

void composite(const QString &opId, Pixels &dst, Pixels &src, float opacity,
               const QBitArray &channelFlags = QBitArray(), const QVector<quint8> &mask = QVector<quint8>(),
               bool constantSource = false)
{
    const KoCompositeOp *op = dst.cs->compositeOp(opId);
    QVERIFY(op);
    QCOMPARE(op->id(), opId);

    op->composite(dst.bytes(), dst.data.size(),
                  src.bytes(), constantSource ? 0 : src.data.size(),
                  mask.isEmpty() ? nullptr : mask.constData(), mask.size(),
                  1, dst.count(), opacity, channelFlags);
}

bool fuzzyCompare(const QColor &a, const QColor &b, int tolerance = 1)
{
    return std::abs(a.red() - b.red()) <= tolerance
        && std::abs(a.green() - b.green()) <= tolerance
        && std::abs(a.blue() - b.blue()) <= tolerance
        && std::abs(a.alpha() - b.alpha()) <= tolerance;
}

#define COMPARE_COLOR(actual, expected, tolerance) \
    QVERIFY2(fuzzyCompare(actual, expected, tolerance), \
             qPrintable(QString("got %1 alpha %2, expected %3 alpha %4") \
                            .arg(actual.name()).arg(actual.alpha()).arg(expected.name()).arg(expected.alpha())))

const QColor BLUE(0, 33, 133);
const QColor YELLOW(252, 211, 0);

void addRgbColorSpaces()
{
    QTest::addColumn<const KoColorSpace *>("cs");

    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    QTest::newRow("U8") << registry->rgb8();
    QTest::newRow("U16") << registry->rgb16();
    QTest::newRow("F16") << registry->colorSpace(RGBAColorModelID.id(), Float16BitsColorDepthID.id(), QString());
    QTest::newRow("F32") << registry->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), QString());
}

/// float color spaces use a linear profile by default, so allow more rounding there
int toleranceFor(const KoColorSpace *cs)
{
    return cs->channels()[0]->channelValueType() == KoChannelInfo::UINT8 ? 1 : 2;
}

}

void TestKoCompositeOpSpectral::testAvailableOnlyInRgb()
{
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();

    QList<const KoColorSpace *> rgbSpaces = {
        registry->rgb8(),
        registry->rgb16(),
        registry->colorSpace(RGBAColorModelID.id(), Float16BitsColorDepthID.id(), QString()),
        registry->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), QString()),
    };

    Q_FOREACH (const KoColorSpace *cs, rgbSpaces) {
        QVERIFY(cs);
        QVERIFY(cs->hasCompositeOp(COMPOSITE_SPECTRAL));
        QVERIFY(cs->hasCompositeOp(COMPOSITE_COPY_SPECTRAL));
    }

    QList<const KoColorSpace *> otherSpaces = {
        registry->colorSpace(CMYKAColorModelID.id(), Integer8BitsColorDepthID.id(), QString()),
        registry->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), QString()),
        registry->lab16(),
        registry->alpha8(),
    };

    Q_FOREACH (const KoColorSpace *cs, otherSpaces) {
        QVERIFY(cs);
        QVERIFY(!cs->hasCompositeOp(COMPOSITE_SPECTRAL));
        QVERIFY(!cs->hasCompositeOp(COMPOSITE_COPY_SPECTRAL));
    }

    // the UI lists ops by category, filtered by the color space
    const KoCompositeOpRegistry &opRegistry = KoCompositeOpRegistry::instance();
    QVERIFY(opRegistry.getCompositeOps(registry->rgb8()).contains(KoID(COMPOSITE_SPECTRAL)));
    QVERIFY(!opRegistry.getCompositeOps(registry->lab16()).contains(KoID(COMPOSITE_SPECTRAL)));
    QVERIFY(!opRegistry.getCompositeOps(registry->rgb8()).contains(KoID(COMPOSITE_COPY_SPECTRAL)));
}

void TestKoCompositeOpSpectral::testMissingOpFallsBack()
{
    // e.g. a brush preset with the Spectral mode used on a CMYK layer
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(), Integer8BitsColorDepthID.id(), QString());
    const KoCompositeOp *op = cs->compositeOp(COMPOSITE_SPECTRAL);
    QVERIFY(op);
    QCOMPARE(op->id(), COMPOSITE_OVER);
}

void TestKoCompositeOpSpectral::testOver_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testOver()
{
    QFETCH(const KoColorSpace *, cs);

    Pixels dst(cs, {YELLOW, YELLOW, YELLOW});
    Pixels src(cs, {BLUE, BLUE, BLUE});

    composite(COMPOSITE_SPECTRAL, dst, src, 0.5f);

    const QColor mixed = dst.at(0);
    QCOMPARE(mixed.alpha(), 255);
    // spectral.js gives #3d943e for a 50:50 mix of these colors; Over uses
    // the plain opacity as the factor, so the result is close to it
    QVERIFY2(mixed.green() > mixed.red() + 40 && mixed.green() > mixed.blue() + 40, qPrintable(mixed.name()));
    COMPARE_COLOR(mixed, QColor(61, 148, 62), 2 + toleranceFor(cs));

    // full opacity replaces the color
    Pixels dst2(cs, {YELLOW});
    Pixels src2(cs, {BLUE});
    composite(COMPOSITE_SPECTRAL, dst2, src2, 1.0f);
    COMPARE_COLOR(dst2.at(0), BLUE, toleranceFor(cs));
}

void TestKoCompositeOpSpectral::testOverOpacityAndMask_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testOverOpacityAndMask()
{
    QFETCH(const KoColorSpace *, cs);

    Pixels dst(cs, {YELLOW, YELLOW, YELLOW});
    Pixels src(cs, {BLUE, BLUE, BLUE});

    composite(COMPOSITE_SPECTRAL, dst, src, 0.0f);
    for (int i = 0; i < 3; i++) {
        COMPARE_COLOR(dst.at(i), YELLOW, 0);
    }

    // mask works as a per-pixel opacity, like a selection
    composite(COMPOSITE_SPECTRAL, dst, src, 1.0f, QBitArray(), {0, 128, 255});
    COMPARE_COLOR(dst.at(0), YELLOW, 0);
    COMPARE_COLOR(dst.at(2), BLUE, toleranceFor(cs));

    Pixels expected(cs, {YELLOW});
    Pixels blue(cs, {BLUE});
    composite(COMPOSITE_SPECTRAL, expected, blue, 128.0f / 255.0f);
    COMPARE_COLOR(dst.at(1), expected.at(0), toleranceFor(cs));
}

void TestKoCompositeOpSpectral::testOverOnTransparent_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testOverOnTransparent()
{
    QFETCH(const KoColorSpace *, cs);

    QColor transparentBlack(0, 0, 0, 0);
    QColor halfYellow = YELLOW;
    halfYellow.setAlpha(128);

    Pixels dst(cs, {transparentBlack, halfYellow});
    Pixels src(cs, {BLUE, BLUE});

    composite(COMPOSITE_SPECTRAL, dst, src, 0.5f);

    // the color of a transparent pixel is undefined, so it must not be mixed in
    QColor expected = BLUE;
    expected.setAlpha(128);
    COMPARE_COLOR(dst.at(0), expected, toleranceFor(cs));

    // union of the shapes, mixed in proportion to the alphas
    // 0.5 + 0.5 * (1 - 0.5) == 0.75 -> 191.25 .. 192 depending on the rounding of the depth
    QVERIFY(std::abs(dst.at(1).alpha() - 192) <= 1);
    QVERIFY(dst.at(1).green() > dst.at(1).red());
}

void TestKoCompositeOpSpectral::testAlphaLocked_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testAlphaLocked()
{
    QFETCH(const KoColorSpace *, cs);

    QBitArray alphaLocked(cs->channelCount(), true);
    alphaLocked.clearBit(3);

    QColor transparentBlack(0, 0, 0, 0);
    QColor halfYellow = YELLOW;
    halfYellow.setAlpha(128);
    QColor transparentBlue = BLUE;
    transparentBlue.setAlpha(0);

    for (const QString &opId : {COMPOSITE_SPECTRAL, COMPOSITE_COPY_SPECTRAL}) {
        Pixels dst(cs, {transparentBlack, halfYellow, YELLOW, YELLOW});
        Pixels src(cs, {BLUE, BLUE, BLUE, transparentBlue});

        for (float opacity : {0.3f, 1.0f}) {
            composite(opId, dst, src, opacity, alphaLocked, {255, 200, 255, 255});
        }

        QCOMPARE(dst.at(0).alpha(), 0);
        QCOMPARE(dst.at(1).alpha(), 128);
        QCOMPARE(dst.at(2).alpha(), 255);
        QCOMPARE(dst.at(3).alpha(), 255);

        QVERIFY2(dst.at(1).green() > dst.at(1).red(), qPrintable(opId));
        COMPARE_COLOR(dst.at(2), BLUE, toleranceFor(cs));
        // transparent source doesn't change an alpha-locked pixel
        COMPARE_COLOR(dst.at(3), YELLOW, 0);
    }
}

void TestKoCompositeOpSpectral::testChannelFlags_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testChannelFlags()
{
    QFETCH(const KoColorSpace *, cs);

    // lock everything except red
    QBitArray flags(cs->channelCount(), false);
    const int redIndex = cs->channels()[0]->name() == "Red" ? 0 : 2;
    flags.setBit(redIndex);
    flags.setBit(3);

    Pixels dst(cs, {YELLOW});
    Pixels src(cs, {BLUE});
    composite(COMPOSITE_SPECTRAL, dst, src, 0.5f, flags);

    QVERIFY(dst.at(0).red() < YELLOW.red());
    COMPARE_COLOR(QColor(0, dst.at(0).green(), dst.at(0).blue()), QColor(0, YELLOW.green(), YELLOW.blue()), toleranceFor(cs) - 1);
}

void TestKoCompositeOpSpectral::testSameColorIsUnchanged_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testSameColorIsUnchanged()
{
    QFETCH(const KoColorSpace *, cs);

    const QColor color(87, 140, 23);
    Pixels dst(cs, {color});
    Pixels src(cs, {color});
    const QByteArray original = dst.data;

    for (int i = 0; i < 1000; i++) {
        composite(COMPOSITE_SPECTRAL, dst, src, 0.37f);
        composite(COMPOSITE_COPY_SPECTRAL, dst, src, 0.61f);
    }

    QCOMPARE(dst.data, original);
}

void TestKoCompositeOpSpectral::testConstantSource_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testConstantSource()
{
    QFETCH(const KoColorSpace *, cs);

    const QVector<QColor> canvas = {YELLOW, QColor(200, 30, 30), QColor(255, 255, 255), QColor(0, 0, 0)};

    Pixels dst1(cs, canvas);
    Pixels dst2(cs, canvas);
    Pixels src(cs, {BLUE, BLUE, BLUE, BLUE});
    Pixels singleSrc(cs, {BLUE});

    composite(COMPOSITE_SPECTRAL, dst1, src, 0.6f);
    composite(COMPOSITE_SPECTRAL, dst2, singleSrc, 0.6f, QBitArray(), QVector<quint8>(), true);

    QCOMPARE(dst1.data, dst2.data);
}

void TestKoCompositeOpSpectral::testCopy_data()
{
    addRgbColorSpaces();
}

void TestKoCompositeOpSpectral::testCopy()
{
    QFETCH(const KoColorSpace *, cs);

    QColor halfBlue = BLUE;
    halfBlue.setAlpha(128);

    Pixels dst(cs, {YELLOW, YELLOW, YELLOW});
    Pixels src(cs, {BLUE, halfBlue, BLUE});

    composite(COMPOSITE_COPY_SPECTRAL, dst, src, 1.0f, QBitArray(), {255, 255, 128});

    // full opacity copies color and alpha
    COMPARE_COLOR(dst.at(0), BLUE, toleranceFor(cs));
    COMPARE_COLOR(dst.at(1), halfBlue, toleranceFor(cs));

    // partial opacity: alpha is interpolated, color is mixed spectrally
    QCOMPARE(dst.at(2).alpha(), 255);
    QVERIFY2(dst.at(2).green() > dst.at(2).red() + 40, qPrintable(dst.at(2).name()));
}

void TestKoCompositeOpSpectral::testHdrValuesFallBack()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), QString());

    float dst[4] = {4.0f, 0.5f, 0.1f, 1.0f};
    float src[4] = {0.1f, 0.2f, 0.9f, 1.0f};

    cs->compositeOp(COMPOSITE_SPECTRAL)->composite(reinterpret_cast<quint8 *>(dst), 0,
                                                   reinterpret_cast<quint8 *>(src), 0,
                                                   nullptr, 0, 1, 1, 0.5f);

    // HDR values can't be represented as reflectances, they are mixed linearly
    for (int i = 0; i < 3; i++) {
        QVERIFY(std::isfinite(dst[i]));
    }
    QCOMPARE(dst[0], 2.05f);
    QCOMPARE(dst[3], 1.0f);
}

KISTEST_MAIN(TestKoCompositeOpSpectral)
