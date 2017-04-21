#include "scorecontroller.h"
#include "clientlistdialog.h"


#define DISCOVERY_PORT     45453
#define SERVER_SOCKET_PORT 45454
#define FILE_UPDATER_PORT  45455


#define UPDATE_PERIOD  2000

#define VOLLEY_PANEL 0
#define FIRST_PANEL  VOLLEY_PANEL
#define BASKET_PANEL 1
#define LAST_PANEL   BASKET_PANEL

//#define QT_DEBUG
#define LOG_MESG


ScoreController::ScoreController(QWidget *parent)
    : QWidget(parent)
    , pClientListDialog(new ClientListDialog(this))
    , connectionList(QList<connection>())
    , discoveryPort(DISCOVERY_PORT)
    , serverPort(SERVER_SOCKET_PORT)
    , updaterPort(FILE_UPDATER_PORT)
    , discoveryAddress(QHostAddress("224.0.0.1"))
{
    QString sFunctionName = QString(" Volley_Controller::Volley_Controller ");
    Q_UNUSED(sFunctionName)

    buttonClick.setSource(QUrl::fromLocalFile(":/key.wav"));
    sIpAddresses = QString();
    sNoData      = QString(tr("NoData"));

    QString sBaseDir;
#ifdef Q_OS_ANDROID
    sBaseDir = QString("/storage/extSdCard/");
#else
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
#endif
    sSlideDir   = QString("%1slides").arg(sBaseDir);
    sSpotDir    = QString("%1spots").arg(sBaseDir);
    logFileName = QString("%1volley_controller.txt").arg(sBaseDir);

    logFile       = Q_NULLPTR;
    slideList     = QStringList();
    iCurrentSlide = 0;
    iCurrentSpot  = 0;
    updatePeriod  = UPDATE_PERIOD;

    sDebugInformation.setString(&sDebugMessage);
    sDebugMessage = QString();

    PrepareLogFile();

    pExitTimer = new QTimer(this);
    connect(pExitTimer, SIGNAL(timeout()),
            this, SLOT(close()));

    waitForNetworkReady();

    sHostName = QHostInfo::localHostName();

    prepareDiscovery();

    if(prepareServer() < 0) {
        return;
    }

    // Creating a File Update Service
    pFileUpdaterServer = new FileServer(logFile, Q_NULLPTR);
    connect(pFileUpdaterServer, SIGNAL(fileServerDone(bool)),
            this, SLOT(onFileServerDone(bool)));
    pFileUpdaterServer->setServerPort(updaterPort);
    pFileUpdaterServer->setDirs(sSlideDir, sSpotDir);
    pFileServerThread = new QThread();
    pFileUpdaterServer->moveToThread(pFileServerThread);
    connect(this, SIGNAL(startFileServer()),
            pFileUpdaterServer, SLOT(startServer()));
    pFileServerThread->start(QThread::LowestPriority);
    emit startFileServer();

//    pPeriodicUpdate = new QTimer(this);
//    connect(pPeriodicUpdate, SIGNAL(timeout()),
//            this, SLOT(onPeriodicUpdate()));
//    pPeriodicUpdate->start(updatePeriod);

    // Pan-Tilt Camera management
    connect(pClientListDialog, SIGNAL(disableVideo()),
            this, SLOT(onStopCamera()));
    connect(pClientListDialog, SIGNAL(enableVideo(QString)),
            this, SLOT(onStartCamera(QString)));
    connect(pClientListDialog, SIGNAL(newPanValue(QString,int)),
            this, SLOT(onSetNewPanValue(QString, int)));
    connect(pClientListDialog, SIGNAL(newTiltValue(QString,int)),
            this, SLOT(onSetNewTiltValue(QString, int)));

    QGridLayout*  mainLayout = new QGridLayout();

    mainLayout->addWidget(createGamePanel(),     0, 0, 3, 1);
    mainLayout->addWidget(createGameButtonBox(), 3, 0, 1, 2);
    mainLayout->addWidget(createSpotButtonBox(), 0, 2, 4, 1);
    setLayout(mainLayout);
}


