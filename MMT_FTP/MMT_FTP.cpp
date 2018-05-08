// MMT_FTP.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MMT_FTP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define MAX_BUF_LEN 1000

// The one and only application object

CWinApp theApp;

using namespace std;

struct HostNumber {
	unsigned int h[4];
};

struct PortNumber {
	unsigned int p[2];
};

string PrintResponse(CSocket& receiver) {
	char buffer[MAX_BUF_LEN];
	int byte = receiver.Receive(buffer, MAX_BUF_LEN);
	buffer[byte] = '\0';
	cout << buffer;
	string responseCode(buffer, 3);

	if (buffer[3] == '-') {
		string rc = "---";
		do {
			byte = receiver.Receive(buffer, MAX_BUF_LEN);
			buffer[byte] = '\0';
			cout << buffer;
			rc = (buffer, 3);
		} while (rc != responseCode || buffer[3] != ' ');
	}

	return responseCode;
}

bool Login(CSocket& client) {
	string user, pass, msg;
	string responseCode;
	
	cout << "Username: ";
	cin >> user;
	cin.ignore();
	msg = "user " + user + "\r\n";
	client.Send(msg.c_str(), msg.length());
	responseCode = PrintResponse(client);
	if (responseCode[0] == '5') {
		return false;
	}

	cout << "Password: ";
	cin >> pass;
	cin.ignore();
	msg = "pass " + pass + "\r\n";
	client.Send(msg.c_str(), msg.length());
	responseCode = PrintResponse(client);
	if (responseCode[0] == '5') {
		return false;
	}
	return true;
}

bool AskYN(const string& question) {
	char ans;
	do {
		cout << question << "(Y/N): ";
		cin >> ans;
		ans = toupper(ans);
	} while (ans != 'Y' && ans != 'N');
	cin.ignore();

	return ans == 'Y';
}

HostNumber GetHostNumber(CSocket& socket) {
	CString name;
	UINT port;
	socket.GetSockName(name, port);

	unsigned int a[4] = { 0 };
	int pos = 0;
	CString seperator = _T(".");
	for (int i = 0; i < 4; i++) {
		a[i] = atoi(CT2A(name.Tokenize(seperator, pos)));
	}

	return { a[0], a[1], a[2], a[3] };
}

PortNumber GetPortNumber(CSocket& socket) {
	CString name;
	UINT port;
	socket.GetSockName(name, port);

	return { port / (16 * 16), port % (16 * 16) };
}

void CreateDataConnection(CSocket& socket, HostNumber hn, PortNumber pn) {
	string msg = "port " + to_string(hn.h[0]) + "," + to_string(hn.h[1]) + "," 
						 + to_string(hn.h[2]) + "," + to_string(hn.h[3]) + ","
						 + to_string(pn.p[0]) + "," + to_string(pn.p[1]) + "\r\n";

	socket.Send(msg.c_str(), msg.length());

	string responseCode = PrintResponse(socket);
}

void CreatePassiveConnection(CSocket& client, CSocket& socket) {
	string msg = "pasv\r\n";
	client.Send(msg.c_str(), msg.length());

	char buffer[MAX_BUF_LEN] = { 0 };
	int byte = client.Receive(buffer, MAX_BUF_LEN);
	buffer[byte] = '\0';

	string recvStr(buffer);
	recvStr = recvStr.substr(recvStr.find('(') + 1, recvStr.find(')') - recvStr.find('(') - 1);
	cout << recvStr << endl;

	HostNumber hn;
	PortNumber pn;
	stringstream ss(recvStr);

	for (int i = 0; i < 4; i++) {
		string number;
		getline(ss, number, ',');
		hn.h[i] = atoi(number.c_str());
	}
	
	for (int i = 0; i < 2; i++) {
		string number;
		getline(ss, number, ',');
		pn.p[i] = atoi(number.c_str());
	}

	string host = to_string(hn.h[0]) + "." + to_string(hn.h[1]) + "." + to_string(hn.h[2]) + "." + to_string(hn.h[3]);
	unsigned int port = pn.p[0] * 16 * 16 + pn.p[1];

	socket.Connect(CStringW(host.c_str()), port);
}

