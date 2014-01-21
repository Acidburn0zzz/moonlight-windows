#include "LinkedBlockingQueue.h"

PLINKED_BLOCKING_QUEUE_ENTRY LbqDestroyLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead) {
	PltDeleteMutex(&queueHead->mutex);
	PltCloseEvent(&queueHead->containsDataEvent);
	
	return queueHead->head;
}

int LbqInitializeLinkedBlockingQueue(PLINKED_BLOCKING_QUEUE queueHead, int sizeBound) {
	int err;
	
	err = PltCreateEvent(&queueHead->containsDataEvent);
	if (err != 0) {
		return err;
	}

	err = PltCreateMutex(&queueHead->mutex);
	if (err != 0) {
		return err;
	}

	queueHead->head = NULL;
	queueHead->sizeBound = sizeBound;
	queueHead->currentSize = 0;

	return 0;
}

int LbqOfferQueueItem(PLINKED_BLOCKING_QUEUE queueHead, void* data) {
	PLINKED_BLOCKING_QUEUE_ENTRY entry, lastEntry;

	entry = (PLINKED_BLOCKING_QUEUE_ENTRY) malloc(sizeof(*entry));
	if (entry == NULL) {
		return LBQ_NO_MEMORY;
	}

	entry->next = NULL;
	entry->data = data;

	PltLockMutex(&queueHead->mutex);

	if (queueHead->currentSize == queueHead->sizeBound) {
		PltUnlockMutex(&queueHead->mutex);
		free(entry);
		return LBQ_BOUND_EXCEEDED;
	}

	if (queueHead->head == NULL) {
		LC_ASSERT(queueHead->currentSize == 0);
		queueHead->head = entry;
	}
	else {
		LC_ASSERT(queueHead->currentSize >= 1);
		lastEntry = queueHead->head;
		while (lastEntry->next != NULL) {
			lastEntry = lastEntry->next;
		}
		lastEntry->next = entry;
	}

	queueHead->currentSize++;

	PltUnlockMutex(&queueHead->mutex);

	PltSetEvent(&queueHead->containsDataEvent);

	return LBQ_SUCCESS;
}

int LbqWaitForQueueElement(PLINKED_BLOCKING_QUEUE queueHead, void** data) {
	PLINKED_BLOCKING_QUEUE_ENTRY entry;
	int err;

	for (;;) {
		err = PltWaitForEvent(&queueHead->containsDataEvent);
		if (err != PLT_WAIT_SUCCESS) {
			return LBQ_INTERRUPTED;
		}

		PltLockMutex(&queueHead->mutex);

		if (queueHead->head == NULL) {
			PltUnlockMutex(&queueHead->mutex);
			continue;
		}

		entry = queueHead->head;
		queueHead->head = entry->next;
		queueHead->currentSize--;

		*data = entry->data;

		free(entry);

		if (queueHead->head == NULL) {
			LC_ASSERT(queueHead->currentSize == 0);
			PltClearEvent(&queueHead->containsDataEvent);
		}

		PltUnlockMutex(&queueHead->mutex);

		break;
	}

	return LBQ_SUCCESS;
}
