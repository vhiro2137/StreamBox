#include "playerwidgets.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>
#include <QtMath>

namespace {
constexpr auto Accent = "#6C8CFF";
constexpr auto TextPrimary = "#F3F5F7";
constexpr auto TextSecondary = "#AEB6C4";
constexpr auto TextTertiary = "#737D8E";
constexpr auto Disabled = "#525A68";
constexpr auto Error = "#F06A73";

void paintPlayerIcon(QPainter &p, PlayerIcon icon, const QRectF &r, const QColor &color, qreal lineWidth = 1.8)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal x = r.x(), y = r.y(), w = r.width(), h = r.height();
    auto pt = [&](qreal px, qreal py) { return QPointF(x + px * w / 24.0, y + py * h / 24.0); };

    QPainterPath path;
    switch (icon) {
    case PlayerIcon::Play:
        path.moveTo(pt(8, 5)); path.lineTo(pt(19, 12)); path.lineTo(pt(8, 19)); path.closeSubpath();
        p.setPen(Qt::NoPen); p.setBrush(color); p.drawPath(path); break;
    case PlayerIcon::Pause:
        p.drawLine(pt(8, 6), pt(8, 18)); p.drawLine(pt(16, 6), pt(16, 18)); break;
    case PlayerIcon::Folder:
        path.moveTo(pt(3, 7)); path.lineTo(pt(9, 7)); path.lineTo(pt(11, 9)); path.lineTo(pt(21, 9));
        path.lineTo(pt(20, 19)); path.lineTo(pt(3, 19)); path.closeSubpath(); p.drawPath(path); break;
    case PlayerIcon::Link:
        p.drawLine(pt(9, 15), pt(15, 9));
        p.drawArc(QRectF(pt(1, 9), pt(11, 21)), 40 * 16, 210 * 16);
        p.drawArc(QRectF(pt(12, 3), pt(23, 15)), 220 * 16, 210 * 16); break;
    case PlayerIcon::List:
        for (int yy : {6, 12, 18}) { p.drawPoint(pt(4, yy)); p.drawLine(pt(8, yy), pt(20, yy)); } break;
    case PlayerIcon::More:
        p.setBrush(color); p.setPen(Qt::NoPen);
        for (int xx : {5, 12, 19}) p.drawEllipse(pt(xx, 12), 1.3, 1.3); break;
    case PlayerIcon::Previous:
        p.drawLine(pt(6, 5), pt(6, 19)); path.moveTo(pt(18, 6)); path.lineTo(pt(9, 12)); path.lineTo(pt(18, 18)); path.closeSubpath(); p.drawPath(path); break;
    case PlayerIcon::Next:
        p.drawLine(pt(18, 5), pt(18, 19)); path.moveTo(pt(6, 6)); path.lineTo(pt(15, 12)); path.lineTo(pt(6, 18)); path.closeSubpath(); p.drawPath(path); break;
    case PlayerIcon::Stop:
        p.setPen(Qt::NoPen); p.setBrush(color); p.drawRoundedRect(QRectF(pt(7, 7), pt(17, 17)), 1.5, 1.5); break;
    case PlayerIcon::Volume:
    case PlayerIcon::Muted:
        path.moveTo(pt(4, 10)); path.lineTo(pt(8, 10)); path.lineTo(pt(13, 6)); path.lineTo(pt(13, 18)); path.lineTo(pt(8, 14)); path.lineTo(pt(4, 14)); path.closeSubpath(); p.drawPath(path);
        if (icon == PlayerIcon::Muted) { p.drawLine(pt(17, 9), pt(21, 15)); p.drawLine(pt(21, 9), pt(17, 15)); }
        else { p.drawArc(QRectF(pt(13, 7), pt(21, 17)), -55 * 16, 110 * 16); p.drawArc(QRectF(pt(13, 4), pt(24, 20)), -48 * 16, 96 * 16); }
        break;
    case PlayerIcon::Repeat:
    case PlayerIcon::RepeatOne:
        path.moveTo(pt(17, 4)); path.lineTo(pt(21, 7)); path.lineTo(pt(17, 10)); p.drawPath(path); p.drawLine(pt(4, 7), pt(20, 7));
        path = QPainterPath(); path.moveTo(pt(7, 20)); path.lineTo(pt(3, 17)); path.lineTo(pt(7, 14)); p.drawPath(path); p.drawLine(pt(4, 17), pt(20, 17));
        if (icon == PlayerIcon::RepeatOne) p.drawText(QRectF(pt(8, 8), pt(16, 16)), Qt::AlignCenter, QStringLiteral("1")); break;
    case PlayerIcon::Ordered:
        p.drawLine(pt(5, 6), pt(19, 6)); p.drawLine(pt(5, 12), pt(19, 12)); p.drawLine(pt(5, 18), pt(16, 18));
        path.moveTo(pt(16, 15)); path.lineTo(pt(20, 18)); path.lineTo(pt(16, 21)); p.drawPath(path); break;
    case PlayerIcon::Fullscreen:
    case PlayerIcon::ExitFullscreen: {
        const bool inward = icon == PlayerIcon::ExitFullscreen;
        const qreal a = inward ? 8 : 3, b = inward ? 3 : 8, c = inward ? 16 : 21, d = inward ? 21 : 16;
        p.drawLine(pt(a, b), pt(a, a)); p.drawLine(pt(b, a), pt(a, a));
        p.drawLine(pt(c, b), pt(c, a)); p.drawLine(pt(d, a), pt(c, a));
        p.drawLine(pt(a, d), pt(a, c)); p.drawLine(pt(b, c), pt(a, c));
        p.drawLine(pt(c, d), pt(c, c)); p.drawLine(pt(d, c), pt(c, c)); break;
    }
    case PlayerIcon::Media:
        p.drawRoundedRect(QRectF(pt(3, 5), pt(21, 19)), 2, 2); path.moveTo(pt(10, 9)); path.lineTo(pt(16, 12)); path.lineTo(pt(10, 15)); path.closeSubpath(); p.drawPath(path); break;
    case PlayerIcon::Warning:
        path.moveTo(pt(12, 3)); path.lineTo(pt(22, 20)); path.lineTo(pt(2, 20)); path.closeSubpath(); p.drawPath(path); p.drawLine(pt(12, 9), pt(12, 14)); p.drawPoint(pt(12, 17)); break;
    case PlayerIcon::Music:
        p.drawLine(pt(9, 17), pt(9, 6)); p.drawLine(pt(9, 6), pt(19, 4)); p.drawLine(pt(19, 4), pt(19, 15));
        p.drawEllipse(QRectF(pt(3, 14), pt(9, 20))); p.drawEllipse(QRectF(pt(13, 12), pt(19, 18))); break;
    case PlayerIcon::Close:
        p.drawLine(pt(6, 6), pt(18, 18)); p.drawLine(pt(18, 6), pt(6, 18)); break;
    case PlayerIcon::Trash:
        p.drawLine(pt(7, 8), pt(8, 20)); p.drawLine(pt(17, 8), pt(16, 20)); p.drawLine(pt(8, 20), pt(16, 20));
        p.drawLine(pt(5, 7), pt(19, 7)); p.drawLine(pt(9, 4), pt(15, 4)); break;
    case PlayerIcon::Add:
        p.drawLine(pt(12, 5), pt(12, 19)); p.drawLine(pt(5, 12), pt(19, 12)); break;
    }
    p.restore();
}

