#include "DownloadDialog.h"
#include "Network.h"
#include "utils.h"
#include <tuple>
#include <QtWidgets>
#include "Settings.h" // color

enum { TitleColum, DurationColum, TagColum };

// tree widget must be ContentTreeWidget
class ContentTreeItem: public QTreeWidgetItem
{
public:
    static constexpr int SectionItemType = QTreeWidgetItem::UserType;
    static constexpr int ContentItemType = QTreeWidgetItem::UserType + 1;
    static constexpr int ItemIdRole = Qt::UserRole + 1;

private:

    ContentTreeItem(const Extractor::ContentItem &item): QTreeWidgetItem(ContentItemType)
    {
        setText(TitleColum, item.title);
        setToolTip(TitleColum, item.title);
        QTreeWidgetItem::setData(0, ItemIdRole, item.id);
        QTreeWidgetItem::setData(0, Qt::CheckStateRole, Qt::Unchecked);
        if (item.flags & ContentItemFlag::Disabled) {
            setDisabled(true);
            setFlags(Qt::ItemNeverHasChildren);
        } else {
            setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren);
        }

        QString tag;
        if (item.flags & ContentItemFlag::VipOnly) {
            tag = QStringLiteral("会员");
        } else if (item.flags & ContentItemFlag::PayOnly) {
            tag = QStringLiteral("付费");
        } else if (item.flags & ContentItemFlag::AllowWaitFree) {
            tag = QStringLiteral("等免");
        }
        if (!tag.isEmpty()) {
            setText(TagColum, tag);
            setForeground(TagColum, Qt::white);
            setBackground(TagColum, B23Style::Pink);
        }


        if (item.durationInSec > 0) {
            setText(DurationColum, Utils::secs2HmsStr(item.durationInSec));
            setToolTip(DurationColum, Utils::secs2HmsLocaleStr(item.durationInSec));
        }
    }

    ContentTreeItem(const Extractor::Section &section): QTreeWidgetItem(SectionItemType)
    {
        setText(TitleColum, section.title);
        setChildIndicatorPolicy(ChildIndicatorPolicy::ShowIndicator);
        auto flags = Qt::ItemIsEnabled | Qt::ItemIsAutoTristate;
        if (section.episodes.size() != 0) {
            flags |= Qt::ItemIsUserCheckable;
            QTreeWidgetItem::setData(0, Qt::CheckStateRole, Qt::Unchecked);
        }
        setFlags(flags);

        bool allChildrenDisabled = true;
        for (auto &ep : section.episodes) {
            if (!(ep.flags & ContentItemFlag::Disabled)) {
                allChildrenDisabled = false;
                break;
            }
        }
        if (allChildrenDisabled) {
            setDisabled(true);
        }
    }

public:
    QnList qnList;

    static QList<QTreeWidgetItem*> createItems(const QList<Extractor::ContentItem> &items)
    {
        QList<QTreeWidgetItem*> treeItems;
        treeItems.reserve(items.size());
        for (auto &video : items) {
            treeItems.append(new ContentTreeItem(video));
        }
        return treeItems;
    }

    static QList<QTreeWidgetItem*> createSectionItems(const QList<Extractor::Section> &sections)
    {
        QList<QTreeWidgetItem*> treeItems;
        treeItems.reserve(sections.size());
        for (auto &section : sections) {
            auto sectionItem = new ContentTreeItem(section);
            sectionItem->addChildren(createItems(section.episodes));
            // sectionItem->setExpanded(true);
            treeItems.append(sectionItem);
        }
        return treeItems;
    }

    qint64 contentItemId()
    {
        return data(0, ItemIdRole).toLongLong();
    }

    void setChecked(bool checked)
    {
        QTreeWidgetItem::setData(0, Qt::CheckStateRole, checked ? Qt::Checked : Qt::Unchecked);
    }

    ContentTreeWidget *treeWidget()
    {
        return static_cast<ContentTreeWidget*>(QTreeWidgetItem::treeWidget());
    }

    QString longTitle()
    {
        // 如果 section/item 标题为"正片", 则视其为空
        // 比如电影《星际穿越》, 它的标题为"星际穿越", 有两个section("正片"和"预告花絮"),
        //     标题为"正片"的section包含一个标题为"正片"的视频
        auto clearIfZP = [](QString &s) {
            if (s == QStringLiteral("正片")) {
                s.clear();
            }
        };

        QString ret;
        if (parent() != nullptr) {
            auto sectionTitle = parent()->text(0);
            auto itemTitle = text(0);
            clearIfZP(sectionTitle);
            clearIfZP(itemTitle);
            if (sectionTitle.isEmpty()) {
                ret = itemTitle;
            } else if (!itemTitle.isEmpty()) {
                ret = sectionTitle + " " + itemTitle;
            }
        } else {
            ret = text(0);
            clearIfZP(ret);
        }
        return Utils::legalizedFileName(ret);
    }

