/*
 * Copyright (C) 2007 Robotics at Maryland
 * Copyright (C) 2007 Steve Moskovchenko <stevenm@umd.edu>
 * All rights reserved.
 *
 * Author: Steve Moskovchenko <stevenm@umd.edu>
 * File:  packages/imu/src/imuapi.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef RAM_LINUX
#include <bits/types.h>
#include <linux/serial.h>
#endif

#include "imuapi.h"

static const double DEGS_TO_RADIANS = M_PI / 180;

unsigned char imu_waitByte(int fd)
{
   unsigned char rxb[1];
   printf("WAITING FOR BYTE\n");
   while(read(fd, rxb, 1) != 1);
   printf("GOT BYTE\n");
   return rxb[0];
}

void imu_waitSync(int fd)
{
   int fs=0;
   int syncLen=0;
   while(fs != 4)
   {
      if(imu_waitByte(fd) == 0xFF)
         fs++;
      else
         fs=0;
      syncLen++;
   }

   if(syncLen > 4)
      printf("Warning! IMU sync sequence took longer than 4 bytes!!\n");
}

int imu_convert16(unsigned char msb, unsigned char lsb)
{
   return (signed short) ((msb<<8)|lsb);
}

double imu_convertData(unsigned char msb, unsigned char lsb, double range)
{
   return imu_convert16(msb, lsb) * ((range/2.0)*1.5) / 32768.0;
}

int readIMUData(int fd, RawIMUData* imu)
{
   unsigned char imuData[34];
   int len, i, sum;
   imu_waitSync(fd);

   len = 0;
   i=0;
   sum=0;
   while(len < 34){
      len += read(fd, imuData+len, 34-len);
   }

   imu->messageID = imuData[0];
   imu->sampleTimer = (imuData[3]<<8) | imuData[4];

   imu->gyroX = imu_convertData(imuData[9],imuData[10],600) * DEGS_TO_RADIANS;
   imu->gyroY = imu_convertData(imuData[11],imuData[12],600) * DEGS_TO_RADIANS;
   imu->gyroZ = imu_convertData(imuData[13],imuData[14],600) * DEGS_TO_RADIANS;

   imu->accelX = imu_convertData(imuData[15], imuData[16], 4);
   imu->accelY = imu_convertData(imuData[17], imuData[18], 4);
   imu->accelZ = imu_convertData(imuData[19], imuData[20], 4);

   imu->magX = imu_convertData(imuData[21], imuData[22], 1.9);
   imu->magY = imu_convertData(imuData[23], imuData[24], 1.9);
   imu->magZ = imu_convertData(imuData[25], imuData[26], 1.9);

   imu->tempX = (((imu_convert16(imuData[27], imuData[28])*5.0)/32768.0)/0.0084)+25.0;
   imu->tempY = (((imu_convert16(imuData[29], imuData[30])*5.0)/32768.0)/0.0084)+25.0;
   imu->tempZ = (((imu_convert16(imuData[31], imuData[32])*5.0)/32768.0)/0.0084)+25.0;

   for(i=0; i<33; i++)
      sum+=imuData[i];

   sum += 0xFF * 4;

   imu->checksumValid = (imuData[33] == (sum&0xFF));

   if(!imu->checksumValid)
      printf("WARNING! IMU Checksum Bad!\n");

   return imu->checksumValid;
}


/* Some code from cutecom, which in turn may have come from minicom */
int openIMU(const char* devName)
{
   printf("Opening IMU sensor at %s.\n", devName);
   int fd = open(devName, O_RDWR, O_ASYNC);
   int mcs=0;
   unsigned int _baud=B115200;
   struct termios newtio;

   if(fd == -1){
      printf("IMU %s not found.\n", devName);
      return -1;
   }
   if (tcgetattr(fd, &newtio)!=0)
      printf("Attribute check on fd %d failed.\n", fd);

   cfsetospeed(&newtio, _baud);
   cfsetispeed(&newtio, _baud);

   newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
   newtio.c_cflag |= CLOCAL | CREAD;
   newtio.c_cflag &= ~(PARENB | PARODD);

   newtio.c_cflag &= ~CRTSCTS;
   newtio.c_cflag &= ~CSTOPB;

   newtio.c_iflag=IGNBRK;
   newtio.c_iflag &= ~(IXON|IXOFF|IXANY);

   newtio.c_lflag=0;

   newtio.c_oflag=0;


   newtio.c_cc[VTIME]=1;
   newtio.c_cc[VMIN]=60;

   if (tcsetattr(fd, TCSANOW, &newtio)!=0)
      printf("tsetaddr1 failed!\n");


   ioctl(fd, TIOCMGET, &mcs);
   mcs |= TIOCM_RTS;
   ioctl(fd, TIOCMSET, &mcs);

   if (tcgetattr(fd, &newtio)!=0)
      printf("tcgetattr() 4 failed\n");

   newtio.c_cflag &= ~CRTSCTS;

   if (tcsetattr(fd, TCSANOW, &newtio)!=0)
      printf("tcsetattr() 2 failed\n");

   return fd;
}