void
ScoreController::WaitForNetworkReady() {
    int iResponse;
    while(!isConnectedToNetwork()) {
        iResponse = QMessageBox::critical(this,
                                          "Connessione Assente",
                                          "Connettiti alla rete e ritenta",
                                          QMessageBox::Retry,
                                          QMessageBox::Ignore,
                                          QMessageBox::Abort);

        if(iResponse == QMessageBox::Ignore) {
            break;
        } else if(iResponse == QMessageBox::Abort) {
            pExitTimer->start(1000);
            QCursor waitCursor;
            waitCursor.setShape(Qt::WaitCursor);
            setCursor(waitCursor);
            break;
        }
        QThread::sleep(1);
    }
}


ScoreController::~ScoreController() {
}


void
ScoreController::onFileServerDone(bool bError) {
    QString sFunctionName = QString(" ScoreController::onFileServerDone ");
    if(bError) {
        logMessage(sFunctionName, QString("File server stopped with errors"));
    }
    else {
        logMessage(sFunctionName, QString("File server stopped without errors"));
    }
}


void
ScoreController::logMessage(QString sFunctionName, QString sMessage) {
    Q_UNUSED(sFunctionName)
    Q_UNUSED(sMessage)
#ifdef LOG_MESG
    sDebugMessage = QString();
    sDebugInformation << dateTime.currentDateTime().toString()
                      << sFunctionName
                      << sMessage;
    if(logFile) {
      logFile->write(sDebugMessage.toUtf8().data());
      logFile->write("\n");
      logFile->flush();
    }
    else {
        qDebug() << sDebugMessage;
    }
#endif
}


void
ScoreController::prepareDiscovery() {
    QString sFunctionName = QString(" ScoreController::prepareDiscovery ");
    sIpAddresses = QString();
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
                        sIpAddresses += list[j].ip().toString() + tr(",");
                        discoverySocketArray.append(pDiscoverySocket);
                        connect(pDiscoverySocket, SIGNAL(readyRead()),
                                this, SLOT(onProcessConnectionRequest()));
                        logMessage(sFunctionName,
                                   QString("Listening for connections at address: %1 port:%2")
                                   .arg(discoveryAddress.toString())
                                   .arg(discoveryPort));
                    }
                    else {
                        logMessage(sFunctionName,
                                   QString("Unable to bound %1")
                                   .arg(discoveryAddress.toString()));
                    }
                }
            }// for(int j=0; j<list.count(); j++)
        }
    }// for(int i=0; i<interfaceList.count(); i++)
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
          sMessage = tr("<pan>%1</pan>").arg(newPan);
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
          sMessage = tr("<tilt>%1</tilt>").arg(newTilt);
          SendToOne(connectionList.at(i).pClientSocket, sMessage);
          return;
      }
  }
}


bool
ScoreController::PrepareLogFile() {
#ifdef LOG_MESG
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
        logFile = NULL;
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
    logMessage(sFunctionName, result ? QString("true") : QString("false"));
    return result;
}


void
ScoreController::onProcessConnectionRequest() {
    QString sFunctionName = " ScoreController::onProcessConnectionRequest ";
    sDebugMessage = QString();
    QByteArray datagram;
    QString sToken;
    QUdpSocket* pDiscoverySocket = qobject_cast<QUdpSocket*>(sender());

    while(pDiscoverySocket->hasPendingDatagrams()) {
        datagram.resize(pDiscoverySocket->pendingDatagramSize());

        QHostAddress hostAddress;
        quint16 port;
        pDiscoverySocket->readDatagram(datagram.data(), datagram.size(), &hostAddress, &port);
        sToken = XML_Parse(datagram.data(), "getServer");
        if(sToken != sNoData) {
            logMessage(sFunctionName,
                       QString("Connection request from: %1 at Address %2:%3")
                       .arg(sToken)
                       .arg(hostAddress.toString())
                       .arg(port));
            RemoveClient(hostAddress);
            sMessage = "<serverIP>" + sIpAddresses + "</serverIP>";
            logMessage(sFunctionName, QString("Sent: %1").arg(sMessage));
            sendAcceptConnection(pDiscoverySocket, sMessage, hostAddress, port);
        }
    }
}


