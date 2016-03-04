#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <string>

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

using namespace std;

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
		| FORMAT_MESSAGE_ALLOCATE_BUFFER
		// Important! will fail otherwise, since we're not 
		// (and CANNOT) pass insertion parameters
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
		lErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&errorText,  // output 
		0, // minimum size for output buffer
		NULL);   // arguments - see note 

	if (NULL != errorText)
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

bool isServiceStarted(const WCHAR* name)
{
	SC_HANDLE serviceDbHandle = OpenSCManager(NULL, NULL, GENERIC_READ);
	SC_HANDLE serviceHandle = OpenService(serviceDbHandle, name, GENERIC_READ);

	SERVICE_STATUS_PROCESS status;
	DWORD bytesNeeded;
	QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);

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

bool StartServiceEasy(const WCHAR* name)
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
			delete[] wcErrorInf;
		}
		CloseServiceHandle(hSCManager);
		return false;
	}

	if (!StartService(hService, 0, NULL)) {
		if (GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
			//MessageBox(0,L"The service \"KVPNCSvc\" could not be started...", 0, 0);
			CloseServiceHandle(hSCManager);
			CloseServiceHandle(hService);
			return false;
		}
	};

	return true;

}


bool isConnection(wstring& wsHostName)
{
	WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	int retVal = 0;

	WSAStartup(ver, (LPWSADATA)&wsaData);

	LPHOSTENT hostEnt;

	int resLen = wcstombs(0, wsHostName.c_str(), 0);
	char* pHostName = new char[resLen + 1];
	std::wcstombs(pHostName, wsHostName.c_str(), resLen);

	hostEnt = gethostbyname(pHostName);

	delete[] pHostName;

	if (!hostEnt)
	{
		printf("Unable to collect gethostbyname\n");
		WSACleanup();
		return false;
	}

	//Создаем сокет
	SOCKET clientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (clientSock == SOCKET_ERROR)
	{
		printf("Unable to create socket\n");
		WSACleanup();
		return false;
	}

	SOCKADDR_IN serverInfo;

	serverInfo.sin_family = PF_INET;
	serverInfo.sin_addr = *((LPIN_ADDR)*hostEnt->h_addr_list);
	serverInfo.sin_port = htons(80);

	retVal = connect(clientSock, (LPSOCKADDR)&serverInfo, sizeof(serverInfo));
	if (retVal == SOCKET_ERROR) {
		WSACleanup();
		return false;
	};

	WSACleanup();

	return true;

}

bool exec(wstring& cmd)
{
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	wchar_t cmdline[MAX_PATH + 50];
	memcpy(cmdline, cmd.c_str(), (cmd.length() + 1)*sizeof(wchar_t));

	if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
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

	wstring wsServiceName = L"KVPNCSvc"; // Имя службы Керио
	wstring wsPathToRDP; // Путь к файлу настроек подключения RDP
	wstring wsPathToVPN; //Путь к клиенту
	wstring wsServerName;// Имя удаленного сервера, к которому идет подключение

	// if there is "s" parametr in command line, 
	// then it just needs to start "KVPNCSvc" service and nothing more
	if (*lpCmdLine == 's')
	{
		if (!StartServiceEasy(wsServiceName.c_str())) return -1;
		return 0;
	}

	int numArgs = 0;

	WCHAR** args = CommandLineToArgvW(GetCommandLine(), &numArgs);

	if (numArgs < 3) {

		return -1;
	}

	std::wstring arg;

	for (int i=0; i < numArgs; i++)
	{
		arg = args[i];

		if (arg.substr(0, 4).compare(L"-RDP")==0)
			wsPathToRDP = arg.substr(4, wstring::npos).c_str();
		else if (arg.substr(0, 4).compare(L"-VPN")==0)
			wsPathToVPN = arg.substr(4, wstring::npos).c_str();
		else if (arg.substr(0, 4).compare(L"-SRV")==0) 
			wsServerName = arg.substr(4, wstring::npos).c_str();

	}

	// if we already have connection to a server, we can open a remote desktop 
	if (isConnection(wsServerName))
	{
		exec(wsPathToRDP);
		return 0;
	}

	// if "KVPNCSvc" service is not started yet, we are going to start it now
	// by launching our application with administration privileges
	if (!isServiceStarted(wsServiceName.c_str()))
	{
		WCHAR pAppPath[MAX_PATH];
		GetModuleFileName(NULL, pAppPath, MAX_PATH);
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

	if (!exec(wsPathToVPN)) return -1;

	// waiting till we could get focused window of Kerio VPN Client
	while (true)
	{
		counter++;
		Sleep(100); // check for focused window every 100 milliseconds
		hKerioMainWindow = FindWindow(NULL, L"Kerio VPN Client");
		if (((hKerioMainWindow != NULL) && (GetForegroundWindow() == hKerioMainWindow)) || (counter > 30)) break;
	}

	if (hKerioMainWindow != NULL)
	{
		// emulate press of Enter key to press connection button
		SendMessage(hKerioMainWindow, WM_KEYDOWN, VK_RETURN, 0);
	}

	while (true)
	{
		Sleep(1000);
		if (isConnection(wsServerName))
		{
			exec(wsPathToRDP);
			return 0;
		}
		counter++;
		if (counter == 10) {
			return -1;
		}
	}
	return 0;
}