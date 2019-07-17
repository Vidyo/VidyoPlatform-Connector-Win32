/**
{file:
{name: Logger.h}
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

#include <windows.h>
#include <fstream>
#include <string>

class Logger
{
private:
	Logger();
	Logger(Logger const&);            // copy constructor is private
	Logger& operator=(Logger const&); // assignment operator is private

	const std::string mAppLogPrefix = "VidyoConnector App: ";
	const std::string mLibLogPrefix    = "VidyoClientLibrary: ";
	
	std::ofstream mLogFile;
	HWND          mMasterWnd;

public:
	~Logger();
	static Logger& Instance();
	void Initialize(HWND hwnd);
	void Log(const char* text);
	void LogClientLib(const char* text);
	void MsgBox(const char* text, bool error = true);
};
