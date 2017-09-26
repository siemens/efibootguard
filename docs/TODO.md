# The following items will be implemented #


* Tools modification
	* Make `bg_setenv -c` and the underlying confirm mechanism to backup
	  the current working environment to the (latest-1) environment, so
	  that if the current environment breaks, there is a backup with the
	  latest values.
	* Currently, `bg_setenv` generates a virtual environment copy while
	  parsing arguments and later uses an algorithm to find out, how to
	  merge this with the actual environment being modified. This led to
	  the introduction of a special marker byte which tells the algorithm
	  to not touch the original content. Deletion of user variables led to
	  another special case, where *negative* variables had to be defined by
	  a special `DELETED` type to tell the algorithm that the specific user
	  variable has to be deleted. This is rather complicated and a better
	  aproach has already been discussed using a journal with actions
	  instead of a prebuilt state.

* API refactoring
	* Function / Datatype / Variable names remind of Parted and should be
	  renamed if code developes independent of libparted.

