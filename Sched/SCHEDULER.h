#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "THREAD.h"
#include <time.h>

typedef enum {
	AWAIT,
	RESUMING,
	ACTIVE,
	FINISHED
} THREAD_STATUS_t;

#define EIP_OFFSET 0
#define EAX_OFFSET 4
#define EBX_OFFSET 8
#define ECX_OFFSET 12
#define EDX_OFFSET 16
#define ESI_OFFSET 20
#define EDI_OFFSET 24
#define EBP_OFFSET 28
#define ESP_OFFSET 32
#define FLAGS_OFFSET 36

typedef struct {
	int EIP;

	int EAX;
	int EBX;
	int ECX;
	int EDX;
	int ESI;
	int EDI;

	int EBP;
	int ESP;

	int FLAGS;
} REGISTERS_STATE_t;

typedef struct {
	int size;
	int *start;
} STACK_t;

typedef struct {
	// Registers snapshot
	REGISTERS_STATE_t registers;
	// Stack snapshot
	STACK_t stack;
} THREAD_CONTEXT_t;

typedef struct {
	int id;
	THREAD_STATUS_t status;
	THREAD__entry_point_t* entry_point;
	void * arg;
	THREAD_CONTEXT_t context;
	clock_t last_run;
} THREAD_t;

/* Yield function, to be used by the schedulers threads.
 * Once a thread calls yield, execution should continue from a 
 * different thread. */
void SCHEDULER__yield(void);

/* Start the scheduler. This function will return only when all
 * threads finished their work. */
void SCHEDULER__schedule_threads(void);

/* Initialize a specific thread and add it to the thread pool. */
void SCHEDULER__add_thread(THREAD__entry_point_t *entry_point, void *arg);

void SCHEDULER__init(void);

void SCHEDULER__exit(void);

void SCHEDULER__run_thread(void);

__inline void THREAD_CONTEXT__save_registers(void);

void THREAD_CONTEXT__save_stack(void);

void THREAD_CONTEXT__ready_transfer(void);

void THREAD_CONTEXT__load(void);

__forceinline int get_eip(void);

#endif