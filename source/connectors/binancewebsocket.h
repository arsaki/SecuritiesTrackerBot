#ifndef BINANCEWEBSOCKET_H
#define BINANCEWEBSOCKET_H
#include <QObject>
#include <QWebSocket>
#include "interfacewebsocket.h"
#include "autoreconnectedwebsocket.h"

class SecurityTrackerBot;

enum BinanceType : bool {Spot = false, Perpetual = true};

class BinanceWebSocket :  public InterfaceWebSocket
{
    Q_OBJECT
public:
    BinanceWebSocket(bool type = Spot);
    ~BinanceWebSocket() override;
    virtual void subscribeToProperty(const QString &propertyId) override;
    virtual void unsubscribeFromProperty(const QString &propertyId) override;
    virtual int    getDisconnectionCount() override;
signals:
    void priceUpdated(QString propertyId,double price);
protected slots:
    void webSocketMessageReceived(const QString &message);
private:
    QMap<QString,AutoReconnectedWebSocket*> propertyIdToSocket;
    QString URL;
    bool type;          //Spot or Perpetual
};

#endif // BINANCEWEBSOCKET_H
