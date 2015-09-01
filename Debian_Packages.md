# Introduction #

This page will explain the process of building .deb packages of the code in this project.

The source tarballs in the -debian branch contain the necessary /debian folder with the configuration files that should enable you to re-build an .deb without much effort.

The method prescribed below was carried out on a clean install of Debian 7.7.0 (Wheezy).


---


# Preparation #

## Dependencies ##

In order to build the .deb's from scratch, you will need to make sure you have installed the following packages:

```
$ apt-get update

$ apt-get install build-essential autotools-dev libtool autoconf2.13 \
gnu-standards automake zlib1g-dev dpkg-dev debhelper devscripts \
fakeroot lintian dh-make subversion git libncurses5-dev
```

## Get the Source ##

Now, as a normal user, go grab the source. We don't want to bork our system, so build the packages as a normal user...

The source in the -debian branch for each package is NOT automatically kept in sync with the trunk. You will likely want to do an svn merge to make sure you pick up all the patches from the trunk.

```
$ svn co http://linuxax25.googlecode.com/svn/branches/libax25-debian

$ cd linuxax25-debian

$ svn merge http://linuxax25.googlecode.com/svn/trunk/libax25
```

That should get all the changes from the trunk and merge them in to your -debian branch.


---


# Building .deb's #

Now, we should be able to build the package.

```
$ ./autogen.sh

$ debuild -i -us -uc -b
```

That should build the .deb (two for libax25) in the next level folder up. They are un-signed, but this is all experimental anyways, so that doesn't really matter.


---


# Installing #

Before you try building ax25tools and ax25apps, you will need to INSTALL the libax25 and libax25-dev that you just created (they are required to build the subsequent packages).

```
$ dpkg -i libax25_1.0.5_i386.deb 

$ dpkg -i libax25-dev_1.0.5_i386.deb
```

Now, you can go back and repeat the same process for ax25apps and ax25tools.