#ifndef TASKTABLE_H
#define TASKTABLE_H

#include <QTableWidget>

class QTimer;
class QLabel;
class QMenu;
class QProgressBar;
class QToolButton;
class QPushButton;
class QStackedWidget;

class AbstractDownloadTask;
class ElidedTextLabel;
class TaskCellWidget;

class TaskTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit TaskTableWidget(QWidget *parent = nullptr);

    void save();
    void load();
    void addTasks(const QList<AbstractDownloadTask*> &, bool activate = true);

    void stopAll();
    void startAll();
    void removeAll();

private slots:
    void onCellTaskStopped();
    void onCellTaskFinished();
    void onCellStartBtnClicked();
    void onCellRemoveBtnClicked();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    TaskCellWidget *cellWidget(int row) const;
    int rowOfCell(TaskCellWidget *cell) const;

    QAction *startAllAct;
    QAction *stopAllAct;
    QAction *removeAllAct;

    bool dirty = false;
    QTimer *saveTasksTimer;
    void setDirty();

    int activeTaskCnt = 0;
    void activateWaitingTasks();
};



class TaskCellWidget : public QWidget
{
    Q_OBJECT

public:
    enum State { Stopped, Waiting, Downloading, Finished };

private:
    State state = Stopped;
    AbstractDownloadTask *task = nullptr;

public:
    TaskCellWidget(AbstractDownloadTask *task, QWidget *parent = nullptr);
    ~TaskCellWidget();
    static int cellHeight();

    const AbstractDownloadTask *getTask() const { return task; }
    State getState() const { return state; }
    void setState(State);

    void setWaitState();
    void startDownload();
    void stopDownload(); // not emit stopped signal
    void remove();

signals:
    // state changed from Downloading to Stopped (caused by error or stop button)
    void downloadStopped();
    void downloadFinished();
    void startBtnClicked();
    void removeBtnClicked();

private slots:
    void onErrorOccurred(const QString &errStr);
    void onFinished();
    void open();

private:
    QPushButton *iconButton;
    ElidedTextLabel *titleLabel;
    QLabel *qnDescLabel;
    QLabel *progressLabel;

    QProgressBar *progressBar;

    QLabel *downRateLabel;
    QLabel *timeLeftLabel;
    ElidedTextLabel *statusTextLabel;
    QWidget *downloadStatsWidget;
    QStackedWidget *statusStackedWidget;

    QPushButton *startStopButton;
    QPushButton *removeButton;

    QTimer *downRateTimer;
    QList<qint64> downRateWindow;

    void initProgressWidgets();
    void updateDownloadStats();

    void updateProgressWidgets();
    void updateStartStopBtn();
    void startStopBtnClicked();

    void startCalcDownRate();
    void stopCalcDownRate();
};

#endif // TASKTABLE_H
