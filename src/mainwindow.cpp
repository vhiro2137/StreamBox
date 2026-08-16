#include "mainwindow.h"
#include "player/playerengine.h"
#include "player/playbackpolicy.h"

#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScreen>
#include <QSplitter>
#include <QTimer>
#include <QUrl>

namespace {
QLabel *makeLabel(const QString &text, const QString &objectName = {}, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    if (!objectName.isEmpty()) label->setObjectName(objectName);
    return label;
}

QPushButton *makeTextButton(const QString &text, const char *kind, QWidget *parent = nullptr)
{
    auto *button = new QPushButton(text, parent);
    button->setProperty("class", kind);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::StrongFocus);
    return button;
}

QString mediaKindForFile(const QString &path)
{
    static const QStringList audio = {"mp3", "aac", "flac", "wav", "opus", "ogg", "m4a"};
    const QString suffix = QFileInfo(path).suffix().toLower();
    return audio.contains(suffix) ? QStringLiteral("audio") : QStringLiteral("video");
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("StreamBox"));
    setMinimumSize(960, 600);
    resize(1280, 800);
    setAcceptDrops(true);
    setStyleSheet(streamBoxStyleSheet());
    m_engine = new PlayerEngine(this);
    buildUi();
    connectActions();
    setPlaybackState(VideoCanvas::State::Empty, QStringLiteral("尚未播放"));
}

void MainWindow::openMedia(const QString &source)
{
    const QUrl url(source);
    if ((url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) && url.isValid()) {
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2 · 网络媒体").arg(QFileInfo(url.path()).fileName(), url.host()));
        item->setData(Qt::UserRole, QStringLiteral("video")); item->setData(Qt::UserRole + 1, QFileInfo(url.path()).completeBaseName());
        item->setData(Qt::UserRole + 2, url.host()); item->setData(Qt::UserRole + 3, url.toString()); m_playlist->addItem(item);
        m_playlistCount->setText(QString::number(m_playlist->count())); selectItem(m_playlist->row(item));
    } else addMediaFiles({source}, true);
}

void MainWindow::setPlaybackSpeed(double speed)
{
    const double bounded = qBound(0.5, speed, 2.0);
    m_engine->setSpeed(bounded);
    m_speedButton->setText(QString::number(bounded, 'f', bounded == qRound(bounded) ? 1 : 2) + QStringLiteral("×"));
}

void MainWindow::seekTo(qint64 milliseconds)
{
    m_engine->seek(milliseconds);
}

void MainWindow::setPlayerFullscreen(bool fullscreen)
{
    if (m_fullscreen != fullscreen) toggleFullscreen();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(createToolbar());

    m_workspace = new QSplitter(Qt::Horizontal, central);
    m_workspace->setHandleWidth(1);
    m_workspace->setChildrenCollapsible(false);
    m_canvas = new VideoCanvas(m_workspace);
    m_playlistPanel = createPlaylistPanel();
    m_playlistPanel->setMinimumWidth(300);
    m_playlistPanel->setMaximumWidth(440);
    m_workspace->addWidget(m_canvas);
    m_workspace->addWidget(m_playlistPanel);
    m_workspace->setStretchFactor(0, 1);
    m_workspace->setStretchFactor(1, 0);
    m_workspace->setSizes({920, 360});
    root->addWidget(m_workspace, 1);
    m_controls = createControls();
    root->addWidget(m_controls);
    setCentralWidget(central);

    m_toast = new QLabel(central);
    m_toast->setObjectName(QStringLiteral("toast"));
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->setStyleSheet(QStringLiteral("QLabel#toast { background:#2B303B; border:1px solid #3A414E; border-radius:8px; padding:10px 16px; color:#F3F5F7; }"));
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);

    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(1000);
    m_fullscreenControlsTimer = new QTimer(this);
    m_fullscreenControlsTimer->setSingleShot(true);
    m_fullscreenControlsTimer->setInterval(3000);
    connect(m_fullscreenControlsTimer, &QTimer::timeout, this, [this] {
        if (m_fullscreen && m_controls) m_controls->hide();
    });
    qApp->installEventFilter(this);
}

QWidget *MainWindow::createToolbar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("toolbar"));
    bar->setFixedHeight(48);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(8);

    auto *logo = new IconButton(PlayerIcon::Play, bar);
    logo->setObjectName(QStringLiteral("logo"));
    logo->setProperty("accentIcon", true);
    logo->setFixedSize(24, 24);
    logo->setFocusPolicy(Qt::NoFocus);
    m_brandName = makeLabel(QStringLiteral("StreamBox"), QStringLiteral("brand"), bar);
    layout->addWidget(logo);
    layout->addWidget(m_brandName);
    layout->addSpacing(16);

    auto *fileButton = makeTextButton(QStringLiteral("  打开文件"), "secondary", bar);
    fileButton->setObjectName(QStringLiteral("openFileButton"));
    fileButton->setFixedSize(96, 32);
    fileButton->setToolTip(QStringLiteral("打开文件（Ctrl+O）"));
    auto *urlButton = makeTextButton(QStringLiteral("  打开 URL"), "secondary", bar);
    urlButton->setObjectName(QStringLiteral("openUrlButton"));
    urlButton->setFixedSize(96, 32);
    urlButton->setToolTip(QStringLiteral("打开 URL（Ctrl+U）"));
    layout->addWidget(fileButton);
    layout->addWidget(urlButton);
    layout->addStretch();

    m_listButton = new IconButton(PlayerIcon::List, bar);
    m_listButton->setToolTip(QStringLiteral("收起播放列表"));
    auto *more = new IconButton(PlayerIcon::More, bar);
    more->setToolTip(QStringLiteral("更多设置"));
    more->setObjectName(QStringLiteral("moreButton"));
    layout->addWidget(m_listButton);
    layout->addWidget(more);
    return bar;
}

