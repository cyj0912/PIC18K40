#-------------------------------------------------
#
# Project created by QtCreator 2016-10-14T21:57:29
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = pic18prog
TEMPLATE = app


SOURCES += main.cpp\
        MainWindow.cpp \
    intelhexclass.cpp \
    qhexedit/chunks.cpp \
    qhexedit/commands.cpp \
    qhexedit/qhexedit.cpp

HEADERS  += MainWindow.h \
    intelhexclass.h \
    qhexedit/chunks.h \
    qhexedit/commands.h \
    qhexedit/qhexedit.h

FORMS    += MainWindow.ui

RC_FILE += pic18prog.rc
