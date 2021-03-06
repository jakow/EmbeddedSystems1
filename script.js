const CHECKING = -2;
const ERROR = -1;
const DISABLED = 0;
const ENABLED = 1;
const TRIGGERED = 2;
const SECONDS_PER_DAY = 24* 60 * 60;
/* HELPER FUNCTINONS */
function arrayQuerySelector(domNode, selector) { return Array.prototype.slice.call(domNode.querySelectorAll(selector)); }

function timeString(time) {
	var hr, min, sec;
	hr = Math.floor(time / 3600);
	min = Math.floor(time % 3600/60);
	sec = time % 60;
	hrStr = hr < 10 ? "0" + hr : "" + hr;
	minStr = min < 10 ? "0" + min : "" + min;
	secStr = sec < 10 ? "0" + sec : "" + sec;
	return hrStr + ":" + minStr + ":" + secStr;
}

/* CLASS DEFINITIONS */
function AlarmControlComponent(parentEl, id) {
	// get component elements
	this.id = id;
	this.schTime = 0;
	this.status = CHECKING;
	this.changingStateFlag = false;
	this.changingScheduleFlag = false;

	this.enableButton = arrayQuerySelector(parentEl, '.onoffswitch-checkbox')[0];
	this.hushButton = arrayQuerySelector(parentEl, '.hush.button')[0];
	this.changeScheduleButton = arrayQuerySelector(parentEl, '.change-schedule')[0];
	this.statusEl = arrayQuerySelector(parentEl, '.room--status')[0];
	var timers = arrayQuerySelector(parentEl, '.timer-component');
	this.timerStart = new TimerComponent(timers[0]);
	this.timerEnd = new TimerComponent(timers[1]);

	this.initialiseClickEvents();

}

AlarmControlComponent.prototype.initialiseClickEvents = function() {
	this.enableButton.onclick = () => this.pushStatus(this.enableButton.checked);
//need to bind the method to the AlarmControlComponent instance

	this.hushButton.onclick = () => this.pushStatus(ENABLED);

	this.changeScheduleButton.onclick = ()  => {
		this.changingScheduleFlag = !this.changingScheduleFlag;
		this.changeScheduleButton.classList.toggle('set');
		this.timerStart.setButtonsVisibilityState(this.changingScheduleFlag);
		this.timerEnd.setButtonsVisibilityState(this.changingScheduleFlag);
		if (!this.changingScheduleFlag) { // done changing
			this.pushScheduledTime(this.timerStart.getTime(), this.timerEnd.getTime());
		}
	};
};


AlarmControlComponent.prototype.setStatus = function(status) {
	var STATUS_STRINGS = ['error', 'enabled', 'disabled', 'alarm'];
	var statusString;
	var statusName;
	switch (status) {
		case ERROR:
			statusString = 'ERROR'
			statusName = 'error'
			break;
		case ENABLED:
			statusString = 'Enabled';
			statusName = 'enabled';
			break;
		case DISABLED:
			statusString = 'Disabled';
			statusName = 'disabled';
			break;
		case TRIGGERED:
			statusString = 'ALARM!';
			statusName = 'alarm';
			break;
		default:
			return // invalid state
	}
	this.status = status;
	this.enableButton.checked = status >= ENABLED;
	this.statusEl.classList.remove.apply(this.statusEl.classList, STATUS_STRINGS);
	this.statusEl.classList.add(statusName);
	this.statusEl.innerHTML = statusString;
	this.hushButton.disabled = status != TRIGGERED;
};

AlarmControlComponent.prototype.pushStatus = function(enabled) {
	this.changingStateFlag = true;
	this.setStatus(enabled); // set status eagerly for fast UI
	// this.setStatus(enabled ? ENABLED : DISABLED); // set status eagerly
	var url = "/set_status.cgi?room="+ this.id+ "&status=" + (enabled ? "1" : "0");
	fetch(url)
	.then(response => {
		if (response.ok)
			// do some error validation as well?
			this.setStatus(enabled);	
		else
			this.setStatus(ERROR);
		this.changingStateFlag = false;
	})
	.catch(err => {
		this.setStatus(ERROR);
		this.changingStateFlag = false;
	});

}

AlarmControlComponent.prototype.pushScheduledTime = function (start, end) {
	var url = 'set_enable_time.cgi?room='+ this.id + "&start=" + start + "&end=" + end;
	var handleError = () => {
		this.setStatus(ERROR);
		this.timerStart.clearTime();
		this.timerEnd.clearTime();
		alert('Failed to schedule the alarm.');
	}
	console.log(url);
	fetch(url)
	.then(response => {
		if (!response.ok) {
			handleError();
		}
	})
	.catch(handleError);
};

/* 	Timer component 
*	Constructor parameters:
*/
function TimerComponent(parentEl, initTimeSecs) {
	if (parentEl.nodeType != Node.ELEMENT_NODE) 
		return;

	this.timeEl = parentEl.querySelectorAll('time')[0];
	this.incElems = arrayQuerySelector(parentEl, '.incs');
	this.displayTime = initTimeSecs || 0;


	var increments = [3600, 60, 1, -3600, -60, -1];
	var incrementFunction = (incVal) => () => this.increment(incVal);

	this.incButtons = arrayQuerySelector(parentEl, '.incs a')
	.map( (button, idx) => 
		new IncrementButton(button, incrementFunction(increments[idx])));
	this.setButtonsVisibilityState(false);
}

TimerComponent.prototype.setButtonsVisibilityState = function(visible) {
	if (visible)
		this.incElems.forEach(elem => elem.classList.add('show'));
	else
		this.incElems.forEach(elem => elem.classList.remove('show'));
};