QWidget *MainWindow::createPlaylistPanel()
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("playlistPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget(panel);
    header->setObjectName(QStringLiteral("playlistHeader"));
    header->setFixedHeight(52);
    auto *headLayout = new QHBoxLayout(header);
    headLayout->setContentsMargins(16, 0, 12, 0);
    headLayout->setSpacing(7);
    auto *title = makeLabel(QStringLiteral("播放列表"), {}, header);
    QFont titleFont = title->font(); titleFont.setPointSize(11); titleFont.setWeight(QFont::DemiBold); title->setFont(titleFont);
    m_playlistCount = makeLabel(QStringLiteral("0"), QStringLiteral("tertiary"), header);
    auto *add = makeTextButton(QStringLiteral("添加"), "flat", header);
    add->setObjectName(QStringLiteral("addMediaButton"));
    add->setFixedWidth(54);
    auto *more = new IconButton(PlayerIcon::More, header);
    more->setObjectName(QStringLiteral("playlistMoreButton"));
    more->setToolTip(QStringLiteral("播放列表更多操作"));
    headLayout->addWidget(title);
    headLayout->addWidget(m_playlistCount);
    headLayout->addStretch();
    headLayout->addWidget(add);
    headLayout->addWidget(more);
    layout->addWidget(header);

    m_playlist = new QListWidget(panel);
    m_playlist->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlist->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_playlist->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlist->setTextElideMode(Qt::ElideRight);
    layout->addWidget(m_playlist, 1);
    return panel;
}

