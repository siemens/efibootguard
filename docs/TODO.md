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

* API refactoring
	* Function / Datatype / Variable names remind of Parted and should be
	  renamed if code developes independent of libparted.