int
ScoreController::sendAcceptConnection(QUdpSocket* pDiscoverySocket, QString sMessage, QHostAddress hostAddress, quint16 port) {
    QString sFunctionName = " ScoreController::sendAcceptConnection ";
    sDebugMessage = QString();
    Q_UNUSED(sFunctionName)
    QByteArray datagram = sMessage.toUtf8();
    if(!pDiscoverySocket->isValid()) {
        logMessage(sFunctionName, QString("Discovery Socket Invalid !"));
        return -1;
    }
    qint64 bytesWritten = pDiscoverySocket->writeDatagram(datagram.data(), datagram.size(), hostAddress, port);
    Q_UNUSED(bytesWritten)
    if(bytesWritten != datagram.size()) {
      logMessage(sFunctionName, QString("Unable to send data !"));
    }
    return 0;
}


void
ScoreController::closeEvent(QCloseEvent *event) {
    QString sFunctionName = " ScoreController::closeEvent ";
    sDebugMessage = QString();
    Q_UNUSED(sFunctionName)

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
                connectionList.at(i).pClientSocket->close(QWebSocketProtocol::CloseCodeNormal, "Server Closed");
            }
            connectionList.clear();
        } else
        if(answer == QMessageBox::Yes) {
            sMessage = "<kill>1</kill>";
            SendToAll(sMessage);
        }
        if(logFile) {
            logFile->close();
        }
    }
}


int
ScoreController::prepareServer() {
    QString sFunctionName = " ScoreController::prepareServer ";
    sDebugMessage = QString();
    Q_UNUSED(sFunctionName)

    pPanelServer = new NetServer(QString("PanelServer"), logFile, this);
    pPanelServer->prepareServer(serverPort);

    connect(pPanelServer, SIGNAL(newConnection(QWebSocket *)),
            this, SLOT(onNewConnection(QWebSocket *)));
    return 0;
}


void
ScoreController::onProcessTextMessage(QString sMessage) {
    QString sFunctionName = " ScoreController::onProcessTextMessage ";
    QString sToken;

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
        QStringList values = QStringList(sToken.split(tr(","),QString::SkipEmptyParts));
        pClientListDialog->onRemotePanTiltReceived(values.at(0).toInt(), values.at(1).toInt());
    }// pan_tilt

    sToken = XML_Parse(sMessage, "image_size");
    if(sToken != sNoData) {
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        logMessage(sFunctionName,
                   QString("%1 received %2 bytes")
                   .arg(pClient->peerAddress().toString())
                   .arg(sToken));
    }// image_size

    sToken = XML_Parse(sMessage, "send_image");
    if(sToken != sNoData) {
        if(slideList.isEmpty()) {
            logMessage(sFunctionName,
                       QString("Empty slide list"));
            return;
        }
        QStringList dimension = QStringList(sToken.split(tr(","),QString::SkipEmptyParts));
        if(dimension.count() < 2) {
            logMessage(sFunctionName, QString("Malformed image request"));
            return;
        }
        QSize imageSize = QSize(dimension.at(0).toInt(), dimension.at(1).toInt());
        QImage image;
        if(!image.load(sSlideDir+slideList[iCurrentSlide])) {
            logMessage(sFunctionName,
                       QString("Unable to read:  %1")
                       .arg(sSlideDir+slideList[iCurrentSlide]));
            return;
        }
        QByteArray ba;
        QBuffer buffer(&ba);
        if(!buffer.open(QIODevice::WriteOnly)) {
            logMessage(sFunctionName, QString("Unable to open image buffer"));
            return;
        }
        image = image.scaled(imageSize, Qt::KeepAspectRatio);
        if(!image.save(&buffer, "JPG", -1)) {// writes image into ba in JPEG format
            logMessage(sFunctionName, QString("Unable to save image into buffer"));
            buffer.close();
            return;
        }
        buffer.close();
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        if(pClient->isValid()) {
            logMessage(sFunctionName,
                       QString("Sending image %1 to %2")
                       .arg(iCurrentSlide)
                       .arg(pClient->peerAddress().toString()));
            for(int i=0; i< connectionList.count(); i++) {
               if(connectionList.at(i).clientAddress == pClient->peerAddress()) {
                    int bytesSent = pClient->sendBinaryMessage(ba);
                    if(bytesSent != ba.size()) {
                        logMessage(sFunctionName, QString("Unable to send the image to client"));
                    }
                    break;
                }
            }
            iCurrentSlide = (iCurrentSlide + 1) % slideList.count();
        }
        else {
            logMessage(sFunctionName, QString("Client socket is Invalid !"));
        }
    }// send_image

    sToken = XML_Parse(sMessage, "send_spot_list");
    if(sToken != sNoData) {
        if(spotList.isEmpty())
            return;
        QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
        if(pClient->isValid()) {
            sMessage = QString("<spot_list>");
            for(int i=0; i<spotList.count()-1; i++) {
                sMessage += spotList.at(i).fileName();
                sMessage += QString(";%1,").arg(spotList.at(i).size());
            }
            int i = spotList.count()-1;
            sMessage += spotList.at(i).fileName();
            sMessage += QString(";%1</spot_list>").arg(spotList.at(i).size());
            SendToOne(pClient, sMessage);
        }
    }// send_spot_list

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
            sMessage = QString("<setConf>%1</setConf>").arg(VOLLEY_PANEL);
            SendToOne(pClient, sMessage);
        }
    }// getConf
}


