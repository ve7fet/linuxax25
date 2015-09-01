This is NOT the official home of the Linux AX.25 software suite (libax25, ax25-apps, and ax25-tools).

The official home is hosted by the Linux Ham group at http://www.linux-ax25.org/wiki/Main_Page

That is where official development should take place.

THIS project is a personal one to customize the official source and re-package it.

It is based on the GIT source from the official repository. No guarantees on it being kept current, it will be done ad-hoc when changes in the official source are noted.

The goal of this project is to get the GIT source from the official repository in to pre-packaged binaries, along with some changes that have been made in order to enhance the layout of the code base and keep it current with recent GNU Autotools.

Versioning of these packages will go forward from where the current GIT version was when this project was created. These will not likely be in synchronization with the official source.

If you decide to use this code or packages, you do so at your own risk.

The official developers are welcome to use any or all changes that have been done, as they see fit.

Credits and copyrights by the official owners apply.

---


**NOTE**

There is a notable difference between the libax25 package found here, and the GIT source at linux-ax25.org.

In the linux-ax25.org source, the ax25\_ntoa function in axutils.c was changed so that it mangles callsigns. Specifically, if the callsign has a SSID of -0, it strips it.

This is bad form, and the functions of the library should merely convert the callsign as provided, not changing it in any way.

This (unannounced) change broke other software that depended on the callsign being converted as provided, without being mangled. It could even be considered as violating the AX.25 specification.

We've reverted that bad patch to axutils.c, starting at libax25 version 1.0.2.

Note also that this bad patch of axutils.c only (currently) exists in the CVS source at linux-ax25.org, the "official" released version does not contain it (yet).

So, if you get some strange things happening with your callsigns with -0 SSID's... you know where to look.

**NOTE: It appears (as of Nov 2014), that this bad behaviour of the Official Source may have been reverted. Patches have been noted in the Official Source that seem to get rid of stripping the SSID-0.**

Look in the [Wiki](https://code.google.com/p/linuxax25/w/list) to see a summary of all the diffs between the two code sources.


---


RPM's are available.

The x86 RPMs are built on CentOS 6.6. The x64 RPMs are built on CentOS 7.0.1406. See [About RPM's](About_RPMs.md) and [RPM Packages](RPM_Packages.md) for more information.

Note, if you want to install the libax25-devel package, you will need to "force" the install (rpm -ivh --force), since the header files provided in it are also provided by GlibC. This may break yum updating GlibC in the future. Typically you won't need to install the libax25-devel package, unless you plan on re-compling any of the packages.


---


Debian packages are available.

They are built on Debian 7.7.0 (Wheezy) with kernel 3.2.0-4-686-pae.


---


Raspberry Pi packages are available.

They are built on Raspbian Wheezy 2014-09-09.


---


Source tarballs were created on Debian 7.7.0.


---


# Downloads #

Looks like Google decided to break the project downloads in this application. They now want you to use Google Drive and share a folder, instead of being able to hold them with the project and label them as necessary.

We've put the downloads in a download folder in the source tree. Use the Source link above and Browse to the downloads folder to see what is available. Or, check below for the links you need.

## Source Tarballs ##
[libax25-1.0.5.tar.bz2](https://linuxax25.googlecode.com/svn/downloads/libax25-1.0.5.tar.bz2)
[libax25-1.0.5.tar.gz](https://linuxax25.googlecode.com/svn/downloads/libax25-1.0.5.tar.gz)

[ax25apps-1.0.5.tar.bz2](https://linuxax25.googlecode.com/svn/downloads/ax25apps-1.0.5.tar.bz2)
[ax25apps-1.0.5.tar.gz](https://linuxax25.googlecode.com/svn/downloads/ax25apps-1.0.5.tar.gz)

[ax25tools-1.0.3.tar.bz2](https://linuxax25.googlecode.com/svn/downloads/ax25tools-1.0.3.tar.bz2)
[ax25tools-1.0.3.tar.gz](https://linuxax25.googlecode.com/svn/downloads/ax25tools-1.0.3.tar.gz)

## Debian x86 Packages ##
[libax25\_1.0.5\_i386.deb](https://linuxax25.googlecode.com/svn/downloads/libax25_1.0.5_i386.deb)
[libax25-dev\_1.0.5\_i386.deb](https://linuxax25.googlecode.com/svn/downloads/libax25-dev_1.0.5_i386.deb) (You only need the -dev package if you are going to be compiling other programs that depend on libax25)


[ax25apps\_1.0.5\_i386.deb](https://linuxax25.googlecode.com/svn/downloads/ax25apps_1.0.5_i386.deb)

[ax25tools\_1.0.3\_i386.deb](https://linuxax25.googlecode.com/svn/downloads/ax25tools_1.0.3_i386.deb)

## CentOS x86 and x64 Packages ##
[libax25-1.0.5-140.i386.rpm](https://linuxax25.googlecode.com/svn/downloads/libax25-1.0.5-140.i386.rpm)
[libax25-1.0.5-140.x86\_64.rpm](https://linuxax25.googlecode.com/svn/downloads/libax25-1.0.5-140.x86_64.rpm)

[libax25-devel-1.0.5-140.i386.rpm](https://linuxax25.googlecode.com/svn/downloads/libax25-devel-1.0.5-140.i386.rpm)
[libax25-devel-1.0.5-140.x86\_64.rpm](https://linuxax25.googlecode.com/svn/downloads/libax25-devel-1.0.5-140.x86_64.rpm)

[ax25apps-1.0.5-165.i386.rpm](https://linuxax25.googlecode.com/svn/downloads/ax25apps-1.0.5-165.i386.rpm)

[ax25tools-1.0.3-140.i386.rpm](https://linuxax25.googlecode.com/svn/downloads/ax25tools-1.0.3-140.i386.rpm)
[ax25tools-1.0.3-140.x86\_64.rpm](https://linuxax25.googlecode.com/svn/downloads/ax25tools-1.0.3-140.x86_64.rpm)

## Raspberry Pi (ARM) Packages ##

[libax25\_1.0.5\_armhf.deb](https://linuxax25.googlecode.com/svn/downloads/libax25_1.0.5_armhf.deb)

[libax25-dev\_1.0.5\_armhf.deb](https://linuxax25.googlecode.com/svn/downloads/libax25-dev_1.0.5_armhf.deb) (You likely don't need this, unless you are going to recompile something.)

[ax25apps\_1.0.5\_armhf.deb](https://linuxax25.googlecode.com/svn/downloads/ax25apps_1.0.5_armhf.deb)

[ax25tools\_1.0.3\_armhf.deb](https://linuxax25.googlecode.com/svn/downloads/ax25tools_1.0.3_armhf.deb)
