/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoSpectralMixing.h"

#include <simpletest.h>
#include <testpigment.h>

#include <QRandomGenerator>

#include <cmath>

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <compositeops/KoSpectralColorTransfer.h>
#include <compositeops/KoSpectralMixing.h>

namespace
{

float srgbToLinear(float x)
{
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float x)
{
    x = qBound(0.0f, x, 1.0f);
    return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

void linearFromSrgb8(int r, int g, int b, float result[3])
{
    result[0] = srgbToLinear(r / 255.0f);
    result[1] = srgbToLinear(g / 255.0f);
    result[2] = srgbToLinear(b / 255.0f);
}

int toSrgb8(float linear)
{
    return qRound(linearToSrgb(linear) * 255.0f);
}

float distance(const float a[3], const float b[3])
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

void randomColor(QRandomGenerator &random, float result[3])
{
    for (int i = 0; i < 3; i++) {
        result[i] = float(random.generateDouble());
    }
}

}

void TestKoSpectralMixing::testBlueAndYellowMakeGreen()
{
    float blue[3];
    float yellow[3];
    float mixed[3];
    linearFromSrgb8(0, 33, 133, blue);
    linearFromSrgb8(252, 211, 0, yellow);

    KoSpectral::mix(blue, yellow, 0.5f, mixed);

    QVERIFY(mixed[1] > mixed[0] * 2.0f);
    QVERIFY(mixed[1] > mixed[2] * 2.0f);

    // a plain average in sRGB would be a gray
    float linearAverage[3];
    for (int i = 0; i < 3; i++) {
        linearAverage[i] = 0.5f * (blue[i] + yellow[i]);
    }
    QVERIFY(linearAverage[1] < linearAverage[0] * 1.5f);
}

void TestKoSpectralMixing::testMatchesSpectralJs()
{
    // reference values computed with spectral.js v3 (spectral.mix)
    struct Sample {
        int a[3];
        int b[3];
        float factor;
        int expected[3];
    };

    const Sample samples[] = {
        {{0, 33, 133}, {252, 211, 0}, 0.5f, {61, 148, 62}},
        {{255, 0, 0}, {0, 0, 255}, 0.5f, {74, 24, 43}},
        {{0, 0, 0}, {255, 255, 255}, 0.5f, {166, 166, 166}},
        {{255, 0, 0}, {255, 255, 0}, 0.5f, {255, 68, 15}},
    };

    for (const Sample &s : samples) {
        float a[3];
        float b[3];
        float mixed[3];
        linearFromSrgb8(s.a[0], s.a[1], s.a[2], a);
        linearFromSrgb8(s.b[0], s.b[1], s.b[2], b);

        KoSpectral::mix(a, b, s.factor, mixed);

        for (int i = 0; i < 3; i++) {
            QVERIFY2(std::abs(toSrgb8(mixed[i]) - s.expected[i]) <= 1,
                     qPrintable(QString("channel %1: got %2 expected %3")
                                    .arg(i).arg(toSrgb8(mixed[i])).arg(s.expected[i])));
        }
    }
}

void TestKoSpectralMixing::testEndpointsAreExact()
{
    QRandomGenerator random(1);

    for (int i = 0; i < 1000; i++) {
        float a[3];
        float b[3];
        float mixed[3];
        randomColor(random, a);
        randomColor(random, b);

        KoSpectral::mix(a, b, 0.0f, mixed);
        QCOMPARE(distance(a, mixed), 0.0f);

        KoSpectral::mix(a, b, 1.0f, mixed);
        QCOMPARE(distance(b, mixed), 0.0f);

        // and close to them for factors very near the endpoints
        KoSpectral::mix(a, b, 1e-6f, mixed);
        QVERIFY(distance(a, mixed) < 1e-3f);
    }
}

void TestKoSpectralMixing::testMixingWithItselfIsIdentity()
{
    QRandomGenerator random(2);

    for (int i = 0; i < 1000; i++) {
        float a[3];
        float mixed[3];
        randomColor(random, a);

        for (float factor : {0.1f, 0.5f, 0.9f}) {
            KoSpectral::mix(a, a, factor, mixed);
            QVERIFY2(distance(a, mixed) < 1e-5f, qPrintable(QString("error %1").arg(distance(a, mixed))));
        }
    }
}

void TestKoSpectralMixing::testResultsStayInRange()
{
    QRandomGenerator random(3);

    for (int i = 0; i < 10000; i++) {
        float a[3];
        float b[3];
        float mixed[3];
        randomColor(random, a);
        randomColor(random, b);

        KoSpectral::mix(a, b, float(random.generateDouble()), mixed);

        for (int c = 0; c < 3; c++) {
            QVERIFY(std::isfinite(mixed[c]));
            // saturated mixes may land slightly outside of the sRGB gamut,
            // the composite ops clamp them
            QVERIFY2(mixed[c] > -0.05f && mixed[c] < 1.05f, qPrintable(QString("value %1").arg(mixed[c])));
        }
    }
}

void TestKoSpectralMixing::testBlackAndWhite()
{
    const float black[3] = {0.0f, 0.0f, 0.0f};
    const float white[3] = {1.0f, 1.0f, 1.0f};
    float mixed[3];

    for (float factor : {1e-6f, 0.01f, 0.5f, 0.99f, 1.0f - 1e-6f}) {
        KoSpectral::mix(black, white, factor, mixed);
        for (int c = 0; c < 3; c++) {
            QVERIFY(std::isfinite(mixed[c]));
            QVERIFY(mixed[c] >= -1e-4f && mixed[c] <= 1.0001f);
        }
        // stays neutral
        QVERIFY(std::abs(mixed[0] - mixed[1]) < 1e-3f);
        QVERIFY(std::abs(mixed[1] - mixed[2]) < 1e-3f);
    }

    KoSpectral::mix(black, black, 0.5f, mixed);
    QVERIFY(distance(black, mixed) < 1e-6f);

    KoSpectral::mix(white, white, 0.5f, mixed);
    QVERIFY(distance(white, mixed) < 1e-6f);
}

void TestKoSpectralMixing::testMonotonic()
{
    QRandomGenerator random(4);

    for (int i = 0; i < 200; i++) {
        float a[3];
        float b[3];
        randomColor(random, a);
        randomColor(random, b);

        float previous = 0.0f;
        for (int step = 1; step <= 10; step++) {
            float mixed[3];
            KoSpectral::mix(a, b, step / 10.0f, mixed);
            const float d = distance(a, mixed);
            QVERIFY(d >= previous - 1e-4f);
            previous = d;
        }
    }
}

void TestKoSpectralMixing::testRepeatedMixingDoesNotDrift()
{
    // the smudge engine mixes a color into itself on every dab
    QRandomGenerator random(5);

    for (int i = 0; i < 100; i++) {
        float color[3];
        float original[3];
        randomColor(random, color);
        std::copy(color, color + 3, original);

        for (int dab = 0; dab < 1000; dab++) {
            float mixed[3];
            KoSpectral::mix(color, color, 0.3f, mixed);
            std::copy(mixed, mixed + 3, color);
        }

        QVERIFY2(distance(original, color) < 1e-4f, qPrintable(QString("drift %1").arg(distance(original, color))));
    }
}

void TestKoSpectralMixing::testTransferRoundTrip_data()
{
    QTest::addColumn<QString>("profileName");

    QTest::newRow("sRGB") << "sRGB-elle-V2-srgbtrc.icc";
    QTest::newRow("linear sRGB") << "sRGB-elle-V2-g10.icc";
    QTest::newRow("linear rec2020") << "Rec2020-elle-V4-g10.icc";
    QTest::newRow("ClayRGB gamma 2.2") << "ClayRGB-elle-V2-g22.icc";
    QTest::newRow("ACEScg L* curve") << "ACEScg-elle-V2-labl.icc";
}

void TestKoSpectralMixing::testTransferRoundTrip()
{
    QFETCH(QString, profileName);

    const KoColorProfile *profile = KoColorSpaceRegistry::instance()->profileByName(profileName);
    if (!profile) {
        QSKIP("profile is not installed");
    }

    KoSpectralColorTransfer transfer(profile);

    for (int i = 0; i <= 64; i++) {
        const float value = i / 64.0f;
        const float encoded[3] = {value, 1.0f - value, 0.5f * value};
        float linear[3];
        float back[3];

        transfer.toLinearSrgb(encoded, linear);
        transfer.fromLinearSrgb(linear, back);

        QVERIFY2(distance(encoded, back) < 2e-4f,
                 qPrintable(QString("value %1 error %2").arg(value).arg(distance(encoded, back))));
    }
}

void TestKoSpectralMixing::testTransferMatchesLcms_data()
{
    testTransferRoundTrip_data();
}

void TestKoSpectralMixing::testTransferMatchesLcms()
{
    QFETCH(QString, profileName);

    const KoColorProfile *profile = KoColorSpaceRegistry::instance()->profileByName(profileName);
    if (!profile) {
        QSKIP("profile is not installed");
    }

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), profile);
    const KoColorSpace *linearSrgb = KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(),
                                                                                  KoColorSpaceRegistry::instance()->p709G10Profile());
    QVERIFY(cs);
    if (!linearSrgb) {
        QSKIP("linear sRGB profile is not installed");
    }

    KoSpectralColorTransfer transfer(profile);

    for (int i = 0; i <= 16; i++) {
        const float value = i / 16.0f;
        const float encoded[3] = {value, 0.25f + 0.5f * value, 1.0f - value};

        float pixel[4] = {encoded[0], encoded[1], encoded[2], 1.0f};
        KoColor color(reinterpret_cast<quint8 *>(pixel), cs);
        color.convertTo(linearSrgb, KoColorConversionTransformation::IntentAbsoluteColorimetric, KoColorConversionTransformation::Empty);
        const float *expected = reinterpret_cast<const float *>(color.data());

        float linear[3];
        transfer.toLinearSrgb(encoded, linear);

        QVERIFY2(distance(expected, linear) < 2e-3f,
                 qPrintable(QString("value %1: got (%2 %3 %4) expected (%5 %6 %7)")
                                .arg(value)
                                .arg(linear[0]).arg(linear[1]).arg(linear[2])
                                .arg(expected[0]).arg(expected[1]).arg(expected[2])));
    }
}

