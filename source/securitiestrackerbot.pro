QT       +=  core gui charts network websockets
#greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_LFLAGS += -no-pie
#QMAKE_LFLAGS += -static Вредная настройка, и не нужная!

QMAKE_CC = i686-linux-gnu-gcc
QMAKE_CXX = i686-linux-gnu-g++

QMAKE_POST_LINK=../source/builddeb.sh

CONFIG += c++11 console
CONFIG -= app_bundle



# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    connectors/autoreconnectedwebsocket.cpp \
    connectors/binancewebsocket.cpp \
    connectors/tinkoffwebsocket.cpp \
    main.cpp \
    securitiestrackerbot.cpp \
    telegramAPI/jsonhelper.cpp \
    telegramAPI/telegrambot.cpp \

HEADERS += \
    connectors/autoreconnectedwebsocket.h \
    connectors/binancewebsocket.h \
    connectors/interfacewebsocket.h \
    connectors/tinkoffwebsocket.h \
    securitiestrackerbot.h \
    telegramAPI/jsonhelper.h \
    telegramAPI/telegrambot.h \
    telegramAPI/telegramdatainterface.h \
    telegramAPI/telegramdatastructs.h \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

FORMS +=

DISTFILES += \
    securitiestrackerbot.conf \
    todo.txt
