#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "SongDatabase.h"
#include "Midi/MidiPlayer.h"
#include <LyricsWidget.h>
#include <ChannelMixer.h>

#include "Dialogs/Equalizer31BandDialog.h"

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QLocale>
#include <QSettings>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    SongDatabase* database() { return db; }
    MidiPlayer* midiPlayer() { return player; }
    LyricsWidget* lyricsWidget() { return lyrWidget; }

    bool removeFromPlaylist() { return remove_playlist; }
    bool autoPlayNext() { return auto_playnext; }
    int searchTimeout() { return search_timeout / 1000; }
    int playlistTimout() { return playlist_timeout / 1000; }
    void setRemoveFromPlaylist(bool r) { remove_playlist = r; }
    void setAutoPlayNext(bool p) { auto_playnext = p; }
    void setSearchTimeout(int s) { search_timeout = s*1000; }
    void setPlaylistTimeout(int s) { playlist_timeout = s*1000; }
    void setBackgroundColor(QString colorName);
    void setBackgroundImage(QString img);

    // Synth effect dialog
    Equalizer31BandDialog* equalizer31BandDialog() { return eq31Dlg; }

public slots:
    void play(int index);
    void pause();
    void resume();
    void stop();
    void playNext();
    void playPrevious();

signals:
    void resized(const QSize &s);

protected:
    void mouseMoveEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::MainWindow *ui;
    QSettings *settings;
    SongDatabase *db;
    QTimer *timer1, *timer2, *positionTimer;

    QList<Song*> playlist;
    MidiPlayer *player;
    Song playingSong;
    int playingIndex = -1;
    bool playAfterSeek = false;

    LyricsWidget *lyrWidget;

    int bgType = 0;
    QString bgImg = "";
    bool remove_playlist = true;
    bool auto_playnext = true;
    int search_timeout = 5000;
    int playlist_timeout = 5000;

    QLocale locale;

    // Synth effect dialog
    Equalizer31BandDialog *eq31Dlg;


private slots:
    void showCurrentTime();
    void setFrameSearch(Song* s);

    void showContextMenu(const QPoint &pos);
    void showSettingsDialog();
    void showHideChMix();
    void showFullScreenOrNormal();

    void onPositiomTimerTimeOut();
    void onPlayerDurationMSChanged(qint64 d);
    void onPlayerPositionMSChanged(qint64 p);
    void onPlayerDurationTickChanged(int d);

    void onSliderPositionPressed();
    void onSliderPositionReleased();

    void on_btnVolumeMute_clicked();
    void on_btnPlay_clicked();
    void onSliderVolumeValueChanged(int value);

    void onPlayerThreadFinished();
    void onPlayerPlayingEvent(MidiEvent *e);
};

#endif // MAINWINDOW_H
