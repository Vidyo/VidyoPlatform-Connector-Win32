/**
{file:
{name: VidyoViewController.cpp}
{description: Defines the entry point for the application. }
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

#include "stdafx.h"
#include "resource.h"
#include "Logger.h"
#include <Lmi\VidyoClient\VidyoConnector.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <Shlwapi.h> // 'UrlGet...()', 'URL_PART...'.
#include <WinInet.h> // INTERNET_MAX_URL_LENGTH

// Class used to define locations a rectangular window on screen
class WindowRect {
	public:
		int x, y, width, height;

		WindowRect( int aX, int aY, int aWidth, int aHeight ) :
			x( aX ), y( aY ), width( aWidth ), height( aHeight ) {}
};

enum LocalDevice {
	LOCAL_CAMERA,
	LOCAL_MICROPHONE,
	LOCAL_SPEAKER,
	LOCAL_MONITOR,
	LOCAL_WINDOW,
	VIDEO_CONTENT_SHARE,
	AUDIO_CONTENT_SHARE
};

enum VIDYO_CONNECTOR_STATE {
	VC_CONNECTED,
	VC_DISCONNECTED,
	VC_DISCONNECTED_UNEXPECTED,
	VC_CONNECTION_FAILURE
};

#define MAX_LOADSTRING 100

// VidyoConnector roomkey: Maximum string-length that this program supports.
//  TODO: Current value is an ARBITRARY limit. Remove such limitations.
#define ROOM_KEY_STRLEN_MAX (1024)

// Forward declarations of functions included in this code module:

ATOM MyRegisterClass( HINSTANCE hInstance );
BOOL InitInstance( HINSTANCE, int );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

void Connect();
void Disconnect();
void UpdateToolbarStatus( std::string str );
void InitializeUI( HWND hWnd );
void RefreshUI();
void UpdateControlsView( bool hidden );
void SetWindowIcon( const HWND& hwnd, HMENU iconResourceId, const WindowRect& rect );
void ConnectorStateUpdated( VIDYO_CONNECTOR_STATE state, const char* statusText, bool refreshUI );
void CleanUp( HWND hWnd );
int  GetUniqueId( LocalDevice device );
int  CreateMenuItem( HMENU hmenu, unsigned long menuOffset, const char *itemName, LocalDevice type );
void SetCameraPrivacy();
void SetMicrophonePrivacy();
void SetMaxResolution( UINT menuItemId );
void SetExperimentalOptions( UINT menuItemId );
void extractFieldNameAndFieldValue(TCHAR* queryPair, TCHAR* &fieldName, TCHAR* &valueOfField, int &buffSizeValue, int &buffSizeFieldName);
void extractFieldsFromUrl(std::basic_string<TCHAR>tStr, bool isFromSecondWindow);
void SetUIControlsToNewState();

static void OnConnected(        VidyoConnector *c );
static void OnConnectionFailed( VidyoConnector *c, VidyoConnectorFailReason reason );
static void OnDisconnected(     VidyoConnector *c, VidyoConnectorDisconnectReason reason );

static void OnLocalCameraAdded(        VidyoConnector *c, VidyoLocalCamera *localCamera );
static void OnLocalCameraRemoved(      VidyoConnector *c, VidyoLocalCamera *localCamera );
static void OnLocalCameraSelected(     VidyoConnector *c, VidyoLocalCamera *localCamera );
static void OnLocalCameraStateUpdated( VidyoConnector *c, VidyoLocalCamera *localCamera, VidyoDeviceState state );

static void OnLocalMicrophoneAdded(        VidyoConnector *c, VidyoLocalMicrophone *localMicrophone );
static void OnLocalMicrophoneRemoved(      VidyoConnector *c, VidyoLocalMicrophone *localMicrophone );
static void OnLocalMicrophoneSelected(     VidyoConnector *c, VidyoLocalMicrophone *localMicrophone );
static void OnLocalMicrophoneStateUpdated( VidyoConnector *c, VidyoLocalMicrophone *localMicrophone, VidyoDeviceState state );

static void OnLocalSpeakerAdded(        VidyoConnector *c, VidyoLocalSpeaker *localSpeaker );
static void OnLocalSpeakerRemoved(      VidyoConnector *c, VidyoLocalSpeaker *localSpeaker );
static void OnLocalSpeakerSelected(     VidyoConnector *c, VidyoLocalSpeaker *localSpeaker );
static void OnLocalSpeakerStateUpdated( VidyoConnector *c, VidyoLocalSpeaker *localSpeaker, VidyoDeviceState state );

static void OnLocalMonitorAdded(        VidyoConnector *c, VidyoLocalMonitor *localMonitor );
static void OnLocalMonitorRemoved(      VidyoConnector *c, VidyoLocalMonitor *localMonitor );
static void OnLocalMonitorSelected(     VidyoConnector *c, VidyoLocalMonitor *localMonitor );
static void OnLocalMonitorStateUpdated( VidyoConnector *c, VidyoLocalMonitor *localMonitor, VidyoDeviceState state );

static void OnLocalWindowShareAdded(        VidyoConnector *c, VidyoLocalWindowShare *localWindowShare );
static void OnLocalWindowShareRemoved(      VidyoConnector *c, VidyoLocalWindowShare *localWindowShare );
static void OnLocalWindowShareSelected(     VidyoConnector *c, VidyoLocalWindowShare *localWindowShare );
static void OnLocalWindowShareStateUpdated( VidyoConnector *c, VidyoLocalWindowShare *localWindowShare, VidyoDeviceState state );

static void OnParticipantJoined(         VidyoConnector* c, VidyoParticipant* participant );
static void OnParticipantLeft(           VidyoConnector* c, VidyoParticipant* participant );
static void OnDynamicParticipantChanged(VidyoConnector* c, LmiVector(VidyoParticipant)* participants);
static void OnLoudestParticipantChanged(VidyoConnector* c, const VidyoParticipant* participant, LmiBool audioOnly);
static bool validateCommandLineForInvocationURL(const WCHAR* args, char* url);

static void OnLog( VidyoConnector *c, const VidyoLogRecord *logRecord );
static bool validateCommandLineForQuit(const WCHAR* lpCmdLine, HWND hWnd);

// Global Variables

HINSTANCE hInst;                         // current instance
TCHAR     szTitle[MAX_LOADSTRING];       // The title bar text
TCHAR     szWindowClass[MAX_LOADSTRING]; // the main window class name
HMENU     conferenceSubmenu;             // Conference submenu
HMENU     devicesSubmenu;                // Devices submenu
HMENU     sharesSubmenu;                 // Shares submenu
HMENU     contentSubmenu;                // Content submenu

HWND hMasterWnd;
HWND hConnectButton;
HWND hCameraMuteButton;
HWND hMicMuteButton;
HWND hPortalEdit;
HWND hPortalText;
HWND hRoomkeyEdit;
HWND hRoomkeyText;
HWND hDisplayNameEdit;
HWND hDisplayNameText;
HWND hPinEdit;
HWND hPinText;
HWND hVideoPanel;
HWND hToolbar;
HWND hParticipantStatus;
HWND hVidyoIcon;

bool startupStringContainedPortal;
bool startupStringContainedRoomKey;
bool startupStringContainedPin;
bool startupStringContainedDisplayName;
bool startupStringContainedAutoJoin;
bool startupStringContainedCameraPrivacy;
bool startupStringContainedMicrophonePrivacy;
bool startupStringContainedEnableDebug;
bool startupStringContainedExperimentalOptions;
bool startupStringContainedHideConfig;
bool startupstringContainedAllowReconnect;


static const LPCWSTR SINGLE_INSTANCE_MUTEX_NAME = L"Vidyo_Connector_Mutex";
static const char* APP_NAME = "VidyoConnector";

FLOAT dpiX = 96.f;
FLOAT dpiY = 96.f;

static FLOAT scaleX(FLOAT x)
{
	return x * dpiX / 96.f;
}

static FLOAT scaleY(FLOAT y)
{
	return y * dpiY / 96.f;
}

static void scaleRect(WindowRect* rect)
{
	rect->x = static_cast<INT>(scaleX(rect->x));
	rect->y = static_cast<INT>(scaleY(rect->y));
	rect->width = static_cast<INT>(scaleX(rect->width));
	rect->height = static_cast<INT>(scaleY(rect->height));
}
static ULONG_PTR gdiPlusToken;

static HBRUSH hBlackBrush = NULL;
static HBRUSH hWhiteBrush = NULL;

static std::map<HMENU, HBITMAP> iconMap;

// Indicates whether app is quitting; Helps coordinate disconnect and clean-up.
static volatile bool appIsQuitting = false;

// VidyoConnector globals
static VidyoConnector vc;
static std::map<int, VidyoLocalCamera*>      cameraMap;
static std::map<int, VidyoLocalMicrophone*>  microphoneMap;
static std::map<int, VidyoLocalSpeaker*>     speakerMap;
static std::map<int, VidyoLocalMonitor*>     monitorShareMap;
static std::map<int, VidyoLocalWindowShare*> windowShareMap;
static std::map<int, VidyoLocalCamera*>      videoContentShareMap;
static std::map<int, VidyoLocalMicrophone*>  audioContentShareMap;

static VIDYO_CONNECTOR_STATE vidyoConnectorState = VC_DISCONNECTED;
static LmiBool microphonePrivacy = LMI_FALSE;
static LmiBool cameraPrivacy = LMI_FALSE;
static std::recursive_mutex deviceLock;
static bool hideConfig = false;
static bool autoJoin = false;
static bool allowReconnect = true;
static bool enableDebug = false;
static char portal[256 + sizeof('\0')] = "";
static char roomKey[ROOM_KEY_STRLEN_MAX + sizeof('\0')] = "";
static char displayName[256 + sizeof('\0')] = "DemoUser";
static char roomPin[256 + sizeof('\0')] = "";
static char experimentalOptions[512 + sizeof('\0')] = "";
static bool vidyoCloudJoin = false;           // Used for VidyoCloud systems, not Vidyo.io
//static char portal[256 + sizeof('\0')] = "";  // Used for VidyoCloud systems, not Vidyo.io
//static char roomKey[256 + sizeof('\0')] = ""; // Used for VidyoCloud systems, not Vidyo.io
//static char roomPin[256 + sizeof('\0')] = ""; // Used for VidyoCloud systems, not Vidyo.io
static VidyoLocalCamera *selectedLocalCamera = NULL;
static int selectedMaxResolutionMenuItem = ID_MAX_RESOLUTION_720P;
static HINSTANCE lastInstance;

// UI variables
//                               x,   y,   w,   h     (in points)
WindowRect vidyoIconRect      ( 115,  65, 190, 225 );
WindowRect portalTextRect     (  10, 325,  90,  30 );
WindowRect portalEditRect     ( 105, 325, 200,  30 );
WindowRect roomkeyTextRect    (  10, 375,  90,  30 );
WindowRect roomkeyEditRect    ( 105, 375, 200,  30 );
WindowRect displayNameTextRect(  10, 425,  90,  30 );
WindowRect displayNameEditRect( 105, 425, 200,  30 );
WindowRect pinTextRect        (  10, 475,  90,  30 );
WindowRect pinEditRect        ( 105, 475, 200,  30 );

// These are part of the toolbar and dynamically positioned, hence x = 0, y = 0 initially
WindowRect cameraMuteRect     (   0,   0,  25,  25 );
WindowRect connectRect        (   0,   0,  25,  25 );
WindowRect micMuteRect        (   0,   0,  25,  25 );

static const int  CONTROLS_VIEW_WIDTH           = 420; // fixed width of left side of screen
static const LONG TOOLBAR_HEIGHT                = 55;
static const LONG PARTICIPANT_STATUS_INIT_WIDTH = 300;
static const LmiUint NUM_REMOTE_PARTICIPANTS    = 15;

// Baseline menu offsets for no camera, no mic, no speaker, no monitor share, no window share

static const unsigned long NO_CAMERA_MENU_OFFSET              = 1; // "Camera" text
static const unsigned long NO_MICROPHONE_MENU_OFFSET          = 4; // plus "None" camera, divider, "Microphone" text
static const unsigned long NO_SPEAKER_MENU_OFFSET             = 7; // plus "None" microphone, divider, "Speaker" text
static const unsigned long NO_MONITOR_SHARE_MENU_OFFSET       = 1; // "Monitor" text
static const unsigned long NO_WINDOW_SHARE_MENU_OFFSET        = 4; // plus "None" monitor, divider, "Window" text
static const unsigned long NO_VIDEO_CONTENT_SHARE_MENU_OFFSET = 1; // "Video Content Share" text
static const unsigned long NO_AUDIO_CONTENT_SHARE_MENU_OFFSET = 4; // plus "None" video content share, divider, "Audio Content Share" text

//////////////////////////////////////////////////////////////////////////////////////////////////////

int APIENTRY _tWinMain( _In_     HINSTANCE hInstance,
                        _In_opt_ HINSTANCE hPrevInstance,
                        _In_     LPTSTR    lpCmdLine,
                        _In_     int       nCmdShow ) {
	HANDLE hMutex = NULL;
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );
	if (lastInstance != hInstance){
		hMutex = ::OpenMutex(
			MUTEX_ALL_ACCESS, 0, SINGLE_INSTANCE_MUTEX_NAME);
		if (!hMutex)
		{
			hMutex = ::CreateMutex(0, 0, SINGLE_INSTANCE_MUTEX_NAME);
			lastInstance = hInstance;
		}
		else{
			// This is a second instance. Bring the
			// original instance to the top.

			// 1. Convert name to wchar with a new allocation
			const size_t cSize = strlen(APP_NAME) + 1;
			wchar_t* wc = new wchar_t[cSize];
			size_t ret;
			mbstowcs_s(&ret, wc,cSize, APP_NAME, cSize);
			// 2. Find window to bring on top with that name
			HWND hWnd = ::FindWindow(L"VidyoConnector", L"VidyoConnector App");
			if (hWnd == NULL)
			{
				free(wc);
				exit(1);
			}
			else
			{
				free(wc);
			}

			// 3. Free memory from wchar string
			::SetForegroundWindow(hWnd);

			CHAR urlPath[INTERNET_MAX_URL_LENGTH] = { 0 };
			bool isQuitting = false;

			COPYDATASTRUCT cds;

			if (validateCommandLineForQuit(lpCmdLine, hWnd))
			{
				isQuitting = true;
			}

			if (isQuitting || validateCommandLineForInvocationURL(lpCmdLine, urlPath))
			{
				//cds.dwData = isQuitting ? NEO_PROCESS_QUIT_ID : NEO_PROCESS_ROOM_URL_ID;
				cds.cbData = (DWORD)(sizeof(TCHAR) * (_tcslen(lpCmdLine) + 1));

				cds.lpData = (LPVOID)lpCmdLine;
				::SendMessage(
					hWnd, WM_COPYDATA, (WPARAM)hWnd, (LPARAM)(LPVOID)&cds);
			}
			// Command line is not empty. Send the
			// command line in a WM_COPYDATA message.
			return 0;
		}
	}
	//Logger::Instance().Log(lpCmdLine);
	// Parse command line args
	if ( __argc > 1 ) {
		std::wstring wStr;
		std::string  str;
		std::size_t  strLen;

		// For simpler source code, use generic-text mappings with C++ Standard Library strings.
		typedef std::basic_string<TCHAR> tstring;

		//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Check command-line arguments for use of custom URI/URL scheme.

		// Prepare to look for indicator at start of first argument.
		wchar_t** args = __targv;
		tstring tStr = __targv[1];

		// Prepare for case-insensitive text search:  Normalize case.
		std::transform(tStr.begin(), tStr.end(), tStr.begin(), ::tolower);
		extractFieldsFromUrl(tStr, false);
	}

	// Initialize GDI+
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup( &gdiPlusToken, &gdiplusStartupInput, NULL );

	// Initialize global strings
	LoadString( hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING );
	LoadString( hInstance, IDC_VIDYOCONNECTOR, szWindowClass, MAX_LOADSTRING );
	MyRegisterClass( hInstance );

	// Perform application initialization:
	if ( !InitInstance( hInstance, nCmdShow ) ) {
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDC_VIDYOCONNECTOR ) );

	// Main message loop:
	MSG msg;
	while ( GetMessage( &msg, NULL, 0, 0 ) ) {
		if ( !TranslateAccelerator(msg.hwnd, hAccelTable, &msg ) ) {
			if ( IsDialogMessage( hMasterWnd, &msg ) == 0 ) { // This is added in order for tabs to be functional in the edit boxes
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
		}
	}

	return (int)msg.wParam;
}

// Register the window class.
ATOM MyRegisterClass( HINSTANCE hInstance ) {
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof( WNDCLASSEX );

	wcex.style		= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon		= LoadIcon( hInstance, MAKEINTRESOURCE( IDI_VIDYOCONNECTOR ) );
	wcex.hCursor		= LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground	= ( HBRUSH )( COLOR_WINDOW+1 );
	wcex.lpszMenuName	= MAKEINTRESOURCE( IDC_VIDYOCONNECTOR );
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon( wcex.hInstance, MAKEINTRESOURCE( IDI_SMALL ) );

	return RegisterClassEx( &wcex );
}
// Save instance handle and create main window.
// In this function, we save the instance handle in a global variable and
// create and display the main program window.
BOOL InitInstance( HINSTANCE hInstance, int nCmdShow ) {
	hInst = hInstance; // Store instance handle in our global variable

	hMasterWnd = CreateWindow(szWindowClass,
					szTitle,
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT,
					0,
					CW_USEDEFAULT,
					0,
					NULL,
					NULL,
					hInstance,
					NULL );

	if ( !hMasterWnd ) {
		return FALSE;
	}

	ShowWindow( hMasterWnd, nCmdShow );
	UpdateWindow( hMasterWnd );

	return TRUE;
}

// Processes messages for the main window.
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	std::wstring urlString;
	std::basic_string<TCHAR> arg;
	COPYDATASTRUCT* pcds;
	LPCTSTR lpszString;
	CHAR urlPath[INTERNET_MAX_URL_LENGTH] = { 0 };
	switch ( message ) {
		case WM_COMMAND: {
			wmId    = LOWORD( wParam );
			wmEvent = HIWORD( wParam );

			// Check whether the command was selecting a camera, microphone or output
			if ( ( wmId >= ID_DEVICES_CAMERA_ITEMS_MIN ) && ( wmId < ID_DEVICES_CAMERA_ITEMS_MAX ) ) {
				// Search the camera map for the selected camera
				std::map<int, VidyoLocalCamera* >::iterator it = cameraMap.find( wmId );

				if ( it != cameraMap.end() ) {
					// Tell VidyoConnector to select the camera
					VidyoConnectorSelectLocalCamera( &vc, it->second );
				}
			} else if ( ( wmId >= ID_DEVICES_MICROPHONE_ITEMS_MIN ) && ( wmId < ID_DEVICES_MICROPHONE_ITEMS_MAX ) ) {
				// Search the microphone map for the selected mic
				std::map<int, VidyoLocalMicrophone* >::iterator it = microphoneMap.find( wmId );

				if ( it != microphoneMap.end() ) {
					// Tell VidyoConnector to select the mic
					VidyoConnectorSelectLocalMicrophone( &vc, it->second );
				}
			} else if ( ( wmId >= ID_DEVICES_SPEAKER_ITEMS_MIN ) && ( wmId < ID_DEVICES_SPEAKER_ITEMS_MAX ) ) {
				// Search the speaker map for the selected speaker
				std::map<int, VidyoLocalSpeaker* >::iterator it = speakerMap.find( wmId );

				if ( it != speakerMap.end() ) {
					// Tell VidyoConnector to select the mic
					VidyoConnectorSelectLocalSpeaker( &vc, it->second );
				}
			} else if ( ( wmId >= ID_MONITOR_SHARE_ITEMS_MIN ) && ( wmId < ID_MONITOR_SHARE_ITEMS_MAX ) ) {
				// Search the monitor map for the selected monitor
				std::map<int, VidyoLocalMonitor* >::iterator it = monitorShareMap.find( wmId );

				if ( it != monitorShareMap.end() ) {
					// Select the monitor to share
					VidyoConnectorSelectLocalMonitor( &vc, it->second );
				}
			} else if ( ( wmId >= ID_WINDOW_SHARE_ITEMS_MIN ) && ( wmId < ID_WINDOW_SHARE_ITEMS_MAX ) ) {
				// Search the window share map for the selected window
				std::map<int, VidyoLocalWindowShare* >::iterator it = windowShareMap.find( wmId );

				if ( it != windowShareMap.end() ) {
					// Select the window to share
					VidyoConnectorSelectLocalWindowShare( &vc, it->second );
				}
			} else if ( ( wmId >= ID_VIDEO_CONTENT_SHARE_MIN ) && ( wmId < ID_VIDEO_CONTENT_SHARE_MAX ) ) {
				// Iterate through the videoContentShareMap to find the selected camera
				for (std::map<int, VidyoLocalCamera*>::iterator vcsIt = videoContentShareMap.begin(); vcsIt != videoContentShareMap.end(); ++vcsIt) {
					if ( vcsIt->first == wmId ) {
						VidyoConnectorSelectVideoContentShare( &vc, vcsIt->second );
						CheckMenuItem( contentSubmenu, vcsIt->first, MF_BYCOMMAND | MF_CHECKED );

						// Disable the selected camera in the devices submenu
						for (std::map<int, VidyoLocalCamera*>::iterator cmIt = cameraMap.begin(); cmIt != cameraMap.end(); ++cmIt) {
							EnableMenuItem( devicesSubmenu, cmIt->first, VidyoLocalCameraEqual(cmIt->second, vcsIt->second) ? MF_GRAYED : MF_ENABLED );
						}
					} else {
						CheckMenuItem( contentSubmenu, vcsIt->first, MF_BYCOMMAND | MF_UNCHECKED );
					}
				}
				// Uncheck the "None" menu item
				CheckMenuItem(contentSubmenu, ID_VIDEO_CONTENT_SHARE_NONE, MF_BYCOMMAND | MF_UNCHECKED);

			} else if ( ( wmId >= ID_AUDIO_CONTENT_SHARE_MIN ) && ( wmId < ID_AUDIO_CONTENT_SHARE_MAX ) ) {
				// Iterate through the audioContentShareMap to find the selected microphone
				for (std::map<int, VidyoLocalMicrophone*>::iterator acsIt = audioContentShareMap.begin(); acsIt != audioContentShareMap.end(); ++acsIt) {
					if (acsIt->first == wmId) {
						VidyoConnectorSelectAudioContentShare(&vc, acsIt->second);
						CheckMenuItem(contentSubmenu, acsIt->first, MF_BYCOMMAND | MF_CHECKED);

						// Disable the selected microphone in the Devices submenu
						for (std::map<int, VidyoLocalMicrophone*>::iterator mmIt = microphoneMap.begin(); mmIt != microphoneMap.end(); ++mmIt) {
							EnableMenuItem(devicesSubmenu, mmIt->first, VidyoLocalMicrophoneEqual(mmIt->second, acsIt->second) ? MF_GRAYED : MF_ENABLED);
						}
					} else {
						CheckMenuItem(contentSubmenu, acsIt->first, MF_BYCOMMAND | MF_UNCHECKED);
					}
				}
				// Uncheck the "None" menu item
				CheckMenuItem(contentSubmenu, ID_AUDIO_CONTENT_SHARE_NONE, MF_BYCOMMAND | MF_UNCHECKED);

			} else {
				// Non zero HIWORDs should not be handled in the switch below
				if ( wmEvent != 0 ) {
					return DefWindowProc(hWnd, message, wParam, lParam);
				}

				// Parse the menu selections:
				switch ( wmId ) {
					case ID_HELP_VERSION: {
						std::string version = "VidyoConnector App version " + std::string( LmiStringCStr( VidyoConnectorGetVersion(&vc) ) ) + "\n\nCopyright (C) 2016-2018 Vidyo, Inc. All rights reserved.";
						Logger::Instance().MsgBox( version.c_str(), false );
						break;
					}

					case ID_VC_CONNECT: {
						// Handle Connect being selected from menu.
						// If already connected or in the process of connecting, do not call the Connect function which would call VidyoConnectorConnect again
						std::map<HMENU, HBITMAP>::iterator it = iconMap.find( ( HMENU )IDB_CALL_END );
						if ( ( it != iconMap.end() ) && ( it->second == ( HBITMAP )SendMessage( hConnectButton, STM_GETIMAGE, IMAGE_BITMAP, 0 ) ) ) {
							if ( vidyoConnectorState == VC_CONNECTED ) {
								UpdateToolbarStatus( "Already connected" );
							}
						} else {
							Connect();
						}
						break;
					}

					case ID_VC_DISCONNECT: {
						// Handle Disconnect being selected from menu.
						// If already disconnected or in the process of disconnecting, do not call the Disconnect function which would call VidyoConnectorDisconnect again
						std::map<HMENU, HBITMAP>::iterator it = iconMap.find( ( HMENU )IDB_CALL_START );
						if ( ( it != iconMap.end() ) && ( it->second == ( HBITMAP )SendMessage( hConnectButton, STM_GETIMAGE, IMAGE_BITMAP, 0 ) ) ) {
							if ( vidyoConnectorState != VC_CONNECTED ) {
								UpdateToolbarStatus( "Already disconnected" );
							}
						} else {
							Disconnect();
						}
						break;
					}

					case ID_VC_TOGGLE_DEBUG: {
						if ( enableDebug ) {
							VidyoConnectorDisableDebug( &vc );
							CheckMenuItem( conferenceSubmenu, ID_VC_TOGGLE_DEBUG, MF_BYCOMMAND | MF_UNCHECKED );
						} else {
							VidyoConnectorEnableDebug( &vc, 7776, "info@VidyoClient info@VidyoConnector warning" );
							CheckMenuItem( conferenceSubmenu, ID_VC_TOGGLE_DEBUG, MF_BYCOMMAND | MF_CHECKED );
						}
						enableDebug = !enableDebug;
						break;
					}

					case ID_MAX_RESOLUTION_180P:
					case ID_MAX_RESOLUTION_270P:
					case ID_MAX_RESOLUTION_360P:
					case ID_MAX_RESOLUTION_540P:
					case ID_MAX_RESOLUTION_720P:
					case ID_MAX_RESOLUTION_1080P:
					case ID_MAX_RESOLUTION_2160P: {
						SetMaxResolution( wmId );
						break;
					}

					case ID_EXPERIMENTAL_BACKLIGHT_COMP: {
						bool enable = false;

						// Toggle the state of the menu item (add or remove checkmark)
						UINT state = GetMenuState( conferenceSubmenu, ID_EXPERIMENTAL_BACKLIGHT_COMP, MF_BYCOMMAND );
						if ( !( state & MF_CHECKED ) ) {
							CheckMenuItem( conferenceSubmenu, ID_EXPERIMENTAL_BACKLIGHT_COMP, MF_BYCOMMAND | MF_CHECKED );
							enable = true;
						} else {
							CheckMenuItem( conferenceSubmenu, ID_EXPERIMENTAL_BACKLIGHT_COMP, MF_BYCOMMAND | MF_UNCHECKED );
							enable = false;
						}

						// Set the Backlight Compensation if a local camera is selected
						if ( selectedLocalCamera ) {
							VidyoLocalCameraSetBacklightCompensation( selectedLocalCamera, enable );
						}
						break;
					}

					case ID_EXPERIMENTAL_PTZ:
					case ID_EXPERIMENTAL_FORCE_DIG_PTZ:
					case ID_EXPERIMENTAL_VP9: {
						SetExperimentalOptions( wmId );
						break;
					}

					case ID_CAMERA_NONE: {
						// "None" camera was selected from the menu
						VidyoConnectorSelectLocalCamera( &vc, NULL );
						break;
					}

					case ID_MICROPHONE_NONE: {
						// "None" microphone was selected from the menu
						VidyoConnectorSelectLocalMicrophone( &vc, NULL );
						break;
					}

					case ID_OUTPUT_NONE: {
						// "None" output/speaker was selected from the menu
						VidyoConnectorSelectLocalSpeaker( &vc, NULL );
						break;
					}

					case ID_MONITOR_SHARE_NONE: {
						// "None" monitor was selected from the menu
						VidyoConnectorSelectLocalMonitor( &vc, NULL );
						break;
					}

					case ID_WINDOW_SHARE_NONE: {
						// "None" window share was selected from the menu
						VidyoConnectorSelectLocalWindowShare( &vc, NULL );
						break;
					}

					case ID_VIDEO_CONTENT_SHARE_NONE: {
						// "None" video content share was selected from the menu
						VidyoConnectorSelectVideoContentShare(&vc, NULL);

						// Iterate through the videoContentShareMap and uncheck all items
						for (std::map<int, VidyoLocalCamera*>::iterator it = videoContentShareMap.begin(); it != videoContentShareMap.end(); ++it) {
							CheckMenuItem(contentSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED);
						}
						// Check the "None" menu item
						CheckMenuItem(contentSubmenu, ID_VIDEO_CONTENT_SHARE_NONE, MF_BYCOMMAND | MF_CHECKED);

						// Iterate through the cameraMap and enable all items in the Devices submenu
						for (std::map<int, VidyoLocalCamera*>::iterator cmIt = cameraMap.begin(); cmIt != cameraMap.end(); ++cmIt) {
							EnableMenuItem(devicesSubmenu, cmIt->first, MF_ENABLED);
						}
						break;
					}

					case ID_AUDIO_CONTENT_SHARE_NONE: {
						// "None" audio content share was selected from the menu
						VidyoConnectorSelectAudioContentShare(&vc, NULL);

						// Iterate through the audioContentShareMap and uncheck all items
						for (std::map<int, VidyoLocalMicrophone*>::iterator it = audioContentShareMap.begin(); it != audioContentShareMap.end(); ++it) {
							CheckMenuItem(contentSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED);
						}
						// Check the "None" menu item
						CheckMenuItem(contentSubmenu, ID_AUDIO_CONTENT_SHARE_NONE, MF_BYCOMMAND | MF_CHECKED);

						// Iterate through the microphoneMap and enable all items in the Devices submenu
						for (std::map<int, VidyoLocalMicrophone*>::iterator mmIt = microphoneMap.begin(); mmIt != microphoneMap.end(); ++mmIt) {
							EnableMenuItem(devicesSubmenu, mmIt->first, MF_ENABLED);
						}
						break;
					}

					case IDC_TOGGLE_CONNECT_BUTTON: {
						// Handle toggle connect button being pressed.
						// If the hConnectButton is the call end image, then either user is connected to a resource or is in the process
						// of connecting to a resource; call VidyoConnectorDisconnect to disconnect or abort the connection attempt
						std::map<HMENU, HBITMAP>::iterator it = iconMap.find( ( HMENU )IDB_CALL_END );
						if ( ( it != iconMap.end() ) && ( it->second == ( HBITMAP )SendMessage( hConnectButton, STM_GETIMAGE, IMAGE_BITMAP, 0 ) ) ) {
							Disconnect();
						}
						else {
							Connect();
						}

						break;
					}

					case IDC_CAMERA_MUTE_BUTTON:
						cameraPrivacy = ( cameraPrivacy == LMI_FALSE ) ? LMI_TRUE : LMI_FALSE;
						SetCameraPrivacy();
						break;

					case IDC_MIC_MUTE_BUTTON:
						microphonePrivacy = ( microphonePrivacy == LMI_FALSE ) ? LMI_TRUE : LMI_FALSE;
						SetMicrophonePrivacy();
						break;

					case IDM_EXIT:
						// Clean up the app for graceful exit
						CleanUp( hWnd );
						break;

					default:
						return DefWindowProc( hWnd, message, wParam, lParam );
				}
			}

			break;
		}
		case WM_CREATE: {
			// Get handle to submenus
			HMENU mainMenu = GetMenu( hWnd );
			conferenceSubmenu = GetSubMenu( mainMenu, 1 ); // 1 is index of the Conference submenu
			devicesSubmenu    = GetSubMenu( mainMenu, 2 ); // 2 is index of the Devices submenu
			contentSubmenu    = GetSubMenu( mainMenu, 3 ); // 3 is index of the Content submenu
			sharesSubmenu     = GetSubMenu( mainMenu, 4 ); // 4 is index of the Shares submenu

			// Create white and black brushes
			if ( hBlackBrush == NULL ) {
				hBlackBrush = CreateSolidBrush( RGB( 0, 0, 0 ) );
			}

			if ( hWhiteBrush == NULL ) {
				hWhiteBrush = CreateSolidBrush( RGB( 255, 255, 255 ) );
			}

			// Initialize logger
			Logger::Instance().Initialize( hWnd );

			// Initialize the UI elements
			InitializeUI( hWnd );

			if ( hideConfig ) {
				// Get dimensions of master window
				RECT masterRect;
				GetClientRect( hMasterWnd, &masterRect );

				// Update the videoPanel frame to be full-screen
				SetWindowPos(hVideoPanel,
								HWND_TOP,
								0,
								0,
								masterRect.right,
								masterRect.bottom - TOOLBAR_HEIGHT,
								SWP_SHOWWINDOW);
			} else {
				// Display the controls view which is not visible when option controls are created
				UpdateControlsView( false );
			}

			// Get videoPanel size
			RECT vidyoRect;
			GetClientRect( hVideoPanel, &vidyoRect );

			// Initialize VidyoClient library - this should only be called once in the lifetime of the application.
			VidyoConnectorInitialize();

			if ( VidyoConnectorConstruct( &vc,
											&hVideoPanel,
											VIDYO_CONNECTORVIEWSTYLE_Default,
											NUM_REMOTE_PARTICIPANTS,
											"info@VidyoClient info@VidyoConnector warning",
											"",
											NULL ) ) {

				// Set initial position
				VidyoConnectorShowViewAt( &vc, NULL, 0, 0, vidyoRect.right, vidyoRect.bottom );

				// Register for Camera callbacks
				if ( !VidyoConnectorRegisterLocalCameraEventListener( &vc,
												OnLocalCameraAdded,
												OnLocalCameraRemoved,
												OnLocalCameraSelected,
												OnLocalCameraStateUpdated) ) {
					Logger::Instance().MsgBox( "OnCamera registration failed." );
				}

				// Register for Microphone callbacks
				if ( !VidyoConnectorRegisterLocalMicrophoneEventListener( &vc,
													OnLocalMicrophoneAdded,
													OnLocalMicrophoneRemoved,
													OnLocalMicrophoneSelected,
													OnLocalMicrophoneStateUpdated) ) {
					Logger::Instance().MsgBox( "OnMicrophone registration failed" );
				}

				// Register for Speaker/output callbacks
				if ( !VidyoConnectorRegisterLocalSpeakerEventListener( &vc,
												OnLocalSpeakerAdded,
												OnLocalSpeakerRemoved,
												OnLocalSpeakerSelected,
												OnLocalSpeakerStateUpdated) ) {
					Logger::Instance().MsgBox( "OnSpeaker registration failed." );
				}

				// Register for monitor callbacks
				if ( !VidyoConnectorRegisterLocalMonitorEventListener( &vc,
												OnLocalMonitorAdded,
												OnLocalMonitorRemoved,
												OnLocalMonitorSelected,
												OnLocalMonitorStateUpdated) ) {
					Logger::Instance().MsgBox( "LocalMonitorEventListener registration failed." );
				}

				// Register for window share callbacks
				if ( !VidyoConnectorRegisterLocalWindowShareEventListener( &vc,
												OnLocalWindowShareAdded,
												OnLocalWindowShareRemoved,
												OnLocalWindowShareSelected,
												OnLocalWindowShareStateUpdated) ) {
					Logger::Instance().MsgBox( "LocalWindowShareEventListener registration failed." );
				}

				// Register for participant callbacks
				if ( !VidyoConnectorRegisterParticipantEventListener( &vc,
												OnParticipantJoined,
												OnParticipantLeft,
												OnDynamicParticipantChanged,
												OnLoudestParticipantChanged ) ) {
					Logger::Instance().MsgBox( "ParticipantEventListener registration failed." );
				}

				// Register for log callbacks
				if ( !VidyoConnectorRegisterLogEventListener( &vc,
											OnLog, "info@VidyoClient info@VidyoConnector warning" ) ) {
					Logger::Instance().MsgBox( "OnLog registration failed." );
				}

				// If cameraPrivacy is configured then mute the camera
				if ( cameraPrivacy ) {
					SetCameraPrivacy();
				}

				// If microphonePrivacy is configured then mute the microphone
				if ( microphonePrivacy ) {
					SetMicrophonePrivacy();
				}

				// If configured to auto-join, then connect to the VidyoConnector
				if ( autoJoin ) {
					Connect();
				}

				// If enableDebug is configured then enable debugging
				if ( enableDebug ) {
					VidyoConnectorEnableDebug( &vc, 7776, "info@VidyoClient info@VidyoConnector warning" );
					CheckMenuItem( conferenceSubmenu, ID_VC_TOGGLE_DEBUG, MF_BYCOMMAND | MF_CHECKED );
				}

				// Set experimental options
				VidyoConnectorSetExperimentalOptions( experimentalOptions );
			} else {
				Logger::Instance().MsgBox( "VidyoConnectorConstruct failed - cannot connect..." );
			}
			break;
		}
		case WM_GETMINMAXINFO: {
			// Set the minimimum height and width of the window
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
			lpmmi->ptMinTrackSize.x = 2 * CONTROLS_VIEW_WIDTH;
			lpmmi->ptMinTrackSize.y = 650;
			break;
		}

		case WM_SIZE:
			// Resize the video upon a window resize
			RefreshUI();
			break;

		case WM_PAINT:
			hdc = BeginPaint( hWnd, &ps );

			// Add any drawing code here...

			EndPaint( hWnd, &ps );
			break;

		case WM_ERASEBKGND: {
			// Set background color

			HBRUSH hbrWhite = ( HBRUSH )GetStockObject( WHITE_BRUSH );
			//HBRUSH hbrGray  = (HBRUSH)GetStockObject(LTGRAY_BRUSH);

			RECT rc;
			hdc = ( HDC )wParam;
			GetClientRect( hWnd, &rc );
			FillRect( hdc, &rc, hbrWhite );

			return 1L;
		}
		case WM_CTLCOLORSTATIC:
			// Set the background of the static text fields

			hdc = ( HDC )wParam;

			// Make the toolbar background black with white text and all other static text fields white with black text
			if ( hToolbar == ( HWND )lParam || hParticipantStatus == ( HWND )lParam ) {
				SetTextColor( hdc, RGB( 255, 255, 255 ) ); // White
				SetBkColor( hdc, RGB( 0, 0, 0 ) );         // Black
				SetBkMode( hdc, TRANSPARENT );
				return ( INT_PTR )hBlackBrush;
			} else {
				//COLORREF grayColor = RGB(214, 211, 206);
				//HBRUSH grayBrush = CreateSolidBrush(grayColor);

				SetTextColor( hdc, RGB( 0, 0, 0 ) );     // Black
				SetBkColor( hdc, RGB( 255, 255, 255 ) ); // White
				return ( INT_PTR )hWhiteBrush;
			}
			break;

		case WM_CLOSE:
			// Clean up the app for graceful exit
			CleanUp( hWnd );
			break;

		case WM_DESTROY:
			// Destroy/clean-up items in the reverse order of their creation.

			// Release the devices from the VidyoConnector
			if ( selectedLocalCamera ) {
				VidyoLocalCameraDestruct( selectedLocalCamera );
				free( selectedLocalCamera );
				selectedLocalCamera = NULL;
			}
			VidyoConnectorSelectLocalCamera( &vc, NULL );
			VidyoConnectorSelectLocalMicrophone( &vc, NULL );
			VidyoConnectorSelectLocalSpeaker( &vc, NULL );

			// Destroy the VidyoConnector
			VidyoConnectorDestruct( &vc );

			// Uninitialize VidyoClient library - this should only be called once in the lifetime of the application.
			VidyoConnectorUninitialize();

			// Delete the brushes
			DeleteObject( hWhiteBrush );
			DeleteObject( hBlackBrush );

			// Shutdown Gdiplus
			Gdiplus::GdiplusShutdown( gdiPlusToken );

			// Finished app-specific clean-up. Ready to quit.
			PostQuitMessage( 0 );
			break;
		case WM_COPYDATA:
			//if we are connected to a vidyo call just ignore the incoming request
			if (vidyoConnectorState == VC_CONNECTED){
				break;
			}
			pcds = (COPYDATASTRUCT*)lParam;
			lpszString = (LPCTSTR)(pcds->lpData);
			urlString = std::wstring(lpszString);
			size_t ret;
			wcstombs_s(&ret, urlPath, INTERNET_MAX_URL_LENGTH, urlString.c_str(), INTERNET_MAX_URL_LENGTH);
			arg.assign(urlString);
			if (arg.find(_T("\"")) == 0){
				arg.erase(0, 1);
			}
			if (arg.find_last_of(_T("\"")) == arg.length() - 1){
				arg.erase(arg.length() - 1, 1);
			}
			extractFieldsFromUrl(arg, true);
			SetUIControlsToNewState();
		case WM_DPICHANGED:
			OutputDebugString((LPCWSTR)"WM_DPIChanged");
		default:
			return DefWindowProc( hWnd, message, wParam, lParam );
	}
	return 0;
}



// Update the status text on the right side of the toolbar
void UpdateToolbarStatus( std::string str ) {
	Logger::Instance().Log( str.c_str() );

	str.append( "   " );
	SetWindowTextA( hToolbar, str.c_str() );
	RedrawWindow(hToolbar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

// Update the participant status text on the left side of the toolbar
void UpdateParticipantStatus( std::string str ) {
	Logger::Instance().Log( str.c_str() );

	str.insert(0, "   ");
	SetWindowTextA( hParticipantStatus, str.c_str() );
	RedrawWindow(hParticipantStatus, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

// Signal the VidyoConnector object to connect to the backend.
void Connect() {
	// Abort the Connect call if resourceId is invalid. It cannot contain empty spaces or "@".
	GetWindowTextA( hPinEdit, ( LPSTR )roomPin, _countof( roomPin ) );
	//GetWindowText( hResourceIdEdit, ( LPWSTR )resourceId,  256 ); // unicode 16
	std::string pin( roomPin );
	if ( pin.find( " " ) != std::string::npos || pin.find( "@" ) != std::string::npos ) {
		UpdateToolbarStatus( "Invalid pin" );
	} else {
		// Obtains input from the textboxes and puts into the char array
		GetWindowTextA( hPortalEdit,        ( LPSTR )portal, _countof(portal));
		GetWindowTextA( hRoomkeyEdit,       ( LPSTR )roomKey, _countof(roomKey));
		GetWindowTextA( hDisplayNameEdit, ( LPSTR )displayName, _countof(displayName));

		UpdateToolbarStatus( "Connecting..." );

		// Connect to either a Vidyo.io resource or a VidyoCloud room.
		LmiBool status;
		status = VidyoConnectorConnectToRoomAsGuest(&vc,
			portal,
			displayName,
			roomKey,
			roomPin,
			OnConnected,
			OnConnectionFailed,
			OnDisconnected);

		if ( status == LMI_FALSE ) {
			ConnectorStateUpdated( VC_CONNECTION_FAILURE, "Connection failed", false );
		}
		else {
			// Change image of hConnectButton to the call end image
			SetWindowIcon( hConnectButton, ( HMENU )IDB_CALL_END, connectRect );
		}

		std::stringstream ss;
		ss << "VidyoConnectorConnect status = " << ( int )status;
		Logger::Instance().Log( ss.str().c_str() );
	}
}

// Signal the VidyoConnector object to disconnect from the backend.
void Disconnect() {
	UpdateToolbarStatus( "Disconnecting..." );

	VidyoConnectorDisconnect( &vc );
}

// Update the UI in either preview or full-screen mode
void RefreshUI() {
	// Get dimensions of master window
	RECT masterRect;
	GetClientRect( hMasterWnd, &masterRect );
	int width = 0;
	int xPos  = 0;

	if ( ( vidyoConnectorState == VC_CONNECTED ) || hideConfig ) {
		// Set width to fill entire screen
		width = masterRect.right;
	} else {
		// Set width to fill right side of screen
		xPos = scaleX(CONTROLS_VIEW_WIDTH);
		width = masterRect.right - scaleX(CONTROLS_VIEW_WIDTH);
	}

	// Set the position of the video
	SetWindowPos( hVideoPanel,
				  HWND_TOP,
				  xPos,
				  0,
				  width,
				  masterRect.bottom - scaleY(TOOLBAR_HEIGHT),
				  SWP_SHOWWINDOW );

	// Resize the VidyoConnector
	VidyoConnectorShowViewAt(&vc, NULL, 0, 0, width, masterRect.bottom - scaleY(TOOLBAR_HEIGHT));

	/////////////////////////////// resize the toolbar
	LONG participantStatusWidth = masterRect.right / 2 - scaleX(120);
	SetWindowPos( hToolbar,
				  HWND_TOP,
				  participantStatusWidth,
				  masterRect.bottom - scaleY(TOOLBAR_HEIGHT),
				  masterRect.right - participantStatusWidth,
				  scaleY(TOOLBAR_HEIGHT),
				  SWP_SHOWWINDOW );
	SetWindowPos( hParticipantStatus,
				  HWND_TOP,
				  0,
				  masterRect.bottom - scaleY(TOOLBAR_HEIGHT),
				  participantStatusWidth,
				  scaleY(TOOLBAR_HEIGHT),
				  SWP_SHOWWINDOW );

	// Place the connect button accordingly: centered in the toolbar
	connectRect.x = (masterRect.right / 2) - connectRect.width / 2;
	connectRect.y = masterRect.bottom - (scaleY(TOOLBAR_HEIGHT) - (scaleY(TOOLBAR_HEIGHT) - connectRect.height) / 2);

	SetWindowPos( hConnectButton,
				  hToolbar,
				  connectRect.x,
				  connectRect.y,
				  connectRect.width,
				  connectRect.height,
				  SWP_SHOWWINDOW );

	// Place the camera mute button to the left of the connect button
	cameraMuteRect.x = connectRect.x - cameraMuteRect.width - scaleX(60);
	cameraMuteRect.y = connectRect.y;
	SetWindowPos( hCameraMuteButton,
				  hToolbar,
				  cameraMuteRect.x,
				  cameraMuteRect.y,
				  cameraMuteRect.width,
				  cameraMuteRect.height,
				  SWP_SHOWWINDOW );

	// Place the mic mute button to the right of the connect button
	micMuteRect.x = connectRect.x + connectRect.width + scaleX(60);
	micMuteRect.y = connectRect.y;
	SetWindowPos( hMicMuteButton,
				  hToolbar,
				  micMuteRect.x,
				  micMuteRect.y,
				  micMuteRect.width,
				  micMuteRect.height,
				  SWP_SHOWWINDOW );
	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Tell the window it needs to redraw (sends a WM_PAINT message)
	//InvalidateRect(hMasterWnd, NULL, true);
	RedrawWindow( hMasterWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW );
}

// Hide or show the controls view windows
void UpdateControlsView( bool hidden ) {
	int nCmdShow = hidden ? SW_HIDE : SW_SHOW;

	ShowWindowAsync( hPortalText,        nCmdShow );
	ShowWindowAsync( hPortalEdit,        nCmdShow );
	ShowWindowAsync( hRoomkeyText,       nCmdShow );
	ShowWindowAsync( hRoomkeyEdit,       nCmdShow );
	ShowWindowAsync( hDisplayNameText, nCmdShow );
	ShowWindowAsync( hDisplayNameEdit, nCmdShow );
	ShowWindowAsync( hPinText,  nCmdShow );
	ShowWindowAsync( hPinEdit,  nCmdShow );
	ShowWindowAsync( hVidyoIcon,       nCmdShow );
}

void SetUIControlsToNewState(){
	if (startupStringContainedRoomKey){
		SetWindowTextA(hRoomkeyEdit, (LPCSTR)roomKey);
	}
	if (startupStringContainedPortal){
		SetWindowTextA(hPortalEdit, (LPCSTR)portal);
	}
	if (startupStringContainedDisplayName){
		SetWindowTextA(hDisplayNameEdit, (LPCSTR)displayName);
	}
	if (startupStringContainedPin){
		SetWindowTextA(hPinEdit, (LPCSTR)roomPin);
	}
	if (startupStringContainedCameraPrivacy){
		if (cameraPrivacy){
			SetCameraPrivacy();
		}
	}
	if (startupStringContainedMicrophonePrivacy){
		if (microphonePrivacy) {
			SetMicrophonePrivacy();
		}
	}
	if (startupStringContainedEnableDebug){
		if (enableDebug) {
			VidyoConnectorEnableDebug(&vc, 7776, "info@VidyoClient info@VidyoConnector warning");
			CheckMenuItem(conferenceSubmenu, ID_VC_TOGGLE_DEBUG, MF_BYCOMMAND | MF_CHECKED);
		}
	}
	if (startupStringContainedExperimentalOptions){
		VidyoConnectorSetExperimentalOptions(experimentalOptions);
	}
	if (startupStringContainedHideConfig){
		if (hideConfig) {
			// Get dimensions of master window
			RECT masterRect;
			GetClientRect(hMasterWnd, &masterRect);

			// Update the videoPanel frame to be full-screen
			SetWindowPos(hVideoPanel,
				HWND_TOP,
				0,
				0,
				masterRect.right,
				masterRect.bottom - TOOLBAR_HEIGHT,
				SWP_SHOWWINDOW);
		}
		else {
			// Display the controls view which is not visible when option controls are created
			UpdateControlsView(false);
		}
	}
	if (startupStringContainedAutoJoin){
		if (autoJoin) {
			Connect();
		}
	}
}

// The state of the VidyoConnector connection changed, reconfigure the UI.
// If connected, show the video in the entire window.
// If disconnected, show the video in the preview pane.
void ConnectorStateUpdated( VIDYO_CONNECTOR_STATE state, const char* statusText, bool refreshUI ) {
	vidyoConnectorState = state;

	// Set the status text in the toolbar
	UpdateToolbarStatus( statusText );

	if ( vidyoConnectorState == VC_CONNECTED ) {

		if ( !hideConfig && refreshUI ) {
			// Update the view to hide the controls
			UpdateControlsView( true );

			// Update the videoPanel frame to be full-screen
			RefreshUI();
		}
	} else {
		// VidyoConnector is disconnected

		// Change image of hConnectButton to the call start image
		SetWindowIcon(hConnectButton, (HMENU)IDB_CALL_START, connectRect);

		UpdateParticipantStatus("");

		// If the allow-reconnect flag is set to false and a normal (non-failure) disconnect occurred,
		// then disable the toggle connect button, in order to prevent reconnection.
		if ( !allowReconnect && ( vidyoConnectorState == VC_DISCONNECTED ) ) {
			EnableWindow( hConnectButton, FALSE );
			UpdateToolbarStatus( "Call ended" );

			// Disable the Connect and Disconnect menu items
			EnableMenuItem( conferenceSubmenu, ID_VC_CONNECT, MF_GRAYED );
			EnableMenuItem( conferenceSubmenu, ID_VC_DISCONNECT, MF_GRAYED );
		}

		if ( !hideConfig && refreshUI ) {
			// Update the view to display the controls
			UpdateControlsView( false );

			// Update the video frame to be in preview mode
			RefreshUI();
		}
	}
}

// Intialize the user interface
void InitializeUI( HWND hWnd ) {
	// Create a Calibri font
	HDC hdc = GetDC( NULL );
	long lfHeight = -MulDiv( 12, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
	dpiX = static_cast<FLOAT>(GetDeviceCaps(hdc, LOGPIXELSX));
	dpiY = static_cast<FLOAT>(GetDeviceCaps(hdc, LOGPIXELSY));
	ReleaseDC( NULL, hdc );
	HFONT font = CreateFont( lfHeight, 0, 0, 0, 550, 0, 0, 0, 0, 0, 0, 0, 0, L"Calibri" );
	//HGDIOBJ hfDefault = GetStockObject(DEFAULT_GUI_FONT);

	// scale rect width/height with scale factor (relevent to high DPI monitor)
	scaleRect(&vidyoIconRect);
	scaleRect(&portalTextRect);
	scaleRect(&portalEditRect);
	scaleRect(&roomkeyTextRect);
	scaleRect(&roomkeyEditRect);
	scaleRect(&displayNameTextRect);
	scaleRect(&displayNameEditRect);
	scaleRect(&pinTextRect);
	scaleRect(&pinEditRect);
	scaleRect(&cameraMuteRect);
	scaleRect(&connectRect);
	scaleRect(&micMuteRect);

	// Get dimensions of master window
	RECT masterRect;
	GetClientRect( hWnd, &masterRect );

	///////////////////////////////////////////////////////////////////////////////////////

	// Initialize button locations

	// Set the connect button rect accordingly: centered and just above bottom of screen
	connectRect.x = (masterRect.right / 2) - scaleX(connectRect.width) / 2;
	connectRect.y = masterRect.bottom - scaleY(TOOLBAR_HEIGHT);

	// Set the camera mute button rect to the left of the connect button
	cameraMuteRect.x = connectRect.x - scaleX(cameraMuteRect.width - 20);
	cameraMuteRect.y = connectRect.y;

	// Set the mic mute button rect to the right of the connect button
	micMuteRect.x = connectRect.x + scaleX(connectRect.width + 20);
	micMuteRect.y = connectRect.y;

	///////////////////////////////////////////////////////////////////////////////////////

	// Load Vidyo icon pic

	if ( hVidyoIcon = CreateWindowExA( NULL,
									   "STATIC",
									   NULL,
									   WS_CHILD | SS_BITMAP,
									   vidyoIconRect.x,
									   vidyoIconRect.y,
									   vidyoIconRect.width,
									   vidyoIconRect.height,
									   hWnd,
									   NULL,
									   GetModuleHandle( NULL ),
									   NULL ) ) {
		SetWindowIcon( hVidyoIcon, ( HMENU )IDB_VIDYO_ICON, vidyoIconRect );
	} else {
		Logger::Instance().MsgBox( "Vidyo Icon Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the video panel

	// Set width to fill right side of screen

	if ( ( hVideoPanel = CreateWindowExA( 0,
										  "STATIC",
										  "",
										  WS_CHILD | WS_VISIBLE | SS_BLACKFRAME | SS_SUNKEN,
										  scaleX(CONTROLS_VIEW_WIDTH),
										  0,
										  masterRect.right - scaleX(CONTROLS_VIEW_WIDTH),
										  masterRect.bottom,
										  hWnd,
										  NULL,
										  GetModuleHandle( NULL ),
										  NULL ) ) == NULL ) {
		Logger::Instance().MsgBox( "Video Panel Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the host text box
	if ( ( hPortalText = CreateWindowExA( 0,
										"STATIC",
										"Portal",
										WS_CHILD | SS_RIGHT,
										portalTextRect.x,
										portalTextRect.y,
										portalTextRect.width,
										portalTextRect.height,
										hWnd,
										( HMENU )IDC_HOST_TEXT,
										GetModuleHandle( NULL ),
										NULL ) ) != NULL ) {
		SendMessage( hPortalText, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
	} else {
		Logger::Instance().MsgBox( "Host Text Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the host edit box

	if ( ( hPortalEdit = CreateWindowExA( WS_EX_CLIENTEDGE,
										"EDIT",
										portal,
										WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
										portalEditRect.x,
										portalEditRect.y,
										portalEditRect.width,
										portalEditRect.height,
										hWnd,
										( HMENU )IDC_HOST_EDIT,
										GetModuleHandle( NULL ),
										NULL ) ) != NULL ) {
		SendMessage( hPortalEdit, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
		SendMessage( hPortalEdit, EM_LIMITTEXT, _countof(portal) - sizeof('\0'), 0);
	} else {
		Logger::Instance().MsgBox( "Host Edit Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the roomkey text box
	if ( ( hRoomkeyText = CreateWindowExA( 0,
										 "STATIC",
										 "Roomkey",
										 WS_CHILD | SS_RIGHT,
										 roomkeyTextRect.x,
										 roomkeyTextRect.y,
										 roomkeyTextRect.width,
										 roomkeyTextRect.height,
										 hWnd,
										 ( HMENU )IDC_TOKEN_TEXT,
										 GetModuleHandle( NULL ),
										 NULL ) ) != NULL ) {
		SendMessage( hRoomkeyText, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
	} else {
		Logger::Instance().MsgBox( "roomkey Text Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the roomkey edit box

	if ( ( hRoomkeyEdit = CreateWindowExA( WS_EX_CLIENTEDGE,
										 "EDIT",
										 roomKey,
										 WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
										 roomkeyEditRect.x,
										 roomkeyEditRect.y,
										 roomkeyEditRect.width,
										 roomkeyEditRect.height,
										 hWnd,
										 ( HMENU )IDC_TOKEN_EDIT,
										 GetModuleHandle( NULL ),
										 NULL ) ) != NULL ) {
		SendMessage( hRoomkeyEdit, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
		SendMessage( hRoomkeyEdit, EM_LIMITTEXT, ROOM_KEY_STRLEN_MAX, 0 );
	} else {
		Logger::Instance().MsgBox( "roomkey Edit Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the display name text box
	if ( ( hDisplayNameText = CreateWindowExA( 0,
											   "STATIC",
											   "Display Name",
											   WS_CHILD | SS_RIGHT,
											   displayNameTextRect.x,
											   displayNameTextRect.y,
											   displayNameTextRect.width,
											   displayNameTextRect.height,
											   hWnd,
											   ( HMENU )IDC_DISPLAY_NAME_TEXT,
											   GetModuleHandle( NULL ),
											   NULL ) ) != NULL ) {
		SendMessage( hDisplayNameText, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
	}
	else {
		Logger::Instance().MsgBox( "Display Name Text Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the display name edit box

	if ( ( hDisplayNameEdit = CreateWindowExA( WS_EX_CLIENTEDGE,
											   "EDIT",
											   displayName,
											   WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
											   displayNameEditRect.x,
											   displayNameEditRect.y,
											   displayNameEditRect.width,
											   displayNameEditRect.height,
											   hWnd,
											   ( HMENU )IDC_DISPLAY_NAME_EDIT,
											   GetModuleHandle( NULL ),
											   NULL ) ) != NULL ) {
		SendMessage( hDisplayNameEdit, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
		SendMessage( hDisplayNameEdit, EM_LIMITTEXT, _countof(displayName) - sizeof('\0'), 0);
	}
	else {
		Logger::Instance().MsgBox( "Display Name Edit Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the resource ID text box
	if ( ( hPinText = CreateWindowExA( 0,
											  "STATIC",
											  "Pin",
											  WS_CHILD | SS_RIGHT,
											  pinTextRect.x,
											  pinTextRect.y,
											  pinTextRect.width,
											  pinTextRect.height,
											  hWnd,
											  ( HMENU )IDC_RESOURCEID_TEXT,
											  GetModuleHandle( NULL ),
											  NULL ) ) != NULL ) {
		SendMessage( hPinText, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
	} else {
		Logger::Instance().MsgBox(" Resouce ID Text Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the resouce ID edit box

	if ( ( hPinEdit = CreateWindowExA( WS_EX_CLIENTEDGE,
											  "EDIT",
											  roomPin,
											  WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
											  pinEditRect.x,
											  pinEditRect.y,
											  pinEditRect.width,
											  pinEditRect.height,
											  hWnd,
											  ( HMENU )IDC_RESOURCEID_EDIT,
											  GetModuleHandle( NULL ),
											  NULL ) ) != NULL ) {
		SendMessage( hPinEdit, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
		SendMessage( hPinEdit, EM_LIMITTEXT, _countof(roomPin) - sizeof('\0'), 0);
	} else {
		Logger::Instance().MsgBox( "Resource ID Edit Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the camera mute button

	// Making this a STATIC rather than a BUTTON in order to have an image without a lined border
	if ( hCameraMuteButton = CreateWindowExA( NULL,
											  "STATIC",
											  NULL,
											  WS_VISIBLE | WS_CHILD | SS_NOTIFY | SS_BITMAP,
											  cameraMuteRect.x,
											  cameraMuteRect.y,
											  cameraMuteRect.width,
											  cameraMuteRect.height,
											  hWnd,
											  ( HMENU )IDC_CAMERA_MUTE_BUTTON,
											  GetModuleHandle( NULL ),
											  NULL ) ) {
		SetWindowIcon( hCameraMuteButton, ( HMENU )IDB_CAMERA_ON, cameraMuteRect );
	} else {
		Logger::Instance().MsgBox( "Camera Mute Button Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the mic mute button

	// Making this a STATIC rather than a BUTTON in order to have an image without a lined border
	if ( hMicMuteButton = CreateWindowExA( NULL,
										   "STATIC",
										   NULL,
										   WS_VISIBLE | WS_CHILD | SS_NOTIFY | SS_BITMAP,
										   micMuteRect.x,
										   micMuteRect.y,
										   micMuteRect.width,
										   micMuteRect.height,
										   hWnd,
										   ( HMENU )IDC_MIC_MUTE_BUTTON,
										   GetModuleHandle( NULL ),
										   NULL ) ) {
		SetWindowIcon( hMicMuteButton, ( HMENU )IDB_MIC_ON, micMuteRect );
	} else {
		Logger::Instance().MsgBox( "Microphone Mute Button Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create the connect button

	// Making this a STATIC rather than a BUTTON in order to have an image without a lined border
	if ( hConnectButton = CreateWindowExA( NULL,
										   "STATIC",
										   NULL,
										   WS_VISIBLE | WS_CHILD | SS_NOTIFY | SS_BITMAP,
										   connectRect.x,
										   connectRect.y,
										   connectRect.width,
										   connectRect.height,
										   hWnd,
										   ( HMENU )IDC_TOGGLE_CONNECT_BUTTON,
										   GetModuleHandle( NULL ),
										   NULL ) ) {
		SetWindowIcon( hConnectButton, ( HMENU )IDB_CALL_START, connectRect );
	} else {
		Logger::Instance().MsgBox( "Connect Button Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create toolbar text box with black background at bottom of screen
	if ( hToolbar = CreateWindowExA( 0,
                                     "STATIC",
									 "Ready to Connect   ",
									 WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
									 scaleX(PARTICIPANT_STATUS_INIT_WIDTH),
									 masterRect.bottom - scaleY(TOOLBAR_HEIGHT),
									 masterRect.right,
									 scaleY(TOOLBAR_HEIGHT),
									 hWnd,
									 ( HMENU )IDC_TOOLBAR_TEXT,
									 GetModuleHandle(NULL),
									 NULL ) ) {
		SendMessage( hToolbar, WM_SETFONT, ( WPARAM )font, MAKELPARAM( FALSE, 0 ) );
	} else {
		Logger::Instance().MsgBox("Black Background Text Window Creation Failed!");
	}

	///////////////////////////////////////////////////////////////////////////////////////

	// Create participant status text box with black background at bottom of screen
	if ( hParticipantStatus = CreateWindowExA( 0,
									"STATIC",
									"",
									WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
									0,
									masterRect.bottom - scaleY(TOOLBAR_HEIGHT),
									scaleX(PARTICIPANT_STATUS_INIT_WIDTH),
									scaleY(TOOLBAR_HEIGHT),
									hWnd,
									( HMENU )IDC_PARTICIPANT_STATUS,
									GetModuleHandle( NULL ),
									NULL ) ) {
		SendMessage( hParticipantStatus, WM_SETFONT, (WPARAM)font, MAKELPARAM( FALSE, 0 ) );
	}
	else {
		Logger::Instance().MsgBox( "Participant Status Text Window Creation Failed!" );
	}

	///////////////////////////////////////////////////////////////////////////////////////

	Logger::Instance().Log( "GUI has been initialized..." );
}

// Set the icon of a static control window
void SetWindowIcon( const HWND &hwnd, HMENU iconResourceId, const WindowRect &rect ) {
	// If the icon was not yet created in the map, then create it
	if ( iconMap.find( iconResourceId ) == iconMap.end() ) {
		// Find the icon's resource and load it into a Gdiplus::Image*
		HRSRC	hRsrc = FindResource( NULL, MAKEINTRESOURCE( iconResourceId ), RT_RCDATA );
		HGLOBAL	hGlob1 = LoadResource( NULL, hRsrc );
		int	size = SizeofResource( NULL, hRsrc );
		HGLOBAL hGlobal = GlobalAlloc( GMEM_FIXED, size );
		LPVOID	resPtr = LockResource( hGlob1 );
		memcpy( hGlobal, resPtr, size );
		FreeResource( hGlob1 );
		LPSTREAM pStream;
		CreateStreamOnHGlobal( hGlobal, true, &pStream );
		Gdiplus::Image* originalImage = new Gdiplus::Image( pStream, false );

		// Scale the icon to fit in the appropriate area
		Gdiplus::Bitmap *scaledBmp = new Gdiplus::Bitmap( rect.width, rect.height );

		// application has DPI awareness setting, take that into consideration when calculating scaleFactor
		float horizontalScalingFactor = ( float )(rect.width) / (dpiX / 96.f) / ( float )originalImage->GetWidth();
		float verticalScalingFactor   = ( float )(rect.height) / (dpiY / 96.f) / ( float )originalImage->GetHeight();

		Gdiplus::Graphics g( scaledBmp );
		g.ScaleTransform( horizontalScalingFactor, verticalScalingFactor );
		g.DrawImage( originalImage, 0, 0 );

		HBITMAP bitmap;
		scaledBmp->GetHBITMAP( Gdiplus::Color::Transparent, &bitmap );

		// Add to map
		iconMap[iconResourceId] = bitmap;

		delete originalImage;
		delete scaledBmp;
	}

	// Set the icon
	SendMessage( hwnd, STM_SETIMAGE, IMAGE_BITMAP, ( LPARAM )iconMap[iconResourceId] );
}

int CreateMenuItem(HMENU hmenu, unsigned long menuOffset, const char *itemName, LocalDevice type) {
	// Create and initialize fields of a menu item
	MENUITEMINFOA menuItemInfo;
	memset( &menuItemInfo, 0, sizeof( MENUITEMINFOA ) );
	menuItemInfo.cbSize = sizeof( MENUITEMINFOA );
	menuItemInfo.fMask = MIIM_ID | MIIM_STRING | MIIM_DATA;

	// Populate the menu item and insert it
	menuItemInfo.wID = GetUniqueId( type );
	menuItemInfo.dwTypeData = const_cast<char *>( itemName );
	menuItemInfo.cch = strlen( itemName );
	InsertMenuItemA( hmenu, menuOffset, TRUE, &menuItemInfo);

	return menuItemInfo.wID;
}

void SetCameraPrivacy() {
	if (cameraPrivacy == LMI_FALSE) {
		SetWindowIcon(hCameraMuteButton, (HMENU)IDB_CAMERA_ON, cameraMuteRect);
		Logger::Instance().Log("Camera Privacy: show camera");
	}
	else {
		SetWindowIcon(hCameraMuteButton, (HMENU)IDB_CAMERA_OFF, cameraMuteRect);
		Logger::Instance().Log("Camera Privacy: hide camera");
	}

	VidyoConnectorSetCameraPrivacy(&vc, cameraPrivacy);
}

void SetMicrophonePrivacy() {
	if (microphonePrivacy == LMI_FALSE) {
		SetWindowIcon(hMicMuteButton, (HMENU)IDB_MIC_ON, micMuteRect);
		Logger::Instance().Log("Microphone Privacy: unmute microphone");
	}
	else {
		SetWindowIcon(hMicMuteButton, (HMENU)IDB_MIC_OFF, micMuteRect);
		Logger::Instance().Log("Microphone Privacy: mute microphone");
	}

	VidyoConnectorSetMicrophonePrivacy(&vc, microphonePrivacy);
}

void SetMaxResolution( UINT menuItemId ) {
	unsigned int height = 0, width = 0;
	long frameInterval = 1000000000 / 30; // 30 frames per second

	// Update checkmark in menu if selected resolution has changed
	if ( menuItemId != selectedMaxResolutionMenuItem ) {
		CheckMenuItem( conferenceSubmenu, selectedMaxResolutionMenuItem, MF_BYCOMMAND | MF_UNCHECKED );
		CheckMenuItem( conferenceSubmenu, menuItemId, MF_BYCOMMAND | MF_CHECKED );
		selectedMaxResolutionMenuItem = menuItemId;
	}

	switch ( menuItemId ) {
		case ID_MAX_RESOLUTION_180P:
			width  = 320;
			height = 180;
			break;
		case ID_MAX_RESOLUTION_270P:
			width  = 480;
			height = 270;
			break;
		case ID_MAX_RESOLUTION_360P:
			width  = 640;
			height = 360;
			break;
		case ID_MAX_RESOLUTION_540P:
			width  = 960;
			height = 540;
			break;
		case ID_MAX_RESOLUTION_720P:
			width  = 1280;
			height = 720;
			break;
		case ID_MAX_RESOLUTION_1080P:
			width  = 1920;
			height = 1080;
			break;
		case ID_MAX_RESOLUTION_2160P:
			width  = 3840;
			height = 2160;
			break;
		default:
			Logger::Instance().Log( "Warning: unexpected max resolution selected" );
			break;
	}

	// Set the max constraint as long as a local camera is selected
	if ( selectedLocalCamera && width > 0 ) {
		VidyoLocalCameraSetMaxConstraint( selectedLocalCamera, width, height, frameInterval );
	}
}

void SetExperimentalOptions( UINT menuItemId ) {
	std::string experimentalOptions_("{");

	// Toggle the state of the menu item (add or remove checkmark)
	UINT state = GetMenuState( conferenceSubmenu, menuItemId, MF_BYCOMMAND );
	if ( !( state & MF_CHECKED ) ) {
		CheckMenuItem( conferenceSubmenu, menuItemId, MF_BYCOMMAND | MF_CHECKED );
	} else {
		CheckMenuItem( conferenceSubmenu, menuItemId, MF_BYCOMMAND | MF_UNCHECKED );
	}

	// Iterate through all the experiemntal menu options to find their cumulative states
	std::string value;
	value = ( GetMenuState( conferenceSubmenu, ID_EXPERIMENTAL_PTZ, MF_BYCOMMAND ) & MF_CHECKED ) ? "true" : "false";
	experimentalOptions_.append( "\"PTZ\":" + value + "," );

	value = ( GetMenuState( conferenceSubmenu, ID_EXPERIMENTAL_FORCE_DIG_PTZ, MF_BYCOMMAND ) & MF_CHECKED ) ? "true" : "false";
	experimentalOptions_.append( "\"ForceDigitalPTZ\":" + value + "," );

	value = ( GetMenuState( conferenceSubmenu, ID_EXPERIMENTAL_VP9, MF_BYCOMMAND ) & MF_CHECKED ) ? "true" : "false";
	experimentalOptions_.append( "\"VP9\":" + value + "}" );

	VidyoConnectorSetAdvancedOptions( &vc, experimentalOptions_.c_str() );
}

// Get a unique ID for a particular device
int GetUniqueId( LocalDevice device ) {
	int id = 0;

	switch ( device ) {
		case LOCAL_CAMERA:
			for ( int i = ID_DEVICES_CAMERA_ITEMS_MIN; i <= ID_DEVICES_CAMERA_ITEMS_MAX; ++i ) {
				if ( cameraMap.find( i ) == cameraMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case LOCAL_MICROPHONE:
			for ( int i = ID_DEVICES_MICROPHONE_ITEMS_MIN; i <= ID_DEVICES_MICROPHONE_ITEMS_MAX; ++i ) {
				if ( microphoneMap.find( i ) == microphoneMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case LOCAL_SPEAKER:
			for ( int i = ID_DEVICES_SPEAKER_ITEMS_MIN; i <= ID_DEVICES_SPEAKER_ITEMS_MAX; ++i ) {
				if ( speakerMap.find( i ) == speakerMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case LOCAL_MONITOR:
			for ( int i = ID_MONITOR_SHARE_ITEMS_MIN; i <= ID_MONITOR_SHARE_ITEMS_MAX; ++i ) {
				if ( monitorShareMap.find( i ) == monitorShareMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case LOCAL_WINDOW:
			for ( int i = ID_WINDOW_SHARE_ITEMS_MIN; i <= ID_WINDOW_SHARE_ITEMS_MAX; ++i ) {
				if ( windowShareMap.find( i ) == windowShareMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case VIDEO_CONTENT_SHARE:
			for ( int i = ID_VIDEO_CONTENT_SHARE_MIN; i <= ID_VIDEO_CONTENT_SHARE_MAX; ++i ) {
				if ( videoContentShareMap.find( i ) == videoContentShareMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		case AUDIO_CONTENT_SHARE:
			for ( int i = ID_AUDIO_CONTENT_SHARE_MIN; i <= ID_AUDIO_CONTENT_SHARE_MAX; ++i ) {
				if ( audioContentShareMap.find( i ) == audioContentShareMap.end() ) {
					id = i;
					break;
				}
			}
			break;

		default:
			break;
	}
	return id;
}

// Prepare for the application to quit gracefully
void CleanUp( HWND hWnd ) {
	// Prepare to notify other parts of this app that it is quitting;
	//  Asynchronous or time-consuming functions should check this flag.
	appIsQuitting = true;

	// Since this app is designed with a single main window, closing it means that the app is quitting.
	// In order to quit gracefully, make sure that the VidyoClient is disconnected.
	if ( VC_CONNECTED == vidyoConnectorState ) {
		VidyoConnectorDisconnect( &vc );
		// ...Disconnecting can take time, and it might involve other threads.
		// Therefore, use a wait-loop to check when that operation has completed.
		// The wait condition should occur upon the "disconnected" callback.
		// Wait for a limited time; Let any other threads run while this waits.
		const size_t WAIT_DISCONNECT_MS = 9000; // Total wait time, in milliseconds.
		const size_t WAIT_ITERATION_MS  = 100; // Duration of each iteration, in milliseconds.
		for ( size_t iters = 0; iters < ( WAIT_DISCONNECT_MS / WAIT_ITERATION_MS ); ++iters ) {
			// If desired condition occurs, exit this wait-loop early.
			if ( VC_CONNECTED != vidyoConnectorState ) {
				break;
			}
			Sleep( WAIT_ITERATION_MS );
		}
	} else {
		// Already disconnected: Cannot use the "disconnected" callback for clean-up.
		DestroyWindow( hWnd );
	}
}

/*
 *  Connection Events
 */

