var CHECKING = -1;
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
	if (state == 0) {
		return;
	}
	if (enabled) {
		this.status = 'enabled';
		this.statusEl.classList.remove('disabled');
		this.statusEl.classList.add('enabled');
		this.statusEl.innerHTML = 'Enabled';
	}
}


AlarmControl.prototype.pushStatus = function(enabled) {
	var oldStatus = this.status;
	this.setStatus(enabled); // set status eagerly
	var url = "/set_status.cgi?room="+ this.id+ "&status=" + (enabled ? "1" : "0");
	fetch(url)
	.then(function(resp) {
		if (resp.status >= 400)
			this.setStatus(oldStatus); // if failed roll back
	}.bind(this))
	.catch(function(err) {
		this.setStatus(oldStatus);
	}.bind(this));
}
AlarmControl.prototype.updateStatus = function(status) {
	if (status == TRIGGERED)
	this.setEnabledState(enabled);
};

function updateTime(time) {
	systemTimeEl.innerHTML = time.toString();
}


function updateStatus(alarms, statusArr) {
	alarms.forEach(function(item, idx) {
		item.setStatus(statusArr[idx].status);
		// item.setScheduledTime(statusArr[idx].sched_time);
	});
}

function fetchStatus() {
	/* fetch system status */
	fetch("/alarm_status.cgi").then(function(response) {
		return response.json();
	})
	.then(function(json) {
		updateStatus(alarms, json.alarms);
	})
	.catch(function(err) {
		console.log(err);
	});
}


function init() {
	var elems = Array.prototype.slice.call(document.querySelectorAll('.room'));
	var alarms = elems.map(function(elem, id) {new AlarmControl(elem, id) });
	var systemTimeEl = document.getElementById('system-time-module--time-container');

	fetchStatus();
}

init();
