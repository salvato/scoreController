/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include <QDir>
#include <QTimer>
#include <QMessageBox>
#include <QThread>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QWebSocket>
#include <QBuffer>
#include <QFileDialog>
#include <QPushButton>
#include <QHostInfo>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QProcessEnvironment>

#include "scorecontroller.h"
#include "clientlistdialog.h"
#include "utility.h"
#include "netServer.h"
#include "fileserver.h"


#define DISCOVERY_PORT      45453
#define SERVER_SOCKET_PORT  45454
#define SPOT_UPDATER_PORT   45455
#define SLIDE_UPDATER_PORT  45456



ScoreController::ScoreController(int _panelType, QWidget *parent)
    : QWidget(parent)
    , panelType(_panelType)
    , pClientListDialog(new ClientListDialog(this))
    , connectionList(QList<connection>())
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_SOCKET_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
    , slideUpdaterPort(SLIDE_UPDATER_PORT)
    , spotUpdaterPort(SPOT_UPDATER_PORT)
    , pButtonClick(Q_NULLPTR)
{
    QString sFunctionName = QString(" ScoreController::ScoreController ");
    Q_UNUSED(sFunctionName)

    pSettings = Q_NULLPTR;

    pButtonClick = new QSoundEffect(this);
    pButtonClick->setSource(QUrl::fromLocalFile(":/key.wav"));

    sIpAddresses = QStringList();

    QString sBaseDir;
    bool bFound = false;
#ifdef Q_OS_ANDROID
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QStringList secondaryList = environment.value(QString("SECONDARY_STORAGE")).split(":", QString::SkipEmptyParts);
    secondaryList.append(environment.value(QString("EXTERNAL_STORAGE")).split(":", QString::SkipEmptyParts));
    secondaryList.append(environment.value(QString("ANDROID_STORAGE")).split(":", QString::SkipEmptyParts));
    QDir sdPath;
    for(int i=0; i<secondaryList.count(); i++) {
        sBaseDir = secondaryList.at(i);
        if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
        sdPath = QDir(sBaseDir+QString("slides/"));
        if(sdPath.exists() && sdPath.isReadable()) {
            bFound = true;
            break;
        }
    }
    if(!bFound) sBaseDir = QString("/");
#else
    sBaseDir = QDir::homePath();
#endif

    sSlideDir   = QString("%1slides/").arg(sBaseDir);
    sSpotDir    = QString("%1spots/").arg(sBaseDir);
    logFileName = QString("%1score_controller.txt").arg(sBaseDir);

    QMessageBox::information(this, sFunctionName,
                             QString("Dirs: %1: %2.")
                             .arg(bFound).arg(sBaseDir));

    logFile       = Q_NULLPTR;
    slideList     = QFileInfoList();
    iCurrentSlide = 0;
    iCurrentSpot  = 0;

    PrepareLogFile();
    QStringList list = environment.toStringList();
    for(int i=0; i<list.count(); i++)
        logMessage(logFile,
                   sFunctionName,
                   list.at(i));


    if(bFound)
        logMessage(logFile,
                   sFunctionName,
                   QString("sBaseDir found"));

    else
        logMessage(logFile,
                   sFunctionName,
                   QString("sBaseDir NOT found"));
    if((panelType < FIRST_PANEL) || (panelType > LAST_PANEL)) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Panel Type set to FIRST_PANEL"));
        panelType = FIRST_PANEL;
    }

    connect(&exitTimer, SIGNAL(timeout()),
            this, SLOT(close()));

    // Blocks until a network connection is available
    WaitForNetworkReady();

    // Start listening to the discovery port
    if(!prepareDiscovery()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("!prepareDiscovery()"));
        exitTimer.start(1000);
        QCursor waitCursor;
        waitCursor.setShape(Qt::WaitCursor);
        setCursor(waitCursor);
    }

    // Prepare the server port for the panels to connect
    if(!prepareServer()) {
        exitTimer.start(1000);
        QCursor waitCursor;
        waitCursor.setShape(Qt::WaitCursor);
        setCursor(waitCursor);
    }

    prepareSpotUpdateService();
    prepareSlideUpdateService();

    // Pan-Tilt Camera management
    connect(pClientListDialog, SIGNAL(disableVideo()),
            this, SLOT(onStopCamera()));
    connect(pClientListDialog, SIGNAL(enableVideo(QString)),
            this, SLOT(onStartCamera(QString)));
    connect(pClientListDialog, SIGNAL(newPanValue(QString,int)),
            this, SLOT(onSetNewPanValue(QString, int)));
    connect(pClientListDialog, SIGNAL(newTiltValue(QString,int)),
            this, SLOT(onSetNewTiltValue(QString, int)));

    // Panel orientation management
    connect(pClientListDialog, SIGNAL(getOrientation(QString)),
            this, SLOT(onGetPanelOrientation(QString)));
    connect(pClientListDialog, SIGNAL(changeOrientation(QString,PanelOrientation)),
            this, SLOT(onChangePanelOrientation(QString,PanelOrientation)));
    // Panel Score Only
    connect(pClientListDialog, SIGNAL(getScoreOnly(QString)),
            this, SLOT(onGetIsPanelScoreOnly(QString)));
    connect(pClientListDialog, SIGNAL(changeScoreOnly(QString, bool)),
            this, SLOT(onSetScoreOnly(QString, bool)));
}


