/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSPECTRALCOMPOSITEOPS_H
#define KOSPECTRALCOMPOSITEOPS_H

#include <KoColorSpace.h>
#include <KoCompositeOpRegistry.h>

#include "KoCompositeOpSpectral.h"

/**
 * Adds the spectral (Kubelka-Munk) composite ops to an RGB color space.
 * Called from addStandardCompositeOps() for RGB traits only, so other
 * color models simply don't offer the "Spectral" blending mode.
 */
template<class Traits>
void addSpectralCompositeOps(KoColorSpace *cs)
{
    cs->addCompositeOp(new KoCompositeOpSpectral<Traits, false>(cs, COMPOSITE_SPECTRAL, KoCompositeOp::categoryMix()));
    cs->addCompositeOp(new KoCompositeOpSpectral<Traits, true>(cs, COMPOSITE_COPY_SPECTRAL, KoCompositeOp::categoryMisc()));
}

#endif // KOSPECTRALCOMPOSITEOPS_H
