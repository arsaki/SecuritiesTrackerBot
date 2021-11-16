#ifndef SECURITIESTRACKERBOT_H
#define SECURITIESTRACKERBOT_H

#include <QObject>
#include <QtNetwork>
#include <QTimer>
#include <QtWebSockets/QWebSocket>
#include <iostream>

#include "telegramAPI/telegrambot.h"
#include "connectors/binancewebsocket.h"
#include "connectors/tinkoffwebsocket.h"
#include "connectors/interfacewebsocket.h"

class SecuritiesTrackerBot : public QObject
{
    Q_OBJECT
    //Element of global property list (Property is share, bond, etf or cryptocurrency)
    typedef struct
    {
        QString type;
        QString id;
        QString name;
        QString currency;
    } PropertyDescriptor;

    //Each property has list of such subscriptions
    typedef struct  {
        double price;
        bool upwards;
        qint32 userId;
    } Subscription;

    //Each used property has subscriptions list.
    //Ticker and currency are secondary and needed to accelerate searching
      typedef struct {
        QString ticker;
        QString currency;
        QList<Subscription> list;
    }PropertySubscriptionsList;

    //Each user has list of UserOrder
    typedef struct
    {
        QString ticker;
        QString figi;             //Cryptopair ticker also duplicate here
        double price;
        bool upwards;
    } UserSubscription;
    QMap<QString,PropertyDescriptor> propertiesList;                       //Property ticker to type, ID, full name and currency. Needed to process user requests by ticker.
    QMap<QString,PropertySubscriptionsList> subscriptionsMap;      //Property ID to list of subscriptions map.  Needed to process incoming stock exchange message.
    QMap<qint32, QList<UserSubscription>> usersMap;                     //User telegram ID to corresponding  user subscriptions map. Needed to save user subscriptions in file.
    QMap<QString,QString> figiToTicker;                                             //Reverse table accelerates searching ticker. Needed to process incoming stock exchange message.
    BinanceWebSocket binanceWebSocket;
    TinkoffWebSocket tinkoffWebSocket;
    TelegramBot *mainBot;
    TelegramBot *controlBot;
    QDateTime StartTime;
    QTimer propertiesListRefreshTimer;
    QString tinkoffToken;
    QString telegramMainToken;
    QString telegramControlToken;
    qint32 telegramMasterId = 0;
    QJsonDocument getJSONfromURL(QString URL, QString authHeader = "", QString authToken = "");
    int downloadTinkoffProperties(QString type);
    int downloadBinanceProperties();
    int saveUserToFile(qint32 id);
    int loadSettings(QString filename);
    int loadUsersSubscriptionsFromFiles();
    QString findTicker(QString searchkey);
    double getTinkoffPropertyCurrentPrice(QString figi);
    double getBinancePropertyCurrentPrice(QString cryptoPair);
    void addSubscription(QString figi, Subscription subscription);
    QString getSubscriptions(qint32 id, QString figi);
    void disableSubscriptions(qint32 id, QString figi);
    void cleanSubscriptions(QString figi);
    bool isNumber(QString string);
public:    
    explicit SecuritiesTrackerBot();
    ~SecuritiesTrackerBot() override;
    static void log(QString type, QString message);
    static void delay (int msec);
    static bool loggingEnabled;
public slots:
    void mainBotMessage(TelegramBotUpdate update);
    void controlBotMessage(TelegramBotUpdate update);
    void webSocketMessageReceived(QString propertyId, double price);
    void downloadPropertiesList();
private:

};
#endif // SECURITIESTRACKERBOT_H