QWidget *MainWindow::createControls()
{
    auto *controls = new QWidget(this);
    controls->setObjectName(QStringLiteral("controls"));
    controls->setFixedHeight(124);
    auto *root = new QVBoxLayout(controls);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *progressRow = new QWidget(controls);
    progressRow->setFixedHeight(40);
    auto *progressLayout = new QHBoxLayout(progressRow);
    progressLayout->setContentsMargins(20, 0, 20, 0);
    progressLayout->setSpacing(12);
    m_currentTime = makeLabel(QStringLiteral("00:00"), {}, progressRow);
    m_currentTime->setFixedWidth(52); m_currentTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_progress = new ProgressSlider(Qt::Horizontal, progressRow);
    m_progress->setEnabled(false); m_progress->setBufferedValue(550);
    m_totalTime = makeLabel(QStringLiteral("00:00"), {}, progressRow);
    m_totalTime->setFixedWidth(52);
    progressLayout->addWidget(m_currentTime);
    progressLayout->addWidget(m_progress, 1);
    progressLayout->addWidget(m_totalTime);
    root->addWidget(progressRow);

    auto *mainRow = new QWidget(controls);
    mainRow->setFixedHeight(56);
    auto *mainLayout = new QHBoxLayout(mainRow);
    mainLayout->setContentsMargins(20, 0, 20, 0);
    mainLayout->setSpacing(0);

    auto *nowPlaying = new QWidget(mainRow);
    nowPlaying->setObjectName(QStringLiteral("nowPlaying"));
    auto *nowLayout = new QHBoxLayout(nowPlaying);
    nowLayout->setContentsMargins(0, 0, 0, 0); nowLayout->setSpacing(10);
    auto *thumb = new QLabel(nowPlaying); thumb->setFixedSize(36, 36); thumb->setStyleSheet(QStringLiteral("background:#29324A;border-radius:6px;"));
    auto *copy = new QWidget(nowPlaying); auto *copyLayout = new QVBoxLayout(copy); copyLayout->setContentsMargins(0, 2, 0, 2); copyLayout->setSpacing(1);
    m_nowTitle = makeLabel(QStringLiteral("未选择媒体"), {}, copy);
    QFont bold = m_nowTitle->font(); bold.setWeight(QFont::DemiBold); m_nowTitle->setFont(bold);
    m_nowSubtitle = makeLabel(QStringLiteral("打开文件或网络地址"), QStringLiteral("tertiary"), copy);
    copyLayout->addWidget(m_nowTitle); copyLayout->addWidget(m_nowSubtitle);
    nowLayout->addWidget(thumb); nowLayout->addWidget(copy, 1);
    mainLayout->addWidget(nowPlaying, 1);

    auto *transport = new QWidget(mainRow);
    transport->setFixedWidth(260);
    auto *transportLayout = new QHBoxLayout(transport);
    transportLayout->setContentsMargins(0, 0, 0, 0); transportLayout->setSpacing(8); transportLayout->setAlignment(Qt::AlignCenter);
    auto *previous = new IconButton(PlayerIcon::Previous, transport); previous->setObjectName(QStringLiteral("previousButton")); previous->setFixedSize(36, 36); previous->setToolTip(QStringLiteral("上一首（P）"));
    m_playButton = new IconButton(PlayerIcon::Play, transport); m_playButton->setObjectName(QStringLiteral("playMain")); m_playButton->setProperty("accentIcon", true); m_playButton->setProperty("largeIcon", true); m_playButton->setFixedSize(44, 44); m_playButton->setToolTip(QStringLiteral("播放（Space）"));
    auto *next = new IconButton(PlayerIcon::Next, transport); next->setObjectName(QStringLiteral("nextButton")); next->setFixedSize(36, 36); next->setToolTip(QStringLiteral("下一首（N）"));
    m_stopButton = new IconButton(PlayerIcon::Stop, transport); m_stopButton->setFixedSize(36, 36); m_stopButton->setToolTip(QStringLiteral("停止")); m_stopButton->setEnabled(false);
    transportLayout->addWidget(previous); transportLayout->addWidget(m_playButton); transportLayout->addWidget(next); transportLayout->addWidget(m_stopButton);
    mainLayout->addWidget(transport);

    auto *tools = new QWidget(mainRow);
    auto *toolsLayout = new QHBoxLayout(tools); toolsLayout->setContentsMargins(0, 0, 0, 0); toolsLayout->setSpacing(4); toolsLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_speedButton = makeTextButton(QStringLiteral("1.0×"), "flat", tools); m_speedButton->setFixedSize(56, 32); m_speedButton->setToolTip(QStringLiteral("播放速度"));
    m_modeButton = new IconButton(PlayerIcon::Repeat, tools); m_modeButton->setToolTip(QStringLiteral("列表循环"));
    m_muteButton = new IconButton(PlayerIcon::Volume, tools); m_muteButton->setToolTip(QStringLiteral("静音（M）"));
    m_volume = new QSlider(Qt::Horizontal, tools); m_volume->setObjectName(QStringLiteral("volume")); m_volume->setRange(0, 100); m_volume->setValue(70); m_volume->setFixedSize(88, 20); m_volume->setToolTip(QStringLiteral("音量 70%"));
    m_fullscreenButton = new IconButton(PlayerIcon::Fullscreen, tools); m_fullscreenButton->setToolTip(QStringLiteral("进入全屏（F）")); m_fullscreenButton->setEnabled(false);
    toolsLayout->addWidget(m_speedButton); toolsLayout->addWidget(m_modeButton); toolsLayout->addWidget(m_muteButton); toolsLayout->addWidget(m_volume); toolsLayout->addWidget(m_fullscreenButton);
    mainLayout->addWidget(tools, 1);
    root->addWidget(mainRow);

    auto *statusRow = new QWidget(controls); statusRow->setObjectName(QStringLiteral("statusRow")); statusRow->setFixedHeight(28);
    auto *statusLayout = new QHBoxLayout(statusRow); statusLayout->setContentsMargins(20, 0, 20, 0); statusLayout->setSpacing(7);
    m_statusDot = new QLabel(statusRow); m_statusDot->setFixedSize(6, 6); m_statusDot->setStyleSheet(QStringLiteral("background:#737D8E;border-radius:3px;"));
    m_statusText = makeLabel(QStringLiteral("尚未播放"), QStringLiteral("secondary"), statusRow);
    m_mediaInfo = makeLabel(QString(), QStringLiteral("secondary"), statusRow);
    statusLayout->addWidget(m_statusDot); statusLayout->addWidget(m_statusText); statusLayout->addStretch(); statusLayout->addWidget(m_mediaInfo);
    root->addWidget(statusRow);
    return controls;
}