// Handle successful connection.
void OnConnected( VidyoConnector* c ) {
	Logger::Instance().Log( "Connected." );
	ConnectorStateUpdated( VC_CONNECTED, "Connected", true );
}

// Handle attempted connection failure.
void OnConnectionFailed( VidyoConnector *c, VidyoConnectorFailReason reason ) {
	Logger::Instance().Log( "Connection attempt failed." );
	ConnectorStateUpdated( VC_CONNECTION_FAILURE, "Connection failed", true );
}

// Handle an existing session being disconnected.
void OnDisconnected( VidyoConnector *c, VidyoConnectorDisconnectReason reason ) {
	if ( reason == VIDYO_CONNECTORDISCONNECTREASON_Disconnected ) {
		Logger::Instance().Log(" Successfully disconnected.");
		ConnectorStateUpdated( VC_DISCONNECTED, "Disconnected", true );
	}
	else {
		Logger::Instance().Log( "Unexpected disconnection.");
		ConnectorStateUpdated( VC_DISCONNECTED_UNEXPECTED, "Unexpected disconnection", true );
	}
	// Handle special case: Disconnecting as part of clean-up while app is quitting.
	// - Assumes that app triggered this disconnect during main window's WM_CLOSE.
	// - Assumes that app will perform remaining clean-up upon getting WM_DESTROY.
	if ( appIsQuitting && ( NULL != hMasterWnd ) ) {
		PostMessage( hMasterWnd, WM_DESTROY, 0, 0 );
	}
}

