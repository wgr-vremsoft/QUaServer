#include "quainmemoryhistorizer.h"

#ifdef UA_ENABLE_HISTORIZING

bool QUaInMemoryHistorizer::writeHistoryData(
	const QUaNodeId &nodeId, 
	const QUaHistoryDataPoint& dataPoint,
	QQueue<QUaLog>& logOut)
{
	Q_UNUSED(logOut);
	m_database[nodeId][dataPoint.timestamp] = {
		dataPoint.value,
		dataPoint.status
	};
	return true;
}

bool QUaInMemoryHistorizer::updateHistoryData(
	const QUaNodeId &nodeId,
	const QUaHistoryDataPoint& dataPoint,
	QQueue<QUaLog>& logOut)
{
	Q_UNUSED(logOut);
	Q_ASSERT(
		m_database.contains(nodeId) &&
		m_database[nodeId].contains(dataPoint.timestamp)
	);
	return this->writeHistoryData(nodeId, dataPoint, logOut);
}

bool QUaInMemoryHistorizer::removeHistoryData(
	const QUaNodeId &nodeId,
	const QDateTime& timeStart,
	const QDateTime& timeEnd,
	QQueue<QUaLog>& logOut)
{
	Q_ASSERT(timeStart <= timeEnd);
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error removing history data. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return false;
	}
	auto& table = m_database[nodeId];
	Q_ASSERT(table.contains(timeStart));
	Q_ASSERT(table.contains(timeEnd) || !timeEnd.isValid());
	// get total range to remove
	auto iterIni = table.find(timeStart);
	auto iterEnd = timeEnd.isValid() ? table.find(timeEnd) + 1 : table.end();
	// remove range
	for (auto it = iterIni; it != iterEnd; it++)
	{
		table.erase(it);
	}
	return true;
}

QDateTime QUaInMemoryHistorizer::firstTimestamp(
	const QUaNodeId &nodeId,
	QQueue<QUaLog>& logOut) const
{
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error finding first history timestamp. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return QDateTime();
	}
	return m_database[nodeId].firstKey();
}

QDateTime QUaInMemoryHistorizer::lastTimestamp(
	const QUaNodeId &nodeId,
	QQueue<QUaLog>& logOut) const
{
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error finding most recent history timstamp. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return QDateTime();
	}
	return m_database[nodeId].lastKey();
}

bool QUaInMemoryHistorizer::hasTimestamp(
	const QUaNodeId &nodeId,
	const QDateTime& timestamp,
	QQueue<QUaLog>& logOut) const
{
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error finding history timestamp. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return false;
	}
	return m_database[nodeId].contains(timestamp);
}

QDateTime QUaInMemoryHistorizer::findTimestamp(
	const QUaNodeId &nodeId,
	const QDateTime& timestamp,
	const QUaHistoryBackend::TimeMatch& match,
	QQueue<QUaLog>& logOut) const
{
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error finding history timestamp. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return QDateTime();
	}
	// NOTE : the database might or might not contain the input timestamp
	QDateTime time;
	auto& table = m_database[nodeId];
	switch (match)
	{
	case QUaHistoryBackend::TimeMatch::ClosestFromAbove:
	{
		if (table.contains(timestamp) &&
			table.constFind(timestamp) + 1 != table.end())
		{
			// return next key if available
			auto iter = table.find(timestamp) + 1;
			time = iter.key();
		}
		else
		{
			// return closest key from above or last one if out of range
			auto iter = std::upper_bound(table.keyBegin(), table.keyEnd(), timestamp);
			time = iter == table.keyEnd() ? table.lastKey() : *iter;
		}
	}
	break;
	case QUaHistoryBackend::TimeMatch::ClosestFromBelow:
	{
		if (table.contains(timestamp) &&
			table.constFind(timestamp) != table.begin())
		{
			// return previous key if available
			auto iter = table.find(timestamp) - 1;
			time = iter.key();
		}
		else
		{
			// return closest key from below or last one if out of range
			Q_ASSERT(timestamp <= *table.keyBegin());
			auto iter = std::lower_bound(table.keyBegin(), table.keyEnd(), timestamp);
			time = iter == table.keyEnd() ? table.lastKey() : *iter;
		}
	}
	break;
	default:
		break;
	}
	return time;
}