void MainWindow::connectActions()
{
    connect(findChild<QPushButton *>(QStringLiteral("openFileButton")), &QPushButton::clicked, this, &MainWindow::openFiles);
    connect(findChild<QPushButton *>(QStringLiteral("openUrlButton")), &QPushButton::clicked, this, &MainWindow::openUrlDialog);
    connect(findChild<QPushButton *>(QStringLiteral("addMediaButton")), &QPushButton::clicked, this, [this] {
        QMenu menu(this); QAction *files = menu.addAction(QStringLiteral("添加本地文件")); QAction *url = menu.addAction(QStringLiteral("添加网络地址"));
        QAction *chosen = menu.exec(QCursor::pos()); if (chosen == files) openFiles(); else if (chosen == url) openUrlDialog();
    });
    connect(m_listButton, &QPushButton::clicked, this, &MainWindow::togglePlaylist);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopPlayback);
    connect(findChild<QPushButton *>(QStringLiteral("previousButton")), &QPushButton::clicked, this, &MainWindow::playPrevious);
    connect(findChild<QPushButton *>(QStringLiteral("nextButton")), &QPushButton::clicked, this, &MainWindow::playNext);
    connect(m_speedButton, &QPushButton::clicked, this, &MainWindow::showSpeedMenu);
    connect(m_modeButton, &QPushButton::clicked, this, &MainWindow::showModeMenu);
    connect(m_muteButton, &QPushButton::clicked, this, &MainWindow::toggleMute);
    connect(m_fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    connect(m_canvas, &VideoCanvas::openFilesRequested, this, &MainWindow::openFiles);
    connect(m_canvas, &VideoCanvas::openUrlRequested, this, &MainWindow::openUrlDialog);
    connect(m_canvas, &VideoCanvas::fullscreenRequested, this, &MainWindow::toggleFullscreen);
    connect(m_canvas, &VideoCanvas::filesDropped, this, [this](const QStringList &files) { addMediaFiles(files); });
    connect(m_canvas, &VideoCanvas::cancelRequested, this, [this] { m_engine->stop(); setPlaybackState(VideoCanvas::State::Stopped, QStringLiteral("已取消")); });
    connect(m_canvas, &VideoCanvas::retryRequested, this, [this] {
        if (m_playlist->currentRow() >= 0) selectItem(m_playlist->currentRow());
    });
    connect(m_playlist, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) { selectItem(m_playlist->row(item)); });
    connect(m_playlist, &QListWidget::currentRowChanged, this, [this](int row) { if (row >= 0) updateCurrentMediaUi(); });
    connect(m_playlist, &QListWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QListWidgetItem *item = m_playlist->itemAt(point); if (!item) return;
        QMenu menu(this); QAction *play = menu.addAction(QStringLiteral("播放")); QAction *remove = menu.addAction(QStringLiteral("从列表中移除"));
        QAction *chosen = menu.exec(m_playlist->viewport()->mapToGlobal(point));
        if (chosen == play) selectItem(m_playlist->row(item));
        else if (chosen == remove) {
            const int row = m_playlist->row(item); const bool current = row == m_playlist->currentRow();
            if (current) m_engine->stop(); delete m_playlist->takeItem(row); m_playlistCount->setText(QString::number(m_playlist->count()));
            if (!m_playlist->count()) setPlaybackState(VideoCanvas::State::Empty);
            else if (current) selectItem(qMin(row, m_playlist->count() - 1), false);
        }
    });
    connect(findChild<QPushButton *>(QStringLiteral("playlistMoreButton")), &QPushButton::clicked, this, [this] {
        QMenu menu(this); QAction *clear = menu.addAction(QStringLiteral("清空播放列表")); clear->setEnabled(m_playlist->count() > 0);
        if (menu.exec(QCursor::pos()) == clear) {
            QMessageBox confirm(QMessageBox::Question, QStringLiteral("清空播放列表？"), QStringLiteral("列表中的 %1 个项目将被移除，此操作不会删除本地文件。").arg(m_playlist->count()), QMessageBox::NoButton, this);
            auto *cancel = confirm.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
            auto *clearButton = confirm.addButton(QStringLiteral("清空"), QMessageBox::DestructiveRole);
            confirm.setDefaultButton(cancel); confirm.exec();
            if (confirm.clickedButton() != clearButton) return;
            m_engine->stop(); m_playlist->clear(); m_playlistCount->setText(QStringLiteral("0")); setPlaybackState(VideoCanvas::State::Empty);
        }
    });
    connect(m_progress, &QSlider::valueChanged, this, [this](int value) {
        m_currentTime->setText(formatTime(value));
    });
    connect(m_progress, &QSlider::sliderReleased, this, [this] {
        if (m_engine->duration() > 0) m_engine->seek(qint64(m_progress->value()) * 1000);
    });
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        m_volume->setToolTip(QStringLiteral("音量 %1%").arg(value));
        m_engine->setVolume(value / 100.0f);
        if (value > 0 && m_muted) { m_muted = false; m_muteButton->setIconType(PlayerIcon::Volume); }
    });
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);
    connect(m_engine, &PlayerEngine::videoFrameReady, m_canvas, &VideoCanvas::setVideoFrame);
    connect(m_engine, &PlayerEngine::mediaOpened, this, [this](qint64 durationMs, bool, bool video, const QString &description) {
        m_durationSeconds = int(durationMs / 1000); m_progress->setRange(0, qMax(0, m_durationSeconds));
        m_totalTime->setText(durationMs > 0 ? formatTime(m_durationSeconds) : QStringLiteral("未知"));
        m_mediaInfo->setText(description); m_fullscreenButton->setEnabled(video);
    });
    connect(m_engine, &PlayerEngine::seekabilityChanged, this, [this](bool seekable) {
        m_progress->setEnabled(seekable);
        if (!seekable) { m_progress->setRange(0, 0); m_totalTime->setText(QStringLiteral("直播")); }
    });
    connect(m_engine, &PlayerEngine::retrying, this, [this](int attempt, int maximum, int delayMs) {
        setPlaybackState(VideoCanvas::State::Buffering,
            QStringLiteral("网络连接中断，正在重试（%1/%2，%3 秒）").arg(attempt).arg(maximum).arg(delayMs / 1000));
    });
    connect(m_engine, &PlayerEngine::audioOutputReady, this, [this](const QString &deviceName) {
        m_statusText->setToolTip(QStringLiteral("音频输出设备：%1").arg(deviceName));
        showToast(QStringLiteral("音频输出：%1").arg(deviceName));
    });
    connect(m_engine, &PlayerEngine::positionChanged, this, [this](qint64 positionMs) {
        if (!m_progress->isSliderDown()) m_progress->setValue(int(positionMs / 1000));
    });
    connect(m_engine, &PlayerEngine::stateChanged, this, [this](PlayerEngine::State state) {
        switch (state) {
        case PlayerEngine::State::Idle: setPlaybackState(VideoCanvas::State::Empty); break;
        case PlayerEngine::State::Opening: setPlaybackState(VideoCanvas::State::Opening); break;
        case PlayerEngine::State::Buffering: setPlaybackState(VideoCanvas::State::Buffering); break;
        case PlayerEngine::State::Playing: setPlaybackState(m_engine->hasVideo() ? VideoCanvas::State::Playing : VideoCanvas::State::Audio); break;
        case PlayerEngine::State::Paused: setPlaybackState(VideoCanvas::State::Paused); break;
        case PlayerEngine::State::Stopped: setPlaybackState(m_playlist->count() ? VideoCanvas::State::Stopped : VideoCanvas::State::Empty); break;
        case PlayerEngine::State::Ended: setPlaybackState(VideoCanvas::State::Ended); break;
        case PlayerEngine::State::Error: setPlaybackState(VideoCanvas::State::Error); break;
        }
    });
    connect(m_engine, &PlayerEngine::bufferingChanged, this, [this](bool buffering, int percent) {
        if (buffering) { m_canvas->setBufferPercent(percent < 0 ? 0 : percent); setPlaybackState(VideoCanvas::State::Buffering, percent < 0 ? QStringLiteral("正在缓冲…") : QStringLiteral("正在缓冲 %1%").arg(percent)); }
    });
    connect(m_engine, &PlayerEngine::errorOccurred, this, [this](const QString &message, const QString &details) {
        m_statusText->setText(message); m_statusText->setToolTip(details); showToast(message);
    });
    connect(m_engine, &PlayerEngine::finished, this, [this] {
        if (m_mode == PlaybackMode::RepeatOne) selectItem(m_playlist->currentRow());
        else playNext();
    });
}

