// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "utils/ChildProcessGuard.h"

#include <QDebug>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <csignal>
#if defined(Q_OS_LINUX)
#include <sys/prctl.h>
#include <unistd.h>
#endif
#endif

namespace ChildProcessGuard
{

#if defined(Q_OS_WIN)

// One process-wide job for all wowlet children. Created lazily on first use; the HANDLE is
// intentionally never closed — it must stay open for wowlet's entire lifetime so the job persists.
// On process death (clean OR crash) the kernel closes it, fires KILL_ON_JOB_CLOSE, and reaps the kids.
static HANDLE g_job = nullptr;

static HANDLE jobHandle()
{
    if (g_job)
        return g_job;

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) {
        qWarning() << "ChildProcessGuard: CreateJobObject failed:" << GetLastError();
        return nullptr;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        qWarning() << "ChildProcessGuard: SetInformationJobObject failed:" << GetLastError();
        CloseHandle(job);
        return nullptr;
    }

    g_job = job;
    return g_job;
}

void installPreStart(QProcess *) { /* no-op on Windows: assignment happens post-start */ }

void adoptAfterStart(QProcess *process)
{
    HANDLE job = jobHandle();
    if (!job)
        return;

    // QProcess::start() is asynchronous: right after it returns the child often has no PID yet. Wait
    // (bounded) for it to actually start, else the assign is silently skipped and the child survives a
    // parent crash (orphaned) — this is exactly what stranded wownerod in testing. Returns the instant
    // the process is created, so the UI-thread cost is tiny.
    if (process->state() == QProcess::Starting)
        process->waitForStarted(5000);

    const qint64 pid = process->processId();   // 0 if it still isn't running
    if (pid == 0) {
        qWarning() << "ChildProcessGuard: child has no PID; not assigned to job";
        return;
    }

    // AssignProcessToJobObject needs PROCESS_SET_QUOTA | PROCESS_TERMINATE. QProcess does not expose
    // the child HANDLE, so re-open it by PID.
    HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                              static_cast<DWORD>(pid));
    if (!proc) {
        qWarning() << "ChildProcessGuard: OpenProcess failed for pid" << pid << ":" << GetLastError();
        return;
    }

    if (!AssignProcessToJobObject(job, proc))
        qWarning() << "ChildProcessGuard: AssignProcessToJobObject failed:" << GetLastError();
    else
        qInfo() << "ChildProcessGuard: bound child pid" << pid << "to kill-on-close job";

    CloseHandle(proc);   // closing this per-child handle does NOT remove the process from the job
}

#elif defined(Q_OS_UNIX)

void installPreStart(QProcess *process)
{
#if defined(Q_OS_LINUX)
    // Runs in the forked child between fork() and exec(). Keep it async-signal-safe.
    process->setChildProcessModifier([] {
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
        // Fork-race guard: if the parent already died before prctl ran, self-terminate.
        if (::getppid() == 1)
            ::raise(SIGKILL);
    });
#else
    Q_UNUSED(process)   // macOS/BSD: no PR_SET_PDEATHSIG; rely on aboutToQuit + destructor stop()
#endif
}

void adoptAfterStart(QProcess *) { /* no-op on Unix: handled pre-start */ }

#else

void installPreStart(QProcess *) {}
void adoptAfterStart(QProcess *) {}

#endif

} // namespace ChildProcessGuard