QColor stateColor(const QWidget *widget)
{
    if (!widget->isEnabled()) return QColor(Disabled);
    if (widget->underMouse()) return QColor(TextPrimary);
    return QColor(TextSecondary);
}
}

IconButton::IconButton(PlayerIcon icon, QWidget *parent) : QPushButton(parent), m_icon(icon)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(32, 32);
    setProperty("class", "iconButton");
}

void IconButton::setIconType(PlayerIcon icon) { m_icon = icon; update(); }

void IconButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);
    QPainter painter(this);
    QColor color = stateColor(this);
    if (property("accentIcon").toBool()) color = Qt::white;
    const int size = property("largeIcon").toBool() ? 24 : 20;
    QRectF iconRect((width() - size) / 2.0, (height() - size) / 2.0, size, size);
    paintPlayerIcon(painter, m_icon, iconRect, color);
}

ProgressSlider::ProgressSlider(Qt::Orientation orientation, QWidget *parent) : QSlider(orientation, parent)
{
    setRange(0, 1000);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(20);
}

void ProgressSlider::setBufferedValue(int value) { m_bufferedValue = qBound(minimum(), value, maximum()); update(); }

void ProgressSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(0, (height() - 4) / 2.0, width(), 4);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#343A46")); p.drawRoundedRect(track, 2, 2);
    const qreal buffered = width() * (m_bufferedValue - minimum()) / qreal(maximum() - minimum());
    p.setBrush(QColor("#596273")); p.drawRoundedRect(QRectF(track.x(), track.y(), buffered, track.height()), 2, 2);
    const qreal played = width() * (value() - minimum()) / qreal(maximum() - minimum());
    p.setBrush(isEnabled() ? QColor(Accent) : QColor(Disabled)); p.drawRoundedRect(QRectF(track.x(), track.y(), played, track.height()), 2, 2);
    const qreal knobSize = underMouse() || isSliderDown() ? 14 : 10;
    p.drawEllipse(QPointF(played, height() / 2.0), knobSize / 2.0, knobSize / 2.0);
}

void ProgressSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), int(event->position().x()), width()));
        emit sliderMoved(value());
    }
    QSlider::mousePressEvent(event);
}

VideoCanvas::VideoCanvas(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("videoCanvas"));
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);
    setMinimumSize(500, 300);
}

void VideoCanvas::setState(State state) { m_state = state; update(); }
void VideoCanvas::setMediaTitle(const QString &title, const QString &subtitle) { m_title = title; m_subtitle = subtitle; update(); }
void VideoCanvas::setBufferPercent(int percent) { m_bufferPercent = qBound(0, percent, 100); update(); }
void VideoCanvas::setVideoFrame(const QImage &frame) { m_videoFrame = frame; update(); }

QRect VideoCanvas::centeredButtonRect(int index, int count) const
{
    const int w = index == 0 ? 104 : 104, h = 36, gap = 8;
    const int total = count * w + (count - 1) * gap;
    return QRect((width() - total) / 2 + index * (w + gap), height() / 2 + 92, w, h);
}

void VideoCanvas::drawIcon(QPainter &painter, PlayerIcon icon, const QRectF &rect, const QColor &color, qreal width) const
{
    paintPlayerIcon(painter, icon, rect, color, width);
}

void VideoCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#000000"));

    if (!m_videoFrame.isNull() && (m_state == State::Playing || m_state == State::Paused || m_state == State::Buffering || m_state == State::Ended)) {
        const QSize scaled = m_videoFrame.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target((width() - scaled.width()) / 2, (height() - scaled.height()) / 2, scaled.width(), scaled.height());
        p.drawImage(target, m_videoFrame);
    } else if (m_state == State::Playing || m_state == State::Paused || m_state == State::Buffering || m_state == State::Ended) {
        QRadialGradient glow(QPointF(width() * .6, height() * .35), qMax(width(), height()) * .7);
        glow.setColorAt(0, QColor("#293B49")); glow.setColorAt(.42, QColor("#101820")); glow.setColorAt(1, QColor("#05080B"));
        p.fillRect(rect(), glow);
        p.setPen(QColor(243, 245, 247, 175));
        p.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
        p.drawText(QRect(32, height() - 52, width() - 64, 24), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("北纬 31° · 夜航记录"));
    }

    if (m_dragActive) {
        p.fillRect(rect(), QColor(108, 140, 255, 28));
        p.setPen(QPen(QColor(Accent), 2)); p.drawRect(rect().adjusted(2, 2, -3, -3));
        p.setPen(QColor(TextPrimary)); p.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 15, QFont::DemiBold));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("释放以添加到播放列表"));
        return;
    }

    if (m_state == State::Playing) return;
    if (m_state == State::Paused) {
        p.fillRect(rect(), QColor(0, 0, 0, 82));
        const QRectF circle(width() / 2.0 - 28, height() / 2.0 - 28, 56, 56);
        p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 150)); p.drawEllipse(circle);
        drawIcon(p, PlayerIcon::Pause, QRectF(width()/2.0-14, height()/2.0-14, 28, 28), QColor(TextPrimary));
        return;
    }

    p.fillRect(rect(), QColor(0, 0, 0, m_state == State::Audio || m_state == State::Empty ? 0 : 158));
    const int cx = width() / 2;
    QFont titleFont(QStringLiteral("Microsoft YaHei UI"), 15, QFont::DemiBold);
    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"), 10);
    QFont smallFont(QStringLiteral("Microsoft YaHei UI"), 9);

    if (m_state == State::Empty) {
        drawIcon(p, PlayerIcon::Media, QRectF(cx - 32, height()/2.0 - 108, 64, 64), QColor(TextTertiary), 1.8);
        p.setFont(titleFont); p.setPen(QColor(TextPrimary)); p.drawText(QRect(0, height()/2-28, width(), 30), Qt::AlignCenter, QStringLiteral("打开媒体开始播放"));
        p.setFont(bodyFont); p.setPen(QColor(TextSecondary)); p.drawText(QRect(0, height()/2+7, width(), 24), Qt::AlignCenter, QStringLiteral("选择本地音视频文件，或粘贴 HTTP/HTTPS 媒体地址"));
        for (int i = 0; i < 2; ++i) {
            QRect br = centeredButtonRect(i, 2); p.setPen(i == 0 ? Qt::NoPen : QPen(QColor("#303642"))); p.setBrush(i == 0 ? QColor(Accent) : QColor("#1D212A")); p.drawRoundedRect(br, 8, 8);
            p.setPen(i == 0 ? Qt::white : QColor(TextPrimary)); p.setFont(bodyFont); p.drawText(br, Qt::AlignCenter, i == 0 ? QStringLiteral("打开文件") : QStringLiteral("打开 URL"));
        }
        p.setFont(smallFont); p.setPen(QColor(TextTertiary)); p.drawText(QRect(0, height()/2+138, width(), 22), Qt::AlignCenter, QStringLiteral("也可以将文件拖放到此处"));
        return;
    }

    if (m_state == State::Audio) {
        QLinearGradient bg(0, 0, 0, height()); bg.setColorAt(0, QColor("#10131A")); bg.setColorAt(1, QColor("#171C28")); p.fillRect(rect(), bg);
        QRectF cover(cx - 96, height()/2.0 - 152, 192, 192); p.setPen(Qt::NoPen); p.setBrush(QColor("#252F43")); p.drawRoundedRect(cover, 12, 12);
        drawIcon(p, PlayerIcon::Music, QRectF(cx - 44, height()/2.0 - 100, 88, 88), QColor(Accent), 1.8);
        p.setFont(titleFont); p.setPen(QColor(TextPrimary)); p.drawText(QRect(40, height()/2+60, width()-80, 30), Qt::AlignCenter, m_title.isEmpty() ? QStringLiteral("城市雨夜电台") : m_title);
        p.setFont(smallFont); p.setPen(QColor(TextSecondary)); p.drawText(QRect(40, height()/2+92, width()-80, 20), Qt::AlignCenter, m_subtitle.isEmpty() ? QStringLiteral("音频 · FLAC") : m_subtitle);
        return;
    }

    PlayerIcon icon = PlayerIcon::Media;
    QString title, subtitle;
    if (m_state == State::Opening) { title = QStringLiteral("正在连接网络媒体…"); subtitle = QStringLiteral("请稍候，正在读取媒体信息"); }
    else if (m_state == State::Buffering) { title = QStringLiteral("正在缓冲 %1%").arg(m_bufferPercent); subtitle = QStringLiteral("网络连接较慢，播放将在缓冲完成后继续"); }
    else if (m_state == State::Error) { icon = PlayerIcon::Warning; title = QStringLiteral("无法播放此媒体"); subtitle = QStringLiteral("无法连接到服务器，请检查网络或地址。"); }
    else if (m_state == State::Ended) { icon = PlayerIcon::Play; title = QStringLiteral("播放结束"); subtitle = QStringLiteral("可以重新播放或切换到其他媒体"); }
    else { icon = PlayerIcon::Play; title = QStringLiteral("已停止"); subtitle = QStringLiteral("点击播放从头开始"); }

    if (m_state == State::Opening || m_state == State::Buffering) {
        p.setPen(QPen(QColor("#414753"), 3)); p.drawEllipse(QRectF(cx-16, height()/2.0-92, 32, 32));
        p.setPen(QPen(QColor(Accent), 3)); p.drawArc(QRectF(cx-16, height()/2.0-92, 32, 32), 45*16, 115*16);
    } else drawIcon(p, icon, QRectF(cx-20, height()/2.0-96, 40, 40), m_state == State::Error ? QColor(Error) : QColor(TextTertiary));
    p.setFont(titleFont); p.setPen(QColor(TextPrimary)); p.drawText(QRect(20, height()/2-40, width()-40, 30), Qt::AlignCenter, title);
    p.setFont(bodyFont); p.setPen(QColor(TextSecondary)); p.drawText(QRect(30, height()/2-3, width()-60, 24), Qt::AlignCenter, subtitle);

    const int count = m_state == State::Error ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        QRect br = centeredButtonRect(i, count); p.setPen(i == 0 && m_state == State::Error ? Qt::NoPen : QPen(QColor("#303642")));
        p.setBrush(i == 0 && m_state == State::Error ? QColor(Accent) : QColor("#1D212A")); p.drawRoundedRect(br, 8, 8);
        p.setPen(i == 0 && m_state == State::Error ? Qt::white : QColor(TextPrimary)); p.setFont(bodyFont);
        const QString text = m_state == State::Error ? (i == 0 ? QStringLiteral("重试") : QStringLiteral("打开其他媒体")) : QStringLiteral("取消");
        p.drawText(br, Qt::AlignCenter, text);
    }
}

void VideoCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return QFrame::mouseReleaseEvent(event);
    if (m_state == State::Empty) {
        if (centeredButtonRect(0, 2).contains(event->position().toPoint())) emit openFilesRequested();
        else if (centeredButtonRect(1, 2).contains(event->position().toPoint())) emit openUrlRequested();
    } else if (m_state == State::Error) {
        if (centeredButtonRect(0, 2).contains(event->position().toPoint())) emit retryRequested();
        else if (centeredButtonRect(1, 2).contains(event->position().toPoint())) emit openUrlRequested();
    } else if ((m_state == State::Opening || m_state == State::Buffering) && centeredButtonRect(0, 1).contains(event->position().toPoint())) emit cancelRequested();
    QFrame::mouseReleaseEvent(event);
}

void VideoCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_state != State::Audio && m_state != State::Empty) emit fullscreenRequested();
    QFrame::mouseDoubleClickEvent(event);
}

void VideoCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) { m_dragActive = true; event->acceptProposedAction(); update(); }
}

void VideoCanvas::dragLeaveEvent(QDragLeaveEvent *event) { m_dragActive = false; update(); QFrame::dragLeaveEvent(event); }

void VideoCanvas::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) if (url.isLocalFile()) files << url.toLocalFile();
    m_dragActive = false; update();
    if (!files.isEmpty()) { emit filesDropped(files); event->acceptProposedAction(); }
}

