#ifndef QTDATASYNC_GLOBAL_H
#define QTDATASYNC_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QT_BUILD_DATASYNC_LIB)
#	define Q_DATASYNC_EXPORT Q_DECL_EXPORT
#else
#	define Q_DATASYNC_EXPORT Q_DECL_IMPORT
#endif

namespace QtDataSync {

//! The default setup name
extern Q_DATASYNC_EXPORT const QString DefaultSetup;

struct Q_DATASYNC_EXPORT ObjectKey
{
	QByteArray typeName;
	QString id;

	inline ObjectKey(const QByteArray &typeName = {}, const QString &id = {}) :
		typeName(typeName),
		id(id)
	{}
};

}

#endif // QTDATASYNC_GLOBAL_H
