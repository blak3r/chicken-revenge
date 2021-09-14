# Revenge Chicken

Prank Audio Arduino Project

* Low Power
* Plays MP3s on three triggers: time, motion, and change in light.
* Configurable lock out times (don't play between hours of x)


My family plays a prank where we pass a rubber chicken back and forth in interesting ways... 
On this pass off, the rubber chicken parachuted and got stuck in a tree.


It triggers audio messages based on 3 triggers:

1. A time based trigger, every half hour during the hours permitted. --> play a short chicken cluck... 'buh buh BUHGOK'
2. A motion trigger (uses mpu6050 motion detection interrupt) to detect if the board is moved --> play a mp3 saying "Put me Down"
3. A light sensor trigger - a light sensor placed inside a parachute detects when someone opens the backpack and plays a sound.


Hardware:
* NodeMCU (ESP8266)
* MPU6050 (With A0 pulled to VCC)
* DFMiniPlayer
* Photoresitor.
* RTC DS1307


DFMiniPlayer
------------

I __hate__ this module.  It's a total shit show.  It's been cloned and not all firmwares are compatible. I burned hours and hours trying different libraries to get it to work.  
In the end, i found a comment on an amazon review where the commenter had found many commands worked if you didn't send a checksum and followed that path.  Some commands worked with a valid checksum others didn't.  The firmware on my version had a serious checksum calculation bug. 

There is a project which aims at helping you identify which version you have, what features are supported etc. here: https://github.com/ghmartin77/DFPlayerAnalyzer
In order to get it working, I made my own very basic library that just supported the commands I needed, RESET and PLAY TRACK.

In my case, i couldnt get any of the folder / file naming conventions to work.  I put about 6 files named 001.mp3 --> 006.mp3.  Play 1 did not play the file named 001 it played like 6.
So, i just recorded which track was what through experimental playback... 
