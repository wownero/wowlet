//
// Created by rapeafed on 2021.
//

// You may need to build the project (run Qt uic code generator) to get "ui_WowPlayerWidget.h" resolved

#include <QMediaPlaylist>
#include "wowplayercontrols.h"

#include "wowvideowidget.h"
#include "wowplaylistmodel.h"
//#include "wowhistogramwidget.h"

#include <QMediaService>
#include <QVideoProbe>
#include <QAudioProbe>
#include <QMediaMetaData>
#include <QtWidgets>
#include <QVideoWidget>
#include <QStandardItemModel>

#include <QtMultimedia>
#include <QtMultimediaWidgets>

#include <QMediaService>
#include <QMediaPlaylist>
#include <QVideoProbe>
#include <QAudioProbe>
#include <QMediaMetaData>
#include <QtWidgets>
#include <QMediaPlayer>
//#include <QtMultimedia/QMediaPlayer>
#include "wowplayerwidget.h"
#include "ui_wowplayerwidget.h"




WowPlayerWidget::WowPlayerWidget(QWidget *parent) :
        QWidget(parent), ui(new Ui::WowPlayerWidget) {

//    d = new Private;
  //  d->mediaPlayer = new QMediaPlayer(this, QMediaPlayer::StreamPlayback);
    //d->networkAccessManager = new QNetworkAccessManager(this);
//    QMediaPlayer myAudio;
    ui->setupUi(this);
    //QMediaPlayer *player = new QMediaPlayer();


//! [create-objs]
    //QMediaPlayer player = new QMediaPlayer;
    //QMediaPlayer *player = new QMediaPlayer();
    //m_player = new QMediaPlayer;
    //QMediaPlayer  *player = new QMediaPlayer;
    m_player = new QMediaPlayer(this);
    m_player->setAudioRole(QAudio::VideoRole);
    qInfo() << "Supported audio roles:";
    for (QAudio::Role role : m_player->supportedAudioRoles())
        qInfo() << "    " << role;
    // owned by PlaylistModel
    m_playlist = new QMediaPlaylist();
    m_player->setPlaylist(m_playlist);
//! [create-objs]

    connect(m_player, &QMediaPlayer::durationChanged, this, &WowPlayerWidget::durationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &WowPlayerWidget::positionChanged);
    connect(m_player, QOverload<>::of(&QMediaPlayer::metaDataChanged), this, &WowPlayerWidget::metaDataChanged);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &WowPlayerWidget::playlistPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &WowPlayerWidget::statusChanged);
    connect(m_player, &QMediaPlayer::bufferStatusChanged, this, &WowPlayerWidget::bufferingProgress);
    connect(m_player, &QMediaPlayer::videoAvailableChanged, this, &WowPlayerWidget::videoAvailableChanged);
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, &WowPlayerWidget::displayErrorMessage);
    connect(m_player, &QMediaPlayer::stateChanged, this, &WowPlayerWidget::stateChanged);

   // QHBoxLayout *wowPlayerLayout = new QHBoxLayout(this);;
//! [2]
    m_videoWidget = new VideoWidget(ui->verticalWidget);
    m_player->setVideoOutput(m_videoWidget);

    m_playlistModel = new PlaylistModel(this);
    m_playlistModel->setPlaylist(m_playlist);
