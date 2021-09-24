#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include "DownloadTask.h"
#include "Extractor.h"

class Extractor;
class AbstractVideoDownloadTask;
class ContentTreeWidget;
class ElidedTextLabel;

class QBoxLayout;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QAbstractButton;
class QToolButton;

class DownloadDialog : public QDialog
{
    Q_OBJECT

public:
    DownloadDialog(const QString &url, QWidget *parent);
    ~DownloadDialog();
    QList<AbstractDownloadTask*> getDownloadTasks();
    void open() override;
    void show() = delete;
private:
    using QDialog::exec;

private slots:
    void abortExtract();
    void extratorErrorOccured(const QString &errorString);
    void setupUi();

    void selAllBtnClicked();
    void selCntChanged(int curr, int prev);
    void startGetCurrentItemQnList();
    void getCurrentItemQnListFinished();
    void selectPath();

private:
    void updateQnComboBox(QnList qnList);
    void addPlayDirect(QBoxLayout *layout);

    QDialog *activityTipDialog;
    Extractor *extractor;

    QLabel *titleLabel;

    QToolButton *selAllBtn = nullptr;
    QLabel *selCountLabel = nullptr;
    ContentTreeWidget *tree = nullptr;
    QComboBox *qnComboBox = nullptr;
    QPushButton *getQnListBtn = nullptr;

    ElidedTextLabel *pathLabel;
    QPushButton *selPathButton;
    QPushButton* okButton;

    ContentType contentType;
    qint64 contentId;
    qint64 contentItemId;
    QNetworkReply *getQnListReply;
};


class ContentTreeItem; // definition: DonwloadDialog.cpp

class ContentTreeWidget: public QTreeWidget
{
    Q_OBJECT

    static const int RowSpacing;
    int enabledItemCnt = 0;
    int selCnt = 0;
    int selCntBeforeBlock;
    bool blockSigSelCntChanged = false;

signals:
    void selectedItemCntChanged(int currentCount, int previousCount);

public:
    ContentTreeWidget(const Extractor::Result *content, QWidget *parent = nullptr);

    ContentTreeItem *currentItem() const;

    // QTreeWidget::selectAll() is not satisfactory. see comment in definition for detail.
    void selectAll() override;
    int getEnabledItemCount() const { return enabledItemCnt; }
    int getSelectedItemCount() const { return selCnt; }

    // used in selectAll() and ContentTreeItem
    //      to emit selectedItemCntChanged() only once when select or deselect multiple items
    void beginSelMultItems();
    void endSelMultItems();

private:
    using QTreeWidget::addTopLevelItem;
    using QTreeWidget::addTopLevelItems;
    using QTreeWidget::insertTopLevelItem;
    using QTreeWidget::insertTopLevelItems;


private:
    QList<QTreeWidgetItem*> selection2Items(const QItemSelection &selection);

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
};


#endif // DOWNLOADDIALOG_H