//  Handle a camera being added to the system.
void OnLocalCameraAdded( VidyoConnector *c, VidyoLocalCamera *localCamera ) {
	if ( VidyoConnectorEqual( c, &vc) ) {
		if ( localCamera ) {
			// Add the camera to the camera map

			VidyoLocalCamera *cameraCopy = ( VidyoLocalCamera * )malloc( sizeof( VidyoLocalCamera ) );

			if ( VidyoLocalCameraConstructCopy( cameraCopy, localCamera ) ) {
				deviceLock.lock();

				// Get the name of the camera
				const char *cameraName = LmiStringCStr( VidyoLocalCameraGetName( cameraCopy ) );

				// Create the Camera menu item and add it to cameraMap
				unsigned long menuOffset = NO_CAMERA_MENU_OFFSET + cameraMap.size() + 1;
				int id = CreateMenuItem( devicesSubmenu, menuOffset, cameraName, LOCAL_CAMERA );
				cameraMap[id] = cameraCopy;

				// Create the Video Content Share menu item
				menuOffset = NO_VIDEO_CONTENT_SHARE_MENU_OFFSET + videoContentShareMap.size() + 1;
				id = CreateMenuItem( contentSubmenu, menuOffset, cameraName, VIDEO_CONTENT_SHARE );
				videoContentShareMap[id] = cameraCopy;

				deviceLock.unlock();

				Logger::Instance().Log( "OnLocalCameraAdded success ");
			} else {
				Logger::Instance().Log( "OnLocalCameraAdded failed due to ConstructCopy failure" );
			}
		} else {
			Logger::Instance().Log( "OnLocalCameraAdded received NULL added camera." );
		}
	} else {
		Logger::Instance().Log( "OnLocalCameraAdded unexpected VidyoConnector object received." );
	}
}