void MainWindow::addDemoItems()
{
    const QList<QPair<QString, QString>> items = {
        {QStringLiteral("在雾海边缘"), QStringLiteral("MKV · 本地视频                 58:20")},
        {QStringLiteral("城市雨夜电台"), QStringLiteral("FLAC · 本地音频                42:16")},
        {QStringLiteral("工业设计方法论 03"), QStringLiteral("MP4 · 本地视频              01:22:04")},
        {QStringLiteral("北岸慢行纪录"), QStringLiteral("MP4 · 本地视频                 24:39")},
        {QStringLiteral("产品访谈：从洞察到决策"), QStringLiteral("MKV · 本地视频                 53:08")},
        {QStringLiteral("Jazz Live Session"), QStringLiteral("FLAC · 本地音频                36:45")},
        {QStringLiteral("城市建筑观察"), QStringLiteral("MP4 · 本地视频                 18:22")},
        {QStringLiteral("远程协作工作坊"), QStringLiteral("MP4 · 本地视频              01:05:40")}
    };
    for (int i = 0; i < items.size(); ++i) {
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(items[i].first, items[i].second));
        item->setData(Qt::UserRole, i == 1 || i == 5 ? QStringLiteral("audio") : QStringLiteral("video"));
        item->setData(Qt::UserRole + 1, items[i].first);
        item->setData(Qt::UserRole + 2, items[i].second);
        m_playlist->addItem(item);
    }
    m_playlistCount->setText(QString::number(m_playlist->count()));
}

void MainWindow::addMediaFiles(const QStringList &files, bool startPlayback)
{
    int firstNew = m_playlist->count();
    for (const QString &file : files) {
        QFileInfo info(file); if (!info.exists() || !info.isFile()) continue;
        const QString kind = mediaKindForFile(file);
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2 · 本地%3").arg(info.completeBaseName(), info.suffix().toUpper(), kind == "audio" ? QStringLiteral("音频") : QStringLiteral("视频")));
        item->setData(Qt::UserRole, kind); item->setData(Qt::UserRole + 1, info.completeBaseName()); item->setData(Qt::UserRole + 2, file); item->setData(Qt::UserRole + 3, file);
        m_playlist->addItem(item);
    }
    m_playlistCount->setText(QString::number(m_playlist->count()));
    const int added = m_playlist->count() - firstNew;
    if (added) { showToast(QStringLiteral("已添加 %1 个媒体").arg(added)); if (startPlayback) selectItem(firstNew); }
}

void MainWindow::openFiles()
{
    const QString filter = QStringLiteral("媒体文件 (*.mp3 *.aac *.flac *.wav *.opus *.ogg *.m4a *.mp4 *.mkv *.mov *.webm *.avi *.ts);;所有文件 (*.*)");
    addMediaFiles(QFileDialog::getOpenFileNames(this, QStringLiteral("打开媒体文件"), QString(), filter));
}

