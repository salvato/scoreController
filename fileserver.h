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

#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfoList>

#include "netServer.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class FileServer : public NetServer
{
    Q_OBJECT
public:
    explicit FileServer(QFile *_logFile = Q_NULLPTR, QObject *parent = 0);
    void setServerPort(quint16 _port);
    bool setDirs(QString _sSlideDir, QString _sSpotDir);

private:
    int SendToOne(QWebSocket* pSocket, QString sMessage);

protected:
    QString XML_Parse(QString input_string, QString token);

signals:
    void fileServerDone(bool);
    void goTransfer();
    void serverAddress(QString);

public slots:
    void startServer();
    void senderThreadFinished();
    void onFileTransferDone(bool bSuccess);

private slots:
    void onNewConnection(QWebSocket *pClient);
    void onClientDisconnected();
    void onProcessTextMessage(QString sMessage);
    void onProcessBinaryMessage(QByteArray message);
    void onClientSocketError(QAbstractSocket::SocketError error);
    void onFileServerError(QWebSocketProtocol::CloseCode);

private:
    QString       sNoData;
    quint16       port;
    QString       sSlideDir;
    QStringList   slideList;
    QString       sSpotDir;
    QFileInfoList spotList;

    QVector<QWebSocket*> connections;

    QList<QThread*> senderThreads;
};

#endif // FILESERVER_H
