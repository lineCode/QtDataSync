#include "eventcursor.h"
#include "eventcursor_p.h"
#include "defaults_p.h"
#include "signal_private_connect_p.h"
#include <QtCore/QDataStream>
#include <QtSql/QSqlError>
using namespace QtDataSync;

#define QTDATASYNC_LOG d->logger

EventCursor::EventCursor(const QString &setupName, QObject *parent) :
	QObject{parent},
	d{new EventCursorPrivate{setupName, this}}
{
	EventCursorPrivate::initDatabase(d->defaults, d->database, d->logger, false);

	connect(d->emitter, &EmitterAdapter::dataChanged,
			this, PSIG_NULL(&EventCursor::eventLogChanged));
	connect(d->emitter, &EmitterAdapter::dataResetted,
			this, PSIG_NULL(&EventCursor::eventLogChanged));
}

EventCursor::~EventCursor() = default;

bool EventCursor::isEventLogActive(const QString &setupName)
{
	Defaults defaults{DefaultsPrivate::obtainDefaults(setupName)};
	auto db = defaults.aquireDatabase(nullptr);
	return EventCursorPrivate::isLogActive(defaults, db);
}

EventCursor *EventCursor::first(QObject *parent)
{
	return first(DefaultSetup, parent);
}

EventCursor *EventCursor::first(const QString &setupName, QObject *parent)
{
	auto cursor = new EventCursor{setupName, parent};
	QSqlQuery eventQuery{cursor->d->database};
	eventQuery.prepare(QStringLiteral("SELECT SeqId, Type, Id, Removed, Timestamp "
									  "FROM EventLog "
									  "ORDER BY SeqId ASC "
									  "LIMIT 1"));
	cursor->d->exec(eventQuery);
	if(eventQuery.first())
		cursor->d->readQuery(eventQuery);
	return cursor;
}

EventCursor *EventCursor::last(QObject *parent)
{
	return last(DefaultSetup, parent);
}

EventCursor *EventCursor::last(const QString &setupName, QObject *parent)
{
	auto cursor = new EventCursor{setupName, parent};
	QSqlQuery eventQuery{cursor->d->database};
	eventQuery.prepare(QStringLiteral("SELECT SeqId, Type, Id, Removed, Timestamp "
									  "FROM EventLog "
									  "ORDER BY SeqId DESC "
									  "LIMIT 1"));
	cursor->d->exec(eventQuery);
	if(eventQuery.first())
		cursor->d->readQuery(eventQuery);
	return cursor;
}

EventCursor *EventCursor::create(quint64 index, QObject *parent)
{
	return create(index, DefaultSetup, parent);
}

EventCursor *EventCursor::create(quint64 index, const QString &setupName, QObject *parent)
{
	auto cursor = new EventCursor{setupName, parent};
	QSqlQuery eventQuery{cursor->d->database};
	eventQuery.prepare(QStringLiteral("SELECT SeqId, Type, Id, Removed, Timestamp "
									  "FROM EventLog "
									  "WHERE SeqId = ? "
									  "LIMIT 1"));
	eventQuery.addBindValue(index);
	cursor->d->exec(eventQuery, index);
	if(eventQuery.first())
		cursor->d->readQuery(eventQuery);
	return cursor;
}

EventCursor *EventCursor::load(const QByteArray &data, QObject *parent)
{
	return load(data, DefaultSetup, parent);
}

EventCursor *EventCursor::load(const QByteArray &data, const QString &setupName, QObject *parent)
{
	// cant use serializer here because cursor is not default constructible
	quint64 index = 0;
	bool skipObsolete = false;
	QDataStream stream{data};
	stream.setVersion(QDataStream::Qt_5_10); //MAJOR update
	stream >> index >> skipObsolete;

	auto cursor = EventCursor::create(index, setupName, parent);
	cursor->d->skipObsolete = skipObsolete;
	return cursor;
}

QByteArray EventCursor::save() const
{
	// cant use serializer here because cursor is not default constructible
	QByteArray data;
	QDataStream stream{&data, QIODevice::WriteOnly};
	stream.setVersion(QDataStream::Qt_5_10);
	stream << d->index << d->skipObsolete;
	return data;
}

bool EventCursor::isValid() const
{
	return d->defaults.isValid() && d->index != 0;
}

QString EventCursor::setupName() const
{
	return d->defaults.setupName();
}

quint64 EventCursor::index() const
{
	return d->index;
}

ObjectKey EventCursor::key() const
{
	return d->key;
}

bool EventCursor::wasRemoved() const
{
	return d->wasRemoved;
}

QDateTime EventCursor::timestamp() const
{
	return d->timestamp;
}

bool EventCursor::skipObsolete() const
{
	return d->skipObsolete;
}

bool EventCursor::hasNext() const
{
	QSqlQuery eventQuery{d->database};
	d->prepareNextQuery(eventQuery, false);
	d->exec(eventQuery, d->index);
	return eventQuery.first();
}

