/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorSmudgeSpectralTest.h"

#include "kistest.h"

#include <QColor>

#include <KoCanvasResourcesIds.h>
#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>
#include <brushengine/kis_paint_information.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_settings.h>
#include <kis_canvas_resource_provider.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_resources_snapshot.h>
#include <kis_undo_stores.h>
#include <stroke_testing_utils.h>

Q_DECLARE_METATYPE(const KoColorSpace *)

namespace
{

const QColor BLUE(0, 33, 133);
const QColor YELLOW(252, 211, 0);

const QStringList PRESETS = {
    "test_smudge_20px_dul_nsa_new.0001.kpp",
    "test_smudge_20px_dul_nsa_old.0001.kpp",
    "test_smudge_20px_dul_sa_new.0001.kpp",
    "test_smudge_20px_dul_sa_old.0001.kpp",
    "test_smudge_20px_sme_nsa_new.0001.kpp",
    "test_smudge_20px_sme_nsa_old.0001.kpp",
    "test_smudge_20px_sme_sa_new.0001.kpp",
    "test_smudge_20px_sme_sa_old.0001.kpp",
};

void addPresetsAndColorSpaces()
{
    QTest::addColumn<QString>("preset");
    QTest::addColumn<const KoColorSpace *>("cs");

    const QList<QPair<QString, const KoColorSpace *>> spaces = {
        {"u8", KoColorSpaceRegistry::instance()->rgb8()},
        {"u16", KoColorSpaceRegistry::instance()->rgb16()},
    };

    for (const auto &space : spaces) {
        Q_FOREACH (const QString &preset, PRESETS) {
            const QString name = QString("%1_%2").arg(space.first).arg(preset.mid(12, preset.size() - 12 - 9));
            QTest::addRow("%s", name.toLatin1().data()) << preset << space.second;
        }
    }
}

struct TestImage {
    TestImage(const KoColorSpace *cs)
    {
        image = new KisImage(new KisSurrogateUndoStore(), 200, 200, cs, "spectral smudge test");
        layer = new KisPaintLayer(image, "paint", OPACITY_OPAQUE_U8);
        image->addNode(layer, image->root());
        image->initialRefreshGraph();
    }