TimerComponent.prototype.getTime = function () { return this.displayTime; }

TimerComponent.prototype.clearTime = function() { this.timeEl.innerHTML = '__:__:__';
};

TimerComponent.prototype.timeString = function() { return(timeString(this.displayTime)); }

TimerComponent.prototype.increment = function (number) {
	var time = this.displayTime + number;
	while (time < 0) // wrap back to positive;
		time += SECONDS_PER_DAY; 
	this.setTime( time % SECONDS_PER_DAY);
}

TimerComponent.prototype.setTime = function (time) {
	this.displayTime = time; // should always be less than SECONDS_PER_DAY
	this.timeEl.innerHTML = timeString(time);
}

/* Increment Button with increment speedup functionality */

/*	Constructor parameters:
*	elem - DOM node for the  button
*   incrementFunction - function to be called when user clicks
						or press-and-holds the button
*/

function IncrementButton(elem, incrementFunction) {
	this.elem = elem;
	this.doIncrement = false;
	this.MIN_INTERVAL = 40; // what is the max increment speed.
	this.START_INTERVAL = 400; // how often should it increment at first
	this.interval = this.START_INTERVAL;
	this.count = 0; //how many intervals  have the button been pressed
	this.threshold = 2;
	this.incFun = incrementFunction;
	this.elem.onmousedown = this.startIncrement.bind(this);
	this.elem.onmouseup = this.stopIncrement.bind(this);
	this.elem.onclick = function(ev) {
		ev.preventDefault();
	}
	this.timeout;
}

IncrementButton.prototype.startIncrement = function (event) {
	if (this.doIncrement) return;
	// console.log('Start')
	event.preventDefault();
	this.doIncrement = true;
	this.increment();
}

IncrementButton.prototype.increment = function () {
	if (!this.doIncrement) return;
	this.incFun();
	this.count++;
	this.timeout = setTimeout(this.increment.bind(this), this.interval);
	var numTimes = Math.pow(this.START_INTERVAL/this.interval,3);
	if (this.count >= this.threshold && this.interval > this.MIN_INTERVAL) {
		this.threshold *= 2;
		this.interval = this.interval/2;
	}
}

IncrementButton.prototype.stopIncrement = function (event) {
	// console.log('Stop');
	event.preventDefault();
	clearTimeout(this.timeout);
	this.doIncrement = false;
	this.interval = this.START_INTERVAL;
	this.threshold = 2;
	this.count = 0;

}
/* Class definition end *

/* Helper functions */

//
function updateAlarmStatus(alarms, statusArr) {
	var status;
	alarms.forEach((alarm, idx) => {
		status = statusArr[idx];
		if (!alarm.changingStateFlag) // pushStatus has priority
			alarm.setStatus(status.status);
		if(!alarm.changingScheduleFlag) {
			alarm.timerStart.setTime(status.start_time);
			alarm.timerEnd.setTime(status.end_time)
		}
	});
	if (alarms.some(alarm => alarm.status == TRIGGERED))
		document.body.classList.add('alarm');
	else
		document.body.classList.remove('alarm');
}

// fetch system status - alarm status
function fetchStatus(alarms, systemTimer) {
	/* fetch system status */
	fetch("/alarm_status.cgi")
	.then(function(response) {
		return response.json();
	})
	.then(function(json) {
		if (!systemTimeUpdateFlag)
			// do not change system time if the user
			// is trying to change it
			systemTimer.setTime(json.system_time);
		updateAlarmStatus(alarms, json.alarms);
	})
	.catch(function(err) {
		alarms.forEach(function(alarm) {
			alarm.setStatus(ERROR);
		})
	});
}

// push system time to server
function setSystemTime(time) {
	fetch('set_system_time.cgi?'+time)
	.catch(function(err) {
		// do nothing? or
		alert('Failed to set system time.');
		systemTimer.clearTime();
	});
}

//helper for hush all, enable all, and disable all
function pushStatusChangeAll(alarms, status, url) {
		alarms.forEach((alarm) => {
			alarm.setStatus(status);
			alarm.changingStateFlag = true;
		})
		fetch(url).then(() => {
			alarms.forEach(alarm => alarm.changingStateFlag = false
		)
	})
}

// startup code
var systemTimeUpdateFlag = false;
var systemTimer = new TimerComponent(document.getElementById('system-timer'));
var setSystemTimeButton = document.getElementById('set-system-time');
var enableAllButton = document.getElementById('enable-all-button');
var disableAllButton = document.getElementById('disable-all-button');
var hushAllButton = document.getElementById('hush-all-button');
var alarms = arrayQuerySelector(document, '.room')
	.map(function(elem, id) {
		return new AlarmControlComponent(elem, id) 
	});


// initialise some click handlers
enableAllButton.onclick = function() {
 pushStatusChangeAll(alarms, ENABLED, 'enable_all.cgi')
};
disableAllButton.onclick = function() { 
pushStatusChangeAll(alarms, DISABLED, 'disable_all.cgi');
};

hushAllButton.onclick = function() {
	pushStatusChangeAll(
		alarms.filter(alarm => alarm.status == TRIGGERED), 
		ENABLED, 
		'hush_all.cgi')
};

setSystemTimeButton.onclick = function () {
	systemTimeUpdateFlag = !systemTimeUpdateFlag;
	setSystemTimeButton.classList.toggle('set');
	if (systemTimeUpdateFlag) {
		systemTimer.setButtonsVisibilityState(true);
	} 
	else {
		systemTimer.setButtonsVisibilityState(false);
		setSystemTime(systemTimer.getTime());
	}
};


fetchStatus(alarms, systemTimer);
var interval = setInterval(() => fetchStatus(alarms, systemTimer), 1000)

