

function AlarmControl(parentEl, id) {
	// get component elements
	this.id = id;
	this.parent = parentEl;
	this.enableEl = this.parent.querySelectorAll('.onoffswitch-checkbox')[0];
	this.statusEl = this.parent.querySelectorAll('.room--status')[0];
	this.enableEl.checked = false;
	// this.setEnabledState(true);
	this.enableEl.onclick = () => {
		this.setEnabledState(this.enableEl.checked);
	}
	this.status = 'enabled';
}


AlarmControl.prototype.setEnabledState = function(enabled) {
	if (enabled) {
		this.status = 'enabled';
		this.statusEl.classList.remove('disabled');
		this.statusEl.classList.add('enabled');
		this.statusEl.innerHTML = 'Enabled';
		// if alarm, add alarm class
	}
	else {
		this.statusEl.classList.remove('enabled');
		this.statusEl.classList.add('disabled');
		this.statusEl.innerHTML = 'Disabled';
	}
}

AlarmControl.prototype.updateState = function(enabled, triggered) {
	this.setEnabledState(enabled);
};


function Time(hrOrString, min, sec) {
	var hr;
	if (typeof hrOrString === "undefined") {
		hr = 0;
		min = 0;
		sec = 0;
	}
	if (typeof hrOrString === "string") {
		var tokens = hrOrString.split(':').map(x => parseInt(x));
		console.log(tokens);
		// 
		hr = tokens[0]; 
		min = tokens[1]; 
		sec = tokens[2];
		// probably should do some error checking but...
	}
	this.hr = hrOrString;
	this.min = min;
	this.sec = sec;
}

Time.prototype.toString = function() {
	return this.hr + ":" + this.min + ":" + this.sec;
}

function updateTime(time) {
	systemTimeEl.innerHTML = time.toString();
}



function init() {
	var elems = Array.prototype.slice.call(document.querySelectorAll('.room'));
	var alarms = elems.map((elem, idx) => new AlarmControl(elem, idx));

	var systemTimeEl = document.getElementById('system-time-module--time-container');


	/* fetch system status */
	var headers = new Headers();
	// headers.append('Content-Type', '')
	fetch("/status.cgi").then((response) => {
		console.log(response);
	})
}

init();
