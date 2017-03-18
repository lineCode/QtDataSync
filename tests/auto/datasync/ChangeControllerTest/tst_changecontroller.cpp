#include <QString>
#include <QtTest>
#include <QCoreApplication>
#include "tst.h"
using namespace QtDataSync;

class ChangeControllerTest : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void initTestCase();
	void cleanupTestCase();

	void testSyncOperation_data();
	void testSyncOperation();

	void testLiveChanges_data();
	void testLiveChanges();

private:
	MockLocalStore *store;
	MockStateHolder *holder;
	MockRemoteConnector *remote;
	MockDataMerger *merger;

	AsyncDataStore *async;
	SyncController *controller;

	DataSet generateSyncData(int param);
	StateHolder::ChangeHash generateSyncHash(StateHolder::ChangeState state);
};

void ChangeControllerTest::initTestCase()
{
#ifdef Q_OS_UNIX
	Q_ASSERT(qgetenv("LD_PRELOAD").contains("Qt5DataSync"));
#endif

	tst_init();

	Setup setup;
	mockSetup(setup);
	store = static_cast<MockLocalStore*>(setup.localStore());
	store->enabled = true;
	holder = static_cast<MockStateHolder*>(setup.stateHolder());
	holder->enabled = true;
	remote = static_cast<MockRemoteConnector*>(setup.remoteConnector());
	remote->enabled = true;
	merger = static_cast<MockDataMerger*>(setup.dataMerger());
	setup.create();

	async = new AsyncDataStore(this);
	controller = new SyncController(this);
}

void ChangeControllerTest::cleanupTestCase()
{
	delete async;
	delete controller;
	Setup::removeSetup(Setup::DefaultSetup);
}

