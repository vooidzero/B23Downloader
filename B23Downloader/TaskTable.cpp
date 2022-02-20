#include "TaskTable.h"
#include "DownloadTask.h"
#include "Settings.h"
#include "utils.h"

#include <QtWidgets>

static constexpr int MaxConcurrentTaskCount = 3;
static constexpr int SaveTasksInterval = 5000; // ms

static constexpr int DownRateTimerInterval = 500; // ms
static constexpr int DownRateWindowLength = 10;

TaskTableWidget::TaskTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setColumnCount(1);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet( "QTableWidget {"
        "selection-background-color: rgb(227, 227, 229);"
    "}");

    startAllAct = new QAction("全部开始");
    stopAllAct = new QAction("全部暂停");
    removeAllAct = new QAction("全部删除");
    connect(startAllAct, &QAction::triggered, this, &TaskTableWidget::startAll);
    connect(stopAllAct, &QAction::triggered, this, &TaskTableWidget::stopAll);
    connect(removeAllAct, &QAction::triggered, this, &TaskTableWidget::removeAll);

    saveTasksTimer = new QTimer(this);
    saveTasksTimer->setInterval(SaveTasksInterval);
    saveTasksTimer->setSingleShot(false);
    connect(saveTasksTimer, &QTimer::timeout, this, &TaskTableWidget::save);
}

static QAction* createOpenDirAct(QString path)
{
    auto openDirAct = new QAction("打开文件夹");
    QObject::connect(openDirAct, &QAction::triggered, [path=std::move(path)](){
#ifdef Q_OS_WIN
        QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
    });
    return openDirAct;
}

void TaskTableWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu contextMenu(this);
    auto selection = selectedIndexes();
    if (selection.size() == 1) {
        auto cell = cellWidget(selection.first().row());
        auto path = cell->getTask()->getPath();
        if (QFileInfo::exists(path)) {
            auto openAct = new QAction("打开");
            connect(openAct, &QAction::triggered, [path](){
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });
            contextMenu.addAction(openAct);
            contextMenu.addAction(createOpenDirAct(std::move(path)));
        }
    }

    contextMenu.addAction(startAllAct);
    contextMenu.addAction(stopAllAct);
    contextMenu.addAction(removeAllAct);
    contextMenu.exec(event->globalPos());
}

TaskCellWidget *TaskTableWidget::cellWidget(int row) const
{
    return static_cast<TaskCellWidget*>(QTableWidget::cellWidget(row, 0));
}

int TaskTableWidget::rowOfCell(TaskCellWidget *cell) const
{
    int rowCnt = rowCount();
    for (int row = 0; row < rowCnt; row++) {
        if (cellWidget(row) == cell) {
            return row;
        }
    }
    return -1;
}

void TaskTableWidget::setDirty()
{
    if (dirty) { return; }
    dirty = true;
    if (!saveTasksTimer->isActive()) {
        saveTasksTimer->start();
    }
}

void TaskTableWidget::save()
{
    if (!dirty) {
        return;
    }

    // brute but works as the number of tasks is assumed to be small (at most a one or two thousand).
    QJsonArray array;
    for (int row = 0; row < rowCount(); row++) {
        auto cell = cellWidget(row);
        if (cell->getState() == TaskCellWidget::Finished) {
            continue;
        }

        auto task = cell->getTask();
        if (qobject_cast<const LiveDownloadTask*>(task)) {
            // don't save live tasks
            continue;
        }
        array.append(task->toJsonObj());
    }
    auto settings = Settings::inst();
    settings->setValue("tasks", QJsonDocument(std::move(array)).toJson(QJsonDocument::Compact));

    if (activeTaskCnt == 0) {
        dirty = false;
        saveTasksTimer->stop();
    }
}

void TaskTableWidget::load()
{
    auto settings = Settings::inst();
    auto array = QJsonDocument::fromJson(settings->value("tasks").toByteArray()).array();
    QList<AbstractDownloadTask*> tasks;
    for (auto obj : array) {
        auto task = AbstractDownloadTask::fromJsonObj(obj.toObject());
        if (task != nullptr) {
            tasks.append(task);
        }
    }
    addTasks(tasks, false);
}

