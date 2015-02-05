Informations
------------

The e4rat-lite reduces disk access times through physical file reallocation.
It is based on the online defragmentation ioctl EXT4_IOC_MOVE_EXT from the ext4
filesystem, which was introduced in Linux Kernel 2.6.31. Therefore, other
filesystem types or earlier versions of extended filesystems are not supported.

The e4rat-lite is created from a simply idea of implementating the e4rat-preload-lite
on the main e4rat tree, but it ended up showing a number of features that could be
improved, making e4rat-lite a more independent project, with several optimizations
that causes your system to start even faster than standard e4rat. 

e4rat-lite binaries
===================

The e4rat-lite consists of three binaries:

**e4rat-lite-collect:** Gather relevant files by monitoring file accesses during an
application startup. The generated file list is the fundament of the second step.

**e4rat-lite-realloc:** Files are placed physically in a row on disk. The reallocation
of the files' content yields a higher disk transfer rate which accelerates program
start processes.

**e4rat-lite-preload:** Transfers files into memory in parallel to program startup.
Because a file consists of file content and its I-Node information the preloading
process is divided into two steps. First, it reads the I-Nodes' information which
are still spread over the entire filesystem. In the second step, the files' content
is read without causing any disk seeks.

For more information see: e4rat-lite-collect(8), e4rat-lite-realloc(8)
                          e4rat-lite-preload and e4rat-lite.conf(5).


How to use
----------

**1)** Run e4rat-collect as init process through adding following line to Kernel
parameters:

    init=/usr/bin/e4rat-collect

This can also be done by changing the configuration files of your boot
manager (grub, lilo, syslinux, etc).

**2)** Check the configuration file (/etc/e4rat-lite.conf) and change the variable
init_file to the process used in your system. E.g.:

    init_file=/usr/lib/systemd/systemd

or

    init_file=/sbin/init

**3)** Restart your computer to complete the first data collection.

**4)** After the e4rat-lite-collect end it will generate a list of files, which is
written in /var/lib/e4rat-lite/startup.log (You can change the destination path on
configuration file, in /etc/e4rat-lite.conf).

After complete boot, you can end the e4rat-lite-collect running:

    # e4rat-lite-collect -k

**5)** Now you must run the e4rat-lite-realloc to start the relocation process. Is
recommended that you switch to runlevel 1, so you ensure write access to all binary
processes (See e4rat-lite-realloc(8) for more information).

If you are using a boot system like System V, you can use the following command to
enter runlevel 1:

    # telinit 1

If you are using a boot system like systemd, you can switch to rescue mode:

    # systemctl isolate rescue.target

So do the relocation using the command:

    # e4rat-lite-realloc

By default the e4rat-lite-realloc search for the boot log file in
/var/lib/e4rat-lite/startup.log. If you use another location, you can
indicate the path as a parameter.

**6)** At end of this process, the kernel parameter should be changed again to load the
e4rat-lite-preload, which will in fact speed up the boot of your system:

    init=/usr/bin/e4rat-lite-preload

That's all

You must make a new collection of files after much modification in programs installed, such
as updates and/or boot related files such as libraries and like. The time for this varies
depending on the frequency of changes that you performs on your disk. Keep this in mind.

DEPENDENCIES
-----------

The e4rat-lite toolset has the following external dependencies:

 - Linux Kernel (>= 2.6.31)
 - CMake (>= 2.6)
 - Boost Library (>=1.41)
      [You need the following components: system, filesytem, regex, signals2]
 - Linux Audit Library (libaudit >=0.1.7)
 - Ext2 File System Utilities (e2fsprogs)
 - Gettext (>=0.18)

BUILDING
--------

The build system is based on CMake, which will generate a Makefile.

To build the release version of e4rat-lite run the following commands:

    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=release
    $ make

For install:

    # make install

AUTHORS
-------

e4rat has been developed by Andreas Rid <conso@hs-augsburg.de> under the guidance
of Gundolf Kiefer <gundolf.kiefer@hs-augsburg.de> at the University of Applied Sciences, Augsburg.

e4rat-lite has been developed by Lara Maia <lara@craft.net.br>.

There can be external authors for some files. In this situation it will be indicated in the file head.

e4rat-lite is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

e4rat-lite is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Lara Maia - Lara Maia <lara@craft.net.br>