void
ScoreController::PrepareDirectories() {
    QString sFunctionName = QString(" ScoreController::PrepareDirectories ");
    QDir slideDir(sSlideDir);
    QDir spotDir(sSpotDir);

    if(!slideDir.exists() || !spotDir.exists()) {
        onButtonSetupClicked();
        slideDir.setPath(sSlideDir);
        if(!slideDir.exists()) sSlideDir = QDir::homePath();
        spotDir.setPath(sSpotDir);
        if(!spotDir.exists()) sSpotDir = QDir::homePath();
        pSettings->setValue("directories/slides", sSlideDir);
        pSettings->setValue("directories/spots", sSpotDir);
    }
    else {
        QStringList filter(QStringList() << "*.jpg" << "*.jpeg" << "*.png" << "*.JPG" << "*.JPEG" << "*.PNG");
        slideDir.setNameFilters(filter);
        slideList = slideDir.entryInfoList();
        logMessage(logFile,
                   sFunctionName,
                   QString("Slides directory: %1 Found %2 Slides")
                   .arg(sSlideDir)
                   .arg(slideList.count()));
        QStringList nameFilter(QStringList() << "*.mp4"<< "*.MP4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
        logMessage(logFile,
                   sFunctionName,
                   QString("Spot directory: %1 Found %2 Spots")
                   .arg(sSpotDir)
                   .arg(spotList.count()));
    }
    if(!sSlideDir.endsWith(QString("/"))) sSlideDir+= QString("/");
    if(!sSpotDir.endsWith(QString("/")))  sSpotDir+= QString("/");
}


void
ScoreController::prepareSpotUpdateService() {
    // Creating a Spot Update Service
    pSpotUpdaterServer = new FileServer(QString("SpotUpdater"), logFile, Q_NULLPTR);
    connect(pSpotUpdaterServer, SIGNAL(fileServerDone(bool)),
            this, SLOT(onSpotServerDone(bool)));
    pSpotUpdaterServer->setServerPort(spotUpdaterPort);
    pSpotServerThread = new QThread();
    pSpotUpdaterServer->moveToThread(pSpotServerThread);
    connect(this, SIGNAL(startSpotServer()),
            pSpotUpdaterServer, SLOT(onStartServer()));
    connect(this, SIGNAL(closeSpotServer()),
            pSpotUpdaterServer, SLOT(onCloseServer()));
    pSpotServerThread->start(QThread::LowestPriority);
}


void
ScoreController::prepareSlideUpdateService() {
    // Creating a Slide Update Service
    pSlideUpdaterServer = new FileServer(QString("SlideUpdater"), logFile, Q_NULLPTR);
    connect(pSlideUpdaterServer, SIGNAL(fileServerDone(bool)),
            this, SLOT(onSlideServerDone(bool)));
    pSlideUpdaterServer->setServerPort(slideUpdaterPort);
    pSlideServerThread = new QThread();
    pSlideUpdaterServer->moveToThread(pSlideServerThread);
    connect(this, SIGNAL(startSlideServer()),
            pSlideUpdaterServer, SLOT(onStartServer()));
    connect(this, SIGNAL(closeSlideServer()),
            pSlideUpdaterServer, SLOT(onCloseServer()));
    pSlideServerThread->start(QThread::LowestPriority);
}


void
ScoreController::WaitForNetworkReady() {
    int iResponse;
    while(!isConnectedToNetwork()) {
        iResponse = QMessageBox::critical(this,
                                          tr("Connessione Assente"),
                                          tr("Connettiti alla rete e ritenta"),
                                          QMessageBox::Retry,
                                          QMessageBox::Abort);

        if(iResponse == QMessageBox::Abort) {
            exitTimer.start(1000);
            QCursor waitCursor;
            waitCursor.setShape(Qt::WaitCursor);
            setCursor(waitCursor);
            break;
        }
        QThread::sleep(1);
    }
}


ScoreController::~ScoreController() {
    // All the housekeeping is done in "closeEvent()" manager
    QString sFunctionName = QString("ScoreController::~ScoreController");
    Q_UNUSED(sFunctionName)
}


void
ScoreController::onSlideServerDone(bool bError) {
    QString sFunctionName = QString(" ScoreController::onSlideServerDone ");
    if(bError) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Slide server stopped with errors"));
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   QString("Slide server stopped without errors"));
    }
}


void
ScoreController::onSpotServerDone(bool bError) {
    QString sFunctionName = QString(" ScoreController::onSpotServerDone ");
    if(bError) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Spot server stopped with errors"));
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   QString("Spot server stopped without errors"));
    }
}