// Handle a camera being removed.
void OnLocalCameraRemoved(VidyoConnector *c, VidyoLocalCamera *localCamera ) {
	if ( VidyoConnectorEqual( c, &vc) ) {
		if ( localCamera ) {

			deviceLock.lock();

			// Iterate through the cameraMap
			for ( std::map<int, VidyoLocalCamera*>::iterator it = cameraMap.begin(); it != cameraMap.end(); ++it ) {
				// Check for camera to remove
				if ( VidyoLocalCameraEqual( it->second, localCamera ) ) {
					// remove from menu and remove from map
					RemoveMenu( devicesSubmenu, it->first, MF_BYCOMMAND );
					cameraMap.erase( it );

					// Break out of loop
					break;
				}
			}

			// Iterate through the videoContentShareMap
			for ( std::map<int, VidyoLocalCamera*>::iterator it = videoContentShareMap.begin(); it != videoContentShareMap.end(); ++it ) {
				// Check for camera to remove
				if ( VidyoLocalCameraEqual( it->second, localCamera ) ) {
					// Free the memory, remove from menu, and remove from map
					VidyoLocalCameraDestruct( it->second );
					free( it->second );
					RemoveMenu( contentSubmenu, it->first, MF_BYCOMMAND );
					videoContentShareMap.erase( it );

					// Break out of loop
					break;
				}
			}

			deviceLock.unlock();

			Logger::Instance().Log( "OnLocalCameraRemoved success" );
		} else {
			Logger::Instance().Log( "OnLocalCameraRemoved received NULL removed camera." );
		}
	} else {
		Logger::Instance().Log( "OnLocalCameraRemoved unexpected VidyoConnector object received." );
	}
}

