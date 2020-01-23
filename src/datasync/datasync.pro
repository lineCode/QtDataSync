TARGET = QtDataSync

QT = core restclient networkauth sql scxml remoteobjects core-private
MODULE_CONFIG += $$CPP_VERSION

HEADERS += \
	authenticator.h \
	authenticator_p.h \
	cloudtransformer.h \
	cloudtransformer_p.h \
	databasewatcher_p.h \
	engine.h \
	engine_p.h \
	enginedatamodel_p.h \
	exception.h \
	firebase/realtimedb/firebaseapibase_p.h \
	firebase/realtimedb/querymap_p.h \
	firebase/realtimedb/servertimestamp_p.h \
	firebaseauthenticator_p.h \
	googleauthenticator.h \
	googleauthenticator_p.h \
	googleoauthflow_p.h \
	mailauthenticator.h \
	mailauthenticator_p.h \
	objectkey.h \
	qtdatasync_global.h \
	remoteconnector_p.h \
	setup.h \
	setup_impl.h \
	tabledatamodel_p.h

SOURCES += \
	authenticator.cpp \
	cloudtransformer.cpp \
	databasewatcher.cpp \
	engine.cpp \
	enginedatamodel.cpp \
	exception.cpp \
	firebase/realtimedb/firebaseapibase.cpp \
	firebase/realtimedb/querymap.cpp \
	firebase/realtimedb/servertimestamp.cpp \
	firebaseauthenticator.cpp \
	googleauthenticator.cpp \
	googleoauthflow.cpp \
	mailauthenticator.cpp \
	objectkey.cpp \
	remoteconnector.cpp \
	setup.cpp \
	tabledatamodel.cpp

REST_API_FILES += \
	firebase/auth/auth_api.xml \
	firebase/auth/auth_error.xml \
	firebase/auth/deleterequest.xml \
	firebase/auth/errorelement.xml \
	firebase/auth/errorcontent.xml \
	firebase/auth/refreshresponse.xml \
	firebase/auth/mailapplyresetrequest.xml \
	firebase/auth/mailauthrequest.xml \
	firebase/auth/mailresetdata.xml \
	firebase/auth/mailverifyrequest.xml \
	firebase/auth/oobcoderequest.xml \
	firebase/auth/signinrequest.xml \
	firebase/auth/signinresponse.xml \
	firebase/realtimedb/data.xml \
	firebase/realtimedb/realtimedb_api.xml \
	firebase/realtimedb/realtimedb_error.xml

REPC_MERGED = \
	asyncwatcher.rep

QSCXMLC_NAMESPACE = QtDataSync
STATECHARTS += \
	enginestatemachine_p.scxml \
	tablestatemachine_p.scxml

INCLUDEPATH += \
	firebase \
	firebase/auth \
	firebase/realtimedb

!no_ntp {
	include(../3rdparty/qntp/qntp.pri)
	HEADERS += ntpsync_p.h
	SOURCES += ntpsync.cpp
} else {
	MODULE_DEFINES += QTDATASYNC_NO_NTP
	DEFINES += $$MODULE_DEFINES
}

load(qt_module)

win32 {
	QMAKE_TARGET_PRODUCT = "QtDataSync"
	QMAKE_TARGET_COMPANY = "Skycoder42"
	QMAKE_TARGET_COPYRIGHT = "Felix Barz"
} else:mac {
	QMAKE_TARGET_BUNDLE_PREFIX = "com.skycoder42."
}
