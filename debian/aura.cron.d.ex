#
# Regular cron jobs for the aura package
#
0 4	* * *	root	[ -x /usr/bin/aura_maintenance ] && /usr/bin/aura_maintenance