// Handle a camera being selected.
void OnLocalCameraSelected( VidyoConnector *c, VidyoLocalCamera *localCamera ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		Logger::Instance().Log( "OnLocalCameraSelected camera selected." );
		bool found = false;

		// Destruct and free the previously selected local camera
		if ( selectedLocalCamera ) {
			VidyoLocalCameraDestruct( selectedLocalCamera );
			free( selectedLocalCamera );
		}

		// Set the max resolution for the selected local camera
		if ( localCamera ) {
			selectedLocalCamera = (VidyoLocalCamera *)malloc( sizeof( VidyoLocalCamera ) );
			VidyoLocalCameraConstructCopy( selectedLocalCamera, localCamera ); // must be set prior to SetMaxResolution call
			SetMaxResolution( selectedMaxResolutionMenuItem );
		} else {
			selectedLocalCamera = NULL;
		}

		deviceLock.lock();

		// Iterate through the cameraMap
		for ( std::map<int, VidyoLocalCamera*>::iterator it = cameraMap.begin(); it != cameraMap.end(); ++it ) {
			// Check for selected device
			if ( localCamera && VidyoLocalCameraEqual( it->second, localCamera ) ) {
				// Check the selected device in the menu
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_CHECKED );
				found = true;
			} else {
				// Uncheck each device that was not selected
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Set the state of the "None" camera menu item depending if a camera was selected
		int checked = found ? MF_UNCHECKED : MF_CHECKED;
		CheckMenuItem( devicesSubmenu, ID_CAMERA_NONE, MF_BYCOMMAND | checked );

		// Enable or disable the Content (Video Content Share) submenu items accordingly
		for ( std::map<int, VidyoLocalCamera*>::iterator it = videoContentShareMap.begin(); it != videoContentShareMap.end(); ++it ) {
			// Check for selected device
			if ( localCamera && VidyoLocalCameraEqual( it->second, localCamera ) ) {
				// disable in the menu
				EnableMenuItem( contentSubmenu, it->first, MF_GRAYED );
			} else {
				// enable in the menu
				EnableMenuItem( contentSubmenu, it->first, MF_ENABLED );
			}
		}

		deviceLock.unlock();
	} else {
		Logger::Instance().Log( "OnLocalCameraSelected unexpected VidyoConnector object received." );
	}
}