bool
ScoreController::prepareDiscovery() {
    QString sFunctionName = QString(" ScoreController::prepareDiscovery ");
    bool bSuccess = false;
    sIpAddresses = QStringList();
    QList<QNetworkInterface> interfaceList = QNetworkInterface::allInterfaces();
    for(int i=0; i<interfaceList.count(); i++)
    {
        QNetworkInterface interface = interfaceList.at(i);
        if(interface.flags().testFlag(QNetworkInterface::IsUp) &&
           interface.flags().testFlag(QNetworkInterface::IsRunning) &&
           interface.flags().testFlag(QNetworkInterface::CanMulticast) &&
          !interface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            QList<QNetworkAddressEntry> list = interface.addressEntries();
            for(int j=0; j<list.count(); j++)
            {
                QUdpSocket* pDiscoverySocket = new QUdpSocket(this);
                if(list[j].ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    if(pDiscoverySocket->bind(QHostAddress::AnyIPv4, discoveryPort, QUdpSocket::ShareAddress)) {
                        pDiscoverySocket->joinMulticastGroup(discoveryAddress);
                        sIpAddresses.append(list[j].ip().toString());
                        discoverySocketArray.append(pDiscoverySocket);
                        connect(pDiscoverySocket, SIGNAL(readyRead()),
                                this, SLOT(onProcessConnectionRequest()));
                        bSuccess = true;
#ifdef LOG_VERBOSE
                        logMessage(logFile,
                                   sFunctionName,
                                   QString("Listening for connections at address: %1 port:%2")
                                   .arg(discoveryAddress.toString())
                                   .arg(discoveryPort));
#endif
                    }
                    else {
                        logMessage(logFile,
                                   sFunctionName,
                                   QString("Unable to bound %1")
                                   .arg(discoveryAddress.toString()));
                    }
                }
            }// for(int j=0; j<list.count(); j++)
        }
    }// for(int i=0; i<interfaceList.count(); i++)
    return bSuccess;
}


void
ScoreController::onStartCamera(QString sClientIp) {
    QHostAddress hostAddress(sClientIp);
    for(int i=0; i<connectionList.count(); i++) {
        if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
            QString sMessage = QString("<live>1</live>");
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            sMessage = QString("<getPanTilt>1</getPanTilt>");
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            return;
        }
    }
}


void
ScoreController::onStopCamera() {
    QString sMessage = QString("<endlive>1</endlive>");
    SendToAll(sMessage);
}


void
ScoreController::onSetNewPanValue(QString sClientIp, int newPan) {
  QHostAddress hostAddress(sClientIp);
  for(int i=0; i<connectionList.count(); i++) {
      if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
          QString sMessage = QString("<pan>%1</pan>").arg(newPan);
          SendToOne(connectionList.at(i).pClientSocket, sMessage);
          return;
      }
  }
}


void
ScoreController::onSetNewTiltValue(QString sClientIp, int newTilt) {
  QHostAddress hostAddress(sClientIp);
  for(int i=0; i<connectionList.count(); i++) {
      if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
          QString sMessage = QString("<tilt>%1</tilt>").arg(newTilt);
          SendToOne(connectionList.at(i).pClientSocket, sMessage);
          return;
      }
  }
}


void
ScoreController::onSetScoreOnly(QString sClientIp, bool bScoreOnly) {
    QHostAddress hostAddress(sClientIp);
    for(int i=0; i<connectionList.count(); i++) {
        if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
            QString sMessage = QString("<setScoreOnly>%1</setScoreOnly>").arg(bScoreOnly);
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            return;
        }
    }
}


bool
ScoreController::PrepareLogFile() {
#if defined(LOG_MESG) || defined(LOG_VERBOSE)
    QFileInfo checkFile(logFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(logFileName+QString(".bkp"));
        renamed.rename(logFileName, logFileName+QString(".bkp"));
    }
    logFile = new QFile(logFileName);
    if (!logFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Volley Controller"),
                                 tr("Impossibile aprire il file %1: %2.")
                                 .arg(logFileName).arg(logFile->errorString()));
        delete logFile;
        logFile = Q_NULLPTR;
    }
#endif
    return true;
}


bool
ScoreController::isConnectedToNetwork() {
    QString sFunctionName = " ScoreController::isConnectedToNetwork ";
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    bool result = false;

    for(int i=0; i<ifaces.count(); i++) {
        QNetworkInterface iface = ifaces.at(i);
        if(iface.flags().testFlag(QNetworkInterface::IsUp) &&
           iface.flags().testFlag(QNetworkInterface::IsRunning) &&
           iface.flags().testFlag(QNetworkInterface::CanBroadcast) &&
          !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            for(int j=0; j<iface.addressEntries().count(); j++) {
                if(!result) result = true;
            }
        }
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               sFunctionName,
               result ? QString("true") : QString("false"));
#endif
    return result;
}


