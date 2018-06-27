#-------------------------------------------------
#
# Project created by QtCreator 2017-05-28T16:15:53
#
#-------------------------------------------------

DEFINES+="UNIX_OS=1"
DEFINES+="WINDOWS_OS=2"
DEFINES+="RASPBIAN_OS=3"

TARGET_OS=WINDOWS_OS

DEFINES+="TARGET_OS=$${TARGET_OS}"

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

contains(TARGET_OS, UNIX_OS) {
    INCLUDEPATH += /opt/RasPi/sysroot/usr/include
    LIBS += -L//opt/RasPi/sysroot/usr/lib
    QT += xlsx
}

contains(TARGET_OS, RASPBIAN_OS) {
    INCLUDEPATH += /usr/include
    LIBS += -L/usr/lib -lwiringPi -lusb-1.0
    QT += xlsx
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to po+= axcontainerrt your code away from it.
#DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += main.cpp\
        mainwindow.cpp \
    qcustomplot.cpp \
    plot.cpp \
    utils.cpp \
    tsdialog.cpp \
    keyboard.cpp \
    reportdialog.cpp \
    saveusb.cpp

HEADERS  += mainwindow.h \
    qcustomplot.h \
    utils.h \
    tsdialog.h \
    keyboard.h \
    reportdialog.h \
    saveusb.h

FORMS    += mainwindow.ui \
    tsdialog.ui \
    keyboard.ui \
    reportdialog.ui \
    saveusb.ui

INSTALLS        += target
target.path     = /home/pi

RESOURCES += \
    resource.qrc

OTHER_FILES +=