private:
    void setData(int column, int role, const QVariant &value) override
    {
        if (role == Qt::CheckStateRole) {
            if (type() == SectionItemType) {
                treeWidget()->beginSelMultItems();
                QTreeWidgetItem::setData(column, role, value);
                treeWidget()->endSelMultItems();
                return;
            }
            else if (type() == ContentItemType) {
                // sectionItem's checkbox clicked ==> children.setData(CheckStateRole)
                //   ==> setSelected(here) ==> tree.selectionChanged
                //   ==> item.QTreeWidgetItem::setData(CheckStateRole)
                if (!isDisabled()) {
                    setSelected(value == Qt::Checked);
                }
                return;
            }
        }

        QTreeWidgetItem::setData(column, role, value);
    }
}; // end class ContentTreeItem



const int ContentTreeWidget::RowSpacing = 2;

void ContentTreeWidget::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto opt = option;
    opt.rect.adjust(0, RowSpacing / 2, 0, -RowSpacing / 2);
    QTreeWidget::drawRow(painter, opt, index);
}

ContentTreeWidget::ContentTreeWidget(const Extractor::Result *content, QWidget *parent)
    : QTreeWidget(parent)
{
    Q_ASSERT(content->type != ContentType::Live);

    setAllColumnsShowFocus(true);
    setDragDropMode(DragDropMode::NoDragDrop);
    setSelectionMode(QAbstractItemView::MultiSelection);
    setStyleSheet(QStringLiteral("QTreeView::item{margin-top:%1px;margin-bottom:%1px;}").arg(RowSpacing / 2));
    setMinimumWidth(fontMetrics().horizontalAdvance("+第001话 不可思议的魔法书+++1234:00:00+++会员+"));

    setColumnCount(3);
    if (content->type == ContentType::Comic) {
        setHeaderLabels({ "标题", "", "" });
    } else {
        setHeaderLabels({ "标题", "时长", "" });
    }
    header()->setDefaultAlignment(Qt::AlignCenter);
    header()->setSectionsMovable(false);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(TitleColum, QHeaderView::Stretch);
    header()->setSectionResizeMode(DurationColum, QHeaderView::Fixed);
    header()->setSectionResizeMode(TagColum, QHeaderView::ResizeToContents);
    header()->resizeSection(1, fontMetrics().horizontalAdvance("1234:12:12"));

    auto type = content->type;
    switch (type) {
    case ContentType::PGC: {
        auto sectionListRes = static_cast<const Extractor::SectionListResult*>(content);
        if (sectionListRes->sections.size() == 1) {
            auto videos = sectionListRes->sections.first().episodes;
            addTopLevelItems(ContentTreeItem::createItems(videos));
        } else {
            addTopLevelItems(ContentTreeItem::createSectionItems(sectionListRes->sections));
        }
        break;
    }
    case ContentType::UGC:
    case ContentType::PUGV:
    case ContentType::Comic: {
        auto itemListRes = static_cast<const Extractor::ItemListResult*>(content);
        addTopLevelItems(ContentTreeItem::createItems(itemListRes->items));
        break;
    }
    default:
        break;
    } // end switch

    for (auto it = QTreeWidgetItemIterator(this); (*it); ++it) {
        auto item = (*it);
        if (!item->isDisabled() && item->type() == ContentTreeItem::ContentItemType) {
            enabledItemCnt++;
        }
    }

    // Extractor 返回的内容包含了所有 分P/分集，
    //   如果用户输入的链接是指向单个视频的类型，那么默认(只)选中这个视频并set为currentItem
    //   否则，选择第一个视频作为currentItem
    //   在创建tree结束后，DownloadDialog会根据currentItem获取画质列表

    for (auto it = QTreeWidgetItemIterator(this); (*it); ++it) {
        auto item = static_cast<ContentTreeItem*>(*it);
        if (!item->isDisabled() && item->type() == ContentTreeItem::ContentItemType) {
            if (content->focusItemId == 0) {
                setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
                break;
            } else if (item->contentItemId() == content->focusItemId) {
                setCurrentItem(item); // item is also selected
                break;
            }
        }
    }

    expandAll();
    if (QTreeWidget::currentItem() != nullptr) {
        scrollToItem(QTreeWidget::currentItem());
    }
    QTimer::singleShot(0, this, [this]{ setFocus(); });
}