void MainWindow::openUrlDialog()
{
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("打开网络媒体")); dialog.setModal(true); dialog.setFixedWidth(520);
    auto *layout = new QVBoxLayout(&dialog); layout->setContentsMargins(24, 22, 24, 22); layout->setSpacing(8);
    auto *title = makeLabel(QStringLiteral("打开网络媒体"), {}, &dialog); QFont f = title->font(); f.setPointSize(15); f.setWeight(QFont::DemiBold); title->setFont(f);
    auto *description = makeLabel(QStringLiteral("输入可直接访问的 HTTP/HTTPS 音视频地址。"), QStringLiteral("secondary"), &dialog);
    auto *input = new QLineEdit(&dialog); input->setPlaceholderText(QStringLiteral("https://example.com/media.mp4")); input->setClearButtonEnabled(true);
    auto *validation = makeLabel(QString(), {}, &dialog); validation->setStyleSheet(QStringLiteral("color:#F06A73;font-size:12px;min-height:18px;"));
    auto *buttons = new QHBoxLayout; buttons->setSpacing(8); buttons->addStretch();
    auto *cancel = makeTextButton(QStringLiteral("取消"), "secondary", &dialog); auto *open = makeTextButton(QStringLiteral("打开"), "primary", &dialog); open->setEnabled(false);
    cancel->setFixedHeight(36); open->setFixedHeight(36); buttons->addWidget(cancel); buttons->addWidget(open);
    layout->addWidget(title); layout->addWidget(description); layout->addSpacing(4); layout->addWidget(input); layout->addWidget(validation); layout->addSpacing(4); layout->addLayout(buttons);
    connect(input, &QLineEdit::textChanged, open, [open](const QString &text) { open->setEnabled(!text.trimmed().isEmpty()); });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(open, &QPushButton::clicked, &dialog, [&] {
        const QUrl url(input->text().trimmed());
        if (!url.isValid() || (url.scheme().compare("http", Qt::CaseInsensitive) && url.scheme().compare("https", Qt::CaseInsensitive))) {
            validation->setText(QStringLiteral("请输入有效的 HTTP 或 HTTPS 地址")); input->setProperty("error", true); input->style()->unpolish(input); input->style()->polish(input); input->setFocus(); return;
        }
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2 · 网络媒体").arg(QFileInfo(url.path()).fileName().isEmpty() ? url.host() : QFileInfo(url.path()).fileName(), url.host()));
        item->setData(Qt::UserRole, QStringLiteral("video")); item->setData(Qt::UserRole + 1, QFileInfo(url.path()).completeBaseName().isEmpty() ? url.host() : QFileInfo(url.path()).completeBaseName()); item->setData(Qt::UserRole + 2, url.host()); item->setData(Qt::UserRole + 3, url.toString());
        m_playlist->addItem(item); m_playlistCount->setText(QString::number(m_playlist->count())); m_playlist->setCurrentItem(item); dialog.accept();
    });
    input->setFocus();
    if (dialog.exec() == QDialog::Accepted) {
        updateCurrentMediaUi(); m_engine->open(m_playlist->currentItem()->data(Qt::UserRole + 3).toString());
    }
}

void MainWindow::togglePlayback()
{
    if (!m_playlist->count()) return;
    if (m_playlist->currentRow() < 0) m_playlist->setCurrentRow(0);
    switch (m_engine->state()) {
    case PlayerEngine::State::Playing:
        m_engine->pause();
        break;
    case PlayerEngine::State::Paused:
        m_engine->play();
        break;
    case PlayerEngine::State::Idle:
    case PlayerEngine::State::Stopped:
    case PlayerEngine::State::Ended:
    case PlayerEngine::State::Error:
        selectItem(m_playlist->currentRow());
        break;
    case PlayerEngine::State::Opening:
    case PlayerEngine::State::Buffering:
        break;
    }
}

void MainWindow::stopPlayback()
{
    m_engine->stop(); m_progress->setValue(0);
}

void MainWindow::selectItem(int row, bool startPlayback)
{
    if (row < 0 || row >= m_playlist->count()) return;
    m_playlist->setCurrentRow(row); updateCurrentMediaUi();
    if (startPlayback) {
        const QString source = m_playlist->item(row)->data(Qt::UserRole + 3).toString();
        if (source.isEmpty()) { showToast(QStringLiteral("演示项目没有关联媒体文件")); return; }
        m_engine->open(source);
    }
}

void MainWindow::playPrevious()
{
    if (!m_playlist->count()) return;
    const auto mode = static_cast<PlaybackPolicy::PlaylistMode>(m_mode);
    const int current = m_playlist->currentRow();
    const int row = PlaybackPolicy::previousIndex(current, m_playlist->count(), m_engine->position(), mode);
    if (row == current && m_engine->position() > 3000) { m_engine->seek(0); m_progress->setValue(0); return; }
    selectItem(row);
}

void MainWindow::playNext()
{
    if (!m_playlist->count()) return;
    const int row = PlaybackPolicy::nextIndex(m_playlist->currentRow(), m_playlist->count(), static_cast<PlaybackPolicy::PlaylistMode>(m_mode));
    if (row < 0) { setPlaybackState(VideoCanvas::State::Ended); return; }
    selectItem(row);
}

void MainWindow::showSpeedMenu()
{
    QMenu menu(this); QActionGroup group(&menu); group.setExclusive(true);
    const QStringList speeds = {"2.0×", "1.75×", "1.5×", "1.25×", "1.0×", "0.75×", "0.5×"};
    for (const QString &speed : speeds) { QAction *a = menu.addAction(speed); a->setCheckable(true); a->setChecked(speed == m_speedButton->text()); group.addAction(a); }
    if (QAction *chosen = menu.exec(m_speedButton->mapToGlobal(QPoint(0, -menu.sizeHint().height())))) { m_speedButton->setText(chosen->text()); m_engine->setSpeed(chosen->text().chopped(1).toDouble()); showToast(QStringLiteral("播放速度已调整为 %1").arg(chosen->text())); }
}

