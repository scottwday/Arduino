Arduino SD Card .WAV file player
================================

What?
-----
Plays wav files off an sd card. You can set the file to play via serial.

References
----------
Uses SdFatLib

Limitations
-----------
Setup to run off a 16Mhz crystal

Wav files must be 16Khz 8bit mono 
  8Khz or 32Khz could probably be made to work
  
The wave file header is played
  Can probably fix this by skipping ahead a fixed number of bytes

Folders not supported

Usage
-----
Send 'p' followed by the filename without extension, then enter (LF)
