# Writes "<windows><mirrors>" (or T on timeout) and the screen to $MAPPERTEST_OUT.

set renderer none
set throttle off

proc mappertest_finish {code} {
	set f [open $::env(MAPPERTEST_OUT) w]
	puts $f $code
	if {[catch {puts $f [get_screen]} err]} {
		puts $f "(no screen: $err)"
	}
	close $f
	exit
}

proc mappertest_poll {deadline} {
	# RAM is garbage until the ROM runs. C002h is written last.
	set w [format %c [peek 0xC000]]
	set m [format %c [peek 0xC002]]
	if {$w in {P F} && $m in {P F -}} {
		mappertest_finish $w$m
	} elseif {[machine_info time] > $deadline} {
		mappertest_finish T
	} else {
		after time 0.5 [list mappertest_poll $deadline]
	}
}

after time 3 [list mappertest_poll [expr {[machine_info time] + 60}]]