void MainWindow::showModeMenu()
{
    QMenu menu(this); QActionGroup group(&menu); group.setExclusive(true);
    const QList<QPair<QString, PlaybackMode>> modes = {{QStringLiteral("顺序播放"), PlaybackMode::Ordered}, {QStringLiteral("列表循环"), PlaybackMode::RepeatAll}, {QStringLiteral("单曲循环"), PlaybackMode::RepeatOne}};
    for (const auto &entry : modes) { QAction *a = menu.addAction(entry.first); a->setData(int(entry.second)); a->setCheckable(true); a->setChecked(entry.second == m_mode); group.addAction(a); }
    if (QAction *chosen = menu.exec(m_modeButton->mapToGlobal(QPoint(-72, -menu.sizeHint().height())))) {
        m_mode = PlaybackMode(chosen->data().toInt()); m_modeButton->setToolTip(chosen->text());
        m_modeButton->setIconType(m_mode == PlaybackMode::Ordered ? PlayerIcon::Ordered : m_mode == PlaybackMode::RepeatOne ? PlayerIcon::RepeatOne : PlayerIcon::Repeat);
        showToast(QStringLiteral("已切换为%1").arg(chosen->text()));
    }
}

void MainWindow::toggleMute()
{
    if (!m_muted) { m_volumeBeforeMute = m_volume->value(); m_muted = true; m_engine->setMuted(true); m_muteButton->setIconType(PlayerIcon::Muted); showToast(QStringLiteral("已静音")); }
    else { m_muted = false; m_volume->setValue(qMax(1, m_volumeBeforeMute)); m_engine->setMuted(false); m_muteButton->setIconType(PlayerIcon::Volume); showToast(QStringLiteral("已恢复音量")); }
}

