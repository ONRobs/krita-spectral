/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOCOMPOSITEOPSPECTRAL_H
#define KOCOMPOSITEOPSPECTRAL_H

#include <QBitArray>

#include <algorithm>
#include <cstring>
#include <type_traits>

#include <KoColorSpace.h>
#include <KoColorSpaceMaths.h>
#include <KoCompositeOp.h>

#include "KoSpectralColorTransfer.h"
#include "KoSpectralMixing.h"

namespace KoSpectralCompositeOpPrivate
{

/**
 * A color of one pixel prepared for spectral mixing. Neighbouring pixels
 * very often have the same color (and the source of a brush dab is often
 * a single color), so the expensive preparation is cached per call.
 */
struct CachedColor {
    float encoded[3] {-1.0f, -1.0f, -1.0f};
    float linear[3];
    bool isMixable {false};
    KoSpectral::Pigment pigment;

    void update(const float newEncoded[3], const KoSpectralColorTransfer &transfer)
    {
        if (std::equal(newEncoded, newEncoded + 3, encoded)) {
            return;
        }

        std::copy(newEncoded, newEncoded + 3, encoded);

        // the reflectance model only works for SDR colors; HDR values
        // in floating point color spaces are mixed linearly instead.
        // Color conversions into float spaces leave tiny rounding errors
        // around 0 and 1, which are still SDR.
        static constexpr float tolerance = 1e-3f;

        isMixable = true;
        float clamped[3];
        for (int i = 0; i < 3; i++) {
            isMixable &= encoded[i] >= -tolerance && encoded[i] <= 1.0f + tolerance;
            clamped[i] = std::clamp(encoded[i], 0.0f, 1.0f);
        }

        if (isMixable) {
            transfer.toLinearSrgb(clamped, linear);
            KoSpectral::prepare(linear, pigment);
        }
    }
};

}

/**
 * Composite ops that mix the color channels like paint, using the
 * Kubelka-Munk model in KoSpectral, instead of interpolating them.
 *
 * Alpha is handled exactly like in the classic counterparts:
 *
 *  - isCopy == false: like KoCompositeOpOver ("Spectral" blending mode)
 *  - isCopy == true:  like KoCompositeOpCopy2 (used internally by the
 *                     color smudge engine, not shown in the UI)
 *
 * Only RGB color spaces are supported.
 */
template<class Traits, bool isCopy>
class KoCompositeOpSpectral : public KoCompositeOp
{
    typedef typename Traits::channels_type channels_type;
    typedef KoSpectralCompositeOpPrivate::CachedColor CachedColor;

    static const qint32 channels_nb = Traits::channels_nb;
    static const qint32 alpha_pos = Traits::alpha_pos;
    static const qint32 red_pos = Traits::red_pos;
    static const qint32 green_pos = Traits::green_pos;
    static const qint32 blue_pos = Traits::blue_pos;
    static const qint32 pixel_size = Traits::pixelSize;

public:
    KoCompositeOpSpectral(const KoColorSpace *cs, const QString &id, const QString &category)
        : KoCompositeOp(cs, id, category)
        , m_transfer(cs->profile())
    {
    }

    using KoCompositeOp::composite;

    void composite(const KoCompositeOp::ParameterInfo &params) const override
    {
        const bool allChannelFlags = params.channelFlags.isEmpty() || params.channelFlags == QBitArray(channels_nb, true);
        const bool alphaLocked = !params.channelFlags.isEmpty() && !params.channelFlags.testBit(alpha_pos);

        if (alphaLocked) {
            if (allChannelFlags) {
                genericComposite<true, true>(params);
            } else {
                genericComposite<true, false>(params);
            }
        } else {
            if (allChannelFlags) {
                genericComposite<false, true>(params);
            } else {
                genericComposite<false, false>(params);
            }
        }
    }

private:
    template<bool alphaLocked, bool allChannelFlags>
    void genericComposite(const KoCompositeOp::ParameterInfo &params) const
    {
        using namespace Arithmetic;

        const qint32 srcInc = (params.srcRowStride == 0) ? 0 : channels_nb;
        const channels_type opacity = scale<channels_type>(params.opacity);

        CachedColor srcColor;
        CachedColor dstColor;

        quint8 *dstRowStart = params.dstRowStart;
        const quint8 *srcRowStart = params.srcRowStart;
        const quint8 *maskRowStart = params.maskRowStart;

        for (qint32 r = 0; r < params.rows; ++r) {
            const channels_type *src = reinterpret_cast<const channels_type *>(srcRowStart);
            channels_type *dst = reinterpret_cast<channels_type *>(dstRowStart);
            const quint8 *mask = maskRowStart;

            for (qint32 c = 0; c < params.cols; ++c) {
                const channels_type maskAlpha = mask ? scale<channels_type>(*mask) : unitValue<channels_type>();
                const channels_type srcAlpha = src[alpha_pos];
                const channels_type dstAlpha = dst[alpha_pos];

                if (!allChannelFlags && dstAlpha == zeroValue<channels_type>()) {
                    memset(reinterpret_cast<quint8 *>(dst), 0, pixel_size);
                }

                channels_type newDstAlpha = dstAlpha;

                if (isCopy) {
                    newDstAlpha = composeCopy<alphaLocked, allChannelFlags>(src, srcAlpha, dst, dstAlpha,
                                                                            mul(maskAlpha, opacity),
                                                                            params.channelFlags, srcColor, dstColor);
                } else {
                    newDstAlpha = composeOver<alphaLocked, allChannelFlags>(src, mul(srcAlpha, maskAlpha, opacity), dst, dstAlpha,
                                                                            params.channelFlags, srcColor, dstColor);
                }

                if (!alphaLocked) {
                    dst[alpha_pos] = newDstAlpha;
                }

                src += srcInc;
                dst += channels_nb;

                if (mask) {
                    ++mask;
                }
            }

            srcRowStart += params.srcRowStride;
            dstRowStart += params.dstRowStride;
            maskRowStart += params.maskRowStride;
        }
    }

