#ifndef BITCOIN_QT_IMPORTDIALOG_H
#define BITCOIN_QT_IMPORTDIALOG_H

#include <QDialog>

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
        importAddress
    };

    explicit ImportDialog(Page _page, WalletModel *model = nullptr, QWidget *parent = nullptr);
    ~ImportDialog();

public Q_SLOTS:
    void accept() override;

private:
    Ui::ImportDialog *ui;
    WalletModel *walletModel;
    Page page;
};

#endif // BITCOIN_QT_IMPORTDIALOG_H
