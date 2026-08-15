#pragma once

#include <QFrame>
#include <QImage>
#include <QPushButton>
#include <QSlider>

class QLabel;
class QPaintEvent;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;

enum class PlayerIcon {
    Play, Pause, Folder, Link, List, More, Previous, Next, Stop,
    Volume, Muted, Repeat, RepeatOne, Ordered, Fullscreen, ExitFullscreen,
    Media, Warning, Music, Close, Trash, Add
};

class IconButton final : public QPushButton
{
    Q_OBJECT
public:
    explicit IconButton(PlayerIcon icon, QWidget *parent = nullptr);
    void setIconType(PlayerIcon icon);
    PlayerIcon iconType() const { return m_icon; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    PlayerIcon m_icon;
};

class ProgressSlider final : public QSlider
{
    Q_OBJECT
public:
    explicit ProgressSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    void setBufferedValue(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_bufferedValue = 0;
};

class VideoCanvas final : public QFrame
{
    Q_OBJECT
public:
    enum class State { Empty, Opening, Playing, Paused, Buffering, Audio, Error, Ended, Stopped };

    explicit VideoCanvas(QWidget *parent = nullptr);
    void setState(State state);
    State state() const { return m_state; }
    void setMediaTitle(const QString &title, const QString &subtitle = QString());
    void setBufferPercent(int percent);
    void setVideoFrame(const QImage &frame);

signals:
    void openFilesRequested();
    void openUrlRequested();
    void cancelRequested();
    void retryRequested();
    void fullscreenRequested();
    void filesDropped(const QStringList &files);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QRect centeredButtonRect(int index, int count) const;
    void drawIcon(QPainter &painter, PlayerIcon icon, const QRectF &rect, const QColor &color, qreal width = 2.0) const;
    State m_state = State::Empty;
    QString m_title;
    QString m_subtitle;
    int m_bufferPercent = 42;
    bool m_dragActive = false;
    QImage m_videoFrame;
};

QString streamBoxStyleSheet();