void
ScoreController::onProcessConnectionRequest() {
    QString sFunctionName = " ScoreController::onProcessConnectionRequest ";
    QByteArray datagram, request;
    QString sToken;
    QUdpSocket* pDiscoverySocket = qobject_cast<QUdpSocket*>(sender());
    QString sNoData = QString("NoData");
    QString sMessage;
    QHostAddress hostAddress;
    quint16 port;

    while(pDiscoverySocket->hasPendingDatagrams()) {
        datagram.resize(pDiscoverySocket->pendingDatagramSize());
        pDiscoverySocket->readDatagram(datagram.data(), datagram.size(), &hostAddress, &port);
        request.append(datagram.data());
    }
    sToken = XML_Parse(request.data(), "getServer");
    if(sToken != sNoData) {
        sendAcceptConnection(pDiscoverySocket, hostAddress, port);
        logMessage(logFile,
                   sFunctionName,
                   QString("Connection request from: %1 at Address %2:%3")
                   .arg(sToken)
                   .arg(hostAddress.toString())
                   .arg(port));
        RemoveClient(hostAddress);
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   sFunctionName,
                   QString("Sent: %1")
                   .arg(sMessage));
#endif
        UpdateUI();
    }
}


int
ScoreController::sendAcceptConnection(QUdpSocket* pDiscoverySocket, QHostAddress hostAddress, quint16 port) {
    QString sFunctionName = " ScoreController::sendAcceptConnection ";
    Q_UNUSED(sFunctionName)
    QString sString = QString("%1,%2").arg(sIpAddresses.at(0)).arg(panelType);
    for(int i=1; i<sIpAddresses.count(); i++) {
        sString += QString(";%1,%2").arg(sIpAddresses.at(i)).arg(panelType);
    }
    QString sMessage = "<serverIP>" + sString + "</serverIP>";
    QByteArray datagram = sMessage.toUtf8();
    if(!pDiscoverySocket->isValid()) {
        logMessage(logFile,
                   sFunctionName,
                   QString("Discovery Socket Invalid !"));
        return -1;
    }
    qint64 bytesWritten = pDiscoverySocket->writeDatagram(datagram.data(), datagram.size(), hostAddress, port);
    Q_UNUSED(bytesWritten)
    if(bytesWritten != datagram.size()) {
      logMessage(logFile,
                 sFunctionName,
                 QString("Unable to send data !"));
    }
    return 0;
}


void
ScoreController::closeEvent(QCloseEvent *event) {
    QString sFunctionName = " ScoreController::closeEvent ";
    Q_UNUSED(sFunctionName)
    QString sMessage;
    logMessage(logFile,
               sFunctionName,
               QString("Closing"));
    if(pButtonClick != Q_NULLPTR)
        delete pButtonClick;
    pButtonClick = Q_NULLPTR;

    // Close all the discovery sockets
    for(int i=0; i<discoverySocketArray.count(); i++) {
        QUdpSocket* pSocket = discoverySocketArray.at(i);
        if(pSocket) {
            pSocket->disconnect();
            if(pSocket->isValid())
                pSocket->close();
            delete pSocket;
        }
    }
    discoverySocketArray.clear();

    emit closeSpotServer();
    emit closeSlideServer();

    if(connectionList.count() > 0) {
        int answer = QMessageBox::question(this,
                                           sFunctionName,
                                           tr("Vuoi spegnere anche i pannelli ?"),
                                           QMessageBox::Yes,
                                           QMessageBox::No,
                                           QMessageBox::Cancel|QMessageBox::Default);
        if(answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        } else
        if(answer == QMessageBox::No) {
            for(int i=0; i<connectionList.count(); i++) {
                connectionList.at(i).pClientSocket->disconnect();
                if(connectionList.at(i).pClientSocket->isValid())
                    connectionList.at(i).pClientSocket->close(QWebSocketProtocol::CloseCodeNormal, "Server Closed");
                connectionList.at(i).pClientSocket->deleteLater();
            }
            connectionList.clear();
        } else
        if(answer == QMessageBox::Yes) {
            sMessage = "<kill>1</kill>";
            SendToAll(sMessage);
        }
    }
    pPanelServer->closeServer();
    if(logFile) {
        logFile->flush();
        logFile->close();
        delete logFile;
        logFile = Q_NULLPTR;
    }
    if(pSettings != Q_NULLPTR) delete pSettings;
    pSettings = Q_NULLPTR;
    if(pClientListDialog != Q_NULLPTR) {
        pClientListDialog->disconnect();
        pClientListDialog->deleteLater();
        pClientListDialog = Q_NULLPTR;
    }
    emit panelDone();
}


bool
ScoreController::prepareServer() {
    QString sFunctionName = " ScoreController::prepareServer ";
    Q_UNUSED(sFunctionName)

    pPanelServer = new NetServer(QString("PanelServer"), logFile, this);
    if(!pPanelServer->prepareServer(serverPort))
        return false;
    connect(pPanelServer, SIGNAL(newConnection(QWebSocket *)),
            this, SLOT(onNewConnection(QWebSocket *)));
    return true;
}


