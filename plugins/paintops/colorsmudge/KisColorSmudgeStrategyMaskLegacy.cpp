/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCompositeOpRegistry.h>
#include <kis_algebra_2d.h>
#include <QtMath>
#include "KisColorSmudgeStrategyMaskLegacy.h"
#include "KisColorSmudgeSampleUtils.h"
#include "KisColorSmudgeSpectralUtils.h" // krita-spectral

#include "kis_image.h"

KisColorSmudgeStrategyMaskLegacy::KisColorSmudgeStrategyMaskLegacy(KisPainter *painter, KisImageSP image,
                                                                   bool smearAlpha, bool useDullingMode,
                                                                   bool useOverlayMode)
        : KisColorSmudgeStrategyMask(painter,
                                     image,
                                     smearAlpha,
                                     useDullingMode,
                                     useOverlayMode)
{
}

void KisColorSmudgeStrategyMaskLegacy::sampleDullingColor(const QRect &srcRect, qreal sampleRadiusValue,
                                                          KisColorSmudgeSourceSP sourceDevice,
                                                          KisFixedPaintDeviceSP tempFixedDevice,
                                                          KisFixedPaintDeviceSP maskDab, KoColor *resultColor)
{
    using namespace KisColorSmudgeSampleUtils;
    sampleColor<AveragedSampleWrapper>(srcRect, sampleRadiusValue,
                                       sourceDevice, tempFixedDevice,
                                       maskDab, resultColor);
}

QString KisColorSmudgeStrategyMaskLegacy::smearCompositeOp(bool smearAlpha) const
{
    Q_UNUSED(smearAlpha);
    return KisColorSmudgeSpectralUtils::spectralVariant(COMPOSITE_COPY, m_useSpectral);
}

QString KisColorSmudgeStrategyMaskLegacy::finalCompositeOp(bool smearAlpha) const
{
    return KisColorSmudgeSpectralUtils::spectralVariant(smearAlpha ? COMPOSITE_COPY : COMPOSITE_OVER, m_useSpectral);
}

qreal KisColorSmudgeStrategyMaskLegacy::finalPainterOpacity(qreal opacity, qreal smudgeRateValue)
{
    return qBound(OPACITY_TRANSPARENT_F,
                  smudgeRateValue * opacity,
                  OPACITY_OPAQUE_F);
}

qreal KisColorSmudgeStrategyMaskLegacy::colorRateOpacity(qreal opacity, qreal smudgeRateValue, qreal colorRateValue,
                                                         qreal maxPossibleSmudgeRateValue)
{
    Q_UNUSED(smudgeRateValue);

    const qreal maxColorRate = qMax<qreal>(1.0 - maxPossibleSmudgeRateValue, 0.2);

    return qBound(OPACITY_TRANSPARENT_F,
                  KisAlgebra2D::lerp(0.0, maxColorRate, colorRateValue * opacity),
                  OPACITY_OPAQUE_F);
}

qreal KisColorSmudgeStrategyMaskLegacy::dullingRateOpacity(qreal opacity, qreal smudgeRateValue)
{
    Q_UNUSED(opacity);
    Q_UNUSED(smudgeRateValue);

    return OPACITY_OPAQUE_F;
}

qreal KisColorSmudgeStrategyMaskLegacy::smearRateOpacity(qreal opacity, qreal smudgeRateValue)
{
    Q_UNUSED(opacity);
    Q_UNUSED(smudgeRateValue);

    return OPACITY_OPAQUE_F;
}
