/**HEADER*******************************************************************
 * *
 * * Copyright (c) 2008 Freescale Semiconductor;
 * * All Rights Reserved
 * *
 * * Copyright (c) 1989-2008 ARC International;
 * * All Rights Reserved
 * *
 * ****************************************************************************
 * *
 * * THIS SOFTWARE IS PROVIDED BY FREESCALE  "AS IS" AND ANY EXPRESSED OR
 * * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * * IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * * THE POSSIBILITY OF SUCH DAMAGE.
 * *
 * ****************************************************************************
 * *
 * * Comments:
 * *
 * *   This file contains main initialization for your application
 * *   and infinite loop
 * *
 * *END************************************************************************/

#include "main.h"
#include <tfs.h>
#include <httpd.h>
#include <httpd_types.h>
#include "webpage.h"



#if defined(APPLICATION_HAS_SHELL) && (!SHELLCFG_USES_RTCS)
#error This application requires SHELLCFG_USES_RTCS defined non-zero in user_config.h. Please recompile libraries with this option if any Ethernet interface is available.
#endif


#define BUFFER_LENGTH 256
#define N_ROOMS 4
#define SECONDS_PER_DAY 86400

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
/*  Task number, Entry point, Stack, Pri, String, Autostart, Round robin time slice */
   {MAIN_TASK,   Main_task,   2000,  9,   "main", MQX_AUTO_START_TASK, 50},
   {BUTTON_TASK, Button_task, 2000, 9, "button", 0, 50},
   {FLASH_TASK,  Flash_task, 2000, 9, "flash",   0, 50},
   {SCHED_TASK, Sched_task, 2000, 9, "scheduler", 0, 50},
   {0,           0,           0,     0,   0,      0, 0}
};


static HTTPD_ROOT_DIR_STRUCT http_root_dir[] = {
	{ "", "tfs:"}, { 0,0 } };

// files converted to C strings with XXD
const TFS_DIR_ENTRY static_data[] = {
		{"/index.html",	0, index_html, 	sizeof(index_html)},
		{"/script.js",	0, script_js, 	sizeof(script_js)},
		{"/style.css",	0, style_css, 	sizeof(style_css)},
		{"/logo.png",	0, logo_png, 	sizeof(logo_png)},
		{0,				0, 0,					0}
	};

/*  type definitions */

typedef enum st { DISABLED, ENABLED, TRIGGERED } status_type;

typedef struct room_alarm_struct {
	status_type status;
	int timer_on;
	unsigned int start_time;
	unsigned int end_time;
	int led; // reference to the led
	int button; // reference to the button
} room_alarm;

//globals
HTTPD_STRUCT* http_server;
HMI_CLIENT_STRUCT_PTR hmi;
room_alarm room_alarms[N_ROOMS];
/* function declarations */
void init_room_alarms();
void init_pushbuttons();
void led_update();

/* functions/callbacks */
void trigger_alarm(void *);
void disable_all(void *);
void enable_all(void *);
void hush_all(void *);
void toggle_all(void *);

