// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIFILEUTIL_H
#define BITCOIN_QT_GUIFILEUTIL_H

#include <fs.h>

#include <QIcon>
#include <QString>
#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

/** Filesystem utility functions used by the GUI.
 */
namespace GUIUtil {

#ifdef Q_OS_LINUX
bool IntegrateWithDesktopEnvironment(QIcon icon);
#endif // Q_OS_LINUX

/**
 * Determine default data directory for operating system.
 */
QString getDefaultDataDirectory();

/** Get save filename, mimics QFileDialog::getSaveFileName, except that it appends a default suffix
    when no suffix is provided by the user.

  @param[in] parent  Parent window (or 0)
  @param[in] caption Window caption (or empty, for default)
  @param[in] dir     Starting directory (or empty, to default to documents directory)
  @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
  @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that was selected (or 0).
              Can be useful when choosing the save file format based on suffix.
 */
QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut);

/** Get open filename, convenience wrapper for QFileDialog::getOpenFileName.

  @param[in] parent  Parent window (or 0)
  @param[in] caption Window caption (or empty, for default)
  @param[in] dir     Starting directory (or empty, to default to documents directory)
  @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
  @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that was selected (or 0).
              Can be useful when choosing the save file format based on suffix.
 */
QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut);

// Open debug.log
void openDebugLogfile();

// Open the config file
bool openBitcoinConf();

bool GetStartOnSystemStartup();
bool SetStartOnSystemStartup(bool fAutoStart);

/* Convert QString to OS specific boost path through UTF-8 */
fs::path qstringToBoostPath(const QString &path);

/* Convert OS specific boost path to QString through UTF-8 */
QString boostPathToQString(const fs::path &path);

} // namespace GUIUtil

#endif // BITCOIN_QT_GUIFILEUTIL_H
