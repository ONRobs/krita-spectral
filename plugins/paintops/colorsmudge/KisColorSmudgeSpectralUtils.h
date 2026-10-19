/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCOLORSMUDGESPECTRALUTILS_H
#define KISCOLORSMUDGESPECTRALUTILS_H

#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KoCompositeOpRegistry.h>

/**
 * Helpers for spectral (Kubelka-Munk) smudging.
 *
 * The color smudge engine mixes the brush color into the dab with the
 * brush's blending mode, but smears the canvas and stamps the dab with
 * fixed COPY/OVER ops. When the blending mode is "Spectral", all of these
 * steps have to mix spectrally, otherwise most of the mixing still happens
 * in RGB.
 */
namespace KisColorSmudgeSpectralUtils
{

/**
 * Spectral smudging is used when the brush blending mode is "Spectral" and
 * the color space the smudge engine works in supports it (RGB only).
 */
inline bool useSpectralMixing(const KoColorSpace *cs, const QString &colorRateCompositeOpId)
{
    return colorRateCompositeOpId == COMPOSITE_SPECTRAL && cs->hasCompositeOp(COMPOSITE_SPECTRAL);
}

/**
 * Returns the spectral counterpart of the OVER and COPY ops
 */
inline QString spectralVariant(const QString &compositeOpId, bool useSpectral)
{
    if (!useSpectral) {
        return compositeOpId;
    }

    if (compositeOpId == COMPOSITE_OVER) {
        return COMPOSITE_SPECTRAL;
    } else if (compositeOpId == COMPOSITE_COPY) {
        return COMPOSITE_COPY_SPECTRAL;
    }

    return compositeOpId;
}

/**
 * At full opacity the spectral copy is a plain copy, so the engine's
 * fast paths for COPY apply to it as well
 */
inline bool isCopyOp(const KoCompositeOp *op)
{
    return op->id() == COMPOSITE_COPY || op->id() == COMPOSITE_COPY_SPECTRAL;
}

}

#endif // KISCOLORSMUDGESPECTRALUTILS_H
