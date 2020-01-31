TEMPLATE = app

QT = core testlib datasync datasync-private

CONFIG += console
CONFIG -= app_bundle

TARGET = tst_enginedatamodel

DS_SRC_DIR = $$PWD/../../../../src/datasync

HEADERS += \
	$$DS_SRC_DIR/enginedatamodel_p.h

SOURCES += \
	$$DS_SRC_DIR/enginedatamodel.cpp \
	tst_enginedatamodel.cpp

QSCXMLC_NAMESPACE = QtDataSync
STATECHARTS += \
	$$DS_SRC_DIR/enginestatemachine_p.scxml

debug_and_release {
	CONFIG(debug, debug|release):SUFFIX = /debug
	CONFIG(release, debug|release):SUFFIX = /release
}
INCLUDEPATH += \
	$$OUT_PWD \
	$$OUT_PWD/$$QSCXMLC_DIR$${SUFFIX} \
	$$DS_SRC_DIR \
	$$PWD/../../../../src/3rdparty/qntp/include \
	$$PWD/../../../../src/datasync/firebase \
	$$PWD/../../../../src/datasync/firebase/realtimedb \
	$$OUT_PWD/../../../../src/datasync \
	$$OUT_PWD/../../../../src/datasync/$$QRESTBUILDER_DIR$${SUFFIX} \
	$$OUT_PWD/../../../../src/datasync/$$QSCXMLC_DIR$${SUFFIX} \

DEFINES += SRCDIR=\\\"$$PWD/\\\"
DEFINES += $$envDefine(FIREBASE_PROJECT_ID)
DEFINES += $$envDefine(FIREBASE_API_KEY)

include($$PWD/../testlib.pri)
include($$PWD/../../testrun.pri)