ContentTreeItem *ContentTreeWidget::currentItem() const
{
    return static_cast<ContentTreeItem*>(QTreeWidget::currentItem());
}

void ContentTreeWidget::beginSelMultItems()
{
    blockSigSelCntChanged = true;
    selCntBeforeBlock = selCnt;
}

void ContentTreeWidget::endSelMultItems()
{
    blockSigSelCntChanged = false;
    emit selectedItemCntChanged(selCnt, selCntBeforeBlock);
}

QList<QTreeWidgetItem*> ContentTreeWidget::selection2Items(const QItemSelection &selection)
{
    QList<QTreeWidgetItem*> ret;
    for (auto &range : selection) {
        auto index = range.topLeft();
        for (int row = range.top(); row <= range.bottom(); row++) {
            auto item = itemFromIndex(index.sibling(row, 0));
            if (item->type() == ContentTreeItem::SectionItemType) {
                continue;
            }
            ret.append(item);
        }
    }
    return ret;
}

void ContentTreeWidget::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeWidget::selectionChanged(selected, deselected);
    int prevSelCnt = selCnt;

    auto sel = selection2Items(selected);
    selCnt += sel.size();
    for (auto &&item : sel) {
        item->QTreeWidgetItem::setData(0, Qt::CheckStateRole, Qt::Checked);
    }
    auto desel = selection2Items(deselected);
    selCnt -= desel.size();
    for (auto &&item : desel) {
        item->QTreeWidgetItem::setData(0, Qt::CheckStateRole, Qt::Unchecked);
    }

    if (!blockSigSelCntChanged) {
        emit selectedItemCntChanged(selCnt, prevSelCnt);
    }
}

void ContentTreeWidget::selectAll()
{
    /* QAbstractItemView::selectAll() only select top level items;
     * QTreeView::selectAll() doesn't select items whose parent is not expanded;
     * QTreeWidget doesn't override selectAll()
     */
    beginSelMultItems();
    auto it = QTreeWidgetItemIterator(this);
    for (; (*it); ++it) {
        auto item = *it;
        if (item->type() == ContentTreeItem::ContentItemType && !item->isDisabled()) {
            item->setSelected(true);
        }
    }
    endSelMultItems();
}



class ActivityTipDialog: public QDialog
{
public:
    ActivityTipDialog(QString text, QWidget *parent) : QDialog(parent)
    {
        auto layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(text));
        auto cancelBtn = new QPushButton("取消");
        layout->addWidget(cancelBtn, 0, Qt::AlignRight);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }
};



#ifdef Q_OS_WIN
static QString getPotPlayerPath()
{
    auto path = QSettings("DAUM", "PotPlayer64").value("ProgramPath").toString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        return path;
    }

    path = QSettings("DAUM", "PotPlayer").value("ProgramPath").toString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        return path;
    }

    return QString();
}

static void execPotPlayer(QString potPlayerPath, QString biliUrl)
{
    QStringList args {
        biliUrl,
        "/user_agent=" + Network::Bili::UserAgent,
        "/referer=" + Network::Bili::Referer
    };
    QProcess::startDetached(potPlayerPath, args);
}
#endif

/* 你妈的 VLC 报错:『您的输入无法被打开: VLC 无法打开 MRL「https://....」。详情请检查日志。』
 * 为什么会这样啊 ! ! !
static QString getVlcPath()
{
#ifdef Q_OS_WIN
    auto path = QSettings("VideoLAN", "VLC").value(".").toString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        return path;
    }
#endif
    return QString();
}

static void execVlc(QString vlcPath, QString biliUrl)
{
    QStringList args {
        biliUrl,
        "--http-referrer=" + Network::Referer,
        "--http-user-agent=" + Network::UserAgent
    };
    QProcess::startDetached(vlcPath, args);
}
*/

