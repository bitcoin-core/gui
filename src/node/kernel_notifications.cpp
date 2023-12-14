// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/kernel_notifications.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chain.h>
#include <common/args.h>
#include <common/system.h>
#include <kernel/context.h>
#include <logging.h>
#include <node/abort.h>
#include <node/interface_ui.h>
#include <shutdown.h>
#include <util/check.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/translation.h>
#include <warnings.h>

#include <cstdint>
#include <string>
#include <thread>

static void AlertNotify(const std::string& strMessage)
{
    uiInterface.NotifyAlertChanged();
#if HAVE_SYSTEM
    std::string strCmd = gArgs.GetArg("-alertnotify", "");
    if (strCmd.empty()) return;

    // Alert text should be plain ascii coming from a trusted source, but to
    // be safe we first strip anything not in safeChars, then add single quotes around
    // the whole string before passing it to the shell:
    std::string singleQuote("'");
    std::string safeStatus = SanitizeString(strMessage);
    safeStatus = singleQuote+safeStatus+singleQuote;
    ReplaceAll(strCmd, "%s", safeStatus);

    std::thread t(runCommand, strCmd);
    t.detach(); // thread runs free
#endif
}

static void DoWarning(const bilingual_str& warning)
{
    static bool fWarned = false;
    SetMiscWarning(warning);
    if (!fWarned) {
        AlertNotify(warning.original);
        fWarned = true;
    }
}

namespace node {

kernel::InterruptResult KernelNotifications::blockTip(SynchronizationState state, CBlockIndex& index)
{
    uiInterface.NotifyBlockTip(state, &index);
    if (m_stop_at_height && index.nHeight >= m_stop_at_height) {
        StartShutdown();
        return kernel::Interrupted{};
    }
    return {};
}

void KernelNotifications::headerTip(SynchronizationState state, int64_t height, int64_t timestamp, bool presync)
{
    uiInterface.NotifyHeaderTip(state, height, timestamp, presync);
}

void KernelNotifications::progress(const bilingual_str& title, int progress_percent, bool resume_possible)
{
    uiInterface.ShowProgress(title.translated, progress_percent, resume_possible);
}

void KernelNotifications::warning(const bilingual_str& warning)
{
    DoWarning(warning);
}

void KernelNotifications::flushError(const std::string& debug_message)
{
    AbortNode(m_exit_status, debug_message);
}

void KernelNotifications::fatalError(const std::string& debug_message, const bilingual_str& user_message)
{
    node::AbortNode(m_exit_status, debug_message, user_message, m_shutdown_on_fatal_error);
}

void ReadNotificationArgs(const ArgsManager& args, KernelNotifications& notifications)
{
    if (auto value{args.GetIntArg("-stopatheight")}) notifications.m_stop_at_height = *value;
}

} // namespace node