//! [2]

     m_playlistView = new QListView(this);
     m_playlistView->setModel(m_playlistModel);
     m_playlistView->setCurrentIndex(m_playlistModel->index(m_playlist->currentIndex(), 0));

    connect(m_playlistView, &QAbstractItemView::activated, this, &WowPlayerWidget::jump);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, m_player->duration() / 1000);

    m_labelDuration = new QLabel(this);
    connect(m_slider, &QSlider::sliderMoved, this, &WowPlayerWidget::seek);
    //connect(m_slider, &QSlider::, this, &WowPlayerWidget::seek);
    //m_labelHistogram = new QLabel(this);
    //m_labelHistogram->setText("Histogram:");
    //m_videoHistogram = new HistogramWidget(this);
    //m_audioHistogram = new HistogramWidget(this);
    //QHBoxLayout *histogramLayout = new QHBoxLayout;
    //histogramLayout->addWidget(m_labelHistogram);
    //histogramLayout->addWidget(m_videoHistogram, 1);
    //histogramLayout->addWidget(m_audioHistogram, 2);

    //m_videoProbe = new QVideoProbe(this);
    //connect(m_videoProbe, &QVideoProbe::videoFrameProbed, m_videoHistogram, &HistogramWidget::processFrame);
    //m_videoProbe->setSource(m_player);

    //m_audioProbe = new QAudioProbe(this);
    //connect(m_audioProbe, &QAudioProbe::audioBufferProbed, m_audioHistogram, &HistogramWidget::processBuffer);
    //m_audioProbe->setSource(m_player);

    QPushButton *playWowIRCRadioButton = new QPushButton(tr("Play IRC!Radio"), this);
    QPushButton *taesteWowButton = new QPushButton(tr("Tæste Wow"), ui->verticalWidget);
    QPushButton *tuneButton = new QPushButton(tr("!Tune"), ui->verticalWidget);
    QPushButton *openButton = new QPushButton(tr("Open"), this);


    connect(playWowIRCRadioButton, &QPushButton::clicked, this, &WowPlayerWidget::playWowIRCRadio);
    connect(taesteWowButton, &QPushButton::clicked, this, &WowPlayerWidget::taesteWow);
    connect(tuneButton, &QPushButton::clicked, this, &WowPlayerWidget::tune);
    connect(openButton, &QPushButton::clicked, this, &WowPlayerWidget::open);


    PlayerControls *controls = new PlayerControls(this);
    controls->setState(m_player->state());
    controls->setVolume(m_player->volume());
    controls->setMuted(controls->isMuted());

    connect(controls, &PlayerControls::play, m_player, &QMediaPlayer::play);
    connect(controls, &PlayerControls::pause, m_player, &QMediaPlayer::pause);
    connect(controls, &PlayerControls::stop, m_player, &QMediaPlayer::stop);
    connect(controls, &PlayerControls::next, m_playlist, &QMediaPlaylist::next);
    connect(controls, &PlayerControls::previous, this, &WowPlayerWidget::previousClicked);
    connect(controls, &PlayerControls::changeVolume, m_player, &QMediaPlayer::setVolume);
    connect(controls, &PlayerControls::changeMuting, m_player, &QMediaPlayer::setMuted);
    connect(controls, &PlayerControls::changeRate, m_player, &QMediaPlayer::setPlaybackRate);
    connect(controls, &PlayerControls::stop, m_videoWidget, QOverload<>::of(&QVideoWidget::update));

    connect(m_player, &QMediaPlayer::stateChanged, controls, &PlayerControls::setState);
    connect(m_player, &QMediaPlayer::volumeChanged, controls, &PlayerControls::setVolume);
    connect(m_player, &QMediaPlayer::mutedChanged, controls, &PlayerControls::setMuted);

    m_fullScreenButton = new QPushButton(tr("FullScreen"), this);
    m_fullScreenButton->setCheckable(true);

    m_colorButton = new QPushButton(tr("Color Options..."), this);
    m_colorButton->setEnabled(false);
    connect(m_colorButton, &QPushButton::clicked, this, &WowPlayerWidget::showColorDialog);

    QBoxLayout *infoPlanelLayout = new QVBoxLayout;
    m_statusBar = new QPlainTextEdit;
    m_statusLabel = new QLabel;
    //m_TextBrowser = new QTextBrowser;
    m_statusBar->setPlainText("More fun with Wowmero IRC!Radio\nIRC 140.211.166.64:6667\n#wownero-music");
    m_statusLabel->setText("WowPlayer ready");
    QUrl *history = new QUrl("https://radio.wownero.com/history.txt");

    //m_TextBrowser->setSource(QUrl("https://radio.wownero.com/history.txt"));
    //m_TextBrowser->reload();
    //m_TextBrowser->setPlainText(history->toString());
    //m_TextBrowser->append("<a href = \"https://radio.wownero.com/history.txt\"> History </a>");
    infoPlanelLayout->addWidget(m_statusLabel);//, 2);
    infoPlanelLayout->addWidget(m_statusBar);//, 2);
    //infoPlanelLayout->addWidget(m_TextBrowser);
    //infoPlanelLayout->addWidget(playWowIRCRadioButton);

    QBoxLayout *playerButtonsLayout = new QHBoxLayout;
    playerButtonsLayout->addWidget(playWowIRCRadioButton);
    playerButtonsLayout->addWidget(taesteWowButton);
    playerButtonsLayout->addWidget(tuneButton);
    playerButtonsLayout->addWidget(openButton);

    QBoxLayout *displayLayout = new QHBoxLayout;
    displayLayout->addWidget(m_videoWidget, 2);
    displayLayout->addWidget(m_playlistView);

    QBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addLayout(playerButtonsLayout);
    controlLayout->addStretch(1);
    controlLayout->addWidget(controls);
    controlLayout->addStretch(1);
    controlLayout->addWidget(m_fullScreenButton);
    //controlLayout->addWidget(m_colorButton);

    QBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(displayLayout);
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addWidget(m_slider);
    hLayout->addWidget(m_labelDuration);
    layout->addLayout(hLayout);
    layout->addLayout(controlLayout);
    layout->addLayout(infoPlanelLayout);
    //layout->addLayout(histogramLayout);
