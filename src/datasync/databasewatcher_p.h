#ifndef QTDATASYNC_DATABASEWATCHER_H
#define QTDATASYNC_DATABASEWATCHER_H

#include "qtdatasync_global.h"
#include "cloudtransformer.h"

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlField>
#include <QtSql/QSqlDriver>

namespace QtDataSync {

class ExQuery : public QSqlQuery
{
public:
	ExQuery(QSqlDatabase db);

	void prepare(const QString &query);
	void exec();
	void exec(const QString &query);
};

class ExTransaction
{
	Q_DISABLE_COPY(ExTransaction)

public:
	ExTransaction();
	ExTransaction(QSqlDatabase db);
	ExTransaction(ExTransaction &&other) noexcept;
	ExTransaction &operator=(ExTransaction &&other) noexcept;
	~ExTransaction();

	void commit();
	void rollback();

private:
	std::optional<QSqlDatabase> _db;
};

// TODO add public thread sync watcher, that just watches on different threads

class Q_DATASYNC_EXPORT DatabaseWatcher : public QObject
{
	Q_OBJECT

public:
	enum class TableState {
		Inactive = 0,
		Active = 1,
		Corrupted = 2
	};
	Q_ENUM(TableState)

	enum class ChangeState {
		Unchanged = 0,
		Changed = 1,
		Corrupted = 2
	};
	Q_ENUM(ChangeState)

	static const QString MetaTable;
	static const QString TablePrefix;

	explicit DatabaseWatcher(QSqlDatabase &&db, QObject *parent = nullptr);

	QSqlDatabase database() const;

	// setup functions
	bool hasTables() const;
	QStringList tables() const;

	void reactivateTables();
	void addAllTables(QSql::TableType type = QSql::Tables);
	void addTable(const QString &name, const QStringList &fields = {}, QString primaryType = {});

	void removeAllTables();
	void removeTable(const QString &name, bool removeRef = true);

	void unsyncAllTables();
	void unsyncTable(const QString &name, bool removeRef = true);

	// sync functions
	QDateTime lastSync(const QString &tableName);
	void storeData(const LocalData &data);
	std::optional<LocalData> loadData(const QString &name);
	void markUnchanged(const ObjectKey &key, const QDateTime &modified);
	void markCorrupted(const ObjectKey &key, const QDateTime &modified);

Q_SIGNALS:
	void tableAdded(const QString &tableName, QPrivateSignal = {});
	void tableRemoved(const QString &tableName, QPrivateSignal = {});
	void triggerSync(const QString &tableName, QPrivateSignal = {});

	void databaseError(const QString &errorString, QPrivateSignal = {});

private Q_SLOTS:
	void dbNotify(const QString &name);

private:
	QSqlDatabase _db;

	QHash<QString, QStringList> _tables;

	QString sqlTypeName(const QSqlField &field) const;
	QString tableName(const QString &table, bool asSyncTable = false) const;
	QString fieldName(const QString &field) const;

	std::optional<QString> getPKey(const QString &table);
};

Q_DECLARE_LOGGING_CATEGORY(logDbWatcher)

}

#endif // QTDATASYNC_DATABASEWATCHER_H
