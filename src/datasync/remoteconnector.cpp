#include "remoteconnector_p.h"
#include "logger.h"

#include <chrono>

#include <QtCore/QSysInfo>

#include "registermessage_p.h"
#include "loginmessage_p.h"

using namespace QtDataSync;

#define QTDATASYNC_LOG QTDATASYNC_LOG_CONTROLLER

const QString RemoteConnector::keyRemoteEnabled(QStringLiteral("enabled"));
const QString RemoteConnector::keyRemoteUrl(QStringLiteral("remote/url"));
const QString RemoteConnector::keyAccessKey(QStringLiteral("remote/accessKey"));
const QString RemoteConnector::keyHeaders(QStringLiteral("remote/headers"));
const QString RemoteConnector::keyKeepaliveTimeout(QStringLiteral("remote/keepaliveTimeout"));
const QString RemoteConnector::keyDeviceId(QStringLiteral("deviceId"));
const QString RemoteConnector::keyDeviceName(QStringLiteral("deviceName"));
const QByteArray RemoteConnector::PingMessage(1, 0xFF);

RemoteConnector::RemoteConnector(const Defaults &defaults, QObject *parent) :
	Controller("connector", defaults, parent),
	_cryptoController(new CryptoController(defaults, this)),
	_socket(nullptr),
	_idleTimer(new QTimer(this)),
	_changingConnection(false),
	_state(RemoteDisconnected),
	_deviceId()
{}

void RemoteConnector::initialize()
{
	_cryptoController->initialize();

	_idleTimer->setInterval(sValue(keyKeepaliveTimeout).toInt());
	_idleTimer->setSingleShot(true);

	//always "reconnect", because this loads keys etc. and if disabled, also does nothing
	QMetaObject::invokeMethod(this, "reconnect", Qt::QueuedConnection);
}

void RemoteConnector::finalize()
{
	_idleTimer->stop();

	if(_socket && _socket->state() == QAbstractSocket::ConnectedState) {
		_changingConnection = true;
		_socket->close();
		//TODO wait for disconnect?
	}

	_cryptoController->finalize();
}

void RemoteConnector::reconnect()
{
	if(_socket) {
		if(_socket->state() == QAbstractSocket::UnconnectedState) {
			logDebug() << "Removing unconnected but still not deleted socket";
			_socket->deleteLater();
			_socket = nullptr;
			upState(RemoteDisconnected);
			QMetaObject::invokeMethod(this, "reconnect", Qt::QueuedConnection);
		} else if(_socket->state() == QAbstractSocket::ConnectedState) {
			logDebug() << "Closing active connection with server to reconnect";
			connect(_socket, &QWebSocket::destroyed,
					this, &RemoteConnector::reconnect,
					Qt::QueuedConnection);
			_changingConnection = true;
			_socket->close();
			upState(RemoteReconnecting);
		} else
			logDebug() << "Cannot reconnect. Socket in changing state:" << _socket->state(); //TODO not so stable...
	} else {
		upState(RemoteReconnecting);
		QUrl remoteUrl;
		if(!checkCanSync(remoteUrl))
			return;

		_socket = new QWebSocket(sValue(keyAccessKey).toString(),
								 QWebSocketProtocol::VersionLatest,
								 this);

		auto conf = defaults().property(Defaults::SslConfiguration).value<QSslConfiguration>();
		if(!conf.isNull())
			_socket->setSslConfiguration(conf);

		connect(_socket, &QWebSocket::connected,
				this, &RemoteConnector::connected);
		connect(_socket, &QWebSocket::binaryMessageReceived,
				this, &RemoteConnector::binaryMessageReceived);
		connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
				this, &RemoteConnector::error);
		connect(_socket, &QWebSocket::sslErrors,
				this, &RemoteConnector::sslErrors);
		connect(_socket, &QWebSocket::disconnected,
				this, &RemoteConnector::disconnected,
				Qt::QueuedConnection);

		//initialize keep alive timeout
		auto tOut = sValue(keyKeepaliveTimeout).toInt();
		if(tOut > 0) {
			_idleTimer->setInterval(std::chrono::seconds(tOut));
			connect(_socket, &QWebSocket::connected,
					_idleTimer, QOverload<>::of(&QTimer::start));
			connect(_socket, &QWebSocket::disconnected,
					_idleTimer, &QTimer::stop);
			connect(_idleTimer, &QTimer::timeout,
					this, &RemoteConnector::timeout);
		}

		QNetworkRequest request(remoteUrl);
		request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
		request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
		request.setAttribute(QNetworkRequest::SpdyAllowedAttribute, true);
		request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);

		auto keys = sValue(keyHeaders).value<QHash<QByteArray, QByteArray>>();
		for(auto it = keys.begin(); it != keys.end(); it++)
			request.setRawHeader(it.key(), it.value());

		_changingConnection = true;
		_socket->open(request);
		logDebug() << "Connecting to remote server...";
	}
}