void TestKoSpectralMixing::testTransferKeepsNeutralsNeutral_data()
{
    QTest::addColumn<QString>("profileName");

    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    QTest::newRow("rgb8 default") << registry->rgb8()->profile()->name();
    QTest::newRow("rgb16 default") << registry->rgb16()->profile()->name();
    QTest::newRow("f32 default") << registry->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), QString())->profile()->name();
    QTest::newRow("ClayRGB gamma 2.2") << "ClayRGB-elle-V2-g22.icc";
    QTest::newRow("linear rec2020") << "Rec2020-elle-V4-g10.icc";
}

void TestKoSpectralMixing::testTransferKeepsNeutralsNeutral()
{
    QFETCH(QString, profileName);

    const KoColorProfile *profile = KoColorSpaceRegistry::instance()->profileByName(profileName);
    if (!profile) {
        QSKIP("profile is not installed");
    }

    KoSpectralColorTransfer transfer(profile);

    // a gray in any RGB profile must stay a gray in linear sRGB, otherwise
    // the spectral mixing tints it
    for (int i = 0; i <= 16; i++) {
        const float value = i / 16.0f;
        const float encoded[3] = {value, value, value};
        float linear[3];
        transfer.toLinearSrgb(encoded, linear);

        QVERIFY2(std::abs(linear[0] - linear[1]) < 1e-5f && std::abs(linear[1] - linear[2]) < 1e-5f,
                 qPrintable(QString("%1: (%2 %3 %4)").arg(value).arg(linear[0], 0, 'g', 8).arg(linear[1], 0, 'g', 8).arg(linear[2], 0, 'g', 8)));
    }
}

KISTEST_MAIN(TestKoSpectralMixing)