void
ScoreController::onProcessTextMessage(QString sMessage) {
    QString sFunctionName = " ScoreController::onProcessTextMessage ";
    QString sToken;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "getStatus");
    if(sToken != sNoData) {
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        SendToOne(pClient, FormatStatusMsg());
    }// getStatus

    sToken = XML_Parse(sMessage, "closed_spot");
    if(sToken != sNoData) {
        startStopSpotButton->setText(tr("Avvia\nSpot"));
        startStopSlideShowButton->setDisabled(false);
        startStopLoopSpotButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
    }// closed_spot

    sToken = XML_Parse(sMessage, "closed_spot_loop");
    if(sToken != sNoData) {
        startStopLoopSpotButton->setText(tr("Avvia\nSpot Loop"));
        startStopSlideShowButton->setDisabled(false);
        startStopSpotButton->setDisabled(false);
        startStopLoopSpotButton->setDisabled(false);
    }// closed_spot_loop

    sToken = XML_Parse(sMessage, "pan_tilt");
    if(sToken != sNoData) {
        QStringList values = QStringList(sToken.split(",",QString::SkipEmptyParts));
        pClientListDialog->remotePanTiltReceived(values.at(0).toInt(), values.at(1).toInt());
    }// pan_tilt

    sToken = XML_Parse(sMessage, "send_spot");
    if(sToken != sNoData) {
        if(spotList.isEmpty())
            return;
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        if(pClient->isValid()) {

        }
    }// send_spot

    sToken = XML_Parse(sMessage, "getConf");
    if(sToken != sNoData) {
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        if(pClient->isValid()) {
            sMessage = QString("<setConf>%1</setConf>").arg(panelType);
            SendToOne(pClient, sMessage);
        }
    }// getConf
    
    sToken = XML_Parse(sMessage, "orientation");
    if(sToken != sNoData) {
        bool ok;
        int iOrientation = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Illegal orientation received: %1")
                       .arg(sToken));
            return;
        }
        PanelOrientation orientation = static_cast<PanelOrientation>(iOrientation);
        pClientListDialog->remoteOrientationReceived(orientation);
    }// orientation

    sToken = XML_Parse(sMessage, "isScoreOnly");
    if(sToken != sNoData) {
        bool ok;
        bool isScoreOnly = bool(sToken.toInt(&ok));
        if(!ok) {
            logMessage(logFile,
                       sFunctionName,
                       QString("Illegal orientation received: %1")
                       .arg(sToken));
            return;
        }
        pClientListDialog->remoteScoreOnlyValueReceived(isScoreOnly);
    }// orientation
}


int
ScoreController::SendToAll(QString sMessage) {
    QString sFunctionName = " ScoreController::SendToAll ";
    Q_UNUSED(sFunctionName)
#ifdef LOG_VERBOSE
    logMessage(logFile,
               sFunctionName,
               sMessage);
#endif
    for(int i=0; i< connectionList.count(); i++) {
        SendToOne(connectionList.at(i).pClientSocket, sMessage);
    }
    return 0;
}


