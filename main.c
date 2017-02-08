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


#define BUFFER_LENGTH 64
#define N_ROOMS 4

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
/*  Task number, Entry point, Stack, Pri, String, Autostart, Round robin time slice */
   {MAIN_TASK,   Main_task,   2000,  9,   "main", MQX_AUTO_START_TASK, 50},
   {BUTTON_TASK, Button_task, 2000, 9, "button", 0, 50},
   {FLASH_TASK,  Flash_task, 2000, 9, "flash",   0, 50},
   {0,           0,           0,     0,   0,      0, 50}
};

/*TASK*-----------------------------------------------------------------
 * *
 * * Function Name  : Main_task
 * * Comments       :
 * *    This task initializes MFS and starts SHELL.
 * *
 * *END------------------------------------------------------------------*/

static HTTPD_ROOT_DIR_STRUCT http_root_dir[] = {
	{ "", "tfs:"}, { 0,0 } };


const TFS_DIR_ENTRY static_data[] = {
		{"/index.html",	0, index_html, 	sizeof(index_html)},
		{"/script.js",	0, script_js, 	sizeof(script_js)},
		{"/style.css",	0, style_css, 	sizeof(style_css)},
		{"/logo.svg",	0, logo_svg, 	sizeof(logo_svg)},
		{0,				0, 0,					0}
	};

/*  type definitions */

typedef enum st { DISABLED, ENABLED, TRIGGERED } status_type;