#if defined(Q_OS_QNX)
    // On QNX, the main window doesn't have a title bar (or any other decorations).
    // Create a status bar for the status information instead.
    m_statusLabel = new QLabel;
    m_statusBar = new QStatusBar;
    m_statusBar->addPermanentWidget(m_statusLabel);
    m_statusBar->setSizeGripEnabled(false); // Without mouse grabbing, it doesn't work very well.
    layout->addWidget(m_statusBar);
#endif

    setLayout(layout);

    if (!isPlayerAvailable()) {
        QMessageBox::warning(this, tr("Service not available"),
                             tr("The QMediaPlayer object does not have a valid service.\n"\
                                "Please check the media service plugins are installed."));

        controls->setEnabled(false);
        m_playlistView->setEnabled(false);
        openButton->setEnabled(false);
        m_colorButton->setEnabled(false);
        m_fullScreenButton->setEnabled(false);
    }

    metaDataChanged();

}

WowPlayerWidget::~WowPlayerWidget() {
    delete ui;
}

bool WowPlayerWidget::isPlayerAvailable() const
{
    return m_player->isAvailable();
}

void WowPlayerWidget::playWowIRCRadio()
{
    //PrepareSuchWowSlideShow();

/*
    QImage img = QImage("qrc:///assets/images/wowlet.png").convertToFormat(QImage::Format_ARGB32);
    QVideoSurfaceFormat format(img.size(), QVideoFrame::Format_ARGB32);
    //QVideoWidget *videoWidget = new QVideoWidget;
    m_videoWidget->videoSurface()->start(format);
    m_videoWidget->videoSurface()->present(img);
    //m_videoWidget->show();
*/

    m_playlist->clear();
    m_playlist->addMedia(QUrl("https://radio.wownero.com/wow.ogg"));
    m_player->play();
}

void WowPlayerWidget::taesteWow()
{
    setStatusInfo(tr("Wow"));
  m_playlist->clear();
  m_playlist->addMedia(QUrl("qrc:///assets/videos/do_you_even_wow_special_friend.mp4"));
  m_playlist->addMedia(QUrl("https://radio.wownero.com/wow.ogg"));
  m_player->play();

}

void WowPlayerWidget::tune()
{
    setStatusInfo(tr("Undefined reference HellGate::TrollConnect(me, like, wownero, music)"));
}