quint64 QUaInMemoryHistorizer::numDataPointsInRange(
	const QUaNodeId &nodeId,
	const QDateTime& timeStart,
	const QDateTime& timeEnd,
	QQueue<QUaLog>& logOut) const
{
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error finding history points. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return 0;
	}
	auto& table = m_database[nodeId];
	// the database must contain the start timestamp
	Q_ASSERT(table.contains(timeStart));
	// if the end timestamp is valid, then it must be contained in the database
	// else it means the API is requesting up to the most recent timestamp (end)
	Q_ASSERT(table.contains(timeEnd) || !timeEnd.isValid());
	return static_cast<quint64>(std::distance(
		table.find(timeStart),
		timeEnd.isValid() ? table.find(timeEnd) + 1 : table.end()
	));
}

QVector<QUaHistoryDataPoint> QUaInMemoryHistorizer::readHistoryData(
	const QUaNodeId &nodeId,
	const QDateTime& timeStart,
	const quint64& numPointsToRead,
	QQueue<QUaLog>& logOut) const
{
	auto points = QVector<QUaHistoryDataPoint>();
	if (!m_database.contains(nodeId))
	{
		logOut << QUaLog({
			QObject::tr("Error reading history data. "
				"History database does not contain table for node id %1")
				.arg(nodeId),
			QUaLogLevel::Error,
			QUaLogCategory::History
			});
		return points;
	}
	auto& table = m_database[nodeId];
	Q_ASSERT(table.contains(timeStart));
	// get total range to read
	auto iterIni = table.find(timeStart);
	// resize return value accordingly
	points.resize(numPointsToRead);
	// copy return data points
	std::generate(points.begin(), points.end(),
		[&iterIni, &table]() {
			QUaHistoryDataPoint retVal;
		if (iterIni == table.end())
		{
			// NOTE : return an invalid value if API requests more values than available
			return retVal;
		}
		retVal = {
			iterIni.key(),
			iterIni.value().value,
			iterIni.value().status
		};
		iterIni++;
		return retVal;
	});
	return points;
}

#ifdef UA_ENABLE_SUBSCRIPTIONS_EVENTS

bool QUaInMemoryHistorizer::writeHistoryEventsOfType(
	const QUaNodeId            &eventTypeNodeId,
	const QVector<QUaNodeId>   &emittersNodeIds,
	const QUaHistoryEventPoint &eventPoint,
	QQueue<QUaLog>             &logOut
)
{
	// get a unique integer id (key) for the event
	// timestamp cannot be used because there can be multiple
	// events for the same timestamp
	Q_ASSERT(eventPoint.fields.contains("EventId"));
	QByteArray byteEventId = eventPoint.fields["EventId"].value<QByteArray>();
	uint intEventKey = qHash(byteEventId);
	Q_ASSERT(!m_eventTypeDatabase[eventTypeNodeId].contains(intEventKey));
	// insert in event type table
	m_eventTypeDatabase[eventTypeNodeId][intEventKey] = eventPoint;
	// create reference from each emitter's table
	for (auto &emitterNodeId : emittersNodeIds)
	{
		// NOTE : use QMultiMap::insert which allows 
		// multiple values (eventKey) for same key (time)
		m_eventEmitterDatabase[emitterNodeId][eventTypeNodeId].insert(
			eventPoint.timestamp, 
			intEventKey
		);
	}
	// success
	return true;
}

QVector<QUaNodeId> QUaInMemoryHistorizer::eventTypesOfEmitter(
	const QUaNodeId &emitterNodeId, 
	QQueue<QUaLog>  &logOut
)
{
	return m_eventEmitterDatabase[emitterNodeId].keys().toVector();
}

