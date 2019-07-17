/**
{file:
{name: Logger.cpp}
{description: Logger singleton class to log to file, window, and message box. }
{copyright:
(c) 2016-2018 Vidyo, Inc.,
433 Hackensack Avenue,
Hackensack, NJ  07601.
All rights reserved.
The information contained herein is proprietary to Vidyo, Inc.
and shall not be reproduced, copied (in whole or in part), adapted,
modified, disseminated, transmitted, transcribed, stored in a retrieval
system, or translated into any language in any form by any means
without the express written consent of Vidyo, Inc.}
}
*/

#include "Logger.h"

//  FUNCTION: Logger()
//
//  PURPOSE:  Constructor
//
Logger::Logger()
{
	// Open log file
	mLogFile.open("VidyoConnectorApp.log");
}

//  FUNCTION: ~Logger()
//
//  PURPOSE:  Destructor
//
Logger::~Logger()
{
	// Close log file
	mLogFile.close();
}

//  FUNCTION: Instance()
//
//  PURPOSE:  Get an instance of the singleton object
//
Logger& Logger::Instance()
{
	static Logger instance;
	return instance;
}

//  FUNCTION: Initialize()
//
//  PURPOSE:  Store the window handle
//
void Logger::Initialize(HWND hwnd)
{
	mMasterWnd = hwnd;
}

//  FUNCTION: Log()
//
//  PURPOSE:  Log a message originating from this app to a log file
//
void Logger::Log(const char* text)
{
	mLogFile << mAppLogPrefix << text << std::endl;
}

//  FUNCTION: LogClientLib()
//
//  PURPOSE:  Log a VidyoClientLib message to a log file
//
void Logger::LogClientLib(const char* text)
{
	mLogFile << mLibLogPrefix << text << std::endl;
}

//  FUNCTION: MsgBox()
//
//  PURPOSE:  Create a message box with text; also log the message
//
void Logger::MsgBox(const char* text, bool error)
{
	if (error)
		MessageBoxA(NULL, text, "Error!", MB_ICONEXCLAMATION | MB_OK);
	else
		MessageBoxA(NULL, text, "About VidyoConnector", MB_ICONINFORMATION);

	Log(text);
}