int main(int argc, char* argv[])
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // initialize MFC and print and error on failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: change error code to suit your needs
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            // TODO: code your application's behavior here.
			if (argc != 3) {
				cout << "Usage: <program name> <server id> <port number>\n";
				return 1;
			}

			CSocket client;
			string line;
			string token;
			string firstToken;

			if (AfxSocketInit()) {
				if (client.Create()) {
					if (client.Connect(CStringW(argv[1]), atoi(argv[2]))) {

						PrintResponse(client);

						while (!Login(client) && AskYN("Do you want to login again ?"));

						do {
							cout << "ftp> ";
							getline(cin, line);
							stringstream ss(line);
							ss >> token;
							firstToken = token;
							line += "\r\n";

							if (firstToken == "!ls" || firstToken == "!dir") {
								system("dir");
							}

							if (firstToken == "lcd") {
								ss >> token;
								if (_chdir(token.c_str()) < 0) {
									cout << "Directory doesn't exist\n";
								}
							}

							if (firstToken == "user" || firstToken == "pass" || firstToken == "pwd") {
								client.Send((char*)line.c_str(), line.length());
								PrintResponse(client);
							}

							if (firstToken == "ls" || firstToken == "dir") {
								CSocket connector, sock;
								sock.Create();
								sock.Listen();

								CreateDataConnection(client, GetHostNumber(client), GetPortNumber(sock));
								line.replace(line.begin(), line.begin() + (firstToken == "ls" ? 2 : 3), firstToken == "ls" ? "nlst" : "list");
								client.Send(line.c_str(), line.length());
								PrintResponse(client);
								if (sock.Accept(connector)) {
									PrintResponse(client);
									PrintResponse(connector);
								}

								sock.Close();
							}

							if (firstToken == "get") {
								CSocket connector, sock;
								sock.Create();
								sock.Listen();

								CreateDataConnection(client, GetHostNumber(client), GetPortNumber(sock));
								line.replace(line.begin(), line.begin() + 3, "retr");

								client.Send(line.c_str(), line.length());
								string responseCode = PrintResponse(client);
								if (responseCode[0] == '1' && sock.Accept(connector)) {
									string filename;
									ss >> filename;
									ofstream os(filename.c_str());

									char buffer[MAX_BUF_LEN] = { 0 };
									int len = 0;
									do {
										len = connector.Receive(buffer, MAX_BUF_LEN);
										os.write(buffer, len);
									} while (len == MAX_BUF_LEN);

									PrintResponse(client);
								}
								sock.Close();
							}

							if (firstToken == "pasv") {
								CSocket socket;
								PortNumber pn = GetPortNumber(client);
								if (socket.Create(pn.p[0] * 16 * 16 + pn.p[1] + 1)) {
									CreatePassiveConnection(client, socket);

									ss >> token;
									if (token == "get") {
										line.replace(line.begin(), line.begin() + 8, "retr");
									}

									client.Send(line.c_str(), line.length());
									string responseCode = PrintResponse(client);
									if (responseCode[0] == '1') {
										string filename;
										ss >> filename;
										ofstream os(filename.c_str());

										char buffer[MAX_BUF_LEN] = { 0 };
										int len = 0;
										do {
											len = socket.Receive(buffer, MAX_BUF_LEN);
											os.write(buffer, len);
										} while (len == MAX_BUF_LEN);

										PrintResponse(client);
									}
									socket.Close();
								}
								else {
									cout << "Cannot create port " << pn.p[0] * 16 * 16 + pn.p[1] + 1 << endl;
								}
							}

						} while (firstToken != "exit" && firstToken != "quit");
					}
					else {
						cout << "Problem connecting to Server ID: " << argv[1] << " at Port number: " << argv[2] << endl;
					}
				
					client.Close();
				}
				else {
					cout << "Problem creating the socket\n";
					return 1;
				}
			}
			else {
				cout << "Problem initializing Socket API\n";
				return 1;
			}
        }
    }
    else
    {
        // TODO: change error code to suit your needs
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}
