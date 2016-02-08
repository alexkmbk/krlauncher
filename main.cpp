#include <winsock2.h>
#include <stdio.h>
#include <windows.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

const WCHAR pServiceName[] = L"KVPNCSvc"; // Имя службы Керио
const WCHAR pPathToRDP[] = L"mstsc D:\\bin\\scripts\\test.rdp"; // Путь к файлу настроек подключения RDP
const WCHAR pPathToVPN[] = L"D:\\bin\\KerioVPNClient\\VPN Client\\kvpncgui.exe"; //Путь к клиенту
const char pServerName[] = "server"; // Имя удаленного сервера, к которому идет подключение


WCHAR* GetErrorInf()
{
	int lErrorCode = GetLastError();
	WCHAR* wsErrorDescription = NULL;
	// Пример использования функции FormatMessage для вывода текста ошибки
	// взят отсюдова - http://stackoverflow.com/questions/455434/how-should-i-use-formatmessage-properly-in-c
	LPWSTR errorText = NULL;

	FormatMessageW(
		// use system message tables to retrieve error text
		FORMAT_MESSAGE_FROM_SYSTEM
		// allocate buffer on local heap for error text
		|FORMAT_MESSAGE_ALLOCATE_BUFFER
		// Important! will fail otherwise, since we're not 
		// (and CANNOT) pass insertion parameters
		|FORMAT_MESSAGE_IGNORE_INSERTS,  
		NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
		lErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&errorText,  // output 
		0, // minimum size for output buffer
		NULL);   // arguments - see note 

	if ( NULL != errorText )
	{
		// Выделим память по количеству символов в строке
		int errorTextLen = lstrlenW(errorText);
		wsErrorDescription = new WCHAR[errorTextLen + 1];
		memcpy(wsErrorDescription, errorText, (errorTextLen + 1) * sizeof(WCHAR));
		// release memory allocated by FormatMessage()
		LocalFree(errorText);
		errorText = NULL;
	}

	return wsErrorDescription;

}

bool isServiceStarted(WCHAR* name)
{
	SC_HANDLE serviceDbHandle = OpenSCManager(NULL,NULL,GENERIC_READ);
	SC_HANDLE serviceHandle = OpenService(serviceDbHandle, name, GENERIC_READ);

	SERVICE_STATUS_PROCESS status;
	DWORD bytesNeeded;
	QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO,(LPBYTE) &status,sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);

	if (status.dwCurrentState == SERVICE_RUNNING)
	{
		CloseServiceHandle(serviceDbHandle);
		CloseServiceHandle(serviceHandle);
		return true;
	}

	CloseServiceHandle(serviceDbHandle);
	CloseServiceHandle(serviceHandle);

	return false;
}

bool StartServiceEasy(WCHAR* name)
{

    if (isServiceStarted(name))
	{
		return true;
	}

	SC_HANDLE  hSCManager = OpenSCManager(NULL, NULL, SERVICE_START);

	if (!hSCManager) 
	{
		WCHAR* wcErrorInf = GetErrorInf();
		if (wcErrorInf != NULL) {
			//MessageBox(NULL,wcErrorInf, L"OpenSCManager error", 0);
			delete[] wcErrorInf;
		}
		return false;
	}

	SC_HANDLE hService = OpenService(hSCManager, name, SERVICE_START);

	if (!hService) 
	{
		WCHAR* wcErrorInf = GetErrorInf();
		if (wcErrorInf != NULL) {
			//MessageBox(NULL,wcErrorInf, L"OpenService error", 0);
			delete[] wcErrorInf;
		}
		CloseServiceHandle (hSCManager);
		return false;
	}

	if (!StartService(hService, 0, NULL)) {
		if (GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
			//MessageBox(0,L"The service \"KVPNCSvc\" could not be started...", 0, 0);
			CloseServiceHandle (hSCManager);
			CloseServiceHandle (hService);
			return false;
		}
	};

	return true;

}


bool isConnection(char* pHostName)
{
     WORD ver = MAKEWORD(2,2);
    WSADATA wsaData;
    int retVal=0;
 
    WSAStartup(ver,(LPWSADATA)&wsaData);
 
    LPHOSTENT hostEnt;
 
    hostEnt = gethostbyname(pHostName);
 
    if(!hostEnt)
    {
        printf("Unable to collect gethostbyname\n");
        WSACleanup();
        return false;
    }
 
    //Создаем сокет
    SOCKET clientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
 
    if(clientSock == SOCKET_ERROR)
    {
        printf("Unable to create socket\n");
        WSACleanup();
        return false;
    }
 
    SOCKADDR_IN serverInfo;
 
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr = *((LPIN_ADDR)*hostEnt->h_addr_list);
    serverInfo.sin_port = htons(80);
 
    retVal=connect(clientSock,(LPSOCKADDR)&serverInfo, sizeof(serverInfo));
    if (retVal == SOCKET_ERROR) {
		WSACleanup();
		return false;
	};
	
	WSACleanup();	
	
	return true;

}

bool exec(WCHAR* cmd) 
{
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	
	wchar_t cmdline[ MAX_PATH + 50 ];
	memcpy(cmdline, cmd, (wcslen(cmd) + 1)*sizeof(wchar_t));
	
	if(CreateProcess(NULL, cmdline, NULL, NULL, FALSE,  NORMAL_PRIORITY_CLASS, NULL, NULL, &si , &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else
	{
		//MessageBox(0,L"The process could not be started...", 0, 0);
		return false;
	}

	return true;
}



int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow
)
{
	// if there is "s" parametr in comman line, 
	// then it just needs to start "KVPNCSvc" service and nothing more
	if (*lpCmdLine == 's')
	{
		if (!StartServiceEasy((WCHAR*)pServiceName)) return -1;
		return 0;
	}

	// if we already have connection to a server, we can open a remote desktop 
	if (isConnection((char*)pServerName)) 
	{
		exec((WCHAR*)pPathToRDP);
		return 0;
	}
    
	// if "KVPNCSvc" service is not started yet, we are going to start it now
	// by launching our application with administration privileges
	if (!isServiceStarted((WCHAR*)pServiceName))
	{
		WCHAR pAppPath[MAX_PATH];
		GetModuleFileName( NULL, pAppPath, MAX_PATH);
		ShellExecute(
			NULL,
			L"runas",
			pAppPath,
			L"s",    // params
			NULL,        // directory
			SW_SHOW);
	};

HWND hKerioMainWindow = NULL;

int counter = 0;

if (!exec((WCHAR*)pPathToVPN)) return -1;

// waiting till we could get focused window of Kerio VPN Client
while (true)
{
	counter++;
	Sleep(100); // check for focused window every 100 milliseconds
	hKerioMainWindow = FindWindow(NULL, L"Kerio VPN Client");
	if (((hKerioMainWindow != NULL)&&(GetForegroundWindow() == hKerioMainWindow))||(counter > 30)) break;
}

if (hKerioMainWindow != NULL)
{
	// emulate press of Enter key to press connection button
	SendMessage(hKerioMainWindow, WM_KEYDOWN, VK_RETURN, 0);
}

while (true)
{
	Sleep(1000);
	if (isConnection((char*)pServerName)) 
	{
		exec((WCHAR*)pPathToRDP);
		return 0;
	}
	counter++;
	if (counter == 20) {
		return -1;
	}
}
	return 0;
}