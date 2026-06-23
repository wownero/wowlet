// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_CHILDPROCESSGUARD_H
#define WOWLET_CHILDPROCESSGUARD_H

#include <QProcess>

// wowlet: OS-level "die with parent" for spawned child processes (embedded wownerod, bundled tor).
//
// QProcess does NOT kill its child when the parent exits or crashes; it only detaches. Feather's
// TorManager/DaemonManager relied solely on an explicit stop() in the normal-quit path, so an
// unhandled crash (e.g. a 0xc0000005 access violation) orphaned the children. This binds the
// child's lifetime to wowlet's at the kernel level, which survives a hard crash where no C++
// destructor and no Qt teardown runs.
//
//   Windows: a single process-wide Job Object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE. Assign each
//            child after start(); when wowlet's process dies the job's last handle closes and the
//            kernel terminates every process still in the job.
//   Unix:    prctl(PR_SET_PDEATHSIG, SIGKILL) installed in the child (via setChildProcessModifier),
//            so the kernel SIGKILLs the child when the parent dies.
namespace ChildProcessGuard
{
    // Unix: arrange for `process` to receive SIGKILL when wowlet dies. Call BEFORE process->start().
    // No-op on Windows (the Job Object handles it post-start).
    void installPreStart(QProcess *process);

    // Windows: assign the (now-running) child to wowlet's kill-on-close job. Call right AFTER start()
    // returns and the child has a PID. No-op on Unix.
    void adoptAfterStart(QProcess *process);
}

#endif //WOWLET_CHILDPROCESSGUARD_H