// Handle a camera state update
void OnLocalCameraStateUpdated(VidyoConnector* c, VidyoLocalCamera* localCamera, VidyoDeviceState state) {
	Logger::Instance().Log("OnLocalCameraStateUpdated");
}

// Handle a microphone being added to the system.
void OnLocalMicrophoneAdded( VidyoConnector *c, VidyoLocalMicrophone *localMicrophone ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localMicrophone ) {
			// Add the microphone to the microphone map

			VidyoLocalMicrophone *micCopy = ( VidyoLocalMicrophone * )malloc( sizeof( VidyoLocalMicrophone ) );

			if ( VidyoLocalMicrophoneConstructCopy( micCopy, localMicrophone ) ) {
				deviceLock.lock();

				// Get the name of the microphone
				const char *micName = LmiStringCStr( VidyoLocalMicrophoneGetName( micCopy ) );

				// Create the Microphone menu item and add to microphoneMap
				unsigned long menuOffset = NO_MICROPHONE_MENU_OFFSET + cameraMap.size() + microphoneMap.size() + 1;
				int id = CreateMenuItem( devicesSubmenu, menuOffset, micName, LOCAL_MICROPHONE );
				microphoneMap[id] = micCopy;

				// Create the Audio Content Share menu item
				menuOffset = NO_AUDIO_CONTENT_SHARE_MENU_OFFSET + videoContentShareMap.size() + audioContentShareMap.size() + 1;
				id = CreateMenuItem(contentSubmenu, menuOffset, micName, AUDIO_CONTENT_SHARE);
				audioContentShareMap[id] = micCopy;

				deviceLock.unlock();

				Logger::Instance().Log( "OnLocalMicrophoneAdded success");
			} else {
				Logger::Instance().Log( "OnLocalMicrophoneAdded failed due to ConstructCopy failure" );
			}
		} else {
			Logger::Instance().Log( "OnLocalMicrophoneAdded received NULL added microphone." );
		}
	} else {
		Logger::Instance().Log( "OnLocalMicrophoneAdded unexpected VidyoConnector object received." );
	}
}

// Handle a microphone being removed.
void OnLocalMicrophoneRemoved(VidyoConnector *c, VidyoLocalMicrophone *localMicrophone ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localMicrophone ) {

			deviceLock.lock();

			// Iterate through the microphoneMap
			for ( std::map<int, VidyoLocalMicrophone*>::iterator it = microphoneMap.begin(); it != microphoneMap.end(); ++it ) {
				// Check for microphone to remove
				if ( VidyoLocalMicrophoneEqual( it->second, localMicrophone ) ) {
					// Remove from menu and remove from map
					RemoveMenu( devicesSubmenu, it->first, MF_BYCOMMAND );
					microphoneMap.erase( it );

					// Break out of loop
					break;
				}
			}

			// Iterate through the audioContentShareMap
			for ( std::map<int, VidyoLocalMicrophone*>::iterator it = audioContentShareMap.begin(); it != audioContentShareMap.end(); ++it ) {
				// Check for microphone to remove
				if ( VidyoLocalMicrophoneEqual( it->second, localMicrophone ) ) {
					// Free the memory, remove from menu, and remove from map
					VidyoLocalMicrophoneDestruct( it->second );
					free( it->second );
					RemoveMenu( contentSubmenu, it->first, MF_BYCOMMAND );
					audioContentShareMap.erase(it);

					// Break out of loop
					break;
				}
			}

			deviceLock.unlock();

			Logger::Instance().Log( "OnLocalMicrophoneRemoved success" );
		} else {
			Logger::Instance().Log( "OnLocalMicrophoneRemoved received NULL removed microphone." );
		}
	} else {
		Logger::Instance().Log( "OnLocalMicrophoneRemoved unexpected VidyoConnector object received." );
	}
}

