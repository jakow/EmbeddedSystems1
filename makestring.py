#!/usr/bin/python


def make_string(fname):
	out = []
	with open(fname, 'r') as in_file:
		for line in in_file:
			l = line.replace('\\', '\\\\')
			l = l.replace('"', '\\"')
			l = l.replace('\n', '')

			if l: # remove empty lines
				l = '"' + l + '\\n"\n'
				out.append(l)
	return out

def write_string(file, varname, content):
	out_file.write("unsigned char {}[] = ".format(varname))
	for line in content:
		out_file.write(line)
	out_file.write(";\n")

webpage = make_string('index.html')
style = make_string('style.css')
script = make_string('script.js')



with open('webpage.h', 'w') as out_file:
	write_string(out_file, "WEBPAGE", webpage)
	write_string(out_file, "STYLE", style)
	write_string(out_file, "SCRIPT", script)