bool EventCursor::next()
{
	QSqlQuery eventQuery{d->database};
	d->prepareNextQuery(eventQuery, true);
	d->exec(eventQuery, d->index);
	if(eventQuery.first()) {
		d->readQuery(eventQuery);
		return true;
	} else
		return false;
}

void EventCursor::autoScanLog()
{
	autoScanLog(this, [](const EventCursor *) { return true; }, false);
}

void EventCursor::autoScanLog(std::function<bool (const EventCursor *)> function, bool scanCurrent)
{
	autoScanLog(this, std::move(function), scanCurrent);
}

void EventCursor::autoScanLog(QObject *scope, std::function<bool (const EventCursor *)> function, bool scanCurrent)
{
	if(scanCurrent && !function(this))
		return;

	auto handleNext = [this, LAMBDA_MV(function)]() {
		while (next()) {
			if(!function(this))
				return false;
		}
		return true;
	};
	if(!handleNext())
		return;

	auto cnPtr = QSharedPointer<QMetaObject::Connection>::create();
	*cnPtr = connect(this, &EventCursor::eventLogChanged,
					 scope, [cnPtr, handleNext](){
		if(!handleNext() && *cnPtr)
			QObject::disconnect(*cnPtr);
	});
}

void EventCursor::setSkipObsolete(bool skipObsolete)
{
	if(d->skipObsolete == skipObsolete)
		return;

	d->skipObsolete = skipObsolete;
	emit skipObsoleteChanged(d->skipObsolete, {});
}

void EventCursor::clearEventLog(quint64 offset)
{
	if(d->index < offset) {
		throw EventCursorException {
			d->defaults,
			d->index,
			QStringLiteral("Offset: %1").arg(offset),
			QStringLiteral("offset is bigger than the index - can't clear negative data indexes")
		};
	}

	QSqlQuery eventQuery{d->database};
	eventQuery.prepare(QStringLiteral("DELETE FROM EventLog "
									  "WHERE SeqId < ?"));
	eventQuery.addBindValue(d->index - offset);
	d->exec(eventQuery, d->index - offset);
}



EventCursorException::EventCursorException(const Defaults &defaults, quint64 index, QString context, const QString &message) :
	Exception{defaults, message},
	_index{index},
	_context{std::move(context)}
{}

quint64 EventCursorException::index() const
{
	return _index;
}

QString EventCursorException::context() const
{
	return _context;
}

EventCursorException::EventCursorException(const EventCursorException * const other) :
	Exception{other},
	_index{other->_index},
	_context{other->_context}
{}

QByteArray EventCursorException::className() const noexcept
{
	return QTDATASYNC_EXCEPTION_NAME(EventCursorException);
}

QString EventCursorException::qWhat() const
{
	auto msg = Exception::qWhat();
	if(_index != 0)
		msg += QStringLiteral("\n\tIndex: %L1").arg(_index);
	msg += QStringLiteral("\n\tContext: %1").arg(_context);
	return msg;
}

void EventCursorException::raise() const
{
	throw *this;
}

QException *EventCursorException::clone() const
{
	return new EventCursorException{this};
}

// ------------- PRIVATE IMPLEMENTATION -------------

#undef QTDATASYNC_LOG
#define QTDATASYNC_LOG logger

EventCursorPrivate::EventCursorPrivate(const QString &setupName, EventCursor *q_ptr) :
	defaults{DefaultsPrivate::obtainDefaults(setupName)},
	database{defaults.aquireDatabase(q_ptr)},
	emitter{defaults.createEmitter(q_ptr)},
	logger{defaults.createLogger("eventlogger", q_ptr)}
{}

bool EventCursorPrivate::isLogActive(const Defaults &defaults, DatabaseRef &database)
{
	switch (defaults.property(Defaults::EventLoggingMode).value<Setup::EventMode>()) {
	case Setup::EventMode::Enabled:
		return true;
	case Setup::EventMode::Disabled:
		return false;
	case Setup::EventMode::Unchanged:
		return database->tables().contains(QStringLiteral("EventLog"));
	default:
		Q_UNREACHABLE();
		return false;
	}
}