QDateTime QUaInMemoryHistorizer::findTimestampEventOfType(
	const QUaNodeId                    &emitterNodeId,
	const QUaNodeId                    &eventTypeNodeId,
	const QDateTime                    &timestamp,
	const QUaHistoryBackend::TimeMatch &match,
	QQueue<QUaLog>                     &logOut
)
{
	Q_ASSERT(m_eventEmitterDatabase.contains(emitterNodeId));
	Q_ASSERT(m_eventEmitterDatabase[emitterNodeId].contains(eventTypeNodeId));
	// NOTE : the database might or might not contain the input timestamp
	QDateTime time;
	auto& table = m_eventEmitterDatabase[emitterNodeId][eventTypeNodeId];
	switch (match)
	{
	case QUaHistoryBackend::TimeMatch::ClosestFromAbove:
	{
		if (table.contains(timestamp) &&
			table.constFind(timestamp) + 1 != table.end())
		{
			// return next key if available
			auto iter = table.find(timestamp) + 1;
			time = iter.key();
		}
		else
		{
			// return closest key from above or last one if out of range
			auto iter = std::upper_bound(table.keyBegin(), table.keyEnd(), timestamp);
			time = iter == table.keyEnd() ? table.lastKey() : *iter;
		}
	}
	break;
	case QUaHistoryBackend::TimeMatch::ClosestFromBelow:
	{
		if (table.contains(timestamp) &&
			table.constFind(timestamp) != table.begin())
		{
			// return previous key if available
			auto iter = table.find(timestamp) - 1;
			time = iter.key();
		}
		else
		{
			// return closest key from below or last one if out of range
			auto iter = std::lower_bound(table.keyBegin(), table.keyEnd(), timestamp);
			time = iter == table.keyEnd() ? table.lastKey() : *iter;
		}
	}
	break;
	default:
		break;
	}
	return time;
}

quint64 QUaInMemoryHistorizer::numEventsOfTypeInRange(
	const QUaNodeId &emitterNodeId,
	const QUaNodeId &eventTypeNodeId,
	const QDateTime &timeStart,
	const QDateTime &timeEnd,
	QQueue<QUaLog>  &logOut
)
{
	Q_ASSERT(m_eventEmitterDatabase.contains(emitterNodeId));
	Q_ASSERT(m_eventEmitterDatabase[emitterNodeId].contains(eventTypeNodeId));
	auto& table = m_eventEmitterDatabase[emitterNodeId][eventTypeNodeId];
	// the database must contain the start timestamp
	Q_ASSERT(table.contains(timeStart));
	// if the end timestamp is valid, then it must be contained in the database
	// else it means the API is requesting up to the most recent timestamp (end)
	Q_ASSERT(table.contains(timeEnd) || !timeEnd.isValid());
	auto iterStart = table.find(timeStart);
	quint64 distance = static_cast<quint64>(std::distance(
		iterStart,
		timeEnd.isValid() ? table.find(timeEnd) + 1 : table.end()
	));
	auto iterEnd = iterStart + distance;
	while (timeEnd.isValid() && iterEnd != table.end() && iterEnd.key() == timeEnd) {
		distance++;
		iterEnd++;
	}
	// multimap can have multiple values
	return distance;
}

QVector<QUaHistoryEventPoint> QUaInMemoryHistorizer::readHistoryEventsOfType(
	const QUaNodeId &emitterNodeId,
	const QUaNodeId &eventTypeNodeId,
	const QDateTime &timeStart,
	const quint64   &numPointsOffset,
	const quint64   &numPointsToRead,
	QQueue<QUaLog>  &logOut
)
{
	Q_ASSERT(m_eventEmitterDatabase.contains(emitterNodeId));
	Q_ASSERT(m_eventEmitterDatabase[emitterNodeId].contains(eventTypeNodeId));
	Q_ASSERT(m_eventTypeDatabase.contains(eventTypeNodeId));
	auto& table = m_eventEmitterDatabase[emitterNodeId][eventTypeNodeId];
	auto& source = m_eventTypeDatabase[eventTypeNodeId];
	Q_ASSERT(table.contains(timeStart));
	auto points = QVector<QUaHistoryEventPoint>();
	// get starting point to read
	auto iterIni = table.find(timeStart) + numPointsOffset;
	// resize return value accordingly
	points.resize(numPointsToRead);
	// copy return data points
	std::generate(points.begin(), points.end(),
		[&iterIni, &table, &source]()
		{
			QUaHistoryEventPoint retVal;
			if (iterIni == table.end())
			{
				// NOTE : return an invalid value if API requests more values than available
				return retVal;
			}
			uint& intEventKey = iterIni.value();
			Q_ASSERT(source.contains(intEventKey));
			retVal = source[intEventKey];
			iterIni++;
			return retVal;
		});
	return points;
}

#endif // UA_ENABLE_SUBSCRIPTIONS_EVENTS

#endif // UA_ENABLE_HISTORIZING