static auto replyDeleter = [](QNetworkReply *reply) {
    reply->deleteLater();
};

void DownloadDialog::addPlayDirect(QBoxLayout *layout)
{
    using std::move;
#ifdef Q_OS_WIN
    if (contentType != ContentType::Live) {
        return;
    }

    std::function<void(QString)> handler;
    QString playAppName;

    QString path;
    if (!(path = getPotPlayerPath()).isEmpty()) {
        playAppName = "PotPlayer";
        handler = [path=move(path)](QString url) { execPotPlayer(path, url); };
    }
    /* else if (!(path = getVlcPath()).isEmpty()) {
        playAppName = "VLC";
        handler = [path=move(path)](QString url) { execVlc(path, url); };
    } */

    if (!handler) {
        return;
    }

    auto btn = new QPushButton(QString("直接用%1播放").arg(playAppName));
    layout->insertWidget(0, btn);
    connect(btn, &QPushButton::clicked, this, [this, handler=move(handler)]{
        auto rawPtr = LiveDownloadTask::getPlayUrlInfo(contentId, qnComboBox->currentData().toInt());
        std::unique_ptr<QNetworkReply, decltype(replyDeleter)> reply(rawPtr, replyDeleter);
        connect(rawPtr, &QNetworkReply::finished, this, [this, reply=move(reply), handler=move(handler)] {
            auto [json, errStr] = Network::Bili::parseReply(reply.get());
            if (!errStr.isEmpty()) {
                return;
            }
            auto key = LiveDownloadTask::playUrlInfoDataKey;
            auto url = LiveDownloadTask::getPlayUrlFromPlayUrlInfo(json[key].toObject());
            handler(url);
            this->reject();
        });
    });
    return;
#endif
}

DownloadDialog::~DownloadDialog() = default;

DownloadDialog::DownloadDialog(const QString &url, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("下载");

    extractor = new Extractor;
    connect(extractor, &Extractor::success, this, &DownloadDialog::setupUi);
    connect(extractor, &Extractor::errorOccurred, this, &DownloadDialog::extratorErrorOccured);

    activityTipDialog = new ActivityTipDialog("获取URL内容中...", parent);
    activityTipDialog->setWindowTitle("下载");
    connect(activityTipDialog, &QDialog::rejected, this, &DownloadDialog::abortExtract);

    QTimer::singleShot(0, this, [this, url]{ extractor->start(url); });
}

void DownloadDialog::open()
{
    activityTipDialog->open();
}

void DownloadDialog::abortExtract()
{
    extractor->abort();
    extractor->deleteLater();
    activityTipDialog->deleteLater();
    reject();
}

void DownloadDialog::extratorErrorOccured(const QString &errorString)
{
    extractor->deleteLater();
    activityTipDialog->accept();
    activityTipDialog->deleteLater();
    QMessageBox::critical(this->parentWidget(), "下载", errorString);
    reject();
}

static auto qnComboBoxToolTip =
    "点击 \"获取当前项画质\" 按钮来获取单个视频的画质.\n"
    "非 * 开头的为该视频可用的画质\n"
    "* 开头的画质不属于该视频, 但其他视频可能有\n"
    "注: 未登录/无会员 可能导致较高画质不可用";