int
ScoreController::SendToAll(QString sMessage) {
    QString sFunctionName = " ScoreController::SendToAll ";
    logMessage(sFunctionName, sMessage);
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
                    logMessage(sFunctionName, QString("Error writing %1").arg(sMessage));
                }
                else {
                    logMessage(sFunctionName,
                               QString("Sent %1 to: %2")
                               .arg(sMessage)
                               .arg(pClient->peerAddress().toString()));
                }
                break;
            }
        }
    }
    else {
        logMessage(sFunctionName, QString("Client socket is invalid !"));
        RemoveClient(pClient->peerAddress());
    }
    return 0;
}


void
ScoreController::RemoveClient(QHostAddress hAddress) {
    QString sFunctionName = " ScoreController::RemoveClient ";
    QString sFound = tr(" Not present");
    QWebSocket *pClientToClose = NULL;
    pClientListDialog->clear();

    for(int i=connectionList.count()-1; i>=0; i--) {
        if((connectionList.at(i).clientAddress.toIPv4Address() == hAddress.toIPv4Address()))
        {
            pClientToClose = connectionList.at(i).pClientSocket;
            disconnect(pClientToClose, 0, 0, 0); // No more events from this socket
            pClientToClose->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, tr("Timeout in connection"));
            connectionList.removeAt(i);
            sFound = " Removed !";
            logMessage(sFunctionName,
                       QString("%1 %2")
                       .arg(hAddress.toString())
                       .arg(sFound));
        } else {
            pClientListDialog->addItem(connectionList.at(i).clientAddress.toString());
        }
    }
    if(connectionList.count() == 1)
        shutdownButton->setText(QString("Spegni %1\nTabellone").arg(connectionList.count()));
    else
        shutdownButton->setText(QString("Spegni %1\nTabelloni").arg(connectionList.count()));
    if(pClientToClose != NULL) {
        pClientToClose->abort();
        pClientToClose->deleteLater();
    }
    if(connectionList.count() == 0) {
        startStopLoopSpotButton->setDisabled(true);
        startStopSpotButton->setDisabled(true);
        startStopSlideShowButton->setDisabled(true);
        startStopLiveCameraButton->setDisabled(true);
        cameraControlButton->setDisabled(true);
        generalSetupButton->setDisabled(false);
        shutdownButton->setDisabled(true);
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
    if(connectionList.count() == 1) {
        startStopLoopSpotButton->setDisabled(false);
        startStopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
        cameraControlButton->setDisabled(false);
        generalSetupButton->setDisabled(true);
        shutdownButton->setDisabled(false);
    }
    shutdownButton->setText(QString("Spegni %1\nTabelloni").arg(connectionList.count()));
    logMessage(sFunctionName,
               QString("Client connected: %1")
               .arg(sAddress));
}


void
ScoreController::onClientDisconnected() {
    QString sFunctionName = " ScoreController::onClientDisconnected ";
    QWebSocket* pClient = qobject_cast<QWebSocket *>(sender());
    QString sDiconnectedAddress = pClient->peerAddress().toString();
    logMessage(sFunctionName,
               QString("%1 disconnected because %2. Close code: %3")
               .arg(sDiconnectedAddress)
               .arg(pClient->closeReason())
               .arg(pClient->closeCode()));
    RemoveClient(pClient->peerAddress());
}


void
ScoreController::onProcessBinaryMessage(QByteArray message) {
    QString sFunctionName = " ScoreController::onProcessBinaryMessage ";
    Q_UNUSED(message)
    logMessage(sFunctionName, QString("Unexpected binary message received !"));
}


