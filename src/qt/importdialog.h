#ifndef BITCOIN_QT_IMPORTDIALOG_H
#define BITCOIN_QT_IMPORTDIALOG_H

#include <QDialog>
#include <qt/importentry.h>

class WalletModel;

namespace Ui {
    class ImportDialog;
}

class ImportDialog : public QDialog
{
    Q_OBJECT

public:
    enum Page {
        importPubkey,
        importPrivkey,
        importAddress,
        importMulti,
        importDescriptors
    };

    explicit ImportDialog(Page _page, WalletModel *model = nullptr, QWidget *parent = nullptr);
    ~ImportDialog();

public Q_SLOTS:
    void accept() override;
private Q_SLOTS:
    ImportEntry *addEntry();
    void removeEntry(ImportEntry* entry);

private:
    Ui::ImportDialog *ui;
    WalletModel *walletModel;
    Page page;
    int64_t GetImportTimestamp(const int64_t &entry, int64_t now);
};

#endif // BITCOIN_QT_IMPORTDIALOG_H
