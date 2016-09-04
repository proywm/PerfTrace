Perf-trace is a system-wide tracing tool. It traces samples of programs running in the system and visualizes the behavior of each CPU.

Build:
	make
There will be two files generated: read_sample and hpcconvert in the same directory.

Usage:
	sudo perf record -a -g
	perf script > data
	read_sample data
	hpcconvert hpctoolkit-syswide-measurement
There will generate a directory called hpctoolkit-syswide-database
	hpctraceviewer hpctoolkit-syswide-database
hpctraceviewer can be downloaded from hpctoolkit.org.