int
ScoreController::SendToOne(QWebSocket* pClient, QString sMessage) {
    QString sFunctionName = " ScoreController::SendToOne ";
    if (pClient->isValid()) {
        for(int i=0; i< connectionList.count(); i++) {
           if(connectionList.at(i).clientAddress.toIPv4Address() == pClient->peerAddress().toIPv4Address()) {
                qint64 written = pClient->sendTextMessage(sMessage);
                Q_UNUSED(written)
                if(written != sMessage.length()) {
                    logMessage(logFile,
                               sFunctionName,
                               QString("Error writing %1").arg(sMessage));
                }
#ifdef LOG_VERBOSE
                else {
                    logMessage(logFile,
                               sFunctionName,
                               QString("Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pClient->peerAddress().toString()));
                }
#endif
                break;
            }
        }
    }
    else {
        logMessage(logFile,
                   sFunctionName,
                   QString("Client socket is invalid !"));
        RemoveClient(pClient->peerAddress());
        UpdateUI();
    }
    return 0;
}


void
ScoreController::RemoveClient(QHostAddress hAddress) {
    QString sFunctionName = " ScoreController::RemoveClient ";
    Q_UNUSED(sFunctionName)
    QString sFound = QString(" Not present");
    Q_UNUSED(sFound)
    QWebSocket *pClientToClose = Q_NULLPTR;
    pClientListDialog->clear();

    for(int i=0; i<connectionList.count(); i++) {
        if((connectionList.at(i).clientAddress.toIPv4Address() == hAddress.toIPv4Address()))
        {
            pClientToClose = connectionList.at(i).pClientSocket;
            pClientToClose->disconnect(); // No more events from this socket
            if(pClientToClose->isValid())
                pClientToClose->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, tr("Timeout in connection"));
            pClientToClose->deleteLater();
            pClientToClose = Q_NULLPTR;
            connectionList.removeAt(i);
#ifdef LOG_VERBOSE
            sFound = " Removed !";
            logMessage(logFile,
                       sFunctionName,
                       QString("%1 %2")
                       .arg(hAddress.toString())
                       .arg(sFound));
#endif
        } else {
            pClientListDialog->addItem(connectionList.at(i).clientAddress.toString());
        }
    }
}


void
ScoreController::UpdateUI() {
    QString sFunctionName = " ScoreController::RemoveClient ";
    Q_UNUSED(sFunctionName)
    if(connectionList.count() == 1) {
        startStopLoopSpotButton->setDisabled(false);
        startStopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
        panelControlButton->setDisabled(false);
        generalSetupButton->setDisabled(true);
        shutdownButton->setText(QString(tr("Spegni %1\nTabellone")).arg(connectionList.count()));
        shutdownButton->setDisabled(false);
        shutdownButton->show();
    }
    else if(connectionList.count() == 0) {
        startStopLoopSpotButton->setDisabled(true);
        startStopLoopSpotButton->setText(tr("Avvia\nSpot Loop"));
        startStopSpotButton->setDisabled(true);
        startStopSpotButton->setText(tr("Avvia\nSpot Singolo"));
        startStopSlideShowButton->setDisabled(true);
        startStopSlideShowButton->setText(tr("Avvia\nSlide Show"));
        startStopLiveCameraButton->setDisabled(true);
        startStopLiveCameraButton->setText(tr("Avvia\nLive Camera"));
        panelControlButton->setDisabled(true);
        generalSetupButton->setDisabled(false);
        shutdownButton->hide();
    }
    else {
        shutdownButton->setText(QString(tr("Spegni %1\nTabelloni")).arg(connectionList.count()));
    }
}


void
ScoreController::onNewConnection(QWebSocket *pClient) {
    QString sFunctionName = " ScoreController::onNewConnection ";

    QHostAddress address = pClient->peerAddress();
    QString sAddress = address.toString();

    connect(pClient, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onProcessTextMessage(QString)));
    connect(pClient, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onProcessBinaryMessage(QByteArray)));
    connect(pClient, SIGNAL(disconnected()),
            this, SLOT(onClientDisconnected()));

    RemoveClient(address);

    connection newConnection;
    newConnection.pClientSocket = pClient;
    newConnection.clientAddress = address;
    connectionList.append(newConnection);
    pClientListDialog->addItem(sAddress);
    UpdateUI();
    logMessage(logFile,
               sFunctionName,
               QString("Client connected: %1")
               .arg(sAddress));
}


void
ScoreController::onClientDisconnected() {
    QString sFunctionName = " ScoreController::onClientDisconnected ";
    QWebSocket* pClient = qobject_cast<QWebSocket *>(sender());
    QString sDiconnectedAddress = pClient->peerAddress().toString();
    logMessage(logFile,
               sFunctionName,
               QString("%1 disconnected because %2. Close code: %3")
               .arg(sDiconnectedAddress)
               .arg(pClient->closeReason())
               .arg(pClient->closeCode()));
    RemoveClient(pClient->peerAddress());
    UpdateUI();
}


void
ScoreController::onProcessBinaryMessage(QByteArray message) {
    QString sFunctionName = " ScoreController::onProcessBinaryMessage ";
    Q_UNUSED(message)
    logMessage(logFile,
               sFunctionName,
               QString("Unexpected binary message received !"));
}


QGroupBox*
ScoreController::CreateSpotButtonBox() {
    QGroupBox* spotButtonBox = new QGroupBox();
    QGridLayout* spotButtonLayout = new QGridLayout();

    startStopLoopSpotButton = new QPushButton(tr("Avvia\nSpot Loop"));
    startStopSpotButton = new QPushButton(tr("Avvia\nSpot Singolo"));
    startStopSlideShowButton = new QPushButton(tr("Avvia\nSlide Show"));
    startStopLiveCameraButton = new QPushButton(tr("Avvia\nLive Camera"));

    panelControlButton = new QPushButton(tr("Controlo\nTabelloni"));

    generalSetupButton = new QPushButton(tr("Setup\nGenerale"));
    shutdownButton = new QPushButton(QString(tr("Spegni %1\nTabelloni")).arg(connectionList.count()));

    startStopLoopSpotButton->setDisabled(true);
    startStopSpotButton->setDisabled(true);
    startStopSlideShowButton->setDisabled(true);
    startStopLiveCameraButton->setDisabled(true);

    panelControlButton->setDisabled(true);
    generalSetupButton->setDisabled(false);
    shutdownButton->setDisabled(true);
    shutdownButton->hide();

    connect(panelControlButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));
    connect(panelControlButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonPanelControlClicked()));

    connect(startStopLoopSpotButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSpotLoopClicked()));
    connect(startStopLoopSpotButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));
    connect(startStopSpotButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSpotClicked()));
    connect(startStopSpotButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));

    connect(startStopSlideShowButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSlideShowClicked()));
    connect(startStopSlideShowButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));

    connect(startStopLiveCameraButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopLiveCameraClicked()));
    connect(startStopLiveCameraButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));

    connect(generalSetupButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonSetupClicked()));
    connect(generalSetupButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));

    connect(shutdownButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonShutdownClicked()));
    connect(shutdownButton, SIGNAL(clicked()),
            pButtonClick, SLOT(play()));

    spotButtonLayout->addWidget(startStopLoopSpotButton,   0, 0, 1, 1);
    spotButtonLayout->addWidget(startStopSpotButton,       1, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            2, 0, 1, 1);
    spotButtonLayout->addWidget(startStopSlideShowButton,  3, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            4, 0, 1, 1);
    spotButtonLayout->addWidget(startStopLiveCameraButton, 5, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            6, 0, 1, 1);
    spotButtonLayout->addWidget(panelControlButton,        7, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            8, 0, 1, 1);

    spotButtonLayout->addWidget(generalSetupButton,        9, 0, 1, 1);
    spotButtonLayout->addWidget(shutdownButton,           10, 0, 1, 1);

    spotButtonBox->setLayout(spotButtonLayout);
    return spotButtonBox;
}


