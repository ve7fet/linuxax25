## Introduction ##

This page will explain the process of building RPM packages of the code in this project.

The source tarballs contain a compiled .spec file that should enable you to re-build an RPM without much effort.

If you download a copy if the svn trunk source code, it includes a .spec.in file that will get compiled in to a .spec file after you run ./configure for the first time on the source.

The method prescribed below was carried out on a clean install of CentOS 6.


## Preparation ##

In order to re-build the RPM's, you will need an environment to do so. This shell script will help you set that environment up in your home directory.

```
#!/bin/bash

RPMBUILD=`pwd`/rpmbuild
RPMMACROS="$HOME/.rpmmacros"

if [ -f $RPMMACROS ] ; then
  cp $RPMMACROS $RPMMACROS~
fi

echo "%_topdir  $RPMBUILD" > $RPMMACROS
echo "%_tmppath $RPMBUILD/tmp" >> $RPMMACROS
echo "%_rpmdir $RPMBUILD/RPMS" >> $RPMMACROS
echo "%mkrel(c:) %(echo mdv20010)" >> $RPMMACROS

mkdir -p rpmbuild/{SOURCES,SRPMS,BUILD,RPMS,SPECS,tmp}
```

The script above will create an rpmbuild folder under wherever you run it (along with all the rest of the folders you need to build an RPM).

Now you can go ahead and download the tarballs you want in rpmbuild/SOURCES.

Extract the tarball(s) and move the .spec file from inside them in to rpmbuild/SPECS.

Delete the extracted tarball folders, rpmbuild will extract them elsewhere when you to the build.


### Dependencies ###

In order to build the RPM's from scratch, you will need to make sure you have installed the following packages:

```
make
gcc 
gcc-c++ 
kernel-devel
rpm-build
glibc
zlib-devel
ncurses-devel
```

If you don't you will get complaints from rpmbuild, and it will halt.


### Building RPM's ###

So now you should be ready to build the RPM's.

Go in to the rpmbuild/SPECS folder and run rpmbuild on the spec file you want. For example:

```
rpmbuild -bb libax25.spec
```

The above command will build the binary packages libax25 and libax25-devel. The process should complete clean if you have the dependencies installed.

You should find the completed RPM's in rpmbuild/RPMS/i386 all ready to install.


### Installing ###

To install your packages, you should be able to go in to the rpmbuild/RPMS/i386 folder and run rpm -U on the .rpm you want.

NOTE: If you are installing the libax25-devel package, you will have to force it:

```
rpm -U --force libax25-devel-1.0.5-140.i686.rpm
```

Why you ask? Well, it is because the glibc package already contains

```
/usr/include/netax25/ax25.h
/usr/include/netrom/netrom.h
/usr/include/netrose/netrose.h
```

From an unknown source. Who knows the last time those were updated, so the ones you are installing should be the preferred ones.


### RPM's from SVN Source ###

If you download the source directly from the SVN repository, you will need to build the .spec file before you can build the RPM.

In order to do this, you need to run ./configure on the source after you extract it somewhere. You don't need any options with ./configure, just run it wherever you checked out the source to.

That will build a .spec file you can use.

Alternatively, you can run ./configure && make && make distcheck which will build you a gzipped tarball you can put in rpmbuild/SOURCES as well as build you the .spec file you can put in rpmbuild/SPECS. This is a good test to see if you have all the dependencies installed too, since ./configure will fail if you are missing something (well, it should).

Make sure you have the dependencies installed that are noted above before you try building your own tarball.

### Links ###

If you want to read all about RPM's, here's a good reference:

[Fedora RPM Guide](http://docs.fedoraproject.org/en-US/Fedora_Draft_Documentation/0.1/html/RPM_Guide/)