/* initialise webserver paths */
_mqx_int cgi_set_system_time(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_get_timer_status(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_set_enable_time(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_set_status(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_alarm_status(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_enable_all(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_hush_all(HTTPD_SESSION_STRUCT *);
_mqx_int cgi_disable_all(HTTPD_SESSION_STRUCT *);


// list of CGI routes
static HTTPD_CGI_LINK_STRUCT http_cgi_params[] = {
	{ "set_system_time", 	cgi_set_system_time },
	{ "timer_status", 		cgi_get_timer_status },
	{ "enable_all", 		cgi_enable_all },
	{ "hush_all", 			cgi_hush_all },
	{ "disable_all", 		cgi_disable_all}, 
	{ "set_enable_time", 	cgi_set_enable_time },
	{ "set_status", 		cgi_set_status},
	{ "alarm_status", 		cgi_alarm_status },
	{ 0, 0 }
};

void Main_task(uint_32 initial_data) {
	// install filesystem
	_io_tfs_install("tfs:", static_data);

	// initialise the LED driver
	hmi = _bsp_btnled_init();
	// initialise rtcs
	rtcs_init();
	// setup server
	http_server = httpd_server_init_af(http_root_dir, "\\index.html", AF_INET);
	HTTPD_SET_PARAM_CGI_TBL(http_server, http_cgi_params);

	//setup rtc
	_rtc_init(RTC_INIT_FLAG_ENABLE);

	//initialise alarm structs
	init_room_alarms();
	// run slave tasks
	_task_create(0, FLASH_TASK, 0); //(processor, task name, parameter)
	_task_create(0, BUTTON_TASK, 0);
	_task_create(0, SCHED_TASK, 0);
	// run server
	httpd_server_run(http_server);
	// poll for and handle HTTP requests
	while (TRUE) {
		ipcfg_task_poll();
		_sched_yield(); //yield to other tasks
	}
}



void Button_task(uint_32 initial_data) {
	init_pushbuttons();
	while(TRUE) {
		btnled_poll(hmi);
		_sched_yield(); //yield to other tasks
	}
}

void Flash_task(uint_32 initial_data) {
	uint_32 toggle_state;
	toggle_state = 0;
	led_update(toggle_state); // initial led update
	while(TRUE) {
    	led_update(toggle_state);
    	_time_delay(200);
    	toggle_state = !toggle_state;
	}
}

void Sched_task(uint_32 initial_data) {
	RTC_TIME_STRUCT time_struct;
	int i;
	unsigned int curr_time, start, end;
	while (TRUE) {

		_rtc_get_time(&time_struct);
		curr_time = time_struct.seconds;
		if (curr_time >= SECONDS_PER_DAY) {
			// wrap the clock around
			curr_time = curr_time % SECONDS_PER_DAY;
			time_struct.seconds = curr_time;
			_rtc_set_time(&time_struct);
		}

		for (i = 0; i < N_ROOMS; i++) {
			// do not do anything to triggered alarms
			if (room_alarms[i].status == TRIGGERED)
				continue;

			start = room_alarms[i].start_time;
			end = room_alarms[i].end_time;
			if (start > end) {
				// the clock wraps when the alarm is scheduled between 17:00 to 04:00
				// so need to handle this correctly
				if (curr_time >= start || curr_time < end) 
					room_alarms[i].status = ENABLED;
				else 
					room_alarms[i].status = DISABLED;
			}
			else if (start < end) {
				// otherwise the clock
				if (curr_time >= start && curr_time < end)
					room_alarms[i].status = ENABLED;
				else
					room_alarms[i].status = DISABLED;
			}
			// if times are equal, then the alarm is not scheduled, so no 'else' needed
		}
		_time_delay(1000);
	}
}

void init_room_alarms() {
	int i; //iterator
	// set values of buttons and leds
	for (i = 0; i < N_ROOMS; ++i) {
		room_alarms[i].led = HMI_GET_LED_ID(i+1);
		room_alarms[i].button = HMI_GET_BUTTON_ID(i+1);
		room_alarms[i].status = ENABLED;
		room_alarms[i].timer_on = 0;
		room_alarms[i].start_time = 0;
		room_alarms[i].end_time = 0;
		//attach alarm trigger callbacks
		btnled_add_clb(hmi,
			room_alarms[i].button,
			HMI_VALUE_PUSH,
			trigger_alarm, (void*) &room_alarms[i]);
	}
}

void init_pushbuttons() {
	btnled_add_clb(hmi, HMI_BUTTON_5, HMI_VALUE_RELEASE, toggle_all, NULL);
	btnled_add_clb(hmi, HMI_BUTTON_6, HMI_VALUE_RELEASE, hush_all, NULL);
}


void trigger_alarm(void* room_alarm_ptr) {
	room_alarm* r = (room_alarm*) room_alarm_ptr; // cast to room_alarm
	if (r->status == ENABLED) 
		r->status = TRIGGERED; // flash if triggered
}

void toggle_all(void* dummy) {
	int i;
	int already_enabled = 0; 
	for (i = 0; i < N_ROOMS; i++) {
		if (room_alarms[i].status == ENABLED) 
			already_enabled++;
		else
			room_alarms[i].status = ENABLED;
	}
	if (already_enabled == N_ROOMS) {
		for (i = 0; i < N_ROOMS; i++)
			room_alarms[i].status = DISABLED;
	}
}

void disable_all(void* dummy) {
	int i;
	for (i = 0; i < N_ROOMS; ++i) {
    	room_alarms[i].status = DISABLED;
	}
}

void enable_all(void* dummy) {
	int i;
	for (i = 0; i < N_ROOMS; ++i) { //enable all alarms
		room_alarms[i].status = ENABLED;
	}
}


void hush_all(void* dummy) {
	int i;
	for (i = 0; i < N_ROOMS; ++i) { //enable all alarms
		if (room_alarms[i].status == TRIGGERED)
			room_alarms[i].status = ENABLED;
	}
}

// helper for LED flashing
void led_update(uint_32 toggle_state) {
	int i, value;
	for (i = 0; i < N_ROOMS; ++i) {
		switch(room_alarms[i].status) {
			case TRIGGERED: 
				value = toggle_state;
				break;
			case ENABLED:
				value = HMI_VALUE_ON;
				break;
			case DISABLED:
				value = HMI_VALUE_OFF;
				break;
		}
		btnled_set_value(hmi, room_alarms[i].led, value);
	}
}

// CGI routines

_mqx_int cgi_set_system_time(HTTPD_SESSION_STRUCT *session) {
	RTC_TIME_STRUCT new_time;
	char buffer[BUFFER_LENGTH];

	sscanf(session->request.urldata, "%u", &new_time.seconds);
	new_time.seconds = new_time.seconds % SECONDS_PER_DAY;
	_rtc_set_time(&new_time);

	sprintf(buffer, "{ \"system_time\" : %u }", new_time.seconds);
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

_mqx_int cgi_get_timer_status(HTTPD_SESSION_STRUCT *session) {
	int num;
	char buffer[BUFFER_LENGTH];
	sscanf(session->request.urldata, "room=%01u", &num);
	sprintf(buffer, "{ \"room\" : %01u, \"timer_status\" : %01u, \"start_time\" : %u, \"end_time\" : %u }", 
		num, room_alarms[num].timer_on, room_alarms[num].start_time, 
		room_alarms[num].end_time);
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

_mqx_int cgi_enable_all(HTTPD_SESSION_STRUCT *session) {
	char buffer[BUFFER_LENGTH];
	enable_all(NULL);
	sprintf(buffer, "All alarms enabled");	
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

// hush all alarms
_mqx_int cgi_hush_all(HTTPD_SESSION_STRUCT *session) {
	char buffer[BUFFER_LENGTH];
	hush_all(NULL);
	sprintf(buffer, "All alarms hushed");	
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

// disable all alarms
_mqx_int cgi_disable_all(HTTPD_SESSION_STRUCT *session) {
	char buffer[BUFFER_LENGTH];
	disable_all(NULL);
	sprintf(buffer, "All alarms disabled");	
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

// set alarm enable times
_mqx_int cgi_set_enable_time(HTTPD_SESSION_STRUCT *session) {
	unsigned int num = 0;
	char buffer[BUFFER_LENGTH];
	unsigned int start, end;
	sscanf(session->request.urldata, "room=%01u&start=%u&end=%u", &num, 
		&start, &end);
	room_alarms[num].start_time = start;
	room_alarms[num].end_time = end;
	room_alarms[num].timer_on = 1;

	sprintf(buffer, "{ \"room\" : %01u, \"start_time\" : %u, \"end_time\" : %u }", 
						num, start, end);
	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}

//set the enabled status of a room - ENABLED or DISABLED
_mqx_int cgi_set_status(HTTPD_SESSION_STRUCT *session) {
  char buffer[32];
  int num, status;
  sscanf(session->request.urldata, "room=%u&status=%u", &num, &status);
  room_alarms[num].status = (status_type) status;
  sprintf(buffer, "{ \"status\" : %u }", status);
  httpd_sendstr(session->sock, buffer);
  return session->request.content_len;
}

// get status of the whole system. 
_mqx_int cgi_alarm_status(HTTPD_SESSION_STRUCT *session) {
	char buffer[512];
	RTC_TIME_STRUCT curr_time;
	_rtc_get_time(&curr_time);
  sprintf(buffer, (const char*) status_json, // fill a JSON template with data. See status.json file
          room_alarms[0].status, room_alarms[0].timer_on, room_alarms[0].start_time, room_alarms[0].end_time,
          room_alarms[1].status, room_alarms[1].timer_on, room_alarms[1].start_time, room_alarms[1].end_time,
          room_alarms[2].status, room_alarms[2].timer_on, room_alarms[2].start_time, room_alarms[2].end_time,
          room_alarms[3].status, room_alarms[3].timer_on, room_alarms[3].start_time, room_alarms[3].end_time,
          curr_time.seconds);
  httpd_sendstr(session->sock, buffer);
  return session->request.content_len;
}
