#include <windows.h>
#include "debug.h"
#include "dataprotectorUser.h"


#define MAX_QUEUE_NUM 20

PIDPARAMETER *gpPIDParaQueue = NULL;
int gpPIDParaQueueIndex = 0;
int gpPIDParaQueueCount = 0;

int InitQueue() {
	gpPIDParaQueue = HeapAlloc(
		gHeap,
		HEAP_ZERO_MEMORY,
		MAX_QUEUE_NUM * sizeof(PIDPARAMETER)
	);
	if (!gpPIDParaQueue) {
		dprint(LEVEL_ERROR, "Can't alloc gpPIDParaQueue");
		return FALSE;
	}
	return TRUE;
}

void UninitQueue() {
	if (gpPIDParaQueue) {
		HeapFree(gHeap, MEM_RELEASE, gpPIDParaQueue);
	}
}

int Push(PIDPARAMETER * pData) {
	if (gpPIDParaQueueCount == MAX_QUEUE_NUM) {
		return FALSE;
	}
	memcpy(&gpPIDParaQueue[gpPIDParaQueueIndex],
		pData,
		sizeof(PIDPARAMETER)
	);
	gpPIDParaQueueIndex = (gpPIDParaQueueIndex + 1) % MAX_QUEUE_NUM;
	gpPIDParaQueueCount += 1;
	return TRUE;
}

PIDPARAMETER* Pop() {
	if (gpPIDParaQueueCount) {
		gpPIDParaQueueCount -= 1;
		if (!gpPIDParaQueueIndex) {
			return &gpPIDParaQueue[MAX_QUEUE_NUM - 1];
		}
		return &gpPIDParaQueue[gpPIDParaQueueIndex - 1];
	}
	return NULL;
}