TEMPLATE = app

QT = core testlib datasync

CONFIG += console
CONFIG -= app_bundle

TARGET = tst_tablemachine

HEADERS += \
	tabledatamodel_p.h

SOURCES += \
	tabledatamodel.cpp \
	tst_tablemachine.cpp

STATECHARTS += \
	$$PWD/../../../../src/datasync/tablestatemachine_p.scxml

DEFINES += SRCDIR=\\\"$$PWD/\\\"

include($$PWD/../../testrun.pri)