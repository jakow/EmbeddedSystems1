//generate a range of numbers
function* range(start, end, increment) {
	if (typeof increment === 'undefined') 
		increment = 1;
	var value = start;
	function done() {
		if (increment > 0)
			return value > end;
		else if (increment < 0)
			return value < end;
		else
			return true;
	}
	while(!done())
		yield value;
		value += increment;

}