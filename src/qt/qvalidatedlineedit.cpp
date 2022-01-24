// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/qvalidatedlineedit.h>

#include <qt/bitcoinaddressvalidator.h>
#include <qt/guiconstants.h>

#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>

QValidatedLineEdit::QValidatedLineEdit(QWidget *parent) :
    QLineEdit(parent),
    valid(true),
    checkValidator(nullptr),
    errorLocator(nullptr)
{
    connect(this, &QValidatedLineEdit::textChanged, this, &QValidatedLineEdit::markValid);
}

void QValidatedLineEdit::setValid(bool _valid)
{
    if(_valid == this->valid)
    {
        return;
    }

    if(_valid)
    {
        setStyleSheet("");
    }
    else
    {
        setStyleSheet(STYLE_INVALID);
    }
    this->valid = _valid;
}

void QValidatedLineEdit::focusInEvent(QFocusEvent *evt)
{
    // Clear invalid flag on focus
    setValid(true);

    QLineEdit::focusInEvent(evt);
}

void QValidatedLineEdit::focusOutEvent(QFocusEvent *evt)
{
    checkValidity();

    QLineEdit::focusOutEvent(evt);
}

void QValidatedLineEdit::markValid()
{
    // As long as a user is typing ensure we display state as valid
    setValid(true);
}

void QValidatedLineEdit::clear()
{
    setValid(true);
    QLineEdit::clear();
}

void QValidatedLineEdit::setEnabled(bool enabled)
{
    if (!enabled)
    {
        // A disabled QValidatedLineEdit should be marked valid
        setValid(true);
    }
    else
    {
        // Recheck validity when QValidatedLineEdit gets enabled
        checkValidity();
    }

    QLineEdit::setEnabled(enabled);
}

void QValidatedLineEdit::checkValidity()
{
    if (text().isEmpty())
    {
        setValid(true);
    }
    else if (hasAcceptableInput())
    {
        setValid(true);

        // Check contents on focus out
        if (checkValidator)
        {
            QString address = text();
            int pos = 0;
            if (checkValidator->validate(address, pos) == QValidator::Acceptable)
                setValid(true);
            else
                setValid(false);
        }
    }
    else
        setValid(false);

    Q_EMIT validationDidChange(this);
}

void QValidatedLineEdit::setCheckValidator(const QValidator *v)
{
    checkValidator = v;
}

void QValidatedLineEdit::setErrorLocator(const ErrorLocator *e)
{
    errorLocator = e;
}

bool QValidatedLineEdit::isValid()
{
    // use checkValidator in case the QValidatedLineEdit is disabled
    if (checkValidator)
    {
        QString address = text();
        int pos = 0;
        if (checkValidator->validate(address, pos) == QValidator::Acceptable)
            return true;
    }

    return valid;
}

void QValidatedLineEdit::locateErrors()
{
    if (!errorLocator) return;

    // Perform error location on the input
    QString input = text();
    std::vector<int> error_locations{};
    std::string error_message;
    errorLocator->locateErrors(input, error_message, &error_locations);

    if (error_message.empty()) {
        QMessageBox::information(this, "Error locator", "No errors detected.", QMessageBox::Ok);
    } else {
        QString error_text = "Error: " + QString::fromStdString(error_message) + "<br />";
        if (!error_locations.empty()) {
            error_text += "<pre>";
            int prev_loc = 0;
            for (int loc : error_locations) {
                error_text += input.mid(prev_loc, loc - prev_loc);
                error_text +=  QString("<FONT COLOR='#ff0000'>%1</FONT>").arg(input[loc]);
                prev_loc = loc + 1;
            }
            if (prev_loc < input.size()) {
                error_text += input.right(input.size() - prev_loc);
            }
            error_text += "</pre>";
        }
        QMessageBox msg = QMessageBox(QMessageBox::Information, "Error locator", error_text, QMessageBox::Ok, parentWidget());
        msg.exec();
    }
}

void QValidatedLineEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *contextMenu = createStandardContextMenu();
    if (errorLocator) {
        contextMenu->addAction(tr("Attempt error location"), this, &QValidatedLineEdit::locateErrors);
    }
    contextMenu->exec(event->globalPos());
    delete contextMenu;
}

QValidatedLineEdit::~QValidatedLineEdit()
{
    if (errorLocator) {
        delete errorLocator;
    }
}