void ChangeControllerTest::testSyncOperation_data()
{
	QTest::addColumn<DataSet>("localData");
	QTest::addColumn<StateHolder::ChangeHash>("localState");
	QTest::addColumn<DataSet>("remoteData");
	QTest::addColumn<StateHolder::ChangeHash>("remoteState");
	QTest::addColumn<DataMerger::SyncPolicy>("syncPolicy");
	QTest::addColumn<DataMerger::MergePolicy>("mergePolicy");
	QTest::addColumn<DataSet>("localResult");
	QTest::addColumn<DataSet>("remoteResult");
	QTest::addColumn<int>("mergeCount");

	QTest::newRow("empty") << DataSet()
						   << StateHolder::ChangeHash()
						   << DataSet()
						   << StateHolder::ChangeHash()
						   << DataMerger::PreferUpdated
						   << DataMerger::KeepLocal
						   << DataSet()
						   << DataSet()
						   << 0;

	QTest::newRow("unchanged:unchanged") << generateSyncData(0)
										 << StateHolder::ChangeHash()
										 << generateSyncData(1)
										 << StateHolder::ChangeHash()
										 << DataMerger::PreferUpdated
										 << DataMerger::KeepLocal
										 << generateSyncData(0)
										 << generateSyncData(1)
										 << 0;
	QTest::newRow("unchanged:changed") << generateSyncData(0)
									   << StateHolder::ChangeHash()
									   << generateSyncData(1)
									   << generateSyncHash(StateHolder::Changed)
									   << DataMerger::PreferUpdated
									   << DataMerger::KeepLocal
									   << generateSyncData(1)
									   << generateSyncData(1)
									   << 0;
	QTest::newRow("unchanged:deleted") << generateSyncData(0)
									   << StateHolder::ChangeHash()
									   << DataSet()
									   << generateSyncHash(StateHolder::Deleted)
									   << DataMerger::PreferUpdated
									   << DataMerger::KeepLocal
									   << DataSet()
									   << DataSet()
									   << 0;

	QTest::newRow("changed:unchanged") << generateSyncData(0)
									   << generateSyncHash(StateHolder::Changed)
									   << generateSyncData(1)
									   << StateHolder::ChangeHash()
									   << DataMerger::PreferUpdated
									   << DataMerger::KeepLocal
									   << generateSyncData(0)
									   << generateSyncData(0)
									   << 0;
	QTest::newRow("changed:changed:local") << generateSyncData(0)
										   << generateSyncHash(StateHolder::Changed)
										   << generateSyncData(1)
										   << generateSyncHash(StateHolder::Changed)
										   << DataMerger::PreferUpdated
										   << DataMerger::KeepLocal
										   << generateSyncData(0)
										   << generateSyncData(0)
										   << 0;
	QTest::newRow("changed:changed:remote") << generateSyncData(0)
											<< generateSyncHash(StateHolder::Changed)
											<< generateSyncData(1)
											<< generateSyncHash(StateHolder::Changed)
											<< DataMerger::PreferUpdated
											<< DataMerger::KeepRemote
											<< generateSyncData(1)
											<< generateSyncData(1)
											<< 0;
	QTest::newRow("changed:changed:merge") << generateSyncData(0)
										   << generateSyncHash(StateHolder::Changed)
										   << generateSyncData(1)
										   << generateSyncHash(StateHolder::Changed)
										   << DataMerger::PreferUpdated
										   << DataMerger::Merge
										   << generateSyncData(0)
										   << generateSyncData(0)
										   << 1;
	QTest::newRow("changed:deleted:updated") << generateSyncData(0)
											 << generateSyncHash(StateHolder::Changed)
											 << DataSet()
											 << generateSyncHash(StateHolder::Deleted)
											 << DataMerger::PreferUpdated
											 << DataMerger::KeepLocal
											 << generateSyncData(0)
											 << generateSyncData(0)
											 << 0;
	QTest::newRow("changed:deleted:deleted") << generateSyncData(0)
											 << generateSyncHash(StateHolder::Changed)
											 << DataSet()
											 << generateSyncHash(StateHolder::Deleted)
											 << DataMerger::PreferDeleted
											 << DataMerger::KeepLocal
											 << DataSet()
											 << DataSet()
											 << 0;
	QTest::newRow("changed:deleted:local") << generateSyncData(0)
										   << generateSyncHash(StateHolder::Changed)
										   << DataSet()
										   << generateSyncHash(StateHolder::Deleted)
										   << DataMerger::PreferLocal
										   << DataMerger::KeepLocal
										   << generateSyncData(0)
										   << generateSyncData(0)
										   << 0;
	QTest::newRow("changed:deleted:remote") << generateSyncData(0)
											<< generateSyncHash(StateHolder::Changed)
											<< DataSet()
											<< generateSyncHash(StateHolder::Deleted)
											<< DataMerger::PreferRemote
											<< DataMerger::KeepLocal
											<< DataSet()
											<< DataSet()
											<< 0;

	QTest::newRow("deleted:unchanged") << DataSet()
									   << generateSyncHash(StateHolder::Deleted)
									   << generateSyncData(1)
									   << StateHolder::ChangeHash()
									   << DataMerger::PreferUpdated
									   << DataMerger::KeepLocal
									   << DataSet()
									   << DataSet()
									   << 0;
	QTest::newRow("deleted:changed:updated") << DataSet()
											 << generateSyncHash(StateHolder::Deleted)
											 << generateSyncData(1)
											 << generateSyncHash(StateHolder::Changed)
											 << DataMerger::PreferUpdated
											 << DataMerger::KeepLocal
											 << generateSyncData(1)
											 << generateSyncData(1)
											 << 0;
	QTest::newRow("deleted:changed:deleted") << DataSet()
											 << generateSyncHash(StateHolder::Deleted)
											 << generateSyncData(1)
											 << generateSyncHash(StateHolder::Changed)
											 << DataMerger::PreferDeleted
											 << DataMerger::KeepLocal
											 << DataSet()
											 << DataSet()
											 << 0;
	QTest::newRow("deleted:changed:local") << DataSet()
										   << generateSyncHash(StateHolder::Deleted)
										   << generateSyncData(1)
										   << generateSyncHash(StateHolder::Changed)
										   << DataMerger::PreferLocal
										   << DataMerger::KeepLocal
										   << DataSet()
										   << DataSet()
										   << 0;
	QTest::newRow("deleted:changed:remote") << DataSet()
											<< generateSyncHash(StateHolder::Deleted)
											<< generateSyncData(1)
											<< generateSyncHash(StateHolder::Changed)
											<< DataMerger::PreferRemote
											<< DataMerger::KeepLocal
											<< generateSyncData(1)
											<< generateSyncData(1)
											<< 0;
	QTest::newRow("deleted:deleted") << DataSet()
									 << generateSyncHash(StateHolder::Deleted)
									 << DataSet()
									 << generateSyncHash(StateHolder::Deleted)
									 << DataMerger::PreferUpdated
									 << DataMerger::KeepLocal
									 << DataSet()
									 << DataSet()
									 << 0;
}

