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
#include "KoSpectralParallel.h"

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

    /// returns true if the color changed
    bool update(const float newEncoded[3], const KoSpectralColorTransfer &transfer)
    {
        if (std::equal(newEncoded, newEncoded + 3, encoded)) {
            return false;
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

        return true;
    }
};

/**
 * The colors of the current source and destination pixel and the results
 * of mixing them. In flat areas the same two colors get mixed over and over,
 * with only a few distinct factors (a brush mask has 256 levels at most), so
 * the results are cached per factor until one of the colors changes.
 */
struct MixCache {
    CachedColor src;
    CachedColor dst;

    struct Entry {
        quint32 generation {0};
        float factor {0.0f};
        float result[3];
    };

    static constexpr int SIZE = 256;
    Entry entries[SIZE];
    quint32 generation {1}; ///< entries of older generations are stale

    Entry &entryFor(float factor)
    {
        quint32 bits;
        std::memcpy(&bits, &factor, sizeof(bits));
        return entries[(bits ^ (bits >> 7) ^ (bits >> 15)) & (SIZE - 1)];
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
    typedef KoSpectralCompositeOpPrivate::MixCache MixCache;

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

    /**
     * Spectral mixing is much slower than the other blending modes, so large
     * areas (big brush dabs, the color smudge buffers) are split between
     * threads. Smaller areas, like the 64x64 tiles of the projection, are
     * done directly.
     */
    void composite(const KoCompositeOp::ParameterInfo &params) const override
    {
        static constexpr qint64 minPixelsPerJob = 1024;

        const qint64 pixels = qint64(params.rows) * params.cols;
        const int jobs = KoSpectralParallel::jobCount(int(std::min<qint64>(pixels / minPixelsPerJob, 64)));

        if (jobs <= 1) {
            compositeRange(params);
            return;
        }

        // split into bands of rows, or of columns for single row calls
        // (the smudge engine composites whole buffers as one long row)
        const bool splitRows = params.rows > 1;
        const int length = splitRows ? params.rows : params.cols;
        const int parts = std::min(jobs * 4, length);

        KoSpectralParallel::run(parts, [&](int part) {
            const int begin = qint64(length) * part / parts;
            const int end = qint64(length) * (part + 1) / parts;

            KoCompositeOp::ParameterInfo range(params);

            if (splitRows) {
                range.rows = end - begin;
                range.dstRowStart += qint64(begin) * params.dstRowStride;
                range.srcRowStart += qint64(begin) * params.srcRowStride;
                if (range.maskRowStart) {
                    range.maskRowStart += qint64(begin) * params.maskRowStride;
                }
            } else {
                range.cols = end - begin;
                range.dstRowStart += qint64(begin) * pixel_size;
                if (params.srcRowStride != 0) {
                    range.srcRowStart += qint64(begin) * pixel_size;
                }
                if (range.maskRowStart) {
                    range.maskRowStart += begin;
                }
            }

            compositeRange(range);
        });
    }

private:
    void compositeRange(const KoCompositeOp::ParameterInfo &params) const
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

        MixCache cache;

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
                                                                            params.channelFlags, cache);
                } else {
                    newDstAlpha = composeOver<alphaLocked, allChannelFlags>(src, mul(srcAlpha, maskAlpha, opacity), dst, dstAlpha,
                                                                            params.channelFlags, cache);
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
                                     MixCache &cache) const
    {
        using namespace Arithmetic;

        if (srcAlpha == zeroValue<channels_type>()) {
            return dstAlpha;
        }

        if (alphaLocked || dstAlpha == unitValue<channels_type>()) {
            mixColor<allChannelFlags>(src, dst, scale<float>(srcAlpha), channelFlags, cache);
            return dstAlpha;
        }

        if (dstAlpha == zeroValue<channels_type>()) {
            // the color of the destination is undefined, don't mix with it
            copyColor<allChannelFlags>(src, dst, channelFlags);
            return srcAlpha;
        }

        const channels_type newDstAlpha = unionShapeOpacity(srcAlpha, dstAlpha);
        mixColor<allChannelFlags>(src, dst, scale<float>(srcAlpha) / scale<float>(newDstAlpha), channelFlags, cache);
        return newDstAlpha;
    }

    /// same alpha handling as KoCompositeOpCopy2
    template<bool alphaLocked, bool allChannelFlags>
    inline channels_type composeCopy(const channels_type *src, channels_type srcAlpha,
                                     channels_type *dst, channels_type dstAlpha,
                                     channels_type opacity,
                                     const QBitArray &channelFlags,
                                     MixCache &cache) const
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

        mixColor<allChannelFlags>(src, dst, factor, channelFlags, cache);
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
                         MixCache &cache) const
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

        const bool srcChanged = cache.src.update(srcEncoded, m_transfer);
        const bool dstChanged = cache.dst.update(dstEncoded, m_transfer);

        if (srcChanged || dstChanged) {
            cache.generation++;
        }

        float result[3];

        if (cache.src.isMixable && cache.dst.isMixable) {
            MixCache::Entry &entry = cache.entryFor(factor);

            if (entry.generation == cache.generation && entry.factor == factor) {
                std::copy(entry.result, entry.result + 3, result);
            } else {
                float mixed[3];
                KoSpectral::mix(cache.dst.pigment, cache.dst.linear, cache.src.pigment, cache.src.linear, factor, mixed);
                m_transfer.fromLinearSrgb(mixed, result);

                for (int i = 0; i < 3; i++) {
                    result[i] = std::clamp(result[i], 0.0f, 1.0f);
                }

                entry.generation = cache.generation;
                entry.factor = factor;
                std::copy(result, result + 3, entry.result);
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
