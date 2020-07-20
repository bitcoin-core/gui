// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guifileutil.h>

#include <chainparams.h>
#include <util/system.h>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif // WIN32

#ifdef Q_OS_LINUX
#include <cassert>
#include <stdlib.h>
#include <unistd.h>
#endif // Q_OS_LINUX

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QRegExp>
#include <QStandardPaths>
#include <QString>
#ifdef Q_OS_MAC
#include <QProcess>
#endif // Q_OS_MAC

namespace {
#ifdef Q_OS_LINUX
std::string GetExecutablePathAsString()
{
    char exe_path[MAX_PATH + 1];
    ssize_t r = readlink("/proc/self/exe", exe_path, MAX_PATH);
    if (r == -1 || r == sizeof(exe_path)) {
        r = 0;
    }
    exe_path[r] = '\0';
    return exe_path;
}

fs::path GetUserApplicationsDir()
{
    const char* home_dir = getenv("HOME");
    if (!home_dir) return fs::path();
    return fs::path(home_dir) / ".local" / "share" / "applications";
}

fs::path GetDesktopFilePath(std::string chain)
{
    const auto app_dir = GetUserApplicationsDir();
    if (app_dir.empty()) return fs::path();
    if (!chain.empty()) {
        chain = "_" + chain;
    }
    return app_dir / strprintf("org.bitcoincore.BitcoinQt%s.desktop", chain);
}

fs::path GetUserIconsDir()
{
    const char* home_dir = getenv("HOME");
    if (!home_dir) return fs::path();
    return fs::path(home_dir) / ".local" / "share" / "icons";
}

fs::path GetIconPath(std::string chain)
{
    const auto icons_dir = GetUserIconsDir();
    if (icons_dir.empty()) return fs::path();
    if (!chain.empty()) {
        chain = "-" + chain;
    }
    return icons_dir / strprintf("bitcoin%s.png", chain);
}
#endif // Q_OS_LINUX
} // namespace

namespace GUIUtil {

#ifdef Q_OS_LINUX
bool IntegrateWithDesktopEnvironment(QIcon icon)
{
    std::string chain = gArgs.GetChainName();
    assert(chain == CBaseChainParams::MAIN || chain == CBaseChainParams::TESTNET);
    if (chain == CBaseChainParams::MAIN) {
        chain.clear();
    }

    const auto icon_path = GetIconPath(chain);
    if (icon_path.empty() || !icon.pixmap(256).save(boostPathToQString(icon_path))) return false;
    const auto exe_path = GetExecutablePathAsString();
    if (exe_path.empty()) return false;
    const auto desktop_file_path = GetDesktopFilePath(chain);
    if (desktop_file_path.empty()) return false;
    fsbridge::ofstream desktop_file(desktop_file_path, std::ios_base::out | std::ios_base::trunc);
    if (!desktop_file.good()) return false;

    desktop_file << "[Desktop Entry]\n";
    desktop_file << "Type=Application\n";
    desktop_file << "Version=1.1\n";
    desktop_file << "GenericName=Bitcoin client\n";
    desktop_file << "Comment=Bitcoin full node and wallet\n";
    desktop_file << strprintf("Icon=%s\n", icon_path.stem().string());
    desktop_file << strprintf("TryExec=%s\n", exe_path);
    desktop_file << "Categories=Network;Office;Finance;\n";
    if (chain.empty()) {
        desktop_file << "Name=" PACKAGE_NAME "\n";
        desktop_file << strprintf("Exec=%s %%u\n", exe_path);
        desktop_file << "Actions=Testnet;\n";
        desktop_file << "[Desktop Action Testnet]\n";
        desktop_file << strprintf("Exec=%s -testnet\n", exe_path);
        desktop_file << "Name=Testnet mode\n";
    } else {
        desktop_file << "Name=" PACKAGE_NAME " - Testnet\n";
        desktop_file << strprintf("Exec=%s -testnet %%u\n", exe_path);
    }

    desktop_file.close();
    return true;
}
#endif // Q_OS_LINUX

QString getDefaultDataDirectory()
{
    return boostPathToQString(GetDefaultDataDir());
}

QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter));

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(parent, caption, myDir, filter, &selectedFilter));

    if(selectedSuffixOut)
    {
        /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
        QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
        QString selectedSuffix;
        if(filter_re.exactMatch(selectedFilter))
        {
            selectedSuffix = filter_re.cap(1);
        }
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

void openDebugLogfile()
{
    fs::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(pathDebug)));
}

bool openBitcoinConf()
{
    fs::path pathConfig = GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));

    /* Create the file */
    fsbridge::ofstream configFile(pathConfig, std::ios_base::app);

    if (!configFile.good())
        return false;

    configFile.close();

    /* Open bitcoin.conf with the associated application */
    bool res = QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(pathConfig)));
#ifdef Q_OS_MAC
    // Workaround for macOS-specific behavior; see #15409.
    if (!res) {
        res = QProcess::startDetached("/usr/bin/open", QStringList{"-t", boostPathToQString(pathConfig)});
    }
#endif

    return res;
}

#ifdef WIN32
fs::path static StartupShortcutPath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Bitcoin.lnk";
    if (chain == CBaseChainParams::TESTNET) // Remove this special case when CBaseChainParams::TESTNET = "testnet4"
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Bitcoin (testnet).lnk";
    return GetSpecialFolderPath(CSIDL_STARTUP) / strprintf("Bitcoin (%s).lnk", chain);
}

bool GetStartOnSystemStartup()
{
    // check for Bitcoin*.lnk
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(nullptr);

        // Get a pointer to the IShellLink interface.
        IShellLinkW* psl = nullptr;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLinkW,
            reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            WCHAR pszExePath[MAX_PATH];
            GetModuleFileNameW(nullptr, pszExePath, ARRAYSIZE(pszExePath));

            // Start client minimized
            QString strArgs = "-min";
            // Set -testnet /-regtest options
            strArgs += QString::fromStdString(strprintf(" -chain=%s", gArgs.GetChainName()));

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpecW(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(strArgs.toStdWString().c_str());

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = nullptr;
            hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(StartupShortcutPath().wstring().c_str(), TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}
#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
// http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

fs::path static GetAutostartDir()
{
    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

fs::path static GetAutostartFilePath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetAutostartDir() / "bitcoin.desktop";
    return GetAutostartDir() / strprintf("bitcoin-%s.desktop", chain);
}

bool GetStartOnSystemStartup()
{
    fsbridge::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        fs::remove(GetAutostartFilePath());
    else
    {
        const std::string exe_path = GetExecutablePathAsString();
        if (exe_path.empty()) {
            return false;
        }

        fs::create_directories(GetAutostartDir());

        fsbridge::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out | std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        std::string chain = gArgs.GetChainName();
        // Write a bitcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        if (chain == CBaseChainParams::MAIN)
            optionFile << "Name=Bitcoin\n";
        else
            optionFile << strprintf("Name=Bitcoin (%s)\n", chain);
        optionFile << strprintf("Exec=%s -min -chain=%s\n", exe_path, chain);
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}

#else

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

fs::path qstringToBoostPath(const QString &path)
{
    return fs::path(path.toStdString());
}

QString boostPathToQString(const fs::path &path)
{
    return QString::fromStdString(path.string());
}

} // namespace GUIUtil