void
ScoreController::onButtonStartStopSpotClicked() {
    QString sMessage;
    if(connectionList.count() == 0) {
        startStopSpotButton->setText(tr("Avvia\nSpot Singolo"));
        startStopSpotButton->setDisabled(true);
        return;
    }
    if(startStopSpotButton->text().contains(QString(tr("Avvia")))) {
        for(int i=0; i<connectionList.count(); i++) {
            sMessage = QString("<spot>%1</spot>").arg(iCurrentSpot++);
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
        }
        startStopSpotButton->setText(tr("Chiudi\nSpot"));
        startStopLoopSpotButton->setDisabled(true);
        startStopSlideShowButton->setDisabled(true);
        startStopLiveCameraButton->setDisabled(true);
    }
    else {
        QString sMessage = "<endspot>1</endspot>";
        SendToAll(sMessage);
        startStopSpotButton->setText(tr("Avvia\nSpot Singolo"));
        startStopLoopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
    }
}


void
ScoreController::onButtonStartStopSpotLoopClicked() {
    QString sMessage;
    if(connectionList.count() == 0) {
        startStopLoopSpotButton->setText(tr("Avvia\nSpot Loop"));
        startStopLoopSpotButton->setDisabled(true);
        return;
    }
    if(startStopLoopSpotButton->text().contains(QString(tr("Avvia")))) {
        sMessage = QString("<spotloop>1</spotloop>");
        SendToAll(sMessage);
        startStopLoopSpotButton->setText(tr("Chiudi\nSpot Loop"));
        startStopSpotButton->setDisabled(true);
        startStopSlideShowButton->setDisabled(true);
        startStopLiveCameraButton->setDisabled(true);
    }
    else {
        sMessage = "<endspotloop>1</endspotloop>";
        SendToAll(sMessage);
        startStopLoopSpotButton->setText(tr("Avvia\nSpot Loop"));
        startStopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
    }
}


void
ScoreController::onButtonStartStopLiveCameraClicked() {
    QString sMessage;
    if(connectionList.count() == 0) {
        startStopLiveCameraButton->setText(tr("Avvia\nLive Camera"));
        startStopLiveCameraButton->setDisabled(true);
        return;
    }
    if(startStopLiveCameraButton->text().contains(QString(tr("Avvia")))) {
        sMessage = QString("<live>1</live>");
        SendToAll(sMessage);
        startStopLiveCameraButton->setText(tr("Chiudi\nLive Camera"));
        startStopLoopSpotButton->setDisabled(true);
        startStopSpotButton->setDisabled(true);
        startStopSlideShowButton->setDisabled(true);
    }
    else {
        sMessage = "<endlive>1</endlive>";
        SendToAll(sMessage);
        startStopLiveCameraButton->setText(tr("Avvia\nLive Camera"));
        startStopLoopSpotButton->setDisabled(false);
        startStopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
    }
}


void
ScoreController::onButtonStartStopSlideShowClicked() {
    QString sMessage;
    if(connectionList.count() == 0) {
        startStopSlideShowButton->setText(tr("Avvia\nSlide Show"));
        startStopSlideShowButton->setDisabled(true);
        return;
    }
    if(startStopSlideShowButton->text().contains(QString(tr("Avvia")))) {
        sMessage = "<slideshow>1</slideshow>";
        SendToAll(sMessage);
        startStopLoopSpotButton->setDisabled(true);
        startStopSpotButton->setDisabled(true);
        startStopLiveCameraButton->setDisabled(true);
        startStopSlideShowButton->setText(tr("Chiudi\nSlideshow"));
    }
    else {
        sMessage = "<endslideshow>1</endslideshow>";
        SendToAll(sMessage);
        startStopLoopSpotButton->setDisabled(false);
        startStopSpotButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
        startStopSlideShowButton->setText(tr("Avvia\nSlide Show"));
    }
}