QGroupBox*
ScoreController::createSpotButtonBox() {
    QGroupBox* spotButtonBox = new QGroupBox(tr("Controlli"));
    QGridLayout* spotButtonLayout = new QGridLayout();

    startStopLoopSpotButton = new QPushButton(tr("Avvia\nSpot Loop"));
    startStopSpotButton = new QPushButton(tr("Avvia\nSpot Singolo"));
    startStopSlideShowButton = new QPushButton(tr("Avvia\nSlide Show"));
    startStopLiveCameraButton = new QPushButton(tr("Avvia\nLive Camera"));

    cameraControlButton = new QPushButton(tr("Controlo\nTelecamere"));

    generalSetupButton = new QPushButton(tr("Setup\nGenerale"));
    shutdownButton = new QPushButton(QString("Spegni %1\nTabelloni").arg(connectionList.count()));

    startStopLoopSpotButton->setDisabled(true);
    startStopSpotButton->setDisabled(true);
    startStopSlideShowButton->setDisabled(true);
    startStopLiveCameraButton->setDisabled(true);

    cameraControlButton->setDisabled(true);
    generalSetupButton->setDisabled(false);
    shutdownButton->setDisabled(true);

    connect(cameraControlButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));
    connect(cameraControlButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonCameraControlClicked()));

    connect(startStopLoopSpotButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSpotLoopClicked()));
    connect(startStopLoopSpotButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));
    connect(startStopSpotButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSpotClicked()));
    connect(startStopSpotButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    connect(startStopSlideShowButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopSlideShowClicked()));
    connect(startStopSlideShowButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    connect(startStopLiveCameraButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonStartStopLiveCameraClicked()));
    connect(startStopLiveCameraButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    connect(generalSetupButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonSetupClicked()));
    connect(generalSetupButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    connect(shutdownButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonShutdownClicked()));
    connect(shutdownButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    spotButtonLayout->addWidget(startStopLoopSpotButton,   0, 0, 1, 1);
    spotButtonLayout->addWidget(startStopSpotButton,       1, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            2, 0, 1, 1);
    spotButtonLayout->addWidget(startStopSlideShowButton,  3, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            4, 0, 1, 1);
    spotButtonLayout->addWidget(startStopLiveCameraButton, 5, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            6, 0, 1, 1);
    spotButtonLayout->addWidget(cameraControlButton,       7, 0, 1, 1);

    spotButtonLayout->addWidget(new QLabel(""),            8, 0, 1, 1);

    spotButtonLayout->addWidget(generalSetupButton,        9, 0, 1, 1);
    spotButtonLayout->addWidget(shutdownButton,           10, 0, 1, 1);

    spotButtonBox->setLayout(spotButtonLayout);
    return spotButtonBox;
}


QGroupBox*
ScoreController::createGameButtonBox() {
    QGroupBox* gameButtonBox = new QGroupBox();
    QHBoxLayout* gameButtonLayout = new QHBoxLayout();
    newSetButton  = new QPushButton(tr("Nuovo\nSet"));
    newGameButton = new QPushButton(tr("Nuova\nPartita"));
    changeFieldButton = new QPushButton(tr("Cambio\nCampo"));

    connect(newSetButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonNewSetClicked()));
    connect(newSetButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));
    connect(newGameButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonNewGameClicked()));
    connect(newGameButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));
    connect(changeFieldButton, SIGNAL(clicked(bool)),
            this, SLOT(onButtonChangeFieldClicked()));
    connect(changeFieldButton, SIGNAL(clicked()),
            &buttonClick, SLOT(play()));

    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(newSetButton);
    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(newGameButton);
    gameButtonLayout->addStretch();
    gameButtonLayout->addWidget(changeFieldButton);
    gameButtonLayout->addStretch();
    gameButtonBox->setLayout(gameButtonLayout);
    return gameButtonBox;
}


