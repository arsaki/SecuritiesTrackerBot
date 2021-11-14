#include "securitiestrackerbot.h"
#include "autoreconnectedwebsocket.h"
#define RECONNECT_TIMEOUT_MS 10000


AutoReconnectedWebSocket::AutoReconnectedWebSocket() : QWebSocket()
{
    disconnectionCount = 0;
    connect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
}

AutoReconnectedWebSocket::~AutoReconnectedWebSocket()
{
    if (this->state()!=QAbstractSocket::UnconnectedState)
        close();
}

void AutoReconnectedWebSocket::open(const QUrl &url)
{
    URL = url.toString();
    connectedByURL = true;
    QWebSocket::open(URL);
}

void AutoReconnectedWebSocket::open(const QNetworkRequest &request)
{
    AutoReconnectedWebSocket::request = request;
    connectedByURL = false;
    QWebSocket::open(request);
}

void AutoReconnectedWebSocket::close()
{
    disconnect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
    QEventLoop loop;
    connect(this, &AutoReconnectedWebSocket::disconnected, &loop, &QEventLoop::quit);
    QWebSocket::close();
    loop.exec();
    disconnect(this, &AutoReconnectedWebSocket::disconnected, &loop, &QEventLoop::quit);
}

int AutoReconnectedWebSocket::getDisconnectionCount()
{
    return disconnectionCount;
}


void AutoReconnectedWebSocket::webSocketDisconnected()
{
    disconnectionCount++ ;
    while (this->state() != QAbstractSocket::ConnectedState)
    {
        if (connectedByURL)
            open(URL);
        else
            open(request);
        SecuritiesTrackerBot::delay(RECONNECT_TIMEOUT_MS);
    }
}