void DownloadDialog::setupUi()
{
    activityTipDialog->accept();
    activityTipDialog->deleteLater();
    QTimer::singleShot(0, this, [this]{ QDialog::open(); });

    auto mainLayout = new QVBoxLayout(this);
    auto result = extractor->getResult();
    extractor->deleteLater();
    contentType = result->type;
    contentId = result->id;

    titleLabel = new QLabel(result->title);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleLabel->setFont(titleFont);
    titleLabel->setMinimumWidth(QFontMetrics(titleFont).horizontalAdvance("++魔卡少女樱 Clear Card篇++"));
    mainLayout->addWidget(titleLabel);

    if (contentType != ContentType::Live) {
        auto selLayout = new QHBoxLayout;
        selAllBtn = new QToolButton;
        selAllBtn->setText("全选");
        selAllBtn->setAutoRaise(true);
        selAllBtn->setCursor(Qt::PointingHandCursor);
        selCountLabel = new QLabel;
        selLayout->addWidget(selAllBtn);
        selLayout->addStretch(1);
        selLayout->addWidget(selCountLabel);
        mainLayout->addLayout(selLayout);

        tree = new ContentTreeWidget(result.get());
        tree->setFixedHeight(196);
        mainLayout->addWidget(tree, 1);
    }
    if (contentType == ContentType::UGC && tree->getEnabledItemCount() == 1) {
        selAllBtn->hide();
        selCountLabel->hide();
        tree->hide();
        tree->currentItem()->setText(0, QString());
    }

    if (contentType != ContentType::Comic) {
        auto qnLayout = new QHBoxLayout;
        qnLayout->addWidget(new QLabel("画质："));
        qnComboBox = new QComboBox;
        qnComboBox->setFocusPolicy(Qt::NoFocus);
        qnComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        auto qnTipLabel = new QLabel;
        auto infoIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
        qnTipLabel->setPixmap(infoIcon.pixmap(16, 16));
        qnTipLabel->setToolTip(qnComboBoxToolTip);
        qnLayout->addWidget(qnComboBox);
        qnLayout->addWidget(qnTipLabel);
        qnLayout->addStretch(1);
        if (tree != nullptr) {
            getQnListBtn = new QPushButton("获取当前项画质");
            // getQnListBtn->setToolTip("");
            getQnListBtn->setFlat(true);
            getQnListBtn->setCursor(Qt::PointingHandCursor);
            qnLayout->addSpacing(15);
            qnLayout->addWidget(getQnListBtn);
            connect(getQnListBtn, &QPushButton::clicked, this, &DownloadDialog::startGetCurrentItemQnList);
        }
        mainLayout->addLayout(qnLayout);
    }

    auto pathLayoutFrame = new QFrame;
    pathLayoutFrame->setContentsMargins(1, 1, 1, 1);
    pathLayoutFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    pathLayoutFrame->setLineWidth(1);
    pathLayoutFrame->setMidLineWidth(0);
    auto pathLayout = new QHBoxLayout(pathLayoutFrame);
    pathLayout->addWidget(new QLabel("下载到: "));
    pathLayout->setContentsMargins(2, 0, 1, 0);
    pathLabel = new ElidedTextLabel;
    auto lastDir = Settings::inst()->value("lastDir").toString();
    if (lastDir.isEmpty() || !QDir(lastDir).exists()) {
        auto appDir = QDir{QCoreApplication::applicationDirPath()};
        pathLabel->setText(appDir.absoluteFilePath("Downloads"));
    } else {
        pathLabel->setText(lastDir);
    }
    pathLabel->setElideMode(Qt::ElideMiddle);
    pathLabel->setHintWidthToString("/someDir/anime/百变小樱 第01话 不可思议的魔法书");
    selPathButton = new QPushButton;
    selPathButton->setToolTip("选择文件夹");
    selPathButton->setIconSize(QSize(20, 20));
    selPathButton->setIcon(QIcon(":/icons/folder.svg"));
    selPathButton->setFlat(true);
    selPathButton->setCursor(Qt::PointingHandCursor);
    selPathButton->setStyleSheet("QPushButton:pressed{border:none;}");
    pathLayout->addWidget(pathLabel, 1);
    pathLayout->addWidget(selPathButton);
    mainLayout->addWidget(pathLayoutFrame);

    auto bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch(1);
    okButton = new QPushButton("下载");
    okButton->setEnabled(tree == nullptr ? true : tree->getSelectedItemCount() > 0);
    auto cancelButton = new QPushButton("取消");
    bottomLayout->addWidget(okButton);
    bottomLayout->addWidget(cancelButton);
    mainLayout->addLayout(bottomLayout);

    addPlayDirect(bottomLayout);

    connect(okButton, &QPushButton::clicked, this, &DownloadDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &DownloadDialog::reject);
    connect(selPathButton, &QAbstractButton::clicked, this, &DownloadDialog::selectPath);

    if (tree != nullptr) {
        connect(selAllBtn, &QAbstractButton::clicked, this, &DownloadDialog::selAllBtnClicked);
        connect(tree, &ContentTreeWidget::selectedItemCntChanged, this, &DownloadDialog::selCntChanged);
        selCntChanged(tree->getSelectedItemCount(), 0); // initialization

        connect(tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur, QTreeWidgetItem *pre){
            if (getQnListBtn != nullptr) {
                getQnListBtn->setDisabled(cur == nullptr || cur->type() != ContentTreeItem::ContentItemType);
            }
            qDebug() << "current changed to" << (cur == nullptr ? "NULL" : cur->text(0))
                     << "(previous:" << (pre == nullptr ? "NULL" : pre->text(0));
        });
    }
    if (qnComboBox != nullptr && (tree == nullptr || tree->currentItem() != nullptr)) {
        startGetCurrentItemQnList();
    }
}

