/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSpectralColorTransfer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>

#include "KoColorProfile.h"
#include "KoColorProfileConstants.h"
#include "KoColorimetryUtils.h"

namespace
{

static constexpr int TABLE_SIZE = 4096;

inline float srgbToLinear(float x)
{
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}

inline float linearToSrgb(float x)
{
    return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

inline float sampleTable(const QVector<float> &table, float position)
{
    position = std::clamp(position, 0.0f, 1.0f) * (TABLE_SIZE - 1);
    const int index = std::min(int(position), TABLE_SIZE - 2);
    const float fraction = position - index;
    return table[index] + (table[index + 1] - table[index]) * fraction;
}

inline void multiply(const float m[3][3], const float in[3], float out[3])
{
    for (int i = 0; i < 3; i++) {
        out[i] = m[i][0] * in[0] + m[i][1] * in[1] + m[i][2] * in[2];
    }
}

}

KoSpectralColorTransfer::KoSpectralColorTransfer(const KoColorProfile *profile)
{
    if (!profile) {
        return;
    }

    switch (profile->getTransferCharacteristics()) {
    case TRC_LINEAR:
        m_curve = Linear;
        break;
    case TRC_IEC_61966_2_1:
        m_curve = Srgb;
        break;
    case TRC_GAMMA_1_8:
        m_curve = Gamma;
        m_gamma = 1.8f;
        break;
    case TRC_ITU_R_BT_470_6_SYSTEM_M:
        m_curve = Gamma;
        m_gamma = 2.2f;
        break;
    case TRC_A98:
        m_curve = Gamma;
        m_gamma = 563.0f / 256.0f;
        break;
    case TRC_GAMMA_2_4:
        m_curve = Gamma;
        m_gamma = 2.4f;
        break;
    case TRC_ITU_R_BT_470_6_SYSTEM_B_G:
        m_curve = Gamma;
        m_gamma = 2.8f;
        break;
    default:
        if (profile->hasTRC()) {
            m_curve = Table;
        }
        break;
    }

    if (m_curve == Table) {
        m_linearizeTable.resize(TABLE_SIZE);
        m_delinearizeTable.resize(TABLE_SIZE);

        QVector<qreal> value(3);

        for (int i = 0; i < TABLE_SIZE; i++) {
            const qreal x = qreal(i) / (TABLE_SIZE - 1);

            value.fill(x);
            profile->linearizeFloatValue(value);
            m_linearizeTable[i] = value[0];

            value.fill(x * x);
            profile->delinearizeFloatValue(value);
            m_delinearizeTable[i] = value[0];
        }
    }

    // LcmsColorProfileContainer pads the result with empty entries,
    // so take the last three values. The colorants are adapted to the
    // media white point of the profile, so build the colorimetry with it
    // and let Colorimetry handle the adaptation to D65.
    const QVector<KoColorimetryUtils::XYZ> colorants = profile->getColorantsXYZ();

    if (profile->hasColorants() && colorants.size() >= 3) {
        using KoColorimetryUtils::Colorimetry;

        const KoColorimetryUtils::XYZ *c = colorants.constData() + colorants.size() - 3;
        const Colorimetry profileColorimetry(c[0], c[1], c[2], profile->getWhitePointXYZ());

        const QMatrix4x4 toSrgb = profileColorimetry.relativeColorimetricTo(Colorimetry::BT709);
        const QMatrix4x4 fromSrgb = toSrgb.inverted();

        float maxDeviation = 0.0f;
        bool isFinite = true;

        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 3; column++) {
                m_toSrgb[row][column] = toSrgb(row, column);
                m_fromSrgb[row][column] = fromSrgb(row, column);
                isFinite &= std::isfinite(m_toSrgb[row][column]) && std::isfinite(m_fromSrgb[row][column]);
                maxDeviation = std::max(maxDeviation, std::abs(m_toSrgb[row][column] - (row == column ? 1.0f : 0.0f)));
            }
        }

        // sRGB profiles from different sources differ slightly in their
        // colorants; don't add a lossy matrix multiplication for that
        m_hasMatrix = isFinite && maxDeviation > 0.002f;
    }
}

bool KoSpectralColorTransfer::isLinear() const
{
    return m_curve == Linear;
}

float KoSpectralColorTransfer::linearize(float value) const
{
    switch (m_curve) {
    case Linear:
        return value;
    case Srgb:
        return srgbToLinear(value);
    case Gamma:
        return value > 0.0f ? std::pow(value, m_gamma) : 0.0f;
    case Table:
        return sampleTable(m_linearizeTable, value);
    }
    return value;
}

float KoSpectralColorTransfer::delinearize(float value) const
{
    switch (m_curve) {
    case Linear:
        return value;
    case Srgb:
        return linearToSrgb(value);
    case Gamma:
        return value > 0.0f ? std::pow(value, 1.0f / m_gamma) : 0.0f;
    case Table:
        return sampleTable(m_delinearizeTable, std::sqrt(std::max(0.0f, value)));
    }
    return value;
}

void KoSpectralColorTransfer::toLinearSrgb(const float encoded[3], float linear[3]) const
{
    float values[3] = {linearize(encoded[0]), linearize(encoded[1]), linearize(encoded[2])};

    if (m_hasMatrix) {
        multiply(m_toSrgb, values, linear);
    } else {
        std::copy(values, values + 3, linear);
    }
}

void KoSpectralColorTransfer::fromLinearSrgb(const float linear[3], float encoded[3]) const
{
    float values[3];

    if (m_hasMatrix) {
        multiply(m_fromSrgb, linear, values);
    } else {
        std::copy(linear, linear + 3, values);
    }

    for (int i = 0; i < 3; i++) {
        encoded[i] = delinearize(values[i]);
    }
}