void
ScoreController::onButtonStartStopSpotClicked() {
    if(connectionList.count() == 0) {
        startStopSpotButton->setText(tr("Avvia\nSpot Singolo"));
        startStopSpotButton->setDisabled(true);
        return;
    }
    if(startStopSpotButton->text().contains(QString("Avvia"))) {
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
        sMessage = "<endspot>1</endspot>";
        SendToAll(sMessage);
        startStopSpotButton->setText(tr("Avvia\nSpot Singolo"));
        startStopLoopSpotButton->setDisabled(false);
        startStopSlideShowButton->setDisabled(false);
        startStopLiveCameraButton->setDisabled(false);
    }
}


void
ScoreController::onButtonStartStopSpotLoopClicked() {
    if(connectionList.count() == 0) {
        startStopLoopSpotButton->setText(tr("Avvia\nSpot Loop"));
        startStopLoopSpotButton->setDisabled(true);
        return;
    }
    if(startStopLoopSpotButton->text().contains(QString("Avvia"))) {
        for(int i=0; i<connectionList.count(); i++) {
            sMessage = QString("<spotloop>1</spotloop>");
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
        }
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
    if(connectionList.count() == 0) {
        startStopLiveCameraButton->setText(tr("Avvia\nLive Camera"));
        startStopLiveCameraButton->setDisabled(true);
        return;
    }
    if(startStopLiveCameraButton->text().contains(QString("Avvia"))) {
        for(int i=0; i<connectionList.count(); i++) {
            sMessage = QString("<live>1</live>");
            SendToOne(connectionList.at(i).pClientSocket, sMessage);
        }
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
    if(connectionList.count() == 0) {
        startStopSlideShowButton->setText(tr("Avvia\nSlide Show"));
        startStopSlideShowButton->setDisabled(true);
        return;
    }
    if(startStopSlideShowButton->text().contains(QString("Avvia"))) {
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
    sMessage = "<kill>1</kill>";
    SendToAll(sMessage);
}


void
ScoreController::onButtonCameraControlClicked() {
    pClientListDialog->exec();
}


void
ScoreController::onButtonSetupClicked() {
    QString sFunctionName = QString(" ScoreController::onButtonSetupClicked ");
    QFileDialog chooseDir(this, Qt::Dialog);
    chooseDir.setViewMode(QFileDialog::List);

    QDir slideDir(sSlideDir);
    if(slideDir.exists()) {
        sSlideDir = chooseDir.getExistingDirectory(
                        this,
                        "Seleziona la cartella con le Slide",
                        sSlideDir,
                        QFileDialog::ShowDirsOnly |
                        QFileDialog::DontResolveSymlinks);
    }
    else {
        sSlideDir = chooseDir.getExistingDirectory(
                        this,
                        "Seleziona la cartella con le Slide",
                        QDir::homePath(),
                        QFileDialog::ShowDirsOnly |
                        QFileDialog::DontResolveSymlinks);
    }
    if(!sSlideDir.endsWith(QString("/"))) sSlideDir+= QString("/");
    slideDir = QDir(sSlideDir);
    if(sSlideDir != QString() && slideDir.exists()) {
        pSettings->setValue("directories/slides", sSlideDir);
        QStringList filter(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
        slideDir.setNameFilters(filter);
        slideList = slideDir.entryList();
    }
    logMessage(sFunctionName, QString("Found %1 slides").arg(slideList.count()));

    QDir spotDir(sSpotDir);
    if(spotDir.exists()) {
        sSpotDir = chooseDir.getExistingDirectory(
                       this,
                       tr("Seleziona la cartella con gli Spot"),
                       sSpotDir,
                       QFileDialog::ShowDirsOnly |
                       QFileDialog::DontResolveSymlinks);
    }
    else {
        sSpotDir = chooseDir.getExistingDirectory(
                       this,
                       tr("Seleziona la cartella con gli Spot"),
                       QDir::homePath(),
                       QFileDialog::ShowDirsOnly |
                       QFileDialog::DontResolveSymlinks);
    }
    if(!sSpotDir.endsWith(QString("/"))) sSpotDir+= QString("/");
    spotDir = QDir(sSpotDir);
    if(sSpotDir != QString() && spotDir.exists()) {
        pSettings->setValue("directories/spots", sSpotDir);
        QStringList nameFilter(QStringList() << "*.mp4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
    logMessage(sFunctionName, QString("Found %1 spots").arg(spotList.count()));
//    pFileUpdaterServer->setDirs(sSlideDir, sSpotDir);
//    sMessage = QString("<reloadSpot>1</reloadSpot>");
//    SendToAll(sMessage);
}