void
ScoreController::onButtonShutdownClicked() {
    int iRes = QMessageBox::question(this,
                                     tr("ScoreController"),
                                     tr("Vuoi Spegnere i Tabelloni ?"),
                                     QMessageBox::Yes,
                                     QMessageBox::No|QMessageBox::Default,
                                     QMessageBox::NoButton);
    if(iRes != QMessageBox::Yes) return;
    QString sMessage = "<kill>1</kill>";
    SendToAll(sMessage);
}


void
ScoreController::onButtonPanelControlClicked() {
    hide();
    pClientListDialog->exec();
    show();
}


void
ScoreController::onButtonSetupClicked() {
    QString sFunctionName = QString(" ScoreController::onButtonSetupClicked ");

    QString sBaseDir;
#ifdef Q_OS_ANDROID
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    sBaseDir = environment.value(QString("SECONDARY_STORAGE"), QString(""));
    if(sBaseDir == QString("")) {
        sBaseDir = environment.value(QString("EXTERNAL_STORAGE"), QString("/storage/extSdCard/"));
    }
    else {
        QStringList secondaryList = sBaseDir.split(":", QString::SkipEmptyParts);
        sBaseDir = secondaryList.at(0);
    }
#else
    sBaseDir = QDir::homePath();
#endif

    QDir slideDir(sSlideDir);

    if(slideDir.exists()) {
        sSlideDir = QFileDialog::getExistingDirectory(
                        this,
                        tr("Slide Dir"),
                        sSlideDir,
                        QFileDialog::ShowDirsOnly);
    }
    else {
        sSlideDir = QFileDialog::getExistingDirectory(
                        this,
                        tr("Slide Dir"),
                        sBaseDir,
                        QFileDialog::ShowDirsOnly);
    }
    if(!sSlideDir.endsWith(QString("/"))) sSlideDir+= QString("/");
    slideDir = QDir(sSlideDir);
    if(sSlideDir != QString() && slideDir.exists()) {
        QStringList filter(QStringList() << "*.jpg" << "*.jpeg" << "*.png" << "*.JPG" << "*.JPEG" << "*.PNG");
        slideDir.setNameFilters(filter);
        slideList = slideDir.entryInfoList();
    }
    else {
        sSlideDir = QDir::homePath();
        if(!sSlideDir.endsWith(QString("/"))) sSlideDir+= QString("/");
        slideList = QFileInfoList();
    }
    logMessage(logFile,
               sFunctionName,
               QString("Found %1 slides").arg(slideList.count()));

    QDir spotDir(sSpotDir);
    if(spotDir.exists()) {
        sSpotDir = QFileDialog::getExistingDirectory(
                       this,
                       tr("Spot Dir"),
                       sSpotDir,
                       QFileDialog::ShowDirsOnly);
    }
    else {
        sSpotDir = QFileDialog::getExistingDirectory(
                       this,
                       tr("Spot Dir"),
                       sBaseDir,
                       QFileDialog::ShowDirsOnly);
    }

    if(!sSpotDir.endsWith(QString("/"))) sSpotDir+= QString("/");
    spotDir = QDir(sSpotDir);
    if(sSpotDir != QString() && spotDir.exists()) {
        QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
    else {
        sSpotDir = QDir::homePath();
        if(!sSpotDir.endsWith(QString("/"))) sSpotDir+= QString("/");
        spotList = QFileInfoList();
    }
    logMessage(logFile,
               sFunctionName,
               QString("Found %1 spots")
               .arg(spotList.count()));
    SaveStatus();
}


void
ScoreController::SaveStatus() {
}


QString
ScoreController::FormatStatusMsg() {
    QString sFunctionName = " ScoreController::FormatStatusMsg ";
    Q_UNUSED(sFunctionName)
    QString sMessage = QString();
    return sMessage;
}


void
ScoreController::onGetPanelOrientation(QString sClientIp) {
    QHostAddress hostAddress(sClientIp);
    for(int i=0; i<connectionList.count(); i++) {
        if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
            QString sMessage = "<getOrientation>1</getOrientation>";
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            return;
        }
    }
}


void
ScoreController::onGetIsPanelScoreOnly(QString sClientIp) {
    QHostAddress hostAddress(sClientIp);
    for(int i=0; i<connectionList.count(); i++) {
        if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
            QString sMessage = "<getScoreOnly>1</getScoreOnly>";
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            return;
        }
    }
}


void
ScoreController::onChangePanelOrientation(QString sClientIp, PanelOrientation orientation) {
    QHostAddress hostAddress(sClientIp);
    for(int i=0; i<connectionList.count(); i++) {
        if(connectionList.at(i).clientAddress.toIPv4Address() == hostAddress.toIPv4Address()) {
            QString sMessage = QString("<setOrientation>%1</setOrientation>")
                                       .arg(static_cast<int>(orientation));
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
            return;
        }
    }
}
