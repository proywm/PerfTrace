Perf-trace is a system-wide tracing tool. It traces samples of programs running in the system and visualizes the behavior of each CPU.<br />

Build:<br />
	make<br />
There will be two files generated: read_sample and hpcconvert in the same directory.<br />

Usage:<br />
	sudo perf record -a -g<br />
	perf script > data<br />
	read_sample data<br />
	hpcconvert hpctoolkit-syswide-measurement<br />
There will generate a directory called hpctoolkit-syswide-database<br />
	hpctraceviewer hpctoolkit-syswide-database<br />
hpctraceviewer can be downloaded from hpctoolkit.org.