void TaskTableWidget::addTasks(const QList<AbstractDownloadTask *> &tasks, bool activate)
{
    auto shouldSetDirty = false;
    auto rowHt = TaskCellWidget::cellHeight();
    for (auto task : tasks) {
        auto cell = new TaskCellWidget(task);
        int idx = rowCount();
        insertRow(idx);
        setRowHeight(idx, rowHt);
        setCellWidget(idx, 0, cell);

        connect(cell, &TaskCellWidget::downloadStopped, this, &TaskTableWidget::onCellTaskStopped);
        connect(cell, &TaskCellWidget::downloadFinished, this, &TaskTableWidget::onCellTaskFinished);
        connect(cell, &TaskCellWidget::startBtnClicked, this, &TaskTableWidget::onCellStartBtnClicked);
        connect(cell, &TaskCellWidget::removeBtnClicked, this, &TaskTableWidget::onCellRemoveBtnClicked);

        if (activate) {
            if (activeTaskCnt < MaxConcurrentTaskCount) {
                activeTaskCnt++;
                shouldSetDirty = true;
                cell->startDownload();
            } else {
                cell->setWaitState();
            }
        }
    }

    if (shouldSetDirty) {
        setDirty();
    }
}

void TaskTableWidget::stopAll()
{
    for (int row = 0; row < rowCount(); row++) {
        cellWidget(row)->stopDownload();
    }
    activeTaskCnt = 0;
}

void TaskTableWidget::startAll()
{
    auto shouldSetDirty = false;
    for (int row = 0; row < rowCount(); row++) {
        auto cell = cellWidget(row);
        if (cell->getState() != TaskCellWidget::Stopped) {
            continue;
        }
        if (activeTaskCnt < MaxConcurrentTaskCount) {
            activeTaskCnt++;
            shouldSetDirty = true;
            cell->startDownload();
        } else {
            cell->setWaitState();
        }
    }
    if (shouldSetDirty) {
        setDirty();
    }
}

void TaskTableWidget::removeAll()
{
    auto rowCnt = rowCount();
    for (int row = 0; row < rowCnt; row++) {
        cellWidget(row)->remove();
    }
    activeTaskCnt = 0;
    for (int row = rowCnt - 1; row >= 0; row--) {
        removeRow(row);
    }
    if (rowCnt != 0) {
        setDirty();
    }
}

void TaskTableWidget::activateWaitingTasks()
{
    auto rowCnt = rowCount();
    for (int row = 0; activeTaskCnt < MaxConcurrentTaskCount && row < rowCnt; row++) {
        auto cell = cellWidget(row);
        if (cell->getState() == TaskCellWidget::Waiting) {
            activeTaskCnt++;
            cell->startDownload();
        }
    }
}

void TaskTableWidget::onCellTaskStopped()
{
    activeTaskCnt--;
    activateWaitingTasks();
}

void TaskTableWidget::onCellTaskFinished()
{
    auto cell = static_cast<TaskCellWidget*>(sender());
    QTimer::singleShot(3000, this, [=]{
        removeRow(rowOfCell(cell));
    });
    activeTaskCnt--;
    activateWaitingTasks();
    setDirty();
}

void TaskTableWidget::onCellStartBtnClicked()
{
    auto cell = static_cast<TaskCellWidget*>(sender());
    if (activeTaskCnt < MaxConcurrentTaskCount) {
        activeTaskCnt++;
        cell->startDownload();
    } else {
        cell->setWaitState();
    }

    setDirty();
}

void TaskTableWidget::onCellRemoveBtnClicked()
{
    auto cell = static_cast<TaskCellWidget*>(sender());
    removeRow(rowOfCell(cell));
    setDirty();
}



int TaskCellWidget::cellHeight()
{
    auto lineSpacing = QFontMetrics(QApplication::font()).lineSpacing();
    auto style = QApplication::style();
    auto layoutTopMargin = style->pixelMetric(QStyle::PM_LayoutTopMargin);
    auto layoutBtmMargin = style->pixelMetric(QStyle::PM_LayoutBottomMargin);
    auto layoutSpacing = style->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    return lineSpacing * 2 + layoutTopMargin + layoutSpacing + layoutBtmMargin + 2;
}


TaskCellWidget::~TaskCellWidget()
{
    delete task;
}

static void flattenPushButton(QPushButton *btn)
{
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet("QPushButton:pressed{border:none; }");
}