// Handle a microphone being selected.
void OnLocalMicrophoneSelected( VidyoConnector *c, VidyoLocalMicrophone *localMicrophone ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		Logger::Instance().Log( "OnLocalMicrophoneSelected microphone selected." );
		bool found = false;

		deviceLock.lock();

		// Iterate through the microphoneMap
		for ( std::map<int, VidyoLocalMicrophone*>::iterator it = microphoneMap.begin(); it != microphoneMap.end(); ++it ) {
			// Check for selected device
			if ( localMicrophone && VidyoLocalMicrophoneEqual( it->second, localMicrophone ) ) {
				// Check the selected device in the menu
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_CHECKED );
				found = true;
			} else {
				// Uncheck each device that was not selected
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Set the state of the "None" microphone menu item depending if a microphone was selected
		int checked = found ? MF_UNCHECKED : MF_CHECKED;
		CheckMenuItem( devicesSubmenu, ID_MICROPHONE_NONE, MF_BYCOMMAND | checked );

		// Enable or disable the Content (Audio Content Share) submenu items accordingly
		for ( std::map<int, VidyoLocalMicrophone*>::iterator it = audioContentShareMap.begin(); it != audioContentShareMap.end(); ++it ) {
			// Check for selected device
			if ( localMicrophone && VidyoLocalMicrophoneEqual( it->second, localMicrophone ) ) {
				// disable in the menu
				EnableMenuItem( contentSubmenu, it->first, MF_GRAYED );
			} else {
				// enable in the menu
				EnableMenuItem( contentSubmenu, it->first, MF_ENABLED );
			}
		}

		deviceLock.unlock();
	} else {
		Logger::Instance().Log( "OnLocalMicrophoneSelected unexpected VidyoConnector object received." );
	}
}

// Handle a microphone state update
void OnLocalMicrophoneStateUpdated(VidyoConnector* c, VidyoLocalMicrophone* localMicrophone, VidyoDeviceState state) {
	Logger::Instance().Log("OnLocalMicrophoneStateUpdated");
}

// Handle a speaker being added to the system.
void OnLocalSpeakerAdded( VidyoConnector *c, VidyoLocalSpeaker *localSpeaker ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localSpeaker ) {
			// Add the speaker to the speaker map

			VidyoLocalSpeaker *speakerCopy = ( VidyoLocalSpeaker * )malloc( sizeof( VidyoLocalSpeaker ) );

			if ( VidyoLocalSpeakerConstructCopy( speakerCopy, localSpeaker ) ) {
				deviceLock.lock();

				// Get the name of the speaker
				const char *speakerName = LmiStringCStr( VidyoLocalSpeakerGetName( speakerCopy ) );

				// Create the menu item
				unsigned long menuOffset = NO_SPEAKER_MENU_OFFSET + cameraMap.size() + microphoneMap.size() + speakerMap.size() + 1;
				int id = CreateMenuItem( devicesSubmenu, menuOffset, speakerName, LOCAL_SPEAKER );

				// Add to map
				speakerMap[id] = speakerCopy;

				deviceLock.unlock();

				Logger::Instance().Log( "OnLocalSpeakerAdded success ");
			} else {
				Logger::Instance().Log( "OnLocalSpeakerAdded failed due to ConstructCopy failure" );
			}
		} else {
			Logger::Instance().Log( "OnLocalSpeakerAdded received NULL added speaker." );
		}
	} else {
		Logger::Instance().Log( "OnLocalSpeakerAdded unexpected VidyoConnector object received." );
	}
}

// Handle a speaker being removed.
void OnLocalSpeakerRemoved(VidyoConnector *c, VidyoLocalSpeaker *localSpeaker) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localSpeaker ) {

			deviceLock.lock();

			// Iterate through the speakerMap
			for ( std::map<int, VidyoLocalSpeaker*>::iterator it = speakerMap.begin(); it != speakerMap.end(); ++it ) {
				// Check for speaker to remove
				if ( VidyoLocalSpeakerEqual( it->second, localSpeaker ) ) {
					// Free the memory, remove from menu, and remove from map
					VidyoLocalSpeakerDestruct( it->second );
					free( it->second );
					RemoveMenu( devicesSubmenu, it->first, MF_BYCOMMAND );
					speakerMap.erase( it );

					// Break out of loop
					break;
				}
			}

			deviceLock.unlock();

			Logger::Instance().Log( "OnLocalSpeakerRemoved success" );
		} else {
			Logger::Instance().Log( "OnLocalSpeakerRemoved received NULL removed speaker." );
		}
	} else {
		Logger::Instance().Log( "OnLocalSpeakerRemoved unexpected VidyoConnector object received." );
	}
}

// Handle a speaker being selected.
void OnLocalSpeakerSelected(VidyoConnector *c, VidyoLocalSpeaker *localSpeaker) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		Logger::Instance().Log( "OnLocalSpeakerSelected speaker selected." );
		bool found = false;

		deviceLock.lock();

		// Iterate through the speakerMap
		for ( std::map<int, VidyoLocalSpeaker*>::iterator it = speakerMap.begin(); it != speakerMap.end(); ++it ) {
			// Check for selected device
			if ( localSpeaker && VidyoLocalSpeakerEqual( it->second, localSpeaker ) ) {
				// Check the selected device in the menu
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_CHECKED );
				found = true;
			} else {
				// Uncheck each device that was not selected
				CheckMenuItem( devicesSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Set the state of the "None" speaker menu item depending if a speaker was selected
		int checked = found ? MF_UNCHECKED : MF_CHECKED;
		CheckMenuItem( devicesSubmenu, ID_OUTPUT_NONE, MF_BYCOMMAND | checked );

		deviceLock.unlock();

	} else {
		Logger::Instance().Log( "OnLocalSpeakerSelected unexpected VidyoConnector object received." );
	}
}

// Handle a speaker state update
void OnLocalSpeakerStateUpdated(VidyoConnector* c, VidyoLocalSpeaker* localSpeaker, VidyoDeviceState state) {
	Logger::Instance().Log("OnLocalSpeakerStateUpdated");
}

// Handle a monitor being added to the system.
void OnLocalMonitorAdded( VidyoConnector *c, VidyoLocalMonitor *localMonitor ) {
	if ( VidyoConnectorEqual( c, &vc) ) {
		if ( localMonitor ) {
			// Add the monitor to the monitorShareMap

			VidyoLocalMonitor *monitorCopy = ( VidyoLocalMonitor * )malloc( sizeof( VidyoLocalMonitor ) );

			if ( VidyoLocalMonitorConstructCopy( monitorCopy, localMonitor ) ) {
				deviceLock.lock();

				// Get the name of the monitor
				const char *monitorName = LmiStringCStr( VidyoLocalMonitorGetName( monitorCopy ) );

				// Create the menu item
				unsigned long menuOffset = NO_MONITOR_SHARE_MENU_OFFSET + monitorShareMap.size() + 1;
				int id = CreateMenuItem( sharesSubmenu, menuOffset, monitorName, LOCAL_MONITOR );

				// Add to map
				monitorShareMap[id] = monitorCopy;

				deviceLock.unlock();

				Logger::Instance().Log( "OnLocalMonitorAdded success ");
			} else {
				Logger::Instance().Log( "OnLocalMonitorAdded failed due to ConstructCopy failure" );
			}
		} else {
			Logger::Instance().Log( "OnLocalMonitorAdded received NULL added monitor." );
		}
	} else {
		Logger::Instance().Log( "OnLocalMonitorAdded unexpected VidyoConnector object received." );
	}
}

// Handle a monitor being removed.
void OnLocalMonitorRemoved(VidyoConnector *c, VidyoLocalMonitor *localMonitor ) {
	if ( VidyoConnectorEqual( c, &vc) ) {
		if ( localMonitor ) {

			deviceLock.lock();

			// Iterate through the monitorShareMap
			for ( std::map<int, VidyoLocalMonitor*>::iterator it = monitorShareMap.begin(); it != monitorShareMap.end(); ++it ) {
				// Check for monitor to remove
				if ( VidyoLocalMonitorEqual( it->second, localMonitor ) ) {
					// Free the memory, remove from menu, and remove from map
					VidyoLocalMonitorDestruct( it->second );
					free( it->second );
					RemoveMenu( sharesSubmenu, it->first, MF_BYCOMMAND );
					monitorShareMap.erase( it );

					// Break out of loop
					break;
				}
			}

			deviceLock.unlock();

			Logger::Instance().Log( "OnLocalMonitorRemoved success" );
		} else {
			Logger::Instance().Log( "OnLocalMonitorRemoved received NULL removed monitor." );
		}
	} else {
		Logger::Instance().Log( "OnLocalMonitorRemoved unexpected VidyoConnector object received." );
	}
}

// Handle a monitor being selected.
void OnLocalMonitorSelected( VidyoConnector *c, VidyoLocalMonitor *localMonitor ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		Logger::Instance().Log( "OnLocalMonitorSelected monitor selected." );
		bool found = false;

		deviceLock.lock();

		// Iterate through the monitorShareMap
		for ( std::map<int, VidyoLocalMonitor*>::iterator it = monitorShareMap.begin(); it != monitorShareMap.end(); ++it ) {
			// Check for selected monitor
			if ( localMonitor && VidyoLocalMonitorEqual( it->second, localMonitor ) ) {
				// Check the selected monitor in the menu
				CheckMenuItem( sharesSubmenu, it->first, MF_BYCOMMAND | MF_CHECKED );
				found = true;
			} else {
				// Uncheck each monitor that was not selected
				CheckMenuItem( sharesSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Set the state of the "None" monitor menu item depending if a monitor was selected
		int checked = found ? MF_UNCHECKED : MF_CHECKED;
		CheckMenuItem( sharesSubmenu, ID_MONITOR_SHARE_NONE, MF_BYCOMMAND | checked );

		deviceLock.unlock();
	} else {
		Logger::Instance().Log( "OnLocalMonitorSelected unexpected VidyoConnector object received." );
	}
}

// Handle a monitor state update
void OnLocalMonitorStateUpdated(VidyoConnector* c, VidyoLocalMonitor* localMonitor, VidyoDeviceState state) {
	Logger::Instance().Log("OnLocalMonitorStateUpdated");
}

// Handle a window share being added to the system.
void OnLocalWindowShareAdded( VidyoConnector *c, VidyoLocalWindowShare *localWindowShare ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localWindowShare ) {
			if ( LmiStringSize( VidyoLocalWindowShareGetName( localWindowShare ) ) > 0 ) {
				// Add the window share to the windowShareMap

				VidyoLocalWindowShare *windowShareCopy = ( VidyoLocalWindowShare * )malloc( sizeof( VidyoLocalWindowShare ) );

				if ( VidyoLocalWindowShareConstructCopy( windowShareCopy, localWindowShare ) ) {
					deviceLock.lock();

					// Get the name of the window share
					std::string shareName       = std::string( LmiStringCStr( VidyoLocalWindowShareGetName( windowShareCopy ) ) );
					std::string shareAppName    = std::string( LmiStringCStr( VidyoLocalWindowShareGetApplicationName( windowShareCopy ) ) );
					std::string menuItemNameStr = shareName + " : " + shareAppName;
					const char *menuItemName    = menuItemNameStr.c_str();

					// Create the menu item
					unsigned long menuOffset = NO_WINDOW_SHARE_MENU_OFFSET + windowShareMap.size() + monitorShareMap.size() + 1;
					int id = CreateMenuItem( sharesSubmenu, menuOffset, menuItemName, LOCAL_WINDOW );

					// Add to map
					windowShareMap[id] = windowShareCopy;

					deviceLock.unlock();

					Logger::Instance().Log( "OnLocalWindowShareAdded success");
				} else {
					Logger::Instance().Log( "OnLocalWindowShareAdded failed due to ConstructCopy failure" );
				}
			}
		} else {
			Logger::Instance().Log( "OnLocalWindowShareAdded received NULL added window share." );
		}
	} else {
		Logger::Instance().Log( "OnLocalWindowShareAdded unexpected VidyoConnector object received." );
	}
}

// Handle a window share being removed.
void OnLocalWindowShareRemoved(VidyoConnector *c, VidyoLocalWindowShare *localWindowShare ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		if ( localWindowShare ) {

			deviceLock.lock();

			// Iterate through the windowShareMap
			for ( std::map<int, VidyoLocalWindowShare*>::iterator it = windowShareMap.begin(); it != windowShareMap.end(); ++it ) {
				// Check for window share to remove
				if ( VidyoLocalWindowShareEqual( it->second, localWindowShare ) ) {
					// Free the memory, remove from menu, and remove from map
					VidyoLocalWindowShareDestruct( it->second );
					free( it->second );
					RemoveMenu( sharesSubmenu, it->first, MF_BYCOMMAND );
					windowShareMap.erase( it );

					// Break out of loop
					break;
				}
			}

			deviceLock.unlock();

			Logger::Instance().Log( "OnLocalWindowShareRemoved success" );
		} else {
			Logger::Instance().Log( "OnLocalWindowShareRemoved received NULL removed window." );
		}
	} else {
		Logger::Instance().Log( "OnLocalWindowShareRemoved unexpected VidyoConnector object received." );
	}
}

