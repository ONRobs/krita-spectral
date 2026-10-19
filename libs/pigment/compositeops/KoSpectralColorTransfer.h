/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSPECTRALCOLORTRANSFER_H
#define KOSPECTRALCOLORTRANSFER_H

#include <QVector>

#include "kritapigment_export.h"

class KoColorProfile;

/**
 * Converts the normalized RGB values of a color space (in the range
 * [0, 1] for SDR content) into the linear sRGB values expected by
 * KoSpectral::mix() and back.
 *
 * The conversion uses the actual tone curve and primaries of the profile:
 *
 *  - linear, sRGB and pure gamma curves are evaluated exactly
 *  - any other curve is sampled from the profile into lookup tables
 *  - non-sRGB primaries are converted with a 3x3 matrix built from the
 *    profile colorants (relative colorimetric, Bradford adaptation to D65)
 *
 * The object is created once per composite op, so construction may be
 * slow-ish; the conversion functions are meant to be called per pixel.
 */
class KRITAPIGMENT_EXPORT KoSpectralColorTransfer
{
public:
    /**
     * @param profile the profile of the color space, may be null, then
     *                sRGB is assumed
     */
    explicit KoSpectralColorTransfer(const KoColorProfile *profile);

    void toLinearSrgb(const float encoded[3], float linear[3]) const;
    void fromLinearSrgb(const float linear[3], float encoded[3]) const;

    /**
     * True if the profile stores linear values. The color space
     * may then contain HDR values outside [0, 1].
     */
    bool isLinear() const;

private:
    enum Curve {
        Linear,
        Srgb,
        Gamma,
        Table
    };

    float linearize(float value) const;
    float delinearize(float value) const;

private:
    Curve m_curve {Srgb};
    float m_gamma {1.0f};

    bool m_hasMatrix {false};
    float m_toSrgb[3][3];
    float m_fromSrgb[3][3];

    /// linearization, sampled uniformly over the encoded value
    QVector<float> m_linearizeTable;
    /// delinearization, sampled uniformly over sqrt() of the linear value
    /// to get enough samples in the shadows, where the curve is steep
    QVector<float> m_delinearizeTable;
};

#endif // KOSPECTRALCOLORTRANSFER_H