TaskCellWidget::TaskCellWidget(AbstractDownloadTask *task, QWidget *parent)
    : QWidget(parent), task(task)
{
    auto fm = fontMetrics();
    auto lineSpacing = fontMetrics().lineSpacing();
    auto mainLayout = new QHBoxLayout(this);

    iconButton = new QPushButton;
    iconButton->setFixedSize(32, 32);
    iconButton->setIconSize(QSize(32, 32));
    iconButton->setToolTip("打开");
    if (qobject_cast<ComicDownloadTask*>(task)) {
        iconButton->setIcon(QIcon(":/icons/manga.svg"));
    } else {
        iconButton->setIcon(QIcon(":/icons/video.svg"));
    }
    flattenPushButton(iconButton);
    mainLayout->addWidget(iconButton);

    auto leftVLayout = new QVBoxLayout;
    titleLabel = new ElidedTextLabel;
    titleLabel->setText(task->getTitle());
    // titleLabel->setHintWidthToString("魔卡少女樱 Clear Card篇 第01话 小樱与透明卡牌");
    auto layoutUnderTitle = new QHBoxLayout;
    qnDescLabel = new QLabel;
    qnDescLabel->setEnabled(false); // set gray color
    progressLabel = new QLabel;
    progressLabel->setEnabled(false); // set gray color
    layoutUnderTitle->addWidget(qnDescLabel, 1);
    layoutUnderTitle->addWidget(progressLabel, 2);
    leftVLayout->addWidget(titleLabel);
    leftVLayout->addLayout(layoutUnderTitle);
    mainLayout->addLayout(leftVLayout, 1);

    auto centerVLayout = new QVBoxLayout;
    auto centerWidgetsSize = QSize(fm.horizontalAdvance("00:00:00++++123.45KB/s"), lineSpacing);
    progressBar = new QProgressBar;
    progressBar->setFixedSize(centerWidgetsSize);
    progressBar->setAlignment(Qt::AlignCenter);
    centerVLayout->addWidget(progressBar);

    statusStackedWidget = new QStackedWidget;
    statusStackedWidget->setFixedSize(centerWidgetsSize);
    statusTextLabel = new ElidedTextLabel("暂停中...");
    statusTextLabel->setEnabled(false); // set gray color as default color
    timeLeftLabel = new QLabel;
    timeLeftLabel->setEnabled(false); // set gray color
    downRateLabel = new QLabel;
    downRateLabel->setToolTip("下载速度");
    downRateLabel->setEnabled(false); // set gray color
    downloadStatsWidget = new QWidget;
    auto hLayout = new QHBoxLayout(downloadStatsWidget);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(statusTextLabel);
    hLayout->addWidget(timeLeftLabel);
    hLayout->addStretch(1);
    hLayout->addWidget(downRateLabel);
    statusStackedWidget->addWidget(statusTextLabel);
    statusStackedWidget->addWidget(downloadStatsWidget);
    centerVLayout->addWidget(statusStackedWidget);
    mainLayout->addLayout(centerVLayout);
    mainLayout->addSpacing(20);

    startStopButton = new QPushButton;
    flattenPushButton(startStopButton);
    removeButton = new QPushButton;
    removeButton->setToolTip("删除");
    removeButton->setIcon(QIcon(":/icons/remove.svg"));
    flattenPushButton(removeButton);
    mainLayout->addWidget(startStopButton);
    mainLayout->addWidget(removeButton);

    updateStartStopBtn();
    initProgressWidgets();
    qnDescLabel->setText(task->getQnDescription());

    if (qobject_cast<LiveDownloadTask*>(task)) {
        timeLeftLabel->setToolTip("已下载时长");
    } else {
        timeLeftLabel->setToolTip("剩余时间");
    }

    downRateTimer = new QTimer(this);
    downRateTimer->setInterval(DownRateTimerInterval);
    downRateTimer->setSingleShot(false);
    downRateWindow.reserve(DownRateWindowLength);

    connect(iconButton, &QAbstractButton::clicked, this, &TaskCellWidget::open);

    connect(task, &AbstractDownloadTask::errorOccurred, this, &TaskCellWidget::onErrorOccurred);

    connect(task, &AbstractDownloadTask::getUrlInfoFinished, this, [this]{
       initProgressWidgets();
       // titleLabel->setText(this->task->getTitle());
       qnDescLabel->setText(this->task->getQnDescription());
       startCalcDownRate();
    });

    connect(task, &AbstractDownloadTask::downloadFinished, this, &TaskCellWidget::onFinished);

    connect(startStopButton, &QAbstractButton::clicked, this, &TaskCellWidget::startStopBtnClicked);

    connect(removeButton, &QAbstractButton::clicked, this, [this]{
        remove();
        emit removeBtnClicked();
    });

    connect(downRateTimer, &QTimer::timeout, this, &TaskCellWidget::updateDownloadStats);
}