QString streamBoxStyleSheet()
{
    return QString::fromUtf8(R"QSS(
* { font-family: "Microsoft YaHei UI", "Segoe UI"; font-size: 14px; color: #F3F5F7; }
QMainWindow, QWidget#central { background: #111318; }
QWidget#toolbar, QWidget#controls { background: #171A21; }
QWidget#toolbar { border-bottom: 1px solid #303642; }
QWidget#controls { border-top: 1px solid #303642; }
QWidget#playlistPanel { background: #1D212A; border-left: 1px solid #303642; }
QWidget#playlistHeader { border-bottom: 1px solid #303642; }
QLabel#brand { font-weight: 600; }
QLabel#secondary, QLabel.secondary { color: #AEB6C4; }
QLabel#tertiary, QLabel.tertiary { color: #737D8E; font-size: 12px; }
QPushButton { min-height: 32px; border: 1px solid transparent; border-radius: 8px; padding: 0 12px; background: transparent; color: #AEB6C4; }
QPushButton:hover { background: #252A35; color: #F3F5F7; }
QPushButton:pressed { background: #20242D; }
QPushButton:focus { border: 2px solid #6C8CFF; }
QPushButton:disabled { color: #525A68; background: transparent; border-color: transparent; }
QPushButton[class="secondary"] { border: 1px solid #303642; background: #1D212A; color: #F3F5F7; }
QPushButton[class="primary"] { background: #6C8CFF; color: white; border: none; }
QPushButton[class="primary"]:hover { background: #7D9AFF; }
QPushButton[class="primary"]:pressed { background: #5878E8; }
QPushButton[class="iconButton"] { padding: 0; min-height: 0; }
QPushButton#playMain { background: #6C8CFF; border-radius: 22px; }
QPushButton#playMain:hover { background: #7D9AFF; }
QPushButton#logo { background: #6C8CFF; border-radius: 6px; padding: 0; }
QListWidget { background: #1D212A; border: none; outline: none; padding-top: 4px; }
QListWidget::item { height: 62px; border: none; padding-left: 12px; color: #AEB6C4; }
QListWidget::item:hover { background: #252A35; }
QListWidget::item:selected { background: #283354; color: #F3F5F7; border-left: 3px solid #6C8CFF; }
QListWidget QScrollBar:vertical { width: 8px; background: transparent; margin: 2px; }
QListWidget QScrollBar::handle:vertical { background: #3A414E; border-radius: 4px; min-height: 32px; }
QListWidget QScrollBar::add-line:vertical, QListWidget QScrollBar::sub-line:vertical { height: 0; }
QSlider#volume::groove:horizontal { height: 3px; background: #343A46; border-radius: 1px; }
QSlider#volume::sub-page:horizontal { background: #AEB6C4; border-radius: 1px; }
QSlider#volume::handle:horizontal { width: 10px; margin: -4px 0; border-radius: 5px; background: #F3F5F7; }
QMenu { background: #1D212A; border: 1px solid #303642; border-radius: 8px; padding: 6px; }
QMenu::item { height: 32px; min-width: 122px; padding: 2px 28px 2px 30px; border-radius: 6px; color: #AEB6C4; }
QMenu::item:selected { background: #252A35; color: #F3F5F7; }
QMenu::indicator:checked { image: none; background: #6C8CFF; width: 6px; height: 6px; border-radius: 3px; margin-left: 12px; }
QDialog { background: #1D212A; }
QLineEdit { height: 40px; border: 1px solid #303642; border-radius: 8px; background: #252A35; color: #F3F5F7; padding: 0 12px; selection-background-color: #6C8CFF; }
QLineEdit:focus { border: 2px solid #6C8CFF; }
QLineEdit[error="true"] { border: 1px solid #F06A73; }
QToolTip { color: #F3F5F7; background: #252A35; border: 1px solid #303642; padding: 6px; }
QSplitter::handle { background: #303642; width: 1px; }
)QSS");
}
