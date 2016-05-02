#!/bin/bash
# script updated December-8-2015 for VE7FET new AX.25 github repository (F6BVP)
# Copy this script file in /usr/local/src/ax25/updAX25.sh
# cd into /usr/local/src/ax25
# and execute command : sudo chmod a+x updAX25.sh
# execute command to run the script : ./updAX25.sh
# It will update and re-compile AX.25 libraries, AX.25 tools and AX.25 apps

LIBAX25=linuxax25-master/libax25/
TOOLS=linuxax25-master/ax25tools/
APPS=linuxax25-master/ax25apps/

# Color Codes
Reset='\e[0m'
Red='\e[31m'
Green='\e[30;42m'  # Black/Green
Yellow='\e[33m'
YelRed='\e[31;43m' #Red/Yellow
Blue='\e[34m'
White='\e[37m'
BluW='\e[37;44m'

echo -e "${BluW}\t\n\t Script provided by Charles S. Schuman modified by F6BVP for updating AX.25 libraries and applications\t\n\t\t\t ${Red} November-30-2015    \n \t\t${Yellow}       k4gbb1@gmail.com \n${Reset}"

  if ! uid=0
   then su
  fi

#if [ -f /usr/lib/libax25.a ]; then
echo -e "${Green} Removing Old Libax25 files out of the way${Reset}"
  rm -fr /usr/lib/libax25*
  rm -fr /usr/lib/libax25*.*
#fi

# Make directories if not exist
if [ -d /usr/local/src/ax25/ ]
	then echo "directory /usr/local/src/ax25 already exists"
	else 
	mkdir /usr/local/src/ax25/
fi
if ! [ -d /usr/local/etc/ax25/ ]
	then mkdir /usr/local/etc/ax25/
fi
if ! [ -d /usr/local/var/ax25/ ] 
	then 
		mkdir /usr/local/var/
		mkdir /usr/local/var/ax25
fi
#
# Download libax25 source file 
  cd /usr/local/src/ax25
  rm -fr /usr/local/src/ax25/libax25
  rm -f master.zip
  echo -e "${Green} Getting AX25 libraries, AX25 tools and AX25 apps archives${Reset}"
   wget https://github.com/ve7fet/linuxax25/archive/master.zip
  if [ $? -ne 0 ]
   then
     echo -e "${Red}\t Ax25 Source files are Missing${Reset}"
     exit 1
  fi

echo -e "${Green} Now unarchiving AX.25 files ${Reset}"
  rm -fr linuxax25-master
  unzip master.zip
#Libax25 (updating configure.ac for automake > 1.12 compliance)
  cd /usr/local/src/ax25/$LIBAX25
#  
  echo -e "${Green}\t Creating Makefile(s) to prepare libraries compilation ${Reset}"
  ./autogen.sh
  ./configure > liberror.txt 2>&1
  echo -e -n "\t  *"
  echo -e "${Green}\t Compiling Runtime Lib files ${Reset}"

# Clean old binaries
  make clean
# Compile
  echo -n "  *"   
  make
  echo -e "\t  *" 
  if [ $? -ne 0 ]
    then
     echo -e "${Red}\t Libax25 Compile error - See liberror.txt ${Reset}"
     exit 1
  fi
  echo  "  *\n"
# Install
#  make install >> liberror.txt 2>&1
  make install
  if [ $? -ne 0 ]
   then
     echo -e "${Red} Libax25 Install error - See liberror.txt${Reset}"
     exit 1
   else   
     echo -e "${Green} Libax25 Installed${Reset}"
     rm liberror.txt
  fi

# AX25 libraries declaration (into ld.so.conf)
  echo "/usr/local/lib" >> /etc/ld.so.conf && /sbin/ldconfig

# AX25-APPS
  cd /usr/local/src/ax25
  rm -fr /usr/local/src/ax25/ax25apps
  echo -e "${Green} AX.25 applications${Reset}"
#Libax25 (updating configure.ac for automake > 1.12 compliance)
  cd /usr/local/src/ax25/$APPS
#  
  echo -e "${Green}\t Creating Makefile(s) to prepare apps compilation ${Reset}"
  ./autogen.sh
  ./configure > appserror.txt 2>&1
  echo -n -e "\t  *" 
# Clean old binaries
  make clean
# Compile Ax25-apps
  echo -n "  *" 
  echo -e "${Green}\t Compiling Ax25 apps ${Reset}"
  make
  echo -n -e "\t  *" 
  if [ $? -ne 0 ]
   then
     echo -e "${Red}\t Ax25-Apps Compile Error - see appserror.txt ${Reset}"
     exit 1
  fi
# Install Ax25-apps
  echo "  *" 
#  make  install >> appserror.txt 2>&1
  make  install
  echo -e "\t  *" 
  if [ $? -ne 0 ]
  then
     echo -e "${Red} Ax25-Apps Install Error - see appserror.txt ${Reset}"
     exit 1
  else
     echo -e "${Green} Ax25-apps Installed ${Reset}"
     rm appserror.txt
  fi

# AX25-TOOLS
  cd /usr/local/src/ax25
  rm -fr /usr/local/src/ax25/ax25tools
  echo -e "${Green} AX.25 tools${Reset}"
  cd /usr/local/src/ax25/$TOOLS
#  
  echo -n -e "\t  *" 
  echo -e "${Green}\t Creating Makefile(s) to prepare apps compilation ${Reset}"
  ./autogen.sh
  ./configure > toolserror.txt 2>&1
# Clean old binaries
  make clean
# Compile Ax.25 tools
  echo -e "${Green}\t Compiling AX.25 tools ${Reset}"
  echo -e "\t  *" 
  make
  echo -e "\t  *" 
    if [ $? -ne 0 ]
      then
        echo -e "${Red}\t AX.25 tools Compile error - See toolserror.txt ${Reset}"
        exit 1
    fi
# Install Ax.25 tools
  echo "  *" 
  make install
  if [ $? -ne 0 ]
    then
      echo -e "${Red}\t AX.25 tools Install error - See toolserror.txt ${Reset}"
      exit 1
    else
      echo -e "${Green} AX.25 tools Installed  ${Reset}"
      rm toolserror.txt
    fi

# Set permissions for /usr/local/sbin/ and /usr/local/bin
  cd /usr/local/sbin/
  chmod 4775 *
  cd /usr/local/bin/
  chmod 4775 *
  echo -e "\t \e[030;42m   Ax.25 Libraries, applications and tools were successfully rebuilt and installed${Reset}"
      echo -e "${Green} If this is a first install of AX.25 tools execute 'make installconf' from ax25tools directory${Reset}"
      echo -e "${Green} If this is a first install of AX.25 apps execute 'make installconf' from ax25apps directory${Reset}"
      echo -e "${Green} in order to create sample configuration files into /usr/local/etc/ax25/${Reset}"
  echo -e "\t \e[030;42m   Now it is time to compile and install AX.25 application programs${Reset}"
# (End of Script)