void ChangeControllerTest::testSyncOperation()
{
	QFETCH(DataSet, localData);
	QFETCH(StateHolder::ChangeHash, localState);
	QFETCH(DataSet, remoteData);
	QFETCH(StateHolder::ChangeHash, remoteState);
	QFETCH(DataMerger::SyncPolicy, syncPolicy);
	QFETCH(DataMerger::MergePolicy, mergePolicy);
	QFETCH(DataSet, localResult);
	QFETCH(DataSet, remoteResult);
	QFETCH(int, mergeCount);

	QSignalSpy syncStateSpy(controller, &SyncController::syncStateChanged);

	remote->mutex.lock();
	remote->connected = false;
	remote->mutex.unlock();
	remote->updateConnected(false);

	remote->mutex.lock();
	store->mutex.lock();
	holder->mutex.lock();

	store->pseudoStore = localData;
	holder->pseudoState = localState;
	holder->dummyReset = true;

	remote->pseudoStore = remoteData;
	remote->pseudoState = remoteState;
	remote->connected = true;

	merger->setSyncPolicy(syncPolicy);
	merger->setMergePolicy(mergePolicy);
	merger->mergeCount = 0;

	holder->mutex.unlock();
	store->mutex.unlock();
	remote->mutex.unlock();
	remote->updateConnected(true);

	for(auto i = 0; i < 10 && syncStateSpy.count() < 4; i++)
		syncStateSpy.wait(500);
	QCOMPARE(syncStateSpy.count(), 4);
	QCOMPARE(syncStateSpy[0][0], QVariant::fromValue(SyncController::Disconnected));
	QCOMPARE(syncStateSpy[1][0], QVariant::fromValue(SyncController::Loading));
	QCOMPARE(syncStateSpy[2][0], QVariant::fromValue(SyncController::Syncing));
	QCOMPARE(syncStateSpy[3][0], QVariant::fromValue(SyncController::Synced));
	QCOMPARE(controller->syncState(), SyncController::Synced);

	remote->mutex.lock();
	store->mutex.lock();
	holder->mutex.lock();

	remote->connected = false;
	[&](){//catch return to still unlock
		QVERIFY(holder->pseudoState.isEmpty());
		QCOMPARE(store->pseudoStore, localResult);
		QVERIFY(remote->pseudoState.isEmpty());
		QCOMPARE(remote->pseudoStore, remoteResult);
		QCOMPARE(merger->mergeCount, mergeCount);
	}();
	holder->mutex.unlock();
	store->mutex.unlock();
	remote->mutex.unlock();
	remote->updateConnected(false);
}

void ChangeControllerTest::testLiveChanges_data()
{
	QTest::addColumn<TestData>("localChange");
	QTest::addColumn<DataSet>("remoteData");
	QTest::addColumn<StateHolder::ChangeHash>("remoteChange");

	QTest::newRow("localChange") << generateData(13)
								 << DataSet()
								 << StateHolder::ChangeHash();

	QTest::newRow("remoteChange") << TestData()
								  << generateDataJson(77, 78)
								  << generateChangeHash(77, 78, StateHolder::Changed);
}

void ChangeControllerTest::testLiveChanges()
{
	QFETCH(TestData, localChange);
	QFETCH(DataSet, remoteData);
	QFETCH(StateHolder::ChangeHash, remoteChange);

	remote->mutex.lock();
	remote->connected = true;
	remote->mutex.unlock();
	remote->updateConnected(false);
	QThread::msleep(500);

	QSignalSpy syncStateSpy(controller, &SyncController::syncStateChanged);
	syncStateSpy.wait(500);
	syncStateSpy.clear();

	//make shure both sides are stable
	remote->mutex.lock();
	store->mutex.lock();
	holder->mutex.lock();
	[&](){
		QCOMPARE(controller->syncState(), SyncController::Synced);
		QVERIFY(holder->pseudoState.isEmpty());
		QVERIFY(remote->pseudoState.isEmpty());
	}();
	holder->mutex.unlock();
	store->mutex.unlock();
	remote->mutex.unlock();

	if(localChange.id != -1) {//not default constructed
		try {
			auto task = async->save<TestData>(localChange);
			task.waitForFinished();
		} catch(QException &e) {
			QFAIL(e.what());
		}
	}
	if(remoteChange.size() == 1) {
		remote->mutex.lock();
		remote->pseudoStore = remoteData;
		remote->pseudoState = remoteChange;
		remote->emitList = remoteChange.keys();
		remote->mutex.unlock();
		remote->doChangeEmit();
	}

	for(auto i = 0; i < 10 && syncStateSpy.count() < 2; i++)
		syncStateSpy.wait(500);
	QCOMPARE(syncStateSpy.count(), 2);
	QCOMPARE(syncStateSpy[0][0], QVariant::fromValue(SyncController::Syncing));
	QCOMPARE(syncStateSpy[1][0], QVariant::fromValue(SyncController::Synced));
	QCOMPARE(controller->syncState(), SyncController::Synced);
}

DataSet ChangeControllerTest::generateSyncData(int param)
{
	DataSet hash;
	QJsonObject data;
	data["id"] = 42;
	data["text"] = QString::number(param);
	hash.insert(generateKey(42), data);
	return hash;
}

StateHolder::ChangeHash ChangeControllerTest::generateSyncHash(StateHolder::ChangeState state)
{
	return generateChangeHash(42, 43, state);
}

QTEST_MAIN(ChangeControllerTest)

#include "tst_changecontroller.moc"
