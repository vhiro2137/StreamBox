#pragma once

#include "playerwidgets.h"

#include <QMainWindow>

class QLabel;
class QListWidget;
class QSplitter;
class QStackedWidget;
class QTimer;
class QWidget;
class PlayerEngine;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openMedia(const QString &source);
    void setPlaybackSpeed(double speed);
    void seekTo(qint64 milliseconds);
    void setPlayerFullscreen(bool fullscreen);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class PlaybackMode { Ordered, RepeatAll, RepeatOne };

    void buildUi();
    QWidget *createToolbar();
    QWidget *createPlaylistPanel();
    QWidget *createControls();
    void connectActions();
    void addDemoItems();
    void addMediaFiles(const QStringList &files, bool startPlayback = true);
    void openFiles();
    void openUrlDialog();
    void togglePlayback();
    void stopPlayback();
    void selectItem(int row, bool startPlayback = true);
    void playPrevious();
    void playNext();
    void showSpeedMenu();
    void showModeMenu();
    void toggleMute();
    void toggleFullscreen();
    void togglePlaylist();
    void showToast(const QString &message);
    void setPlaybackState(VideoCanvas::State state, const QString &status = QString());
    void updateCurrentMediaUi();
    void updateResponsiveLayout();
    QString formatTime(int seconds) const;

    VideoCanvas *m_canvas = nullptr;
    QSplitter *m_workspace = nullptr;
    QWidget *m_playlistPanel = nullptr;
    QListWidget *m_playlist = nullptr;
    QLabel *m_playlistCount = nullptr;
    QLabel *m_currentTime = nullptr;
    QLabel *m_totalTime = nullptr;
    ProgressSlider *m_progress = nullptr;
    QLabel *m_nowTitle = nullptr;
    QLabel *m_nowSubtitle = nullptr;
    QLabel *m_mediaInfo = nullptr;
    QLabel *m_statusText = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_brandName = nullptr;
    QLabel *m_toast = nullptr;
    IconButton *m_playButton = nullptr;
    IconButton *m_stopButton = nullptr;
    IconButton *m_muteButton = nullptr;
    IconButton *m_fullscreenButton = nullptr;
    IconButton *m_modeButton = nullptr;
    IconButton *m_listButton = nullptr;
    QPushButton *m_speedButton = nullptr;
    QSlider *m_volume = nullptr;
    QTimer *m_toastTimer = nullptr;
    QTimer *m_progressTimer = nullptr;
    QTimer *m_fullscreenControlsTimer = nullptr;
    QWidget *m_controls = nullptr;
    PlayerEngine *m_engine = nullptr;
    bool m_playing = false;
    bool m_muted = false;
    bool m_playlistVisible = true;
    bool m_fullscreen = false;
    int m_volumeBeforeMute = 70;
    int m_durationSeconds = 3500;
    PlaybackMode m_mode = PlaybackMode::RepeatAll;
};
