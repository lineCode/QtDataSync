#ifndef QTDATASYNC_CRYPTO_SYMMETRICCRYPTOCLOUDTRANSFORMER_P_H
#define QTDATASYNC_CRYPTO_SYMMETRICCRYPTOCLOUDTRANSFORMER_P_H

#include "symmetriccryptocloudtransformer.h"
#include "keymanager.h"

#include <QtCore/private/qobject_p.h>

#include <cryptopp/osrng.h>

namespace QtDataSync::Crypto {

class Q_DATASYNC_CRYPTO_EXPORT SymmetricCryptoCloudTransformerBasePrivate : public QObjectPrivate
{
	Q_DECLARE_PUBLIC(SymmetricCryptoCloudTransformerBase)
public:
	mutable CryptoPP::AutoSeededRandomPool rng;
	mutable KeyProvider keyProvider;

	QString base64Encode(const QByteArray &data) const;
	QByteArray base64Decode(const QString &data) const;
	QByteArray base64Decode(const QJsonValue &data) const;
};

}

#endif // QTDATASYNC_CRYPTO_SYMMETRICCRYPTOCLOUDTRANSFORMER_P_H