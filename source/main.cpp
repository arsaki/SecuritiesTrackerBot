#include "securitiestrackerbot.h"

#include <iostream>
#include <QApplication>
#include <QWebSocket>
#include <QSslSocket>
#include <QLabel>
#include <signal.h>


int main(int argc, char *argv[])
{
    if (!QSslSocket::supportsSsl()) {
        std::cout << "This system does not support TLS. Please, install OpenSSL library." << std::endl;
        return 1;
    }
    // Adding "-platform offscreen" options
    argc += 2;
    char ** argvnew = static_cast<char **>(malloc(static_cast<size_t>(argc)*sizeof(char*)));
    memcpy(argvnew,argv, static_cast<size_t>(argc)*sizeof(char*));
    char argplatform[] = "-platform";
    argvnew[argc-2] = argplatform;
    char argoffscreen[] = "offscreen";
    argvnew[argc-1] = argoffscreen;

    QApplication a(argc, argvnew);
    SecuritiesTrackerBot w;

    return a.exec();
}
