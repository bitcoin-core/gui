// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QVALIDATEDLINEEDIT_H
#define BITCOIN_QT_QVALIDATEDLINEEDIT_H

#include <QLineEdit>

class ErrorLocator;

/** Line edit that can be marked as "invalid" to show input validation feedback. When marked as invalid,
   it will get a red background until it is focused.
 */
class QValidatedLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit QValidatedLineEdit(QWidget *parent);
    ~QValidatedLineEdit();
    void clear();
    void setCheckValidator(const QValidator *v);
    void setErrorLocator(const ErrorLocator *e);
    bool isValid();

protected:
    void focusInEvent(QFocusEvent *evt) override;
    void focusOutEvent(QFocusEvent *evt) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    bool valid;
    const QValidator *checkValidator;
    const ErrorLocator *errorLocator;

public Q_SLOTS:
    void setValid(bool valid);
    void setEnabled(bool enabled);

Q_SIGNALS:
    void validationDidChange(QValidatedLineEdit *validatedLineEdit);

private Q_SLOTS:
    void markValid();
    void checkValidity();
    void locateErrors();
};

#endif // BITCOIN_QT_QVALIDATEDLINEEDIT_H
