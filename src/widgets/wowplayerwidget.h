//
// Created by rapeafed on 2021.
//

#ifndef WOWLET_WOWPLAYERWIDGET_H
#define WOWLET_WOWPLAYERWIDGET_H

#include <QWidget>
#include <QList>
#include <QtMultimedia>
#include <QtMultimediaWidgets>
#include <QtMultimedia/QMediaPlayer>
#include "wowplaylistmodel.h"
#include "wowplayercontrols.h"
//#include "wowhistogramwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class WowPlayerWidget; }
QT_END_NAMESPACE

class WowPlayerWidget : public QWidget {
Q_OBJECT

public:
    explicit WowPlayerWidget(QWidget *parent = nullptr);

    ~WowPlayerWidget() override;
    bool isPlayerAvailable() const;

    void addToPlaylist(const QList<QUrl> &urls);
    void setCustomAudioRole(const QString &role);
signals:
    void fullScreenChanged(bool fullScreen);

private slots:
    void open();
    void playWowIRCRadio();
    void taesteWow();
    void tune();
    void durationChanged(qint64 duration);
    void positionChanged(qint64 progress);
    void metaDataChanged();

    void previousClicked();

    void seek(int seconds);
    void jump(const QModelIndex &index);
    void playlistPositionChanged(int);

    void statusChanged(QMediaPlayer::MediaStatus status);
    void stateChanged(QMediaPlayer::State state);
    void bufferingProgress(int progress);
    void videoAvailableChanged(bool available);

    void displayErrorMessage();

    void showColorDialog();

private:
    void clearHistogram();
    void setTrackInfo(const QString &info);
    void setStatusInfo(const QString &info);
    void handleCursor(QMediaPlayer::MediaStatus status);
    void updateDurationInfo(qint64 currentInfo);
    Ui::WowPlayerWidget *ui;

    QMediaPlayer *m_player = nullptr;
    QMediaPlaylist *m_playlist = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QTextBrowser *m_TextBrowser = nullptr;
    QLabel *m_coverLabel = nullptr;
    QSlider *m_slider = nullptr;
    QLabel *m_labelDuration = nullptr;
    QPushButton *m_fullScreenButton = nullptr;
    QPushButton *m_colorButton = nullptr;
    QDialog *m_colorDialog = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPlainTextEdit *m_statusBar = nullptr;

    QLabel *m_labelHistogram = nullptr;
    //HistogramWidget *m_videoHistogram = nullptr;
    //HistogramWidget *m_audioHistogram = nullptr;
    QVideoProbe *m_videoProbe = nullptr;
    QAudioProbe *m_audioProbe = nullptr;

    PlaylistModel *m_playlistModel = nullptr;
    QAbstractItemView *m_playlistView = nullptr;
    QString m_trackInfo;
    QString m_statusInfo;
    qint64 m_duration;

};


#endif //WOWLET_WOWPLAYERWIDGET_H
