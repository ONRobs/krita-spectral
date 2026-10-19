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

static constexpr int LINEARIZE_TABLE_SIZE = 4097;
static constexpr int DELINEARIZE_TABLE_SIZE = 8193;

double srgbToLinear(double x)
{
    return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double x)
{
    return x <= 0.0031308 ? x * 12.92 : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

inline float sampleTable(const QVector<float> &table, float position)
{
    const int size = table.size();
    position = std::clamp(position, 0.0f, 1.0f) * (size - 1);
    const int index = std::min(int(position), size - 2);
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
    enum {
        Srgb,
        Gamma,
        FromProfile
    } curve = Srgb;
    double gamma = 1.0;

    if (profile) {
        switch (profile->getTransferCharacteristics()) {
        case TRC_LINEAR:
            m_isLinear = true;
            break;
        case TRC_IEC_61966_2_1:
            curve = Srgb;
            break;
        case TRC_GAMMA_1_8:
            curve = Gamma;
            gamma = 1.8;
            break;
        case TRC_ITU_R_BT_470_6_SYSTEM_M:
            curve = Gamma;
            gamma = 2.2;
            break;
        case TRC_A98:
            curve = Gamma;
            gamma = 563.0 / 256.0;
            break;
        case TRC_GAMMA_2_4:
            curve = Gamma;
            gamma = 2.4;
            break;
        case TRC_ITU_R_BT_470_6_SYSTEM_B_G:
            curve = Gamma;
            gamma = 2.8;
            break;
        default:
            if (profile->hasTRC()) {
                curve = FromProfile;
            }
            break;
        }
    }

    if (!m_isLinear) {
        m_linearizeTable.resize(LINEARIZE_TABLE_SIZE);
        m_delinearizeTable.resize(DELINEARIZE_TABLE_SIZE);

        QVector<qreal> value(3);

        for (int i = 0; i < LINEARIZE_TABLE_SIZE; i++) {
            const qreal x = qreal(i) / (LINEARIZE_TABLE_SIZE - 1);

            if (curve == Srgb) {
                m_linearizeTable[i] = srgbToLinear(x);
            } else if (curve == Gamma) {
                m_linearizeTable[i] = std::pow(x, gamma);
            } else {
                value.fill(x);
                profile->linearizeFloatValue(value);
                m_linearizeTable[i] = value[0];
            }
        }

        for (int i = 0; i < DELINEARIZE_TABLE_SIZE; i++) {
            const qreal u = qreal(i) / (DELINEARIZE_TABLE_SIZE - 1);
            const qreal x = u * u;

            if (curve == Srgb) {
                m_delinearizeTable[i] = linearToSrgb(x);
            } else if (curve == Gamma) {
                m_delinearizeTable[i] = std::pow(x, 1.0 / gamma);
            } else {
                value.fill(x);
                profile->delinearizeFloatValue(value);
                m_delinearizeTable[i] = value[0];
            }
        }
    }

    if (!profile) {
        return;
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
    return m_isLinear;
}

float KoSpectralColorTransfer::linearize(float value) const
{
    return m_isLinear ? value : sampleTable(m_linearizeTable, value);
}

float KoSpectralColorTransfer::delinearize(float value) const
{
    // sampled over sqrt() of the linear value, to have enough samples
    // in the shadows, where the curves are steep
    return m_isLinear ? value : sampleTable(m_delinearizeTable, std::sqrt(std::max(0.0f, value)));
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
