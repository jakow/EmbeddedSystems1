#!/usr/bin/python
out_lines = []
with open('index.html', 'r') as in_file:
	for line in in_file:
		l = line.replace('\\', '\\\\')
		l = l.replace('"', '\\"')
		l = l.replace('\n', '')

		if l: # remove empty lines
			l = '"' + l + '\\n"\n'
			out_lines.append(l)


with open('webpage.h', 'w') as out_file:
	out_file.write("unsigned char WEBPAGE[] = ")
	for line in out_lines:
		out_file.write(line)
	out_file.write(";")