void DownloadDialog::selAllBtnClicked()
{
    if (tree->getSelectedItemCount() == tree->getEnabledItemCount()) {
        tree->clearSelection();
    } else {
        tree->selectAll();
    }
}

void DownloadDialog::selCntChanged(int curSelCnt, int preSelCnt)
{
    if (curSelCnt == tree->getEnabledItemCount() && preSelCnt != curSelCnt) {
        selAllBtn->setText("全不选");
    } else if (preSelCnt == tree->getEnabledItemCount() && preSelCnt != curSelCnt) {
        selAllBtn->setText("全选");
    }

    if (curSelCnt == 0) {
        selCountLabel->clear();
    } else {
        selCountLabel->setText(QString("已选中%1项").arg(curSelCnt));
    }

    okButton->setEnabled(curSelCnt != 0);
}

void DownloadDialog::startGetCurrentItemQnList()
{
    qint64 itemId = 0;
    if (tree != nullptr) {
        auto item = tree->currentItem();
        if (!item->qnList.empty()) {
            updateQnComboBox(item->qnList);
            return;
        }
        itemId = item->contentItemId();
    }
    switch (contentType) {
    case ContentType::Live:
        getQnListReply = LiveDownloadTask::getPlayUrlInfo(contentId, 0);
        break;
    case ContentType::PGC:
        getQnListReply = PgcDownloadTask::getPlayUrlInfo(itemId, 0);
        break;
    case ContentType::UGC:
        getQnListReply = UgcDownloadTask::getPlayUrlInfo(contentId, itemId, 0);
        break;
    case ContentType::PUGV:
        getQnListReply = PugvDownloadTask::getPlayUrlInfo(itemId, 0);
        break;
    default:;
    }
    connect(getQnListReply, &QNetworkReply::finished, this, &DownloadDialog::getCurrentItemQnListFinished);

    if (tree != nullptr) {
        selAllBtn->setEnabled(false);
        tree->setEnabled(false);
    }
    if (getQnListBtn != nullptr) {
        getQnListBtn->setEnabled(false);
    }
    qnComboBox->setEnabled(false);
    okButton->setEnabled(false);
}

// return a list of qn (in descending order) that exists in a but not in b
static QnList qnListDifference(QnList a, QnList b)
{
    QnList ret;
    //    ret.resize(a.size());
    //    auto end = std::set_difference(a.constBegin(), a.constEnd(), b.constBegin(), b.constEnd(), ret.begin());
    //    ret.resize(end - ret.begin());
    std::sort(a.begin(), a.end(), std::greater<QnList::Type>());
    std::sort(b.begin(), b.end(), std::greater<QnList::Type>());
    auto itA = a.constBegin();
    auto endA = a.constEnd();
    auto itB = b.constBegin();
    auto endB = b.constEnd();
    while (itA != endA && itB != endB) {
        auto cmp = (*itA) - (*itB);
        if (cmp == 0) {
            itA++;
            itB++;
        } else if (cmp > 0) {
            ret.push_back(*itA);
            itA++;
        } else {
            itB++;
        }
    }
    return ret;
}