void RemoteConnector::reloadState()
{
	Q_UNIMPLEMENTED();
}

void RemoteConnector::connected()
{
	logDebug() << "Successfully connected to remote server";
	upState(RemoteConnected);
}

void RemoteConnector::disconnected()
{
	if(!_changingConnection) {
		logWarning().noquote() << "Unexpected disconnect from server with exit code"
							   << _socket->closeCode()
							   << "and reason:"
							   << _socket->closeReason();
		//TODO retry
//		auto delta = retry();
//		logDebug() << "Retrying to connect to server in"
//				   << delta / 1000
//				   << "seconds";
	} else {
		_changingConnection = false;
		logDebug() << "Remote server has been disconnected";
	}

	if(_socket)//better be safe
		_socket->deleteLater();
	_socket = nullptr;
	_cryptoController->clearKeyMaterial();

	//always "disable" remote for the state changer
	upState(RemoteDisconnected);
}

void RemoteConnector::binaryMessageReceived(const QByteArray &message)
{
	if(message == PingMessage) {
		_idleTimer->start();
		_socket->sendBinaryMessage(PingMessage);
		return;
	}

	try {
		QDataStream stream(message);
		setupStream(stream);
		stream.startTransaction();
		QByteArray name;
		stream >> name;
		if(!stream.commitTransaction())
			throw DataStreamException(stream);

		if(isType<IdentifyMessage>(name))
			onIdentify(deserializeMessage<IdentifyMessage>(stream));
		else if(isType<AccountMessage>(name))
			onAccount(deserializeMessage<AccountMessage>(stream));
		else if(isType<WelcomeMessage>(name))
			onWelcome(deserializeMessage<WelcomeMessage>(stream));
		else
			logWarning() << "Unknown message received: " << message;
	} catch(DataStreamException &e) {
		logCritical() << "Remote message error:" << e.what();
	}
}

void RemoteConnector::error(QAbstractSocket::SocketError error)
{
	//TODO retryIndex
//	if(retryIndex == 0) {
		logWarning().noquote() << "Server connection socket error:"
							   << _socket->errorString();
//	} else {
//		logDebug().noquote() << "Repeated server connection socket error:"
//							 << _socket->errorString();
//	}

	tryClose();
}

void RemoteConnector::sslErrors(const QList<QSslError> &errors)
{
	auto shouldClose = true;
	foreach(auto error, errors) {
		if(error.error() == QSslError::SelfSignedCertificate ||
		   error.error() == QSslError::SelfSignedCertificateInChain)
			shouldClose = shouldClose &&
						  (defaults().property(Defaults::SslConfiguration)
						   .value<QSslConfiguration>()
						   .peerVerifyMode() >= QSslSocket::VerifyPeer);
		//TODO retryIndex
//		if(retryIndex == 0) {
			logWarning().noquote() << "Server connection SSL error:"
								   << error.errorString();
//		} else {
//			logDebug().noquote() << "Repeated server connection SSL error:"
//								 << error.errorString();
//		}
	}

	if(shouldClose)
		tryClose();
}

void RemoteConnector::timeout()
{
	logDebug() << "Server connection idle. Reconnecting to server";
	reconnect();
}

bool RemoteConnector::checkCanSync(QUrl &remoteUrl)
{
	//check if sync is enabled
	if(!sValue(keyRemoteEnabled).toBool()) {
		logDebug() << "Remote has been disabled. Not connecting";
		upState(RemoteDisconnected);
		return false;
	}

	//check if remote is defined
	remoteUrl = sValue(keyRemoteUrl).toUrl();
	if(!remoteUrl.isValid()) {
		logDebug() << "Cannot connect to remote - no URL defined";
		upState(RemoteDisconnected);
		return false;
	}

	//load crypto stuff
	if(!loadIdentity()) {
		logCritical() << "Unable to access private keys of user in keystore. Cannot synchronize";
		upState(RemoteDisconnected);
		return false;
	}

	return true;
}