    /**
     * Paints horizontal strokes with the color smudge preset, with its
     * blending mode replaced by \p compositeOp
     */
    void paint(const QString &presetFileName, const QString &compositeOp, const QColor &color,
               const QVector<QPair<QPointF, QPointF>> &strokes)
    {
        QScopedPointer<KoCanvasResourceProvider> manager(utils::createResourceManager(image, layer, presetFileName));
        manager->setResource(KoCanvasResource::ForegroundColor, QVariant::fromValue(KoColor(color, image->colorSpace())));

        KisPaintOpPresetSP preset = manager->resource(KoCanvasResource::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();
        QVERIFY(preset);
        preset->settings()->setPaintOpCompositeOp(compositeOp);
        manager->setResource(KoCanvasResource::CurrentPaintOpPreset, QVariant::fromValue(preset));

        KisResourcesSnapshotSP resources = new KisResourcesSnapshot(image, layer, manager.data());
        QCOMPARE(resources->compositeOpId(), compositeOp);

        KisPainter gc(layer->paintDevice());
        resources->setupPainter(&gc);

        for (const auto &stroke : strokes) {
            KisDistanceInformation dist;
            gc.paintLine(KisPaintInformation(stroke.first, 1.0), KisPaintInformation(stroke.second, 1.0), &dist);
        }
    }

    QColor pixel(int x, int y) const
    {
        KoColor color(image->colorSpace());
        layer->paintDevice()->pixel(x, y, &color);
        return color.toQColor();
    }

    KisImageSP image;
    KisPaintLayerSP layer;
};

QVector<QPair<QPointF, QPointF>> backAndForth(int count, qreal y = 100)
{
    QVector<QPair<QPointF, QPointF>> strokes;
    for (int i = 0; i < count; i++) {
        const qreal offset = (i % 5) * 4 - 8;
        if (i % 2) {
            strokes.append({QPointF(170, y + offset), QPointF(30, y + offset)});
        } else {
            strokes.append({QPointF(30, y + offset), QPointF(170, y + offset)});
        }
    }
    return strokes;
}

bool isGreenish(const QColor &c)
{
    return c.green() > c.red() + 20 && c.green() > c.blue() + 20;
}

/// counts the pixels of a horizontal band that look green
int countGreenishPixels(const TestImage &image)
{
    int result = 0;
    for (int y = 90; y < 110; y++) {
        for (int x = 0; x < 200; x++) {
            result += isGreenish(image.pixel(x, y));
        }
    }
    return result;
}

}

void KisColorSmudgeSpectralTest::testMixesSpectrally_data()
{
    addPresetsAndColorSpaces();
}

void KisColorSmudgeSpectralTest::testMixesSpectrally()
{
    QFETCH(QString, preset);
    QFETCH(const KoColorSpace *, cs);

    int greenish[2] = {0, 0};
    const QString ops[2] = {COMPOSITE_OVER, COMPOSITE_SPECTRAL};

    for (int i = 0; i < 2; i++) {
        TestImage image(cs);
        image.layer->paintDevice()->fill(QRect(0, 0, 100, 200), KoColor(BLUE, cs));
        image.layer->paintDevice()->fill(QRect(100, 0, 100, 200), KoColor(YELLOW, cs));

        // drag yellow paint over the blue area and back
        image.paint(preset, ops[i], YELLOW, backAndForth(6));
        greenish[i] = countGreenishPixels(image);
    }

    // RGB mixing of this blue and yellow gives grays, spectral mixing gives greens.
    // Dulling lays down one averaged color per dab, so it makes fewer of them
    // than smearing, but there must be some
    QVERIFY2(greenish[0] == 0, qPrintable(QString("normal: %1 green pixels").arg(greenish[0])));
    QVERIFY2(greenish[1] > 30, qPrintable(QString("spectral: %1 green pixels").arg(greenish[1])));
}

void KisColorSmudgeSpectralTest::testNoDriftOnFlatColor_data()
{
    addPresetsAndColorSpaces();
}

void KisColorSmudgeSpectralTest::testNoDriftOnFlatColor()
{
    QFETCH(QString, preset);
    QFETCH(const KoColorSpace *, cs);

    const QColor color(87, 140, 23);

    TestImage image(cs);
    image.layer->paintDevice()->fill(QRect(0, 0, 200, 200), KoColor(color, cs));

    image.paint(preset, COMPOSITE_SPECTRAL, color, backAndForth(30));

    for (int y = 80; y < 120; y += 3) {
        for (int x = 20; x < 180; x += 3) {
            const QColor c = image.pixel(x, y);
            QVERIFY2(std::abs(c.red() - color.red()) <= 1 && std::abs(c.green() - color.green()) <= 1 &&
                         std::abs(c.blue() - color.blue()) <= 1 && c.alpha() == 255,
                     qPrintable(QString("(%1, %2) drifted to %3").arg(x).arg(y).arg(c.name())));
        }
    }
}

void KisColorSmudgeSpectralTest::testNeutralsStayNeutral_data()
{
    addPresetsAndColorSpaces();
}

void KisColorSmudgeSpectralTest::testNeutralsStayNeutral()
{
    QFETCH(QString, preset);
    QFETCH(const KoColorSpace *, cs);

    TestImage image(cs);
    image.layer->paintDevice()->fill(QRect(0, 0, 100, 200), KoColor(Qt::black, cs));
    image.layer->paintDevice()->fill(QRect(100, 0, 100, 200), KoColor(Qt::white, cs));

    image.paint(preset, COMPOSITE_SPECTRAL, QColor(128, 128, 128), backAndForth(30));

    for (int y = 80; y < 120; y += 2) {
        for (int x = 0; x < 200; x += 2) {
            const QColor c = image.pixel(x, y);
            QVERIFY2(std::abs(c.red() - c.green()) <= 2 && std::abs(c.green() - c.blue()) <= 2,
                     qPrintable(QString("(%1, %2) is not neutral: %3").arg(x).arg(y).arg(c.name())));
        }
    }
}

void KisColorSmudgeSpectralTest::testAlphaLockedLayer_data()
{
    addPresetsAndColorSpaces();
}

void KisColorSmudgeSpectralTest::testAlphaLockedLayer()
{
    QFETCH(QString, preset);
    QFETCH(const KoColorSpace *, cs);

    TestImage image(cs);
    QColor halfBlue = BLUE;
    halfBlue.setAlpha(128);
    image.layer->paintDevice()->fill(QRect(60, 0, 40, 200), KoColor(BLUE, cs));
    image.layer->paintDevice()->fill(QRect(100, 0, 40, 200), KoColor(halfBlue, cs));
    image.layer->setAlphaLocked(true);

    // a community build crashed when smudging on alpha locked layers
    image.paint(preset, COMPOSITE_SPECTRAL, YELLOW, backAndForth(10));

    for (int y = 80; y < 120; y += 2) {
        for (int x = 0; x < 200; x += 2) {
            const int expectedAlpha = (x >= 60 && x < 100) ? 255 : (x >= 100 && x < 140) ? 128 : 0;
            QVERIFY2(image.pixel(x, y).alpha() == expectedAlpha,
                     qPrintable(QString("(%1, %2) alpha %3, expected %4").arg(x).arg(y).arg(image.pixel(x, y).alpha()).arg(expectedAlpha)));
        }
    }

    // and the color of the locked area got painted
    QVERIFY(image.pixel(80, 100) != BLUE);
}

void KisColorSmudgeSpectralTest::testNonRgbFallsBack()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(), Integer8BitsColorDepthID.id(), QString());
    QVERIFY(cs);

    Q_FOREACH (const QString &preset, PRESETS) {
        TestImage image(cs);
        image.layer->paintDevice()->fill(QRect(0, 0, 100, 200), KoColor(BLUE, cs));

        image.paint(preset, COMPOSITE_SPECTRAL, YELLOW, backAndForth(4));

        // painted with the fallback (Normal) without crashing
        QVERIFY(image.layer->paintDevice()->exactBounds().width() > 100);
    }
}

KISTEST_MAIN(KisColorSmudgeSpectralTest)
