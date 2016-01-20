#include "SCHEDULER.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_THREADS 3

static THREAD_t* _threads[MAX_THREADS];
static int _thread_count = 0;
static int _running_thread_id = -1;

static int _base_esp;
static int _base_ebp;
static int _base_eip;

static int _transfer_stack_ptr;
static int _transfer_stack_size;
static int _transfer_registers_ptr;

static int _save_registers_ptr;

void SCHEDULER__init(void)
{

}

void SCHEDULER__exit(void)
{
	int i;
	for(i = 0; i < _thread_count; i++)
	{
		free(_threads[i]);
	}
}

void SCHEDULER__add_thread(THREAD__entry_point_t *entry_point, void *arg)
{
	THREAD_t *new_thread = (THREAD_t*)malloc (sizeof (THREAD_t));
	memset(new_thread, 0, sizeof (THREAD_t));
	new_thread->id = _thread_count;
	new_thread->status = AWAIT;
	new_thread->last_run = -1;

	new_thread->entry_point = entry_point;
	new_thread->arg = arg;

	// Add ptr to global thread pool
	_threads[_thread_count++] = new_thread;
}

void SCHEDULER__schedule_threads(void)
{
	int new_thread_id;
	clock_t min_clock;
	int i;
	REGISTERS_STATE_t *saved_registers;

	if(_thread_count < 1)
	{
		// No threads were added to the scheduler
		return;
	}

	// Save current registers (I can store the registers locally tho..)
	saved_registers = (REGISTERS_STATE_t*)malloc (sizeof (REGISTERS_STATE_t)); // I can store the registers
	memset(saved_registers, 0, sizeof (REGISTERS_STATE_t));

	_save_registers_ptr = (int)saved_registers;
	THREAD_CONTEXT__save_registers();

	_base_esp = saved_registers->ESP;
	_base_ebp = saved_registers->EBP;

	Base:
	_base_eip = get_eip();

	if(_running_thread_id != -1)
	{
		_threads[_running_thread_id]->status = AWAIT;

		_running_thread_id = -1;

		__asm
		{
			mov esp, _base_esp;
			mov ebp, _base_ebp;
		}

		goto Base;
	}

	// Find a thread the was ran the longest time ago
	new_thread_id = -1;
	min_clock = 0x7FFFFFFF;
	for(i = 0; i < _thread_count; i++)
	{
		if(_threads[i]->last_run < min_clock && _threads[i]->status != FINISHED)
		{
			min_clock = _threads[i]->last_run;
			new_thread_id = i;
		}
	}

	if(new_thread_id != -1)
	{
		// Run/resume thread
		_running_thread_id = new_thread_id;
		SCHEDULER__run_thread();

		// The thread has finished
		goto Base;
	}


	// load back registers
	// REGISTERS_STATE__load(_original_registers_state);
	free(saved_registers);

	SCHEDULER__exit();
}

void SCHEDULER__run_thread(void)
{
	THREAD_t *thread = _threads[_running_thread_id];
	printf("1");
	// Update the thread's last_run to right now
	thread->last_run = clock();

	// Set ptr to save registers at (this is to prevent the need to calculate this ptr when we need to save the registers - which will change the register values)
	_save_registers_ptr = (int)&(thread->context.registers);

	if(thread->context.registers.EIP == 0)
	{
		thread->status = ACTIVE;
		// We are running this thread for the first time, run the thread entry point
		thread->entry_point(thread->arg);

		// The thread has completed
		_running_thread_id = -1;
		thread->status = FINISHED;
	}
	else
	{
		thread->status = RESUMING;

		THREAD_CONTEXT__load();
	}
}

void SCHEDULER__yield(void)
{
	THREAD_CONTEXT__save_registers();
	// Save current context
	THREAD_CONTEXT__save_stack();
	
	_threads[_running_thread_id]->context.registers.EIP = get_eip();

	/* We jump back here upon thread resuming */

	if(_threads[_running_thread_id]->status == ACTIVE)
	{
		__asm
		{
			jmp _base_eip;
		}
	}
	else if(_threads[_running_thread_id]->status == RESUMING)
	{
		_threads[_running_thread_id]->status = ACTIVE;
	}

	__asm
	{
		// EDX was left behind when loading registers because we needed it to contain '_transfer_registers_ptr'.
		push eax;
		mov eax, _transfer_registers_ptr;
		mov edx, [eax + EDX_OFFSET];
		pop eax;
	}
}

