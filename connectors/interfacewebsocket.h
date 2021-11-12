#ifndef INTERFACEWEBSOCKET_H
#define INTERFACEWEBSOCKET_H
#include <QString>
#include <QObject>

class InterfaceWebSocket : public QObject
{
    Q_OBJECT
public:
    virtual void subscribeToProperty(const QString& propertyId) = 0;
    virtual void unsubscribeFromProperty(const QString& propertyId) = 0;
    virtual int getDisconnectionCount() = 0;
signals:
    void priceUpdated(QString propertyId, double price);
};


#endif // INTERFACEWEBSOCKET_H
