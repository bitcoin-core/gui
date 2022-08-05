// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_IMPORTENTRY_H
#define BITCOIN_QT_IMPORTENTRY_H

#include <QStackedWidget>
#include <QValidator>
#include <QRadioButton>
#include <wallet/imports.h>

class WalletModel;

namespace interfaces {
class Node;
} // namespace interfaces

namespace Ui {
    class ImportEntry;
}

class ImportEntry : public QStackedWidget
{
    Q_OBJECT

public:
    enum EntryPage {
        importMultiEntry,
        importDescriptorsEntry
    };
    explicit ImportEntry(EntryPage _entryPage, QWidget *parent = nullptr);
    ~ImportEntry();

    void setModel(WalletModel *model);

    QSize sizeHint() const override
    {
        return currentWidget()->sizeHint();
    }

    QSize minimumSizeHint() const override
    {
        return currentWidget()->minimumSizeHint();
    }

    wallet::ImportDescriptorData getDescriptorData();
    wallet::ImportMultiData getMultiData();
    std::string getDesc();

Q_SIGNALS:
    void removeEntry(ImportEntry* entry);

private Q_SLOTS:
    void deleteEntryClicked();
    void usehiddenButtonClicked();
    void useHideScriptsButtonClicked();
    void hideLabel();
    void hideLabelDesc();
    void changeImportDialog();

private:
    Ui::ImportEntry *ui;
    WalletModel* model{nullptr};
    bool hiddenButtonState = false;
    bool hideScriptsButtonState = false;
    EntryPage entryPage;
};

class Int64_tValidator : public QValidator
{
    Q_OBJECT

public:
    Int64_tValidator(int64_t bottom, int64_t top, QObject *parent = nullptr) : QValidator(parent), b(bottom), t(top) {};

    // doesn't handle -/+ so can only work with bottom of 0 to top int64_t max
    QValidator::State validate(QString &input, int &) const override {
        if (input.isEmpty()) return Intermediate;
        bool ok;
        int64_t num = input.toLongLong(&ok, 10);
        if (!ok) return QValidator::Invalid;
        if (num < b) return QValidator::Intermediate;
        if (num > t) return QValidator::Invalid;
        return QValidator::Acceptable;
    }

    void setBottom(int64_t bottom) {
        setRange(bottom, top());
    }

    void setTop(int64_t top) {
        setRange(bottom(), top);
    }

    void setRange(int64_t bottom, int64_t top) {
        bool rangeChanged = false;
        if (b != bottom) {
            b = bottom;
            rangeChanged = true;
        }
        if (t != top) {
            t = top;
            rangeChanged = true;
        }
        if (rangeChanged)
            changed();
    }

    int64_t bottom() const { return b; }
    int64_t top() const { return t; }

private:
    int64_t b;
    int64_t t;
};

#endif // BITCOIN_QT_IMPORTENTRY_H