void MainWindow::toggleFullscreen()
{
    if (m_canvas->state() == VideoCanvas::State::Audio || m_canvas->state() == VideoCanvas::State::Empty) return;
    m_fullscreen = !m_fullscreen;
    findChild<QWidget *>(QStringLiteral("toolbar"))->setVisible(!m_fullscreen);
    findChild<QWidget *>(QStringLiteral("statusRow"))->setVisible(!m_fullscreen);
    if (m_fullscreen) {
        m_playlistPanel->hide(); m_controls->show(); showFullScreen(); m_fullscreenControlsTimer->start();
    } else {
        m_fullscreenControlsTimer->stop(); m_controls->show(); showNormal(); m_playlistPanel->setVisible(m_playlistVisible);
    }
    m_fullscreenButton->setIconType(m_fullscreen ? PlayerIcon::ExitFullscreen : PlayerIcon::Fullscreen);
    m_fullscreenButton->setToolTip(m_fullscreen ? QStringLiteral("退出全屏（F）") : QStringLiteral("进入全屏（F）"));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_fullscreen && event->type() == QEvent::MouseMove) {
        if (m_controls) { m_controls->show(); m_controls->raise(); }
        if (m_fullscreenControlsTimer) m_fullscreenControlsTimer->start();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::togglePlaylist()
{
    if (m_fullscreen) return;
    m_playlistVisible = !m_playlistVisible; m_playlistPanel->setVisible(m_playlistVisible);
    if (m_playlistVisible) m_workspace->setSizes({qMax(600, width() - 360), 360});
    m_listButton->setToolTip(m_playlistVisible ? QStringLiteral("收起播放列表") : QStringLiteral("展开播放列表"));
}

void MainWindow::showToast(const QString &message)
{
    m_toast->setText(message); m_toast->adjustSize();
    const int w = qBound(260, m_toast->width(), 480); m_toast->resize(w, qMax(40, m_toast->height()));
    m_toast->move((centralWidget()->width() - m_toast->width()) / 2, centralWidget()->height() - 142); m_toast->show(); m_toast->raise(); m_toastTimer->start(3000);
}

void MainWindow::setPlaybackState(VideoCanvas::State state, const QString &status)
{
    m_canvas->setState(state);
    m_playing = state == VideoCanvas::State::Playing || state == VideoCanvas::State::Audio;
    m_playButton->setIconType(m_playing ? PlayerIcon::Pause : PlayerIcon::Play);
    m_playButton->setToolTip(m_playing ? QStringLiteral("暂停（Space）") : QStringLiteral("播放（Space）"));
    const bool hasMedia = m_playlist->currentRow() >= 0;
    m_progress->setEnabled(hasMedia && m_engine->isSeekable() && state != VideoCanvas::State::Opening && state != VideoCanvas::State::Error);
    m_stopButton->setEnabled(hasMedia && state != VideoCanvas::State::Stopped && state != VideoCanvas::State::Empty && state != VideoCanvas::State::Ended);
    const bool isAudio = hasMedia && m_playlist->currentItem()->data(Qt::UserRole).toString() == QStringLiteral("audio");
    m_fullscreenButton->setEnabled(hasMedia && !isAudio && state != VideoCanvas::State::Error);
    QString defaultStatus;
    QString color = QStringLiteral("#41C98E");
    switch (state) {
    case VideoCanvas::State::Empty: defaultStatus = QStringLiteral("尚未播放"); color = QStringLiteral("#737D8E"); break;
    case VideoCanvas::State::Opening: defaultStatus = QStringLiteral("正在连接网络媒体…"); color = QStringLiteral("#F2B84B"); break;
    case VideoCanvas::State::Playing: case VideoCanvas::State::Audio: defaultStatus = QStringLiteral("正在播放"); break;
    case VideoCanvas::State::Paused: defaultStatus = QStringLiteral("已暂停"); break;
    case VideoCanvas::State::Buffering: defaultStatus = QStringLiteral("正在缓冲 42%"); color = QStringLiteral("#F2B84B"); break;
    case VideoCanvas::State::Error: defaultStatus = QStringLiteral("播放失败"); color = QStringLiteral("#F06A73"); break;
    case VideoCanvas::State::Ended: defaultStatus = QStringLiteral("播放结束"); break;
    case VideoCanvas::State::Stopped: defaultStatus = QStringLiteral("已停止"); break;
    }
    m_statusText->setText(status.isEmpty() ? defaultStatus : status);
    m_statusDot->setStyleSheet(QStringLiteral("background:%1;border-radius:3px;").arg(color));
    m_progressTimer->stop();
}

void MainWindow::updateCurrentMediaUi()
{
    QListWidgetItem *item = m_playlist->currentItem(); if (!item) return;
    const QString title = item->data(Qt::UserRole + 1).toString().isEmpty() ? item->text().section('\n', 0, 0) : item->data(Qt::UserRole + 1).toString();
    const QString info = item->data(Qt::UserRole + 2).toString().isEmpty() ? item->text().section('\n', 1, 1) : item->data(Qt::UserRole + 2).toString();
    const bool audio = item->data(Qt::UserRole).toString() == QStringLiteral("audio");
    const QString source = item->data(Qt::UserRole + 3).toString(); const QUrl sourceUrl(source);
    const bool network = sourceUrl.scheme() == QStringLiteral("http") || sourceUrl.scheme() == QStringLiteral("https");
    const QString suffix = QFileInfo(network ? sourceUrl.path() : source).suffix().toUpper();
    const QString kind = audio ? QStringLiteral("音频") : QStringLiteral("视频");
    const QString origin = network ? QStringLiteral("网络%1").arg(kind) : QStringLiteral("本地%1").arg(kind);
    m_nowTitle->setText(title); m_nowTitle->setToolTip(title); m_nowSubtitle->setText(QStringLiteral("%1 · %2").arg(origin, suffix.isEmpty() ? QStringLiteral("未知格式") : suffix));
    m_canvas->setMediaTitle(title, QStringLiteral("%1 · %2").arg(kind, suffix.isEmpty() ? QStringLiteral("未知格式") : suffix));
    m_mediaInfo->clear();
    m_durationSeconds = 0; m_progress->setRange(0, 0); m_progress->setValue(0); m_progress->setBufferedValue(0); m_totalTime->setText(QStringLiteral("未知")); m_currentTime->setText(QStringLiteral("00:00"));
    Q_UNUSED(info)
}

void MainWindow::updateResponsiveLayout()
{
    if (width() < 1100) { m_brandName->hide(); m_volume->setFixedWidth(64); m_nowSubtitle->hide(); }
    else { m_brandName->show(); m_volume->setFixedWidth(width() < 1280 ? 64 : 88); m_nowSubtitle->show(); }
    if (m_playlistVisible && !m_fullscreen) {
        const int target = width() < 1100 ? 300 : width() < 1280 ? 320 : 360;
        m_workspace->setSizes({qMax(500, m_workspace->width() - target), target});
    }
}

QString MainWindow::formatTime(int seconds) const
{
    const int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    return h > 0 ? QStringLiteral("%1:%2:%3").arg(h, 2, 10, QLatin1Char('0')).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'))
                 : QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_fullscreen) { toggleFullscreen(); return; }
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_O) { openFiles(); return; }
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_U) { openUrlDialog(); return; }
    if (event->key() == Qt::Key_Space && !qobject_cast<QLineEdit *>(focusWidget())) { togglePlayback(); return; }
    if (event->key() == Qt::Key_F) { toggleFullscreen(); return; }
    if (event->key() == Qt::Key_M) { toggleMute(); return; }
    if (event->key() == Qt::Key_N) { playNext(); return; }
    if (event->key() == Qt::Key_P) { playPrevious(); return; }
    if (event->key() == Qt::Key_Left && m_progress->isEnabled()) { m_progress->setValue(qMax(0, m_progress->value() - 5)); return; }
    if (event->key() == Qt::Key_Right && m_progress->isEnabled()) { m_progress->setValue(qMin(m_durationSeconds, m_progress->value() + 5)); return; }
    if (event->key() == Qt::Key_Up) { m_volume->setValue(qMin(100, m_volume->value() + 5)); return; }
    if (event->key() == Qt::Key_Down) { m_volume->setValue(qMax(0, m_volume->value() - 5)); return; }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event); updateResponsiveLayout();
    if (m_toast && m_toast->isVisible()) m_toast->move((centralWidget()->width() - m_toast->width()) / 2, centralWidget()->height() - 142);
}
