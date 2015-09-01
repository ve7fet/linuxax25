# Introduction #

This page will explain the process of building .deb packages of the code in this project to be used on a Raspberry Pi (ARM architecture).

The source tarballs in the -debian branch contain the necessary /debian folder with the configuration files that should enable you to re-build an .deb without much effort. These are the same files you would use on a conventional Debian system.

The method prescribed below was carried out on a clean install of Debian 7.7.0 (Wheezy) with a chroot environment running Raspbian Wheezy 2014-09-09.

A chroot environment was used so that a Pi environment could be simulated to compile in, without having to compile on the Pi itself.


---


# Preparation #

## Build a chroot Environment ##

We found a useful post on building a Pi environment [here](http://www.raspberrypi.org/forums/viewtopic.php?f=2&t=3812). The steps will be repeated that we used and found that worked.

Preparing the environment will be done as root.

Head over to [http://downloads.raspberrypi.org/](http://downloads.raspberrypi.org/) and browse until you find the OS image you want. We chose [2014-09-09-wheezy-raspbian.zip](http://downloads.raspberrypi.org/raspbian/images/raspbian-2014-09-12/2014-09-09-wheezy-raspbian.zip)

```
# wget http://downloads.raspberrypi.org/raspbian/images/raspbian-2014-09-12/2014-09-09-wheezy-raspbian.zip

# unzip ./2014-09-09-wheezy-raspbian.zip
```

You should now have an .img file of the OS unzipped and sitting there.

Now we need to grab some packages for our host system.

```
# apt-get install kpartx qemu binfmt-support qemu-user qemu-user-static
```

Now we are going to mount the Raspbian .img on our host system.

```
# kpartx -a ./2014-09-09-wheezy-raspbian.img

# mount /dev/mapper/loop0p2 /mnt

# cp /usr/bin/qemu-arm-static /mnt/usr/bin

# cp /etc/resolv.conf /mnt/etc/resolv.conf

# mount -o bind /dev /mnt/dev

# mount -o bind /proc /mnt/proc

# mount -o bind /sys /mnt/sys

# mount -o bind /dev/pts /mnt/dev/pts
```

Now that our chroot environment is set up, we can enter it.

```
# chroot /mnt
```

You should now be inside the chroot environment (as root), and you should still have working access to the internet (since we copied resolv.conf over from outside).

To get out of the chroot and kill it, you'll need to do something like this when you are done.

```
# exit

# umount /mnt/dev/pts

# umount /mnt/dev

# umount /mnt/proc

# umount /mnt/sys

# umount /mnt

# kpartx -d ./2014-09-09-wheezy-raspbian.img
```

A couple other housekeeping things to do once you get inside the chroot...

You need to edit /etc/ld.so.preload and comment out the first line that is in there. If you don't qemu is going to throw errors.

```
# nano /etc/ld.so.preload
     #/usr/lib/arm-linux-gnueabihf/libcofi_rpi.so
```

Locales don't seem to be set up properly. You probably want to fix that and pick one for your locale.

```
# dpkg-reconfigure locales
```

We also need to create a user to build the packages under.

```
# adduser ve7fet
```


## Dependencies ##

In order to build the .deb's from scratch, you will need to make sure you have installed the following packages (inside the chroot):

```
# apt-get update

# apt-get upgrade

# apt-get install build-essential autotools-dev libtool autoconf2.13 \
gnu-standards automake zlib1g-dev dpkg-dev debhelper devscripts \
fakeroot lintian dh-make subversion git libncurses5-dev
```

## Get the Source ##

Now, as a normal user, go grab the source. We don't want to bork our system, so build the packages as a normal user...

```
# su ve7fet
```

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
$ dpkg -i libax25_1.0.5_armhf.deb 

$ dpkg -i libax25-dev_1.0.5_armhf.deb
```

Now, you can go back and repeat the same process for ax25apps and ax25tools.

Once you build all the .deb's, you can use scp to get them out of the chroot on to another machine (you Pi), or once you exit the chroot, copy them out of /mnt/home/ve7fet before un-mounting everything.