bool RemoteConnector::loadIdentity()
{
	try {
		if(!_cryptoController->canAccessStore()) //no keystore -> can neither save nor load...
			return false;

		_deviceId = sValue(keyDeviceId).toUuid();
		if(_deviceId.isNull()) //no user -> nothing to be loaded
			return true;

		_cryptoController->loadKeyMaterial(_deviceId);
		return true;
	} catch(Exception &e) {
		logCritical() << e.what();
		return false;
	}
}

void RemoteConnector::tryClose()
{
	logDebug() << Q_FUNC_INFO << "socket state:" << _socket->state();
	if(_socket && _socket->state() == QAbstractSocket::ConnectedState) {
		_changingConnection = true;
		_socket->close();
	} else
		upState(RemoteDisconnected);
}

QVariant RemoteConnector::sValue(const QString &key) const
{
	if(key == keyHeaders) {
		if(settings()->childGroups().contains(keyHeaders)) {
			settings()->beginGroup(keyHeaders);
			auto keys = settings()->childKeys();
			QHash<QByteArray, QByteArray> headers;
			foreach(auto key, keys)
				headers.insert(key.toUtf8(), settings()->value(key).toByteArray());
			settings()->endGroup();
			return QVariant::fromValue(headers);
		}
	} else {
		auto res = settings()->value(key);
		if(res.isValid())
			return res;
	}

	auto config = defaults().property(Defaults::RemoteConfiguration).value<RemoteConfig>();
	if(key == keyRemoteUrl)
		return config.url;
	else if(key == keyAccessKey)
		return config.accessKey;
	else if(key == keyHeaders)
		return QVariant::fromValue(config.headers);
	else if(key == keyKeepaliveTimeout)
		return QVariant::fromValue(config.keepaliveTimeout);
	else if(key == keyRemoteEnabled)
		return true;
	else if(key == keyDeviceName)
		return QSysInfo::machineHostName();
	else
		return {};
}

void RemoteConnector::upState(RemoteConnector::RemoteState state)
{
	if(state != _state) {
		_state = state;
		upState(state);
	}
}

void RemoteConnector::onIdentify(const IdentifyMessage &message)
{
	try {
		if(_state != RemoteConnected)
			logWarning() << "Unexpected IdentifyMessage";
		else {
			if(!_deviceId.isNull()) {
				LoginMessage msg(_deviceId,
								 sValue(keyDeviceName).toString(),
								 message.nonce);
				auto signedMsg = _cryptoController->serializeSignedMessage(msg);
				_socket->sendBinaryMessage(signedMsg);
				logDebug() << "Sent login message for device id" << _deviceId;
				upState(RemoteLoggingIn);
			} else {
				_cryptoController->createPrivateKeys(message.nonce);
				RegisterMessage msg(sValue(keyDeviceName).toString(),
									message.nonce,
									_cryptoController->crypto()->signKey(),
									_cryptoController->crypto()->cryptKey(),
									_cryptoController->crypto());
				auto signedMsg = _cryptoController->serializeSignedMessage(msg);
				_socket->sendBinaryMessage(signedMsg);
				logDebug() << "Sent registration message for new id";
				upState(RemoteRegistering);
			}
		}
	} catch(Exception &e) {
		logCritical() << e.what();
	}
}

void RemoteConnector::onAccount(const AccountMessage &message)
{
	try {
		if(_state != RemoteRegistering)
			logWarning() << "Unexpected AccountMessage";
		else {
			_deviceId = message.deviceId;
			settings()->setValue(keyDeviceId, _deviceId);
			_cryptoController->storePrivateKeys(_deviceId);
			logDebug() << "Saved user data stuff";
		}
	} catch(Exception &e) {
		logCritical() << e.what();
	}
}

void RemoteConnector::onWelcome(const WelcomeMessage &message)
{
	Q_UNUSED(message);
	if(_state != RemoteLoggingIn)
		logWarning() << "Unexpected WelcomeMessage";
	else {
		logDebug() << "Login successful. Reloading states";
		upState(RemoteLoading);
		reloadState();
	}
}