void DownloadDialog::getCurrentItemQnListFinished()
{
    if (tree != nullptr) {
        selAllBtn->setEnabled(true);
        tree->setEnabled(true);
        QTimer::singleShot(0, this, [this]{ tree->setFocus(); });
    }
    if (getQnListBtn != nullptr) {
        getQnListBtn->setEnabled(true);
    }
    qnComboBox->setEnabled(true);
    okButton->setEnabled(true);

    auto reply = getQnListReply;
    getQnListReply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return;
    }
    auto [json, errStr] = Network::Bili::parseReply(reply);
    if (!errStr.isNull()) {
        QMessageBox::critical(this, "获取画质列表", "获取画质列表失败: " + errStr);
        return;
    }

    QnList qnList;
    switch (contentType) {
    case ContentType::Live: {
        auto data = json[LiveDownloadTask::playUrlInfoDataKey].toObject();
        qnList = LiveDownloadTask::getQnInfoFromPlayUrlInfo(data).qnList;
        break;
    }
    case ContentType::PGC: {
        auto data = json[PgcDownloadTask::playUrlInfoDataKey].toObject();
        qnList = PgcDownloadTask::getQnInfoFromPlayUrlInfo(data).qnList;
        break;
    }
    case ContentType::UGC: {
        auto data = json[UgcDownloadTask::playUrlInfoDataKey].toObject();
        qnList = UgcDownloadTask::getQnInfoFromPlayUrlInfo(data).qnList;
        break;
    }
    case ContentType::PUGV: {
        auto data = json[PugvDownloadTask::playUrlInfoDataKey].toObject();
        qnList = PugvDownloadTask::getQnInfoFromPlayUrlInfo(data).qnList;
        break;
    }
    default:;
    }
    if (tree != nullptr) {
        tree->currentItem()->qnList = qnList;
    }
    updateQnComboBox(std::move(qnList));
}

void DownloadDialog::updateQnComboBox(QnList qnList)
{
    bool onlyOneItem = (tree == nullptr || tree->getEnabledItemCount() == 1);
    QList<std::pair<int, QString>> qnDescList;
    if (contentType == ContentType::Live) {
        for (auto qn : qnList) {
            qnDescList.emplaceBack(qn, LiveDownloadTask::getQnDescription(qn));
        }
        if (!onlyOneItem) {
            qnList = qnListDifference(LiveDownloadTask::getAllPossibleQn(), std::move(qnList));
            for (auto qn : qnList) {
                qnDescList.emplaceBack(qn, "* " + LiveDownloadTask::getQnDescription(qn));
            }
        }
    } else {
        for (auto qn : qnList) {
            qnDescList.emplaceBack(qn, VideoDownloadTask::getQnDescription(qn));
        }
        if (!onlyOneItem) {
            qnList = qnListDifference(VideoDownloadTask::getAllPossibleQn(), std::move(qnList));
            for (auto qn : qnList) {
                qnDescList.emplaceBack(qn, "* " + VideoDownloadTask::getQnDescription(qn));
            }
        }
    }

    qnComboBox->clear();
    for (auto &[qn, desc] : qnDescList) {
        qnComboBox->addItem(desc, qn);
    }
}

void DownloadDialog::selectPath()
{
    auto path = QFileDialog::getExistingDirectory(this, QString(), pathLabel->text());
    if (!path.isNull()) {
        pathLabel->setText(QDir::toNativeSeparators(path));
    }
}

QList<AbstractDownloadTask*> DownloadDialog::getDownloadTasks()
{
    Settings::inst()->setValue("lastDir", pathLabel->text());

    QList<AbstractDownloadTask*> tasks;
    auto dir = QDir(pathLabel->text());
    int qn = (qnComboBox == nullptr ? 0 : qnComboBox->currentData().toInt());
    auto title = Utils::legalizedFileName(titleLabel->text());

    if (contentType == ContentType::Live) {
        tasks.append(new LiveDownloadTask(contentId, qn, dir.filePath(title)));
    } else {
        QList<std::tuple<qint64, QString>> metaInfos;
        for (auto item : tree->selectedItems()) {
            auto videoItem = static_cast<ContentTreeItem*>(item);
            if (videoItem->type() != ContentTreeItem::ContentItemType) {
                continue;
            }
            auto itemTitle = videoItem->longTitle();
            metaInfos.emplaceBack(
                videoItem->contentItemId(),
                (itemTitle.isEmpty() ? title : title + " " + itemTitle)
            );
        }
        for (auto &[itemId, name] : metaInfos) {
            AbstractDownloadTask *task = nullptr;
            auto path = dir.filePath(name);
            switch (contentType) {
            case ContentType::PGC:
                task = new PgcDownloadTask(contentId, itemId, qn, path);
                break;
            case ContentType::UGC:
                task = new UgcDownloadTask(contentId, itemId, qn, path);
                break;
            case ContentType::PUGV:
                task = new PugvDownloadTask(contentId, itemId, qn, path);
                break;
            case ContentType::Comic:
                task = new ComicDownloadTask(contentId, itemId, path);
                break;
            default:
                break;
            }
            tasks.append(task);
        }
    }

    return tasks;
}