void WowPlayerWidget::open()
{

    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Open Files"));
    QStringList supportedMimeTypes = m_player->supportedMimeTypes();
    if (!supportedMimeTypes.isEmpty()) {
        supportedMimeTypes.append("audio/x-m3u"); // MP3 playlists
        fileDialog.setMimeTypeFilters(supportedMimeTypes);
    }
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));
    if (fileDialog.exec() == QDialog::Accepted)
        addToPlaylist(fileDialog.selectedUrls());
    //m_playlist->load(QUrl("https://radio.wownero.com/wow.ogg"));
    //m_player->setMedia(QUrl("https://radio.wownero.com/wow.ogg"));

     /*m_player->setMedia(QUrl("gst-pipeline: appsrc blocksize=4294967295 ! \
                          video/x-raw,format=BGRx,framerate=30/1,width=200,height=147 ! \
                          coloreffects preset=heat ! videoconvert ! video/x-raw,format=I420 ! jpegenc ! rtpjpegpay ! \
                          udpsink host=127.0.0.1 port=5000"));*/
    //   setMedia(QUrl::fromLocalFile("~/  Music/coolsong.mp3"));
//playbin uri=file:///projects/demo.ogv

}

static bool isPlaylist(const QUrl &url) // Check for ".m3u" playlists.
{
    if (!url.isLocalFile())
        return false;
    const QFileInfo fileInfo(url.toLocalFile());
    return fileInfo.exists() && !fileInfo.suffix().compare(QLatin1String("m3u"), Qt::CaseInsensitive);
}

void WowPlayerWidget::addToPlaylist(const QList<QUrl> &urls)
{
    for (auto &url: urls) {
        if (isPlaylist(url))
            m_playlist->load(url);
        else
            m_playlist->addMedia(url);
    }
}

void WowPlayerWidget::setCustomAudioRole(const QString &role)
{
    m_player->setCustomAudioRole(role);
}

void WowPlayerWidget::durationChanged(qint64 duration)
{
    m_duration = duration / 1000;
    m_slider->setMaximum(m_duration);
}

void WowPlayerWidget::positionChanged(qint64 progress)
{
    if (!m_slider->isSliderDown())
        m_slider->setValue(progress / 1000);

    updateDurationInfo(progress / 1000);
}

void WowPlayerWidget::metaDataChanged()
{
    if (m_player->isMetaDataAvailable()) {
        setTrackInfo(QString("%1 - %2")
                             .arg(m_player->metaData(QMediaMetaData::AlbumArtist).toString())
                             .arg(m_player->metaData(QMediaMetaData::Title).toString()));

        if (m_coverLabel) {
            QUrl url = m_player->metaData(QMediaMetaData::CoverArtUrlLarge).value<QUrl>();

            m_coverLabel->setPixmap(!url.isEmpty()
                                    ? QPixmap(url.toString())
                                    : QPixmap());
        }
    }
}

void WowPlayerWidget::previousClicked()
{
    // Go to previous track if we are within the first 5 seconds of playback
    // Otherwise, seek to the beginning.
    if (m_player->position() <= 5000)
        m_playlist->previous();
    else
        m_player->setPosition(0);
}

void WowPlayerWidget::jump(const QModelIndex &index)
{
    if (index.isValid()) {
        m_playlist->setCurrentIndex(index.row());
        m_player->play();
    }
}

void WowPlayerWidget::playlistPositionChanged(int currentItem)
{
    //clearHistogram();
    m_playlistView->setCurrentIndex(m_playlistModel->index(currentItem, 0));
}

void WowPlayerWidget::seek(int seconds)
{
    m_player->setPosition(seconds * 1000);
}