void THREAD_CONTEXT__save_stack(void)
{
	// Since we save the stack from EBP and downwards, we can contaminate it as much as we want :)
	int copy_size;
	int copy_counter = 0;
	int copy_end;
	int ptr;

	// Pointer to the current thread's stack struct
	STACK_t *stack = &(_threads[_running_thread_id]->context.stack);

	// Dispose previous stack
	if(stack->start != NULL)
	{
		free(stack->start);
	}

	// Calculate where we want to finish copying
	__asm
	{
		mov copy_end, ebp;
	}
	copy_end += 8; // We don't want to copy this stack frame's EBP & return address

	// Calculate the amount we need to copy
	copy_size = (int)(_base_esp) - copy_end;

	// Set it to the size of the stack
	stack->size = copy_size;

	// Allocate memory for the stack data
	stack->start = (int*)malloc(copy_size);
	memset(stack->start, 0, copy_size);

	// ptr points to the next dword we want to copy
	ptr = _base_esp - 4;
	while(ptr > copy_end)
	{
		// Copy the value on stack to the buffer
		*(stack->start + copy_counter) = *(int*)(ptr);
		// Increase copy counter
		copy_counter++;
		// Point to next value on the stack
		ptr -= 4;
	}
}

__forceinline void THREAD_CONTEXT__save_registers(void)
{
	// REGISTERS_STATE_t *registers = &(_threads[_running_thread_id]->context.registers);

	// To prevent calculations (that will change our register values) in order to calculate where the thread's register_state struct is,
	// we use _save_registers_ptr that we have set earlier on

	__asm
	{
		push edx;
		mov edx, _save_registers_ptr;
		mov [edx + EAX_OFFSET], eax;
		mov [edx + EBX_OFFSET], ebx;
		mov [edx + ECX_OFFSET], ecx;
		// mov [edx + EDX_OFFSET], edx;
		mov [edx + ESI_OFFSET], esi;
		mov [edx + EDI_OFFSET], edi;

		mov [edx + EBP_OFFSET], ebp;
		mov [edx + ESP_OFFSET], esp;

		push eax;
		xor eax, eax;
		lahf;
		mov [edx + FLAGS_OFFSET], eax;
		pop eax;

		// mov [edx + EAX_OFFSET], [esp]; // haha fuck EDX!
		pop edx;
	};
}

void THREAD_CONTEXT__ready_transfer(void)
{
	// Point the transfer global variables to the right addresses
	THREAD_t *thread = _threads[_running_thread_id];
	_transfer_stack_ptr = (int)thread->context.stack.start;
	_transfer_stack_size = thread->context.stack.size;
	_transfer_registers_ptr = (int)&(thread->context.registers);
}

void THREAD_CONTEXT__load(void)
{
	THREAD_CONTEXT__ready_transfer();

	/* Build stack on top of _base */
	__asm
	{
		mov esp, _base_esp;
		// mov ebp, _base_ebp; // unnessacary
	}

	__asm
	{
		mov esi, _transfer_stack_ptr;
		mov ebx, esp;
		sub ebx, _transfer_stack_size;

copy_stack:
		push [esi]
		add esi, 4;
		cmp esp, ebx;
		jg copy_stack;
	}

	/* Transfer register values */
	__asm
	{
		mov edx, _transfer_registers_ptr;

		mov eax, [edx + FLAGS_OFFSET];
		sahf;

		mov eax, [edx + EAX_OFFSET];
		mov ebx, [edx + EBX_OFFSET];
		mov ecx, [edx + ECX_OFFSET];
		// mov edx, [edx + EDX_OFFSET];
		mov esi, [edx + ESI_OFFSET];
		mov edi, [edx + EDI_OFFSET];


		mov ebp, [edx + EBP_OFFSET];
		// mov esp, [edx + ESP_OFFSET];

		mov edx, dword ptr [edx + EIP_OFFSET];
		inc edx; // This will cause us to land right after the "pop eax" instruction
        jmp edx;
	};
}

// Helpers

__forceinline int get_eip(void)
{
	int eip_val;
	__asm
	{
        call abc
		abc:
        pop eax
        mov eip_val, eax
	}
	return eip_val;
}