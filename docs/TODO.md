# The following items will be implemented #


* Tools modification
	* Make `bg_setenv -c` and the underlying confirm mechanism to backup
	  the current working environment to the (latest-1) environment, so
	  that if the current environment breaks, there is a backup with the
	  latest values.

* Application specific variables
	* applications may need to store their own variables into the
	  bootloader environment. Currently this is not possible. A generic
	  method must be defined and implemented to account for generic
	  key-value pairs.

* State refactoring
	* Currently, there are three variables 'revision', 'testing',
	  'boot_once', where the latter two are mapped onto a variable called
	  'ustate'. The 'ustate' variable in turn equals an enum type variable
	  within swupdate, so that for the swupdate adapter, a complex mapping
	  must be implemented.  To resolve this issue, the two variables
	  'boot_once' and 'testing' will be unified to the 'ustate' variable,
	  which will have the same enum type as used in 'swupdate'.

* API refactoring
	* Currently, there are two APIs, a lower API 'bg_utils.c', and an
	  adapter-API 'ebgenv.c'. After refactoring the state variable, the API
	  will be simplified as well.  It is possible, that only one API is
	  needed then.
	* Function / Datatype / Variable names remind of Parted and should be
	  renamed if code developes independent of libparted.
	* The number of valid config partitions expected by the bootloader and
	  the tools is currently fixed to the number defined by
	  `CONFIG_PARTITION_COUNT` in `include/envdata.h`. This value should be
	  made configurable by a config flag.