void WowPlayerWidget::statusChanged(QMediaPlayer::MediaStatus status)
{
    handleCursor(status);

    // handle status message
    switch (status) {
        case QMediaPlayer::UnknownMediaStatus:
        case QMediaPlayer::NoMedia:
        case QMediaPlayer::LoadedMedia:
            setStatusInfo(QString());
            break;
        case QMediaPlayer::LoadingMedia:
            setStatusInfo(tr("Loading..."));
            break;
        case QMediaPlayer::BufferingMedia:
        case QMediaPlayer::BufferedMedia:
            setStatusInfo(tr("Buffering %1%").arg(m_player->bufferStatus()));
            break;
        case QMediaPlayer::StalledMedia:
            setStatusInfo(tr("Stalled %1%").arg(m_player->bufferStatus()));
            break;
        case QMediaPlayer::EndOfMedia:
            QApplication::alert(this);
            break;
        case QMediaPlayer::InvalidMedia:
            displayErrorMessage();
            break;
    }
}

void WowPlayerWidget::stateChanged(QMediaPlayer::State state)
{
    //if (state == QMediaPlayer::StoppedState)
      //  clearHistogram();
}

void WowPlayerWidget::handleCursor(QMediaPlayer::MediaStatus status)
{
#ifndef QT_NO_CURSOR
    if (status == QMediaPlayer::LoadingMedia ||
        status == QMediaPlayer::BufferingMedia ||
        status == QMediaPlayer::StalledMedia)
        setCursor(QCursor(Qt::BusyCursor));
    else
        unsetCursor();
#endif
}

void WowPlayerWidget::bufferingProgress(int progress)
{
    if (m_player->mediaStatus() == QMediaPlayer::StalledMedia)
        setStatusInfo(tr("Stalled %1%").arg(progress));
    else
        setStatusInfo(tr("Buffering %1%").arg(progress));
}

void WowPlayerWidget::videoAvailableChanged(bool available)
{
    if (!available) {
        disconnect(m_fullScreenButton, &QPushButton::clicked, m_videoWidget, &QVideoWidget::setFullScreen);
        disconnect(m_videoWidget, &QVideoWidget::fullScreenChanged, m_fullScreenButton, &QPushButton::setChecked);
        m_videoWidget->setFullScreen(false);
    } else {
        connect(m_fullScreenButton, &QPushButton::clicked, m_videoWidget, &QVideoWidget::setFullScreen);
        connect(m_videoWidget, &QVideoWidget::fullScreenChanged, m_fullScreenButton, &QPushButton::setChecked);

        if (m_fullScreenButton->isChecked())
            m_videoWidget->setFullScreen(true);
    }
    //m_colorButton->setEnabled(available);
}

void WowPlayerWidget::setTrackInfo(const QString &info)
{
    m_trackInfo = info;
    //m_statusBar->setPlainText(m_trackInfo);
//    m_statusBar->setPlainText("Wownero IRC!Radio"+m_trackInfo);
  //  m_statusLabel->setText(m_statusInfo);

    if (m_statusBar) {
        m_statusBar->setPlainText("Wownero IRC!Radio"+m_trackInfo);
        m_statusLabel->setText(m_statusInfo);
    } else {
        if (!m_statusInfo.isEmpty())
            setWindowTitle(QString("%1 | %2").arg(m_trackInfo).arg(m_statusInfo));
        else
            setWindowTitle(m_trackInfo);
    }
}

void WowPlayerWidget::setStatusInfo(const QString &info)
{
    m_statusInfo = info;
    //m_statusBar->setPlainText(m_trackInfo);
    //QFile *radioHistoryFile = new QFile("https://radio.wownero.com/history.txt");
    //radioHistoryFile->open(QIODevice::ReadOnly);
    //QString content = QString::fromUtf8(radioHistoryFile->readAll());
    //content+="test";
//    radioHistoryFile->close();
    //m_TextBrowser->setSource(QUrl("https://radio.wownero.com/history.txt"));
    //m_TextBrowser->reload();
    m_statusBar->setPlainText("Wownero IRC!Radio"+m_trackInfo);
    m_statusLabel->setText(m_statusInfo);
/*    if (m_statusBar) {
        m_statusBar->showMessage(m_trackInfo);
        m_statusLabel->setText(m_statusInfo);
    } else {
        if (!m_statusInfo.isEmpty())
            setWindowTitle(QString("%1 | %2").arg(m_trackInfo).arg(m_statusInfo));
        else
            setWindowTitle(m_trackInfo);
    }*/
}