void EventCursorPrivate::initDatabase(const Defaults &defaults, DatabaseRef &database, Logger *logger, bool createTriggers)
{
	auto mode = defaults.property(Defaults::EventLoggingMode).value<Setup::EventMode>();
	if(mode == Setup::EventMode::Enabled) {
		if(!database->tables().contains(QStringLiteral("EventLog"))) {
			QSqlQuery createQuery{database};
			createQuery.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS EventLog ( "
											   "	SeqId		INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
											   "	Type		TEXT NOT NULL, "
											   "	Id			TEXT NOT NULL, "
											   "	Version		INTEGER NOT NULL, "
											   "	Removed		INTEGER NOT NULL, "
											   "	Timestamp	INTEGER NOT NULL "
											   ");"));
			if(!createQuery.exec()) {
				throw EventCursorException {
					defaults,
					0,
					createQuery.executedQuery().simplified(),
					createQuery.lastError().text()
				};
			}
			logDebug() << "Created EventLog table";
		}

		if(createTriggers) {
			for(const auto &type : {std::make_pair(QStringLiteral("INSERT"), false),
									std::make_pair(QStringLiteral("UPDATE"), true)}) {
				QSqlQuery hasTriggersQuery{database};
				hasTriggersQuery.prepare(QStringLiteral("SELECT 1 FROM sqlite_master "
														 "WHERE type = ? AND name = ?"));
				hasTriggersQuery.addBindValue(QStringLiteral("trigger"));
				hasTriggersQuery.addBindValue(QStringLiteral("eventlog_%1").arg(type.first));
				if(!hasTriggersQuery.exec()) {
					throw EventCursorException {
						defaults,
						0,
						hasTriggersQuery.executedQuery().simplified(),
						hasTriggersQuery.lastError().text()
					};
				}

				if(!hasTriggersQuery.first()) {
					QSqlQuery createTriggerQuery{database};
					auto query = QStringLiteral("CREATE TRIGGER IF NOT EXISTS eventlog_%1 "
												"AFTER %1 "
												"ON DataIndex "
												"%2"
												"BEGIN "
												"	INSERT INTO EventLog "
												"	(Type, Id, Version, Removed, Timestamp) "
												"	VALUES(NEW.Type, NEW.Id, NEW.Version, NEW.File IS NULL, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));"
												"END;").arg(type.first);
					if(type.second)
						query = query.arg(QStringLiteral("WHEN NEW.Version != OLD.Version "));
					else
						query = query.arg(QString{});
					createTriggerQuery.prepare(query);
					if(!createTriggerQuery.exec()) {
						throw EventCursorException {
							defaults,
							0,
							createTriggerQuery.executedQuery().simplified(),
							createTriggerQuery.lastError().text()
						};
					}
					logDebug() << "Created eventlog trigger for" << type.first << "operation";
				}
			}
		}
	} else if(mode == Setup::EventMode::Disabled) {
		for(const auto &tableInfo : {std::make_pair(QStringLiteral("eventlog_INSERT"), true),
									 std::make_pair(QStringLiteral("eventlog_UPDATE"), true),
									 std::make_pair(QStringLiteral("EventLog"), false)}) {
			QSqlQuery dropQuery{database};
			dropQuery.prepare(QStringLiteral("DROP %1 IF EXISTS %2")
							  .arg(tableInfo.second ? QStringLiteral("TRIGGER") : QStringLiteral("TABLE"),
								   tableInfo.first));
			if(!dropQuery.exec()) {
				throw EventCursorException {
					defaults,
					0,
					dropQuery.executedQuery().simplified(),
					dropQuery.lastError().text()
				};
			}
			logDebug() << "Dropped EventLog table";
		}
	}
}

void EventCursorPrivate::clearEventLog(const Defaults &defaults, DatabaseRef &database)
{
	if(!isLogActive(defaults, database))
		return;

	QSqlQuery eventQuery(database);
	eventQuery.prepare(QStringLiteral("DELETE FROM EventLog"));
	if(!eventQuery.exec()) {
		throw EventCursorException {
			defaults,
			0,
			eventQuery.executedQuery().simplified(),
			eventQuery.lastError().text()
		};
	}
}

void EventCursorPrivate::exec(QSqlQuery &query, quint64 qIndex) const
{
	if(!query.exec()) {
		throw EventCursorException {
			defaults,
			qIndex,
			query.executedQuery().simplified(),
			query.lastError().text()
		};
	}
}

void EventCursorPrivate::readQuery(const QSqlQuery &query)
{
	index = query.value(0).toULongLong();
	key.typeName = query.value(1).toByteArray();
	key.id = query.value(2).toString();
	wasRemoved = query.value(3).toBool();
	timestamp = query.value(4).toDateTime().toLocalTime();
}

void EventCursorPrivate::prepareNextQuery(QSqlQuery &query, bool withData) const
{
	query.prepare((withData ?
					   QStringLiteral("SELECT EventLog.SeqId, EventLog.Type, EventLog.Id, EventLog.Removed, EventLog.Timestamp ") :
					   QStringLiteral("SELECT EventLog.SeqId ")) +

				  QStringLiteral("FROM EventLog ") +

				  (skipObsolete ?
					   QStringLiteral("LEFT JOIN DataIndex "
									  "ON DataIndex.Type = EventLog.Type AND DataIndex.Id = EventLog.Id "
									  "WHERE SeqId > ? AND (EventLog.Version IS NULL OR EventLog.Version = DataIndex.Version) ") :
					   QStringLiteral("WHERE SeqId > ? ")) +

				  QStringLiteral("ORDER BY SeqId ASC "
								 "LIMIT 1"));
	query.addBindValue(index);
}
