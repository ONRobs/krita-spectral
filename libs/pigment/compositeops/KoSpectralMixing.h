/*
 *  SPDX-FileCopyrightText: 2025 Ronald van Wijnen <ronald.van.wijnen@gmail.com>
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: MIT
 */

#ifndef KOSPECTRALMIXING_H
#define KOSPECTRALMIXING_H

#include "kritapigment_export.h"

/**
 * Kubelka-Munk pigment mixing, ported from spectral.js v3 by Ronald van Wijnen
 * (https://github.com/rvanwijnen/spectral.js, MIT license).
 *
 * A linear sRGB color is turned into a reflectance curve sampled at SIZE
 * wavelengths (a weighted sum of seven base spectra), each band is turned
 * into a K/S (absorption/scattering) ratio, the K/S values of the two colors
 * are averaged with a concentration weight, and the result is converted back
 * to linear sRGB through the CIE colour matching functions.
 *
 * All colors here are *linear* sRGB (D65). Converting from and to the
 * actual color space of the image is the job of KoSpectralColorTransfer.
 *
 * Differences from spectral.js:
 *
 *  - KM() uses the algebraically equivalent form 1 / (1 + KS + sqrt(KS^2 + 2KS)),
 *    which doesn't lose precision for dark colors in single precision floats
 *
 *  - white is a flat spectrum and the spectrum to RGB conversion is
 *    normalized, so that mixtures of grays are exactly neutral
 *
 *  - mix() compensates for the round-trip error of each input color, so that
 *    mixing with factor 0 or 1, or mixing a color with itself, returns the
 *    input exactly. Without that the smudge engine, which re-mixes the same
 *    pixels for every dab, would slowly drift the colors along a stroke.
 */
namespace KoSpectral
{

static constexpr int SIZE = 38;

/// SIZE padded to a multiple of 8, so that the loops over the bands vectorize
static constexpr int PADDED_SIZE = 40;

/**
 * A color prepared for mixing. Preparing is the expensive part, so callers
 * that mix one color with many pixels should prepare it once.
 */
struct Pigment {
    alignas(32) float ks[PADDED_SIZE]; ///< K/S ratio for every wavelength band (and padding)
    float luminance;                   ///< CIE Y of the reflectance curve
    float roundTripError[3];           ///< (spectral round trip of the color) - (the color), linear sRGB
};

/**
 * Prepares a linear sRGB color for mixing. Components outside [0, 1]
 * are clamped for building the reflectance curve; the clamped-away part
 * is preserved through the round-trip error compensation.
 */
KRITAPIGMENT_EXPORT void prepare(const float linearRgb[3], Pigment &pigment);

/**
 * Mixes two prepared colors. \p factor is the weight of \p b,
 * i.e. 0.0 returns exactly \p a and 1.0 returns exactly \p b.
 * The result is linear sRGB and is *not* clamped.
 */
KRITAPIGMENT_EXPORT void mix(const Pigment &a, const float aLinearRgb[3],
                             const Pigment &b, const float bLinearRgb[3],
                             float factor, float result[3]);

/**
 * Convenience overload that prepares both colors itself.
 */
KRITAPIGMENT_EXPORT void mix(const float aLinearRgb[3], const float bLinearRgb[3],
                             float factor, float result[3]);

}

#endif // KOSPECTRALMIXING_H