void WowPlayerWidget::displayErrorMessage()
{
    setStatusInfo(m_player->errorString());
}

void WowPlayerWidget::updateDurationInfo(qint64 currentInfo)
{
    QString tStr;
    if (currentInfo || m_duration) {
        QTime currentTime((currentInfo / 3600) % 60, (currentInfo / 60) % 60,
                          currentInfo % 60, (currentInfo * 1000) % 1000);
        QTime totalTime((m_duration / 3600) % 60, (m_duration / 60) % 60,
                        m_duration % 60, (m_duration * 1000) % 1000);
        QString format = "mm:ss";
        if (m_duration > 3600)
            format = "hh:mm:ss";
        tStr = currentTime.toString(format) + " / " + totalTime.toString(format);
    }
    m_labelDuration->setText(tStr);
}

void WowPlayerWidget::showColorDialog()
{
    if (!m_colorDialog) {
        QSlider *brightnessSlider = new QSlider(Qt::Horizontal);
        brightnessSlider->setRange(-100, 100);
        brightnessSlider->setValue(m_videoWidget->brightness());
        connect(brightnessSlider, &QSlider::sliderMoved, m_videoWidget, &QVideoWidget::setBrightness);
        connect(m_videoWidget, &QVideoWidget::brightnessChanged, brightnessSlider, &QSlider::setValue);

        QSlider *contrastSlider = new QSlider(Qt::Horizontal);
        contrastSlider->setRange(-100, 100);
        contrastSlider->setValue(m_videoWidget->contrast());
        connect(contrastSlider, &QSlider::sliderMoved, m_videoWidget, &QVideoWidget::setContrast);
        connect(m_videoWidget, &QVideoWidget::contrastChanged, contrastSlider, &QSlider::setValue);

        QSlider *hueSlider = new QSlider(Qt::Horizontal);
        hueSlider->setRange(-100, 100);
        hueSlider->setValue(m_videoWidget->hue());
        connect(hueSlider, &QSlider::sliderMoved, m_videoWidget, &QVideoWidget::setHue);
        connect(m_videoWidget, &QVideoWidget::hueChanged, hueSlider, &QSlider::setValue);

        QSlider *saturationSlider = new QSlider(Qt::Horizontal);
        saturationSlider->setRange(-100, 100);
        saturationSlider->setValue(m_videoWidget->saturation());
        connect(saturationSlider, &QSlider::sliderMoved, m_videoWidget, &QVideoWidget::setSaturation);
        connect(m_videoWidget, &QVideoWidget::saturationChanged, saturationSlider, &QSlider::setValue);

        QFormLayout *layout = new QFormLayout;
        layout->addRow(tr("Brightness"), brightnessSlider);
        layout->addRow(tr("Contrast"), contrastSlider);
        layout->addRow(tr("Hue"), hueSlider);
        layout->addRow(tr("Saturation"), saturationSlider);

        QPushButton *button = new QPushButton(tr("Close"));
        layout->addRow(button);

        m_colorDialog = new QDialog(this);
        m_colorDialog->setWindowTitle(tr("Color Options"));
        m_colorDialog->setLayout(layout);

        connect(button, &QPushButton::clicked, m_colorDialog, &QDialog::close);
    }
    m_colorDialog->show();
}

void WowPlayerWidget::clearHistogram()
{
    //QMetaObject::invokeMethod(m_videoHistogram, "processFrame", Qt::QueuedConnection, Q_ARG(QVideoFrame, QVideoFrame()));
   // QMetaObject::invokeMethod(m_audioHistogram, "processBuffer", Qt::QueuedConnection, Q_ARG(QAudioBuffer, QAudioBuffer()));
}