    /// same alpha handling as KoCompositeOpOver
    template<bool alphaLocked, bool allChannelFlags>
    inline channels_type composeOver(const channels_type *src, channels_type srcAlpha,
                                     channels_type *dst, channels_type dstAlpha,
                                     const QBitArray &channelFlags,
                                     CachedColor &srcColor, CachedColor &dstColor) const
    {
        using namespace Arithmetic;

        if (srcAlpha == zeroValue<channels_type>()) {
            return dstAlpha;
        }

        if (alphaLocked || dstAlpha == unitValue<channels_type>()) {
            mixColor<allChannelFlags>(src, dst, scale<float>(srcAlpha), channelFlags, srcColor, dstColor);
            return dstAlpha;
        }

        if (dstAlpha == zeroValue<channels_type>()) {
            // the color of the destination is undefined, don't mix with it
            copyColor<allChannelFlags>(src, dst, channelFlags);
            return srcAlpha;
        }

        const channels_type newDstAlpha = unionShapeOpacity(srcAlpha, dstAlpha);
        mixColor<allChannelFlags>(src, dst, scale<float>(srcAlpha) / scale<float>(newDstAlpha), channelFlags, srcColor, dstColor);
        return newDstAlpha;
    }

    /// same alpha handling as KoCompositeOpCopy2
    template<bool alphaLocked, bool allChannelFlags>
    inline channels_type composeCopy(const channels_type *src, channels_type srcAlpha,
                                     channels_type *dst, channels_type dstAlpha,
                                     channels_type opacity,
                                     const QBitArray &channelFlags,
                                     CachedColor &srcColor, CachedColor &dstColor) const
    {
        using namespace Arithmetic;

        if (opacity == zeroValue<channels_type>()) {
            return dstAlpha;
        }

        if (alphaLocked && srcAlpha == zeroValue<channels_type>()) {
            return dstAlpha;
        }

        if (opacity == unitValue<channels_type>()) {
            copyColor<allChannelFlags>(src, dst, channelFlags);
            return srcAlpha;
        }

        const channels_type newDstAlpha = lerp(dstAlpha, srcAlpha, opacity);

        if (newDstAlpha == zeroValue<channels_type>()) {
            return newDstAlpha;
        }

        if (dstAlpha == zeroValue<channels_type>()) {
            copyColor<allChannelFlags>(src, dst, channelFlags);
            return newDstAlpha;
        }

        // the weight Copy2 gives to the source color in its premultiplied lerp
        const float factor = scale<float>(opacity) * scale<float>(srcAlpha) / scale<float>(newDstAlpha);

        mixColor<allChannelFlags>(src, dst, factor, channelFlags, srcColor, dstColor);
        return newDstAlpha;
    }

    template<bool allChannelFlags>
    inline void copyColor(const channels_type *src, channels_type *dst, const QBitArray &channelFlags) const
    {
        for (qint32 i = 0; i < channels_nb; i++) {
            if (i != alpha_pos && (allChannelFlags || channelFlags.testBit(i))) {
                dst[i] = src[i];
            }
        }
    }

    template<bool allChannelFlags>
    inline void mixColor(const channels_type *src, channels_type *dst, float factor,
                         const QBitArray &channelFlags,
                         CachedColor &srcColor, CachedColor &dstColor) const
    {
        using namespace Arithmetic;

        if (factor <= 0.0f) {
            return;
        }

        if (factor >= 1.0f) {
            copyColor<allChannelFlags>(src, dst, channelFlags);
            return;
        }

        if (src[red_pos] == dst[red_pos] && src[green_pos] == dst[green_pos] && src[blue_pos] == dst[blue_pos]) {
            return;
        }

        const float srcEncoded[3] = {scale<float>(src[red_pos]), scale<float>(src[green_pos]), scale<float>(src[blue_pos])};
        const float dstEncoded[3] = {scale<float>(dst[red_pos]), scale<float>(dst[green_pos]), scale<float>(dst[blue_pos])};

        srcColor.update(srcEncoded, m_transfer);
        dstColor.update(dstEncoded, m_transfer);

        float result[3];

        if (srcColor.isMixable && dstColor.isMixable) {
            float mixed[3];
            KoSpectral::mix(dstColor.pigment, dstColor.linear, srcColor.pigment, srcColor.linear, factor, mixed);
            m_transfer.fromLinearSrgb(mixed, result);

            for (int i = 0; i < 3; i++) {
                result[i] = std::clamp(result[i], 0.0f, 1.0f);
            }
        } else {
            for (int i = 0; i < 3; i++) {
                result[i] = dstEncoded[i] + (srcEncoded[i] - dstEncoded[i]) * factor;
            }
        }

        if (allChannelFlags || channelFlags.testBit(red_pos)) {
            dst[red_pos] = scale<channels_type>(result[0]);
        }
        if (allChannelFlags || channelFlags.testBit(green_pos)) {
            dst[green_pos] = scale<channels_type>(result[1]);
        }
        if (allChannelFlags || channelFlags.testBit(blue_pos)) {
            dst[blue_pos] = scale<channels_type>(result[2]);
        }
    }

private:
    KoSpectralColorTransfer m_transfer;
};

#endif // KOCOMPOSITEOPSPECTRAL_H
