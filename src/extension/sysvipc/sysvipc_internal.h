#ifndef SYSVIPC_INTERNAL_H
#define SYSVIPC_INTERNAL_H

#include "tracee/tracee.h"
#include "sysvipc_sys.h"

#include <sys/queue.h>
#include <sys/msg.h>
#include <stdint.h>
#include <uchar.h>
#include <stdbool.h>

/******************
 * Message queues *
 *****************/
struct SysVIpcMsgQueueItem {
	long mtype;
	char *mtext;
	size_t mtext_length;
	STAILQ_ENTRY(SysVIpcMsgQueueItem) link;
};
STAILQ_HEAD(SysVIpcMsgQueueItems, SysVIpcMsgQueueItem);

struct SysVIpcMsgQueue {
	int32_t key;
	int16_t generation;
	bool valid;
	struct SysVIpcMsgQueueItems *items;
	struct msqid_ds stats;
};

/**************
 * Semaphores *
 *************/

struct SysVIpcSemaphore {
	int32_t key;
	int16_t generation;
	bool valid;
	uint16_t *sems;
	int nsems;
	int semncnt;
	int semzcnt;
};

struct SysVIpcNamespace {
	/** Array of Message Queues
	 * Since arrays are 0-indexed and queues are 1-indexed,
	 * queues with id is at queues[id-1] */
	struct SysVIpcMsgQueue *queues;
	struct SysVIpcSemaphore *semaphores;
};

enum SysVIpcWaitReason {
	WR_NOT_WAITING,
	WR_WAIT_QUEUE_RECV,
	WR_WAIT_SEMOP,
};

enum SysVIpcWaitState {
	WSTATE_NOT_WAITING,
	WSTATE_RESTARTED_INTO_PPOLL_CANCELED,
	WSTATE_RESTARTED_INTO_PPOLL,
	WSTATE_ENTERED_PPOLL,
	WSTATE_SIGNALED_PPOLL,
	WSTATE_ENTERED_GETPID,
};

/** Per-tracee structure with state of this extension */
struct SysVIpcConfig {
	struct SysVIpcNamespace *ipc_namespace;
	enum SysVIpcWaitReason wait_reason;
	enum SysVIpcWaitState wait_state;
	word_t status_after_wait;

	size_t waiting_object_index;

	word_t msgrcv_msgp;
	size_t msgrcv_msgsz;
	int msgrcv_msgtyp;
	int msgrcv_msgflg;

	size_t semop_nsops;
	struct SysVIpcSembuf *semop_sops;
	char semop_wait_type;
};

/**
 * Find given IPC object requested by tracee
 *
 * out_index should be size_t
 * out_object should be pointer to struct SysVIpc[MsgQueue]
 */
#define LOOKUP_IPC_OBJECT(out_index, out_object, objects_array) \
{ \
	int object_id = peek_reg(tracee, CURRENT, SYSARG_1); \
	int object_index = object_id & 0xFFF; \
	if (object_index <= 0 || object_index > (int)talloc_array_length(objects_array)) { \
		return -EINVAL; \
	} \
	out_index = object_index - 1; \
	out_object = &(objects_array)[object_index - 1]; \
	if (out_object->generation != ((object_id >> 12) & 0xFFFF)) { \
		return -EINVAL; \
	} \
}

/**
 * Iterate over all Tracees in given SysVIpc namespace
 *
 * out_tracee should be 'Tracee *'
 * out_config should be 'struct SysVIpcConfig *'
 * checked_namespace is SysVIpc namespace to find tracees in
 */
#define SYSVIPC_FOREACH_TRACEE(out_tracee, out_config, checked_namespace) \
	LIST_FOREACH((out_tracee), get_tracees_list_head(), link) \
	if ( \
		((out_config) = sysvipc_get_config(out_tracee)) != NULL && \
		(out_config)->ipc_namespace == (checked_namespace) \
	)

void sysvipc_wake_tracee(Tracee *tracee, struct SysVIpcConfig *config, int status);
struct SysVIpcConfig *sysvipc_get_config(Tracee *tracee);

int sysvipc_msgget(Tracee *tracee, struct SysVIpcConfig *config);
int sysvipc_msgsnd(Tracee *tracee, struct SysVIpcConfig *config);
int sysvipc_msgrcv(Tracee *tracee, struct SysVIpcConfig *config);
int sysvipc_msgctl(Tracee *tracee, struct SysVIpcConfig *config);

int sysvipc_semget(Tracee *tracee, struct SysVIpcConfig *config);
int sysvipc_semop(Tracee *tracee, struct SysVIpcConfig *config);
void sysvipc_semop_timedout(Tracee *tracee, struct SysVIpcConfig *config);
int sysvipc_semctl(Tracee *tracee, struct SysVIpcConfig *config);

#endif // SYSVIPC_INTERNAL_H
