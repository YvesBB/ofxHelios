/*
SDK for Helios Laser DAC class, HEADER
By Gitle Mikkelsen
gitlem@gmail.com

Dependencies:
Libusb 1.0 (GNU Lesser General Public License, see libusb.h)

Standard: C++14
git repo: https://github.com/Grix/helios_dac.git

BASIC USAGE:
1.	Call OpenDevices() to open devices, returns number of available devices.
2.	To send a frame to the DAC, first call GetStatus(). If the function returns ready (1), 
	then you can call WriteFrame(). The status should be polled until it returns ready. 
	It can and sometimes will fail to return ready on the first try.
3.  To stop output, use Stop(). To restart output you must send a new frame as described above.
4.	When the DAC is no longer needed, destroy the instance (destructors will free everything and close the connection)

The DAC is double-buffered. When it receives its first frame, it starts outputting it. When a second frame is sent to 
the DAC while the first frame is being played, the second frame is stored in the DACs memory until the first frame 
finishes playback, at which point the second, buffered frame will start playing. If the DAC finished playback of a frame
without having received and buffered a second frame, it will by default loop the first frame until a new frame is
received (but the flag HELIOS_FLAG_SINGLE_MODE will make it stop playback instead).
The GetStatus() function actually checks whether or not the buffer on the DAC is empty or full. If it is full, the DAC
cannot receive a new frame until the currently playing frame finishes, freeing up the buffer.
*/

#pragma once

#include "libusb.h"
#include <cstring>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <chrono>

#define HELIOS_SDK_VERSION	6

#define HELIOS_MAX_POINTS	0x1000
#define HELIOS_MAX_RATE		0xFFFF
#define HELIOS_MIN_RATE		7

#define HELIOS_SUCCESS		1		
#define HELIOS_ERROR		-1		//functions return this if something went wrong
	
#define HELIOS_FLAGS_DEFAULT			0
#define HELIOS_FLAGS_START_IMMEDIATELY	(1 << 0)
#define HELIOS_FLAGS_SINGLE_MODE		(1 << 1)
#define HELIOS_FLAGS_DONT_BLOCK			(1 << 2)

//usb properties
#define HELIOS_VID	0x1209
#define HELIOS_PID	0xE500
#define EP_BULK_OUT	0x02
#define EP_BULK_IN	0x81
#define EP_INT_OUT	0x06
#define EP_INT_IN	0x83

#ifdef _DEBUG
#define LIBUSB_LOG_LEVEL LIBUSB_LOG_LEVEL_WARNING
#else
#define LIBUSB_LOG_LEVEL LIBUSB_LOG_LEVEL_NONE
#endif

//point data structure
struct HeliosPoint
{
	std::uint16_t x; //12 bit (from 0 to 0xFFF)
	std::uint16_t y; //12 bit (from 0 to 0xFFF)
	std::uint8_t r;	//8 bit	(from 0 to 0xFF)
	std::uint8_t g;	//8 bit (from 0 to 0xFF)
	std::uint8_t b;	//8 bit (from 0 to 0xFF)
	std::uint8_t i;	//8 bit (from 0 to 0xFF)
	HeliosPoint(
		std::uint16_t _x,
		std::uint16_t _y, 
		std::uint8_t _r,	
		std::uint8_t _g,	
		std::uint8_t _b,		
		std::uint8_t _i	
		) : x(_x),y(_y),r(_r),g(_g),b(_b),i(_i) { }
};

class HeliosDac
{
public:

	HeliosDac();
	~HeliosDac();

	//unless otherwise specified, functions return HELIOS_SUCCESS if OK, and HELIOS_ERROR if something went wrong.

	//initializes drivers, opens connection to all devices.
	//Returns number of available devices.
	//NB: To re-scan for newly connected DACs after this function has once been called before, you must first call CloseDevices()
	int OpenDevices();

	//closes and frees all devices
	int CloseDevices();

	//writes and outputs a frame to the speficied dac
	//devNum: dac number (0 to n where n+1 is the return value from OpenDevices() )
	//pps: rate of output in points per second
	//flags: (default is 0)
	//	Bit 0 (LSB) = if 1, start output immediately, instead of waiting for current frame (if there is one) to finish playing
	//	Bit 1 = if 1, play frame only once, instead of repeating until another frame is written
	//  Bit 2 = if 1, don't let WriteFrame() block execution while waiting for the transfer to finish 
	//			(NB: then the function might return 1 even if it fails)
	//	Bit 3-7 = reserved
	//points: pointer to point data. See point structure declaration earlier in this document
	//numOfPoints: number of points in the frame
	int WriteFrame(unsigned int devNum, unsigned int pps, std::uint8_t flags, HeliosPoint* points, unsigned int numOfPoints);

	//Gets status of DAC, 1 means DAC is ready to receive frame, 0 means it is not
	int GetStatus(unsigned int devNum);

	//Returns firmware version of DAC
	int GetFirmwareVersion(unsigned int devNum);

	//Gets name of DAC (populates name with max 32 characters)
	int GetName(unsigned int devNum, char* name);

	//Sets name of DAC (name must be max 31 characters incl. null terminator)
	int SetName(unsigned int devNum, char* name);

	//Stops output of DAC until new frame is written (NB: blocks for 100ms)
	int Stop(unsigned int devNum);

	//Sets shutter level of DAC
	int SetShutter(unsigned int devNum, bool level);

	//Erase the firmware of the DAC, allowing it to be updated by accessing the SAM-BA bootloader
	int EraseFirmware(unsigned int devNum);

private:

	class HeliosDacDevice //individual dac, interal use
	{
	public:

		HeliosDacDevice(libusb_device_handle*);
		~HeliosDacDevice();
		int SendFrame(unsigned int pps, std::uint8_t flags, HeliosPoint* points, unsigned int numOfPoints);
		int GetStatus();
		int GetFirmwareVersion();
		int GetName(char* name);
		int SetName(char* name);
		int SetShutter(bool level);
		int Stop();
		int EraseFirmware();

	private:

		int DoFrame();
		void FrameHandler();
		int SendControl(std::uint8_t* buffer, unsigned int bufferSize);

		struct libusb_transfer* interruptTransfer = NULL;
		struct libusb_device_handle* usbHandle;
		std::mutex frameLock;
		bool frameReady = false;
		int firmwareVersion = 0;
		char name[32];
		bool closed = true;
		std::uint8_t* frameBuffer;
		unsigned int frameBufferSize;
		int frameResult = -1;

	};

	std::vector<std::unique_ptr<HeliosDacDevice>> deviceList;
	std::mutex threadLock;
	bool inited = false;
};