void TaskCellWidget::onErrorOccurred(const QString &errStr)
{
    statusTextLabel->setErrText(errStr);
    state = State::Stopped;
    updateStartStopBtn();
    emit downloadStopped();
}

void TaskCellWidget::onFinished()
{
    state = State::Finished;
    statusTextLabel->setText("已完成");
    startStopButton->setEnabled(false);
    removeButton->setEnabled(false);
    stopCalcDownRate();
    emit downloadFinished();
}

void TaskCellWidget::open()
{
    auto path = task->getPath();
    if (QFileInfo::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void TaskCellWidget::startCalcDownRate()
{
    downRateWindow.append(task->getDownloadedBytesCnt());
    downRateTimer->start();
    statusStackedWidget->setCurrentWidget(downloadStatsWidget);
}

void TaskCellWidget::stopCalcDownRate()
{
    downRateTimer->stop();
    downRateWindow.clear();
    downRateLabel->clear();
    timeLeftLabel->clear();
    updateProgressWidgets();
    statusStackedWidget->setCurrentWidget(statusTextLabel);
}

void TaskCellWidget::initProgressWidgets()
{
    if (qobject_cast<LiveDownloadTask*>(task)) {
        progressBar->reset();
    } else {
        progressBar->setRange(0, 100);
        updateProgressWidgets();
    }
}

void TaskCellWidget::updateProgressWidgets()
{
    progressLabel->setText(task->getProgressStr());
    auto progress = task->getProgress();
    if (progress >= 0) {
        progressBar->setValue(static_cast<int>(progress * 100));
    }
}

void TaskCellWidget::updateDownloadStats()
{
    qint64 downloadedBytes = task->getDownloadedBytesCnt();
    qint64 bytes = downloadedBytes - downRateWindow.first();
    if (bytes < 0) {
        bytes = 0;
        downRateWindow.clear();
    }
    double seconds = downRateWindow.size() * ((double)DownRateTimerInterval / 1000.0);
    qint64 downBytesPerSec = static_cast<qint64>(static_cast<double>(bytes) / seconds);
    downRateLabel->setText(Utils::formattedDataSize(downBytesPerSec) + "/s");

    if (downRateWindow.size() == DownRateWindowLength) {
        downRateWindow.removeFirst();
    }
    downRateWindow.append(downloadedBytes);

    auto infTime = "--:--:--";
    auto secs = task->estimateRemainingSeconds(downBytesPerSec);
    // secs > 99 * 3600
    timeLeftLabel->setText(secs < 0 ? infTime : Utils::secs2HmsStr(secs));

    updateProgressWidgets();
}

void TaskCellWidget::updateStartStopBtn()
{
    if (state == State::Waiting || state == State::Downloading) {
            startStopButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
            startStopButton->setToolTip("暂停");
    } else {
        startStopButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        startStopButton->setToolTip("开始");
    }
}

void TaskCellWidget::startStopBtnClicked()
{
    switch (state) {
    case State::Stopped:
        emit startBtnClicked();
        break;
    case State::Waiting:
        stopDownload();
        break;
    case State::Downloading:
        stopDownload();
        emit downloadStopped();
        break;
    default:
        break;
    }
}

void TaskCellWidget::stopDownload()
{
    if (state == State::Stopped || state == State::Finished) {
        return;
    }
    statusTextLabel->setText("暂停中");
    if (state == State::Downloading) {
        task->stopDownload();
        stopCalcDownRate();
    }
    state = State::Stopped;
    updateStartStopBtn();
}

void TaskCellWidget::remove()
{
    task->stopDownload();
    task->removeFile();
}

void TaskCellWidget::setWaitState()
{
    if (state != State::Stopped) {
        return;
    }
    statusTextLabel->setText("等待下载");
    state = State::Waiting;
    updateStartStopBtn();
}

void TaskCellWidget::startDownload()
{
    if (state != State::Stopped && state != State::Waiting) {
        return;
    }
    statusTextLabel->setText("请求中");
    state = State::Downloading;
    task->startDownload();
    updateStartStopBtn();
}
