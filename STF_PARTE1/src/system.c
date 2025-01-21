/**********************************************************************
* FILENAME : system.c       
*
* DESCRIPTION : 
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_event.h>
#include <esp_log.h>

#include "system.h"

static const char *TAG = "system";


static void __on_sys_state_change(void* handler_arg, esp_event_base_t base, int32_t id, void* ptr)
{
	system_t *system = (system_t *) handler_arg;
	xSemaphoreTake(system->sys_st_mutex, 0);
	system->sys_state = id;
	xSemaphoreGive(system->sys_new_state);
	xSemaphoreGive(system->sys_st_mutex);
}

// system create
void system_create(system_t* sys, const char* id)
{
	char evt_loop_task_name[32] = "";
	
	//mutex(s) 
	sys->sys_st_mutex = xSemaphoreCreateBinary();
	sys->sys_new_state = xSemaphoreCreateBinary();
	sys->sys_nstates = 0;
	
	// name
	// strlen(id) < 16
	strcpy(sys->sys_id, id);
	
	// system event loop
	strcat(evt_loop_task_name, id);
	strcat(evt_loop_task_name, "__evt_loop_task");
	sys->sys_evt_loop_args.queue_size = 5;
	sys->sys_evt_loop_args.task_name = evt_loop_task_name; 
	sys->sys_evt_loop_args.task_priority = uxTaskPriorityGet(NULL);
	sys->sys_evt_loop_args.task_stack_size = 3072;
	sys->sys_evt_loop_args.task_core_id = tskNO_AFFINITY;
	esp_event_loop_create(&(sys->sys_evt_loop_args), &(sys->sys_evt_loop));
}

// system add state

void system_register_state(system_t *sys, uint8_t st)
{
	esp_event_handler_register_with(sys->sys_evt_loop, (esp_event_base_t) sys->sys_id, st, __on_sys_state_change, sys);
	sys->sys_nstates+=1;
}

// system set default state

void system_set_default_state(system_t *sys, uint8_t default_st)
{
	sys->sys_state = default_st;
	if (!uxSemaphoreGetCount(sys->sys_new_state))
		xSemaphoreGive(sys->sys_new_state);
}

// (common private) system task start

static void __system_task_start(system_t *sys, system_task_t *task, void* args)
{
	// system
	task->system = sys; 
	
	// mutex to stop the task
	
	task->sys_task_stop = xSemaphoreCreateBinary();
	xSemaphoreGive(task->sys_task_stop);

	// args
	task->sys_task_args = args;
}

// system task start
void system_task_start(system_t *sys, system_task_t *task, TaskFunction_t function, const char * const name, configSTACK_DEPTH_TYPE stack_depth, void* args, UBaseType_t priority)
{
	
	__system_task_start(sys, task, args);
	
	// creation 
	xTaskCreate( function, name, stack_depth, task, priority, &(task->sys_task_handler));

	configASSERT(task->sys_task_handler);
}


// system task start in a specific core

void system_task_start_in_core(system_t *sys, system_task_t *task, TaskFunction_t function, const char * const name, configSTACK_DEPTH_TYPE stack_depth, void* args, UBaseType_t priority, BaseType_t coreid)
{
	__system_task_start(sys, task, args);
	
	// creation 
	xTaskCreatePinnedToCore( function, name, stack_depth, task, priority, &task->sys_task_handler, coreid);
	configASSERT(task->sys_task_handler );
}

// system task stop 

void system_task_stop(system_t *sys, system_task_t *task, uint16_t timeout_ms)
{

	// stop task perception
	assert(xSemaphoreTake(task->sys_task_stop, pdMS_TO_TICKS(10)) == pdTRUE);
	if (xSemaphoreTake(task->sys_task_stop, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
	{
		ESP_LOGW(TAG, "Task stop timeout");	
	}
	vTaskDelete(task->sys_task_handler);
	task->sys_task_handler = NULL;
	vSemaphoreDelete(task->sys_task_stop);
	task->sys_task_args = NULL;
	task-> system = NULL; 
}
