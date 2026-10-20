/*
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSpectralParallel.h"

#include <QAtomicInt>
#include <QMutex>
#include <QRunnable>
#include <QSharedPointer>
#include <QThread>
#include <QThreadPool>
#include <QWaitCondition>

#include <algorithm>

namespace
{

/**
 * Shared between the caller and the helper runnables. Helpers may start
 * after the caller has returned; they then find no job left and only touch
 * this (reference counted) state, never the caller's function.
 */
struct State {
    int count {0};
    const std::function<void(int)> *function {nullptr};

    QAtomicInt next {0};

    QMutex mutex;
    QWaitCondition finished;
    int done {0};

    /// runs jobs until there are none left
    void work()
    {
        int finishedHere = 0;

        for (int job = next.fetchAndAddOrdered(1); job < count; job = next.fetchAndAddOrdered(1)) {
            (*function)(job);
            finishedHere++;
        }

        if (finishedHere > 0) {
            QMutexLocker locker(&mutex);
            done += finishedHere;
            if (done == count) {
                finished.wakeAll();
            }
        }
    }
};

class Helper : public QRunnable
{
public:
    Helper(QSharedPointer<State> state)
        : m_state(state)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        m_state->work();
    }

private:
    QSharedPointer<State> m_state;
};

}

namespace KoSpectralParallel
{

int jobCount(int maxJobs)
{
    return std::max(1, std::min(maxJobs, QThread::idealThreadCount()));
}

void run(int count, const std::function<void(int)> &function)
{
    if (count <= 0) {
        return;
    }

    if (count == 1) {
        function(0);
        return;
    }

    QSharedPointer<State> state(new State);
    state->count = count;
    state->function = &function;

    // the caller is one of the workers
    const int helpers = std::min(count, QThread::idealThreadCount()) - 1;
    for (int i = 0; i < helpers; i++) {
        QThreadPool::globalInstance()->start(new Helper(state));
    }

    state->work();

    // all jobs are taken now; wait only for the ones that are running
    QMutexLocker locker(&state->mutex);
    while (state->done < state->count) {
        state->finished.wait(&state->mutex);
    }
}

}
