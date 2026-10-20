/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSPECTRALPARALLEL_H
#define KOSPECTRALPARALLEL_H

#include <functional>

#include "kritapigment_export.h"

namespace KoSpectralParallel
{

/**
 * Runs \p function(0) ... \p function(count - 1) on the global thread pool
 * and returns when all of them have finished.
 *
 * The calling thread takes part in the work, and only waits for the jobs
 * that other threads have already started. So it never waits for a pool
 * thread to become free, which makes it safe to call from a thread of
 * the pool itself (or from any other context that may already keep all
 * pool threads busy).
 */
KRITAPIGMENT_EXPORT void run(int count, const std::function<void(int)> &function);

/**
 * The number of jobs worth splitting work into, at most \p maxJobs
 */
KRITAPIGMENT_EXPORT int jobCount(int maxJobs);

}

#endif // KOSPECTRALPARALLEL_H