typedef struct room_alarm_struct {
	status_type status;
	unsigned int scheduled_time;
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

/* button/touch sensor callbacks*/
void trigger_alarm_callback(void *room_alarm_ptr);
void disable_all_callback(void *room_alarm_ptr);
void enable_all_callback(void *room_alarm_ptr);


/* initialise webserver paths */
_mqx_int hush_one_callback(HTTPD_SESSION_STRUCT *);
_mqx_int set_status_callback(HTTPD_SESSION_STRUCT *);
_mqx_int led_status_json(HTTPD_SESSION_STRUCT *);
_mqx_int alarm_status_json(HTTPD_SESSION_STRUCT *);


static HTTPD_CGI_LINK_STRUCT http_cgi_params[] = {
  { "set_status", set_status_callback},
	{ "led_status", led_status_json },
  { "alarm_status", alarm_status_json },
	{ 0, 0 }
};

void Main_task(uint_32 initial_data) {
	// install filesystem
	_io_tfs_install("tfs:", static_data);

	// initialise the LED driver
	// _bsp_btnled_init() must be called before rtcs_init
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
	// run server
	httpd_server_run(http_server);
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

void init_room_alarms() {
	int i; //iterator
	// set values of buttons and leds
	for (i = 0; i < N_ROOMS; ++i) {
		room_alarms[i].led = HMI_GET_LED_ID(i+1);
		room_alarms[i].button = HMI_GET_BUTTON_ID(i+1);
		room_alarms[i].status = ENABLED;
		//attach alarm trigger callbacks
		btnled_add_clb(hmi,
			room_alarms[i].button,
			HMI_VALUE_PUSH,
			trigger_alarm_callback, (void*) &room_alarms[i]);
	}
}

void init_pushbuttons() {
	btnled_add_clb(hmi, HMI_BUTTON_5, HMI_VALUE_RELEASE, enable_all_callback, NULL);
	btnled_add_clb(hmi, HMI_BUTTON_6, HMI_VALUE_RELEASE, disable_all_callback, NULL);
}


void trigger_alarm_callback(void *room_alarm_ptr) {
	room_alarm* r = (room_alarm*) room_alarm_ptr; // cast to room_alarm
	if (r->status == ENABLED) 
		r->status = TRIGGERED; // flash if triggered
}

void disable_all_callback(void *room_alarm_ptr) {
	int i;
	// mutex lock
	for (i = 0; i < N_ROOMS; ++i) {
    room_alarms[i].status = DISABLED;
	}
	//mutex unlock
}

void enable_all_callback(void *room_alarm_ptr) {
	int i;
	// mutex lock
	for (i = 0; i < N_ROOMS; ++i) { //enable all alarms
		if(room_alarms[i].status != TRIGGERED) 
			room_alarms[i].status = ENABLED;
	}
	// mutex unlock
}

void led_update(uint_32 toggle_state) {
	int i;
	//mutex lock
	for (i = 0; i < N_ROOMS; ++i) {
		switch(room_alarms[i].status)
		if (room_alarms[i].status == TRIGGERED) {
			btnled_set_value(hmi, room_alarms[i].led, toggle_state);
		}
		else if(room_alarms[i].status == ENABLED) {
			btnled_set_value(hmi, room_alarms[i].led, HMI_VALUE_ON);
		}
		else {
			btnled_set_value(hmi, room_alarms[i].led, HMI_VALUE_OFF);
		}
	}
	// mutex unlock
}

// _mqx_int led_callback(HTTPD_SESSION_STRUCT *session) {
// 	int led = atoi(session->request.urldata);
// 	httpd_sendstr(session->sock, "<html><body>LED toggled</body><html>");
// 	btnled_toogle(hmi, HMI_GET_LED_ID(led));
// 	return session->request.content_len;
// }

// _mqx_int rtc_get_callback(HTTPD_SESSION_STRUCT *session) {
// 	RTC_TIME_STRUCT curr_time;
// 	unsigned int hours, minutes, seconds;
// 	char buffer[BUFFER_LENGTH];

// 	_rtc_get_time(&curr_time);
// 	seconds = curr_time.seconds % 60;
// 	minutes = (curr_time.seconds / 60) % 60;
// 	hours = curr_time.seconds / 3600;
// 	snprintf(buffer, BUFFER_LENGTH, "%02u:%02u:%02u\n", hours, minutes, seconds);
// 	httpd_sendstr(session->sock, buffer);
// 	return session->request.content_len;
// }

// _mqx_int rtc_set_callback(HTTPD_SESSION_STRUCT *session) {
// 	unsigned int hours, minutes, seconds;
// 	RTC_TIME_STRUCT new_time;
// 	char buffer[BUFFER_LENGTH];

// 	sscanf(session->request.urldata, "%02u:%02u:%02u\n", &hours, &minutes, &seconds);
// 	new_time.seconds = 3600 * hours + 60 * minutes + seconds;
// 	_rtc_set_time(&new_time);

// 	snprintf(buffer, BUFFER_LENGTH, "Time set to %02u:%02u:%02u\n", hours, minutes, seconds);
// 	httpd_sendstr(session->sock, buffer);
// 	return session->request.content_len;
// }
/* EOF */

_mqx_int hush_one_callback(HTTPD_SESSION_STRUCT *session) {

}

_mqx_int set_status_callback(HTTPD_SESSION_STRUCT *session) {
  char buffer[32];
  int num, enabled;
  sscanf(session->request.urldata, "room=%u&enabled=%u", &num, &enabled);
  room_alarms[num].status = (status_type) enabled;
  sprintf(buffer, "{ \"status\" : %u }", enabled);
  httpd_sendstr(session->sock, buffer);
  return session->request.content_len;
}

_mqx_int led_status_json(HTTPD_SESSION_STRUCT *session) {
	char buffer[BUFFER_LENGTH];
	unsigned char i;
	uint_32 status[4];
	for (i = 0; i < 4; ++i) {
		btnled_get_value(hmi, HMI_GET_LED_ID(i+1), &status[i]);
	}
	snprintf(buffer, BUFFER_LENGTH,
		"{ \"leds\": [%u, %u, %u, %u] }\n\0", !status[0], !status[1], !status[2], !status[3]);

	httpd_sendstr(session->sock, buffer);
	return session->request.content_len;
}


_mqx_int alarm_status_json(HTTPD_SESSION_STRUCT *session) {
	char buffer[512];
  sprintf(buffer, (const char*) status_json,
          room_alarms[0].status,  room_alarms[0].scheduled_time,
          room_alarms[1].status, room_alarms[1].scheduled_time,
          room_alarms[2].status, room_alarms[2].scheduled_time,
          room_alarms[3].status, room_alarms[3].scheduled_time);
  httpd_sendstr(session->sock, buffer);
  return session->request.content_len;
}
