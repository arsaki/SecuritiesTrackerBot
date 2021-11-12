#ifndef TINKOFFWEBSOCKET_H
#define TINKOFFWEBSOCKET_H

#include <QObject>
#include "interfacewebsocket.h"
#include "autoreconnectedwebsocket.h"

class TinkoffWebSocket :  public InterfaceWebSocket
{
    Q_OBJECT
public:
    explicit TinkoffWebSocket();
     ~TinkoffWebSocket() override;
    virtual void subscribeToProperty(const QString &propertyId) override;
    virtual void unsubscribeFromProperty(const QString &propertyId) override;
    virtual int    getDisconnectionCount() override     {return disconnectionCount;}
    void setToken(const QString &token)                     {tinkoffToken = token;}
signals:
    void priceUpdated(QString propertyId,double price);
protected slots:
    void webSocketConnected();
    void webSocketDisconnected(); // ToDo Add connect()!!!
    void webSocketError();
    void webSocketMessageReceived(const QString &message);

private:
    void openTinkoffWebSocket();
    AutoReconnectedWebSocket webSocket;
    QString URL = "wss://api-invest.tinkoff.ru/openapi/md/v1/md-openapi/ws";
    QString tinkoffToken;
    QList<QString> propertiesList;
    int disconnectionCount = 0;
    QTime disconnectTime;
    bool webSocketWasInterrupted = false;
    int tinkoffDisconnectCount = 0;
};

#endif // TINKOFFWEBSOCKET_H
