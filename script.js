var CHECKING = -2;
var ERROR = -1;
var DISABLED = 0;
var ENABLED = 1;
var TRIGGERED = 2;

function AlarmControl(parentEl, id) {
	// get component elements
	this.id = id;
	this.schTime = 0;
	this.status = CHECKING;

	//
	this.parent = parentEl;
	this.enableButton = this.parent.querySelectorAll('.onoffswitch-checkbox')[0];
	this.statusEl = this.parent.querySelectorAll('.room--status')[0];
	
	this.enableButton.checked = false;
	this.hushButton = this.parent.querySelectorAll('.hush.button')[0];
	this.changeScheduleButton = this.parent.querySelectorAll('.change-schedule.button')[0];

	this.notifyChangedListener = null;
	this.initialiseIncrements();
	// this.setEnabledState(true);
	this.enableButton.onclick = function() {
		this.pushStatus(this.enableButton.checked);
	}.bind(this);
}

AlarmControl.prototype.initialiseIncrements = function() {
	var control = this;
	var increments = [-3600, -60, -1, 3600, 60, 1];
	var elems = Array.prototype.slice.call(this.parent.querySelectorAll('.time-container a'));
	// console.log(elems);
	var callback = function (el, inc, ctrl) {
		
	};

	elems.forEach(function(elem,idx) {

	});
}




AlarmControl.prototype.setStatus = function(status) {
	var STATUS_STRINGS = ['error', 'enabled', 'disabled', 'alarm'];
	var statusString;
	var statusName;
	switch (status) {
		case ERROR:
			statusString = 'ERROR'
			statusName = 'error'
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
	STATUS_STRINGS.forEach(function(str) {
		this.statusEl.classList.remove(str);
	}.bind(this));
	this.statusEl.classList.add(statusName);
	this.statusEl.innerHTML = statusString;
	if (this.notifyChangedListener != null) this.notifyChangedListener();
}



AlarmControl.prototype.pushStatus = function(enabled) {
	this.setStatus(enabled ? ENABLED : DISABLED); // set status eagerly
	var url = "/set_status.cgi?room="+ this.id+ "&status=" + (enabled ? "1" : "0");
	fetch(url)
	.then(function(resp) {
		if (resp.status < 400)
			this.setStatus(enabled);	
		else
			this.setStatus(ERROR); // if failed roll back
	}.bind(this))
	.catch(function(err) {
		this.setStatus(ERROR);
	}.bind(this));
}

function updateTime(time) {
	systemTimeEl.innerHTML = time.toString();
}

function updateStatus(alarms, statusArr) {

	alarms.forEach(function(item, idx) {
		item.setStatus(statusArr[idx].status);
		// item.setScheduledTime(statusArr[idx].sched_time);
	});
	if (alarms.some(function(alarm) {return alarm.status == TRIGGERED}))
		document.body.classList.add('alarm');
	else
		document.body.classList.remove('alarm');
}

function fetchStatus(alarms) {
	/* fetch system status */
	fetch("/alarm_status.cgi").then(function(response) {
		return response.json();
	})
	.then(function(json) {
		updateStatus(alarms, json.alarms);
	})
	.catch(function(err) {
		alarms.forEach(function(alarm) {
			alarm.setStatus(ERROR);
		})
	});
}


function init() {
	var elems = Array.prototype.slice.call(document.querySelectorAll('.room'));
	var alarms = elems.map(function(elem, id) {return new AlarmControl(elem, id) });
	var systemTimeEl = document.getElementById('system-time-module--time-container');
	fetchStatus(alarms);
	var interval = setInterval(function () {
		fetchStatus(alarms)
	}, 500)

}

init();