// Handle a window share being selected.
void OnLocalWindowShareSelected( VidyoConnector *c, VidyoLocalWindowShare *localWindowShare ) {
	if ( VidyoConnectorEqual( c, &vc ) ) {
		Logger::Instance().Log( "OnLocalWindowShareSelected window selected." );
		bool found = false;

		deviceLock.lock();

		// Iterate through the windowShareMap
		for ( std::map<int, VidyoLocalWindowShare*>::iterator it = windowShareMap.begin(); it != windowShareMap.end(); ++it ) {
			// Check for selected window share
			if ( localWindowShare && VidyoLocalWindowShareEqual( it->second, localWindowShare ) ) {
				// Check the selected window share in the menu
				CheckMenuItem( sharesSubmenu, it->first, MF_BYCOMMAND | MF_CHECKED );
				found = true;
			} else {
				// Uncheck each window share that was not selected
				CheckMenuItem( sharesSubmenu, it->first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Set the state of the "None" window share menu item depending if a window was selected
		int checked = found ? MF_UNCHECKED : MF_CHECKED;
		CheckMenuItem( sharesSubmenu, ID_WINDOW_SHARE_NONE, MF_BYCOMMAND | checked );

		deviceLock.unlock();
	} else {
		Logger::Instance().Log( "OnLocalWindowShareSelected unexpected VidyoConnector object received." );
	}
}

// Handle a window share state update
void OnLocalWindowShareStateUpdated( VidyoConnector* c, VidyoLocalWindowShare* localWindowShare, VidyoDeviceState state ) {
	Logger::Instance().Log( "OnLocalWindowShareStateUpdated" );
}

// Handle a participant joining
void OnParticipantJoined(VidyoConnector* c, VidyoParticipant* participant ) {
	std::string str = std::string( LmiStringCStr( ( VidyoParticipantGetName( participant ) ) ) ) + " Joined";
	Logger::Instance().Log( str.c_str() );
	UpdateParticipantStatus( str );
}

// Handle a participant leaving
void OnParticipantLeft( VidyoConnector* c, VidyoParticipant* participant ) {
	std::string str = std::string( LmiStringCStr( ( VidyoParticipantGetName( participant ) ) ) ) + " Left";
	Logger::Instance().Log( str.c_str() );
	UpdateParticipantStatus( str );
}

// Handle order of participants changing
void OnDynamicParticipantChanged(VidyoConnector* c, LmiVector(VidyoParticipant)* participants) {
}

// Handle loudest speaker change
void OnLoudestParticipantChanged( VidyoConnector* c, const VidyoParticipant* participant, LmiBool audioOnly ) {
	UpdateParticipantStatus( std::string( LmiStringCStr( ( VidyoParticipantGetName( participant ) ) ) ) + " Speaking" );
}

// Handle a message being logged.
void OnLog(VidyoConnector *c, const VidyoLogRecord *logRecord) {
	Logger::Instance().LogClientLib( VidyoLogRecordGetMessage( logRecord ) );
}

void extractFieldNameAndFieldValue(TCHAR* queryPair, TCHAR* &fieldName, TCHAR* &valueOfField, int &buffSizeValue, int &buffSizeFieldName){
	TCHAR* const firstEqualSignAddress = _tcschr(queryPair, L'=');
	int indexOfFirstEqualSign = firstEqualSignAddress - (queryPair + 1);
	int lenFieldValue = wcslen(queryPair) - indexOfFirstEqualSign;
	size_t sizeFieldName = indexOfFirstEqualSign + 2;
	size_t sizeOfFieldValue = wcslen(queryPair) - indexOfFirstEqualSign;
	if (sizeOfFieldValue > buffSizeValue){
		valueOfField = (TCHAR*)realloc(valueOfField, sizeof(TCHAR) * sizeOfFieldValue);
	}
	if (sizeFieldName > buffSizeFieldName)
	{
		fieldName = (TCHAR*)realloc(fieldName, sizeof(TCHAR) * sizeFieldName);
	}

	wcsncpy_s(fieldName, sizeFieldName, queryPair, indexOfFirstEqualSign + 1);
	wcsncpy_s(valueOfField, sizeOfFieldValue, queryPair + indexOfFirstEqualSign + 2, sizeOfFieldValue - 1);

}
static bool validateCommandLineForQuit(const WCHAR* lpCmdLine, HWND hWnd)
{
	std::wstring commandLine(lpCmdLine);
	if (commandLine.find(L"--quit") == std::wstring::npos)
	{
		return FALSE;
	}
	return TRUE;
}
static bool validateCommandLineForInvocationURL(const WCHAR* args, char* url)
{
	std::wstring commandLine(args);
	if (commandLine.length() > INTERNET_MAX_URL_LENGTH || commandLine.length() == 0){
		return false;
	}
	size_t ret;
	wcstombs_s(&ret, url, INTERNET_MAX_URL_LENGTH, commandLine.c_str(), INTERNET_MAX_URL_LENGTH);
	std::string urlStr = std::string(url);
	if (urlStr.find("vidyoconnector:") == 0){
		return true;
	}
	if (urlStr.find("\"vidyoconnector:") == 0){
		return true;
	}
	else if (urlStr.find("-pin") == 0){
		return true;
	}
	else if (urlStr.find("-roomkey") == 0){
		return true;
	}
	else if (urlStr.find("-portal") == 0){
		return true;
	}
	else if (urlStr.find("-displayName") == 0){
		return true;
	}
	else if (urlStr.find("-hideConfig") == 0){
		return true;
	}
	else if (urlStr.find("-autoJoin") == 0){
		return true;
	}
	else if (urlStr.find("-cameraPrivacy") == 0){
		return true;
	}
	else if (urlStr.find("-microphonePrivacy") == 0){
		return true;
	}
	else if (urlStr.find("-allowReconnect") == 0){
		return true;
	}
	else if (urlStr.find("-enableDebug") == 0){
		return true;
	}
	else if (urlStr.find("-experimentalOptions") == 0){
		return true;
	}
	return false;
}
void extractFieldsFromUrl(std::basic_string<TCHAR>tStr, bool isFromSecondWindow){
	startupStringContainedPortal = false;
	startupStringContainedRoomKey = false;
	startupStringContainedPin = false;
	startupStringContainedDisplayName = false;
	startupStringContainedAutoJoin = false;
	startupStringContainedCameraPrivacy = false;
	startupStringContainedMicrophonePrivacy = false;
	startupStringContainedEnableDebug = false;
	startupStringContainedExperimentalOptions = false;
	startupStringContainedHideConfig = false;
	startupstringContainedAllowReconnect = false;
	std::wstring wStr;
	std::string  str;
	std::size_t  strLen;

	// Do command-line arguments begin with this app's custom URI scheme?
	if (0 == tStr.find(_T("vidyoconnector:")) || tStr.find(_T("\"vidyoconnector:")) == 0) {
			// ...Yes.  Expect the sequence of arguments to comprise a URI.

			// If the host in the URI scheme is indicating that a VidyoCloud Vidyo
			// system (not Vidyo.io) should be joined, then set appropriate flags.
			if (0 == tStr.find(_T("vidyoconnector://join"))) {
				vidyoCloudJoin = true;
				hideConfig = true;
				startupStringContainedHideConfig = true;
			}


			if (!isFromSecondWindow)
			{
				//// Compensate for the possibility that whitespace (spaces and tabs)
				////  may break up a single URI into multiple command-line arguments:
				////  Combine all arguments into a single string, to parse as a URI.
				tStr = __targv[1];
				for (int index = 2; index < __argc; ++index) {
					tStr += _T("%20"); // Replace whitespace with a URL-encoded space.
					tStr += __targv[index];
				}
			}

			// Parse relevant part(s) of the tentative URI.
			// - Already checked for custom URI scheme:  No need to parse it again.
			// - Any username, password, host, port, path, and fragment are irrelevant.
			// - Only the query string is relevant.
			TCHAR uriStrBuf [INTERNET_MAX_URL_LENGTH + sizeof('\0')];
			DWORD uriStrLen = _countof(uriStrBuf);
			HRESULT const uriGetResult = UrlGetPart(tStr.c_str(), uriStrBuf, &uriStrLen, URL_PART_QUERY, 0);
			switch(uriGetResult) {
			case S_OK: {
					// Callee succeeded, but avoid parsing any garbage in output buffer.
					if (0 == uriStrLen) {
						_tcscpy_s(uriStrBuf, _countof(uriStrBuf), _T("\0"));
					}
					// Traverse and parse tentative field-value pairs in query string.
					TCHAR const *const queryPairDelims = _T("&");
					TCHAR* queryPairContext = NULL;
					TCHAR* queryPair = _tcstok_s(uriStrBuf, queryPairDelims, &queryPairContext);
					TCHAR* valueOfField;
					TCHAR* fieldName;
					int buffSizeValue = 256;
					int buffSizeFieldName = 10;
					valueOfField = (TCHAR*)malloc(sizeof(TCHAR) * buffSizeValue);
					fieldName = (TCHAR*)malloc(sizeof(TCHAR) * buffSizeFieldName);
					while (NULL != queryPair) {

						extractFieldNameAndFieldValue(queryPair, fieldName, valueOfField, buffSizeValue, buffSizeFieldName);
						if (wcslen(fieldName)>1){
							// ...Got first half of field-value pair, which is the field.
							HRESULT const unescFieldResult = UrlUnescapeInPlace(fieldName, 0);
							if (S_OK == unescFieldResult) {
								std::basic_string<TCHAR> tField = fieldName;
								if (wcslen(valueOfField) > 0) {
									HRESULT const unescValueResult = UrlUnescapeInPlace(valueOfField, 0);
									if (S_OK == unescValueResult) {
										std::basic_string<TCHAR> tValue = valueOfField;

										// Prepare to deal with 'char' strings for other parts of this program.
										std::string mbValue;
#ifdef UNICODE
										// ASSUMES that 'sizeof(char) == 1', which is typical.
										size_t const room = tValue.length() * sizeof(TCHAR);
										char* value = new char [room];
										if (NULL != value) {
											size_t countConverted = 0; // For result of callee, but ignored.
											errno_t const errorConverted = wcstombs_s(&countConverted,
												value, room, tValue.c_str(), tValue.length());
											if (0 != errorConverted) {
												// TODO: Report and handle error.
												value[0] = '\0';
											}
											mbValue = value;
										}
										delete[] value;
#else
										mbValue = tValue;
#endif // UNICODE
										// Check which parameter the field matches, if any.
										if (0 == tField.compare(_T("pin"))) {
											strncpy_s(roomPin, _countof(roomPin), mbValue.c_str(), mbValue.length());
											startupStringContainedPin = true;
										}
										else if (0 == tField.compare(_T("roomkey"))) {
											strncpy_s(roomKey, _countof(roomKey), mbValue.c_str(), mbValue.length());
											startupStringContainedRoomKey = true;
										}
										else if (0 == tField.compare(_T("portal"))) {
											strncpy_s(portal, _countof(portal), mbValue.c_str(), mbValue.length());
											startupStringContainedPortal = true;
										}
										else if (0 == tField.compare(_T("displayName"))) {
											strncpy_s(displayName, _countof(displayName), mbValue.c_str(), mbValue.length());
											startupStringContainedDisplayName = true;
										}
										else if (0 == tField.compare(_T("hideConfig"))) {
											hideConfig = (0 == mbValue.compare("1"));
											startupStringContainedHideConfig = true;
										}
										else if (0 == tField.compare(_T("autoJoin"))) {
											autoJoin = (0 == mbValue.compare("1"));
											startupStringContainedAutoJoin = true;
										}
										else if (0 == tField.compare(_T("cameraPrivacy"))) {
											cameraPrivacy = (0 == mbValue.compare("1"));
											startupStringContainedCameraPrivacy = true;
										}
										else if (0 == tField.compare(_T("microphonePrivacy"))) {
											microphonePrivacy = (0 == mbValue.compare("1"));
											startupStringContainedMicrophonePrivacy = true;
										}
										else if (0 == tField.compare(_T("allowReconnect"))) {
											allowReconnect = (0 == mbValue.compare("1"));
											startupstringContainedAllowReconnect = true;
										}
										else if (0 == tField.compare(_T("enableDebug"))) {
											enableDebug = (0 == mbValue.compare("1"));
											startupStringContainedEnableDebug = true;
										}
										else if (0 == tField.compare(_T("experimentalOptions"))) {
											strncpy_s(experimentalOptions, _countof(experimentalOptions), mbValue.c_str(), mbValue.length());
											startupStringContainedExperimentalOptions = true;
										}
									}
								}
							}
						}
						queryPair = _tcstok_s(NULL, queryPairDelims, &queryPairContext);
					}
					free(valueOfField);
					free(fieldName);
				}
				break;
			case E_POINTER:
				// TODO: Report and handle error.
				break;
			default:
				// TODO: Report and handle error.
				break;
			}
		}
	else{
		//...Expect conventional command-line arguments....
		//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		for (int index = 1; index < __argc; ++index) {
			wStr = __targv[index];
			str.assign(wStr.begin(), wStr.end());

			// TODO:  Should command-line and URI-based arguments be case-insensitive?
			// std::transform( str.begin(), str.end(), str.begin(), ::tolower ); // convert to lower case

			if (str.compare("-pin") == 0) {
				if (index == __argc) break; // error condition where no resource ID is specified
				wStr = __targv[++index];      // increment index before array access
				str.assign(wStr.begin(), wStr.end());
				strLen = str.copy(roomPin, str.length(), 0);
				roomPin[strLen] = '\0';
				startupStringContainedPin = true;
			}
			else if (str.compare("-roomkey") == 0) {
				if (index == __argc) break; // error condition where no roomkey is specified
				wStr = __targv[++index];      // increment index before array access
				str.assign(wStr.begin(), wStr.end());
				strLen = str.copy(roomKey, str.length(), 0);
				roomKey[strLen] = '\0';
				startupStringContainedRoomKey = true;
			}
			else if (str.compare("-portal") == 0) {
				if (index == __argc) break; // error condition where no host is specified
				wStr = __targv[++index];      // increment index before array access
				str.assign(wStr.begin(), wStr.end());
				strLen = str.copy(portal, str.length(), 0);
				portal[strLen] = '\0';
				startupStringContainedPortal = true;
			}
			else if (str.compare("-displayName") == 0) {
				if (index == __argc) break; // error condition where no display name is specified
				wStr = __targv[++index];      // increment index before array access
				str.assign(wStr.begin(), wStr.end());
				strLen = str.copy(displayName, str.length(), 0);
				displayName[strLen] = '\0';
				startupStringContainedDisplayName = true;
			}
			else if (str.compare("-hideConfig") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				hideConfig = (wStr.compare(L"1") == 0);
				startupStringContainedHideConfig = true;
			}
			else if (str.compare("-autoJoin") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				autoJoin = (wStr.compare(L"1") == 0);
				startupStringContainedAutoJoin = true;
			}
			else if (str.compare("-cameraPrivacy") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				cameraPrivacy = (wStr.compare(L"1") == 0);
				startupStringContainedCameraPrivacy = true;
			}
			else if (str.compare("-microphonePrivacy") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				microphonePrivacy = (wStr.compare(L"1") == 0);
				startupStringContainedMicrophonePrivacy = true;
			}
			else if (str.compare("-allowReconnect") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				allowReconnect = (wStr.compare(L"1") == 0);
				startupstringContainedAllowReconnect = true;
			}
			else if (str.compare("-enableDebug") == 0) {
				if (index == __argc) break; // error condition where value is specified
				wStr = __targv[++index];      // increment index before array access
				enableDebug = (wStr.compare(L"1") == 0);
				startupStringContainedEnableDebug = true;
			}
			else if (str.compare("-experimentalOptions") == 0) {
				if (index == __argc) break; // error condition where no experimental options are specified
				wStr = __targv[++index];      // increment index before array access
				str.assign(wStr.begin(), wStr.end());
				strLen = str.copy(experimentalOptions, str.length(), 0);
				experimentalOptions[strLen] = '\0';
				startupStringContainedExperimentalOptions = true;
			}
		}
	}
}
