#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#ifdef WIN32
#include <ws2tcpip.h>
#endif
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

std::vector<int> clientSockets;    
std::mutex MutexclientSockets;    

void usage() {
    printf("\n");
    printf("syntax: echo-server <port> [-e] [-b] [-si <src ip>]\n");
    printf("  -e : echo mode\n");
    printf("  -b : broadcast mode\n");
	printf("sample: echo-server 1234 -e -b\n");
    printf("sample: echo-server 1234 -b -e\n");
    printf("sample: echo-server 1234 -e\n");
    printf("sample: echo-server 1234 -b\n");
}

struct Param {
	bool echo{false};
    bool broadcast{false}; 
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
        if (argc < 2) return false;
        
        port = atoi(argv[1]);
        if (port == 0) return false;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                continue;
            }
            if (strcmp(argv[i], "-si") == 0 && i + 1 < argc) {
                int res = inet_pton(AF_INET, argv[i + 1], &srcIp);
                switch (res) {
                    case 1: break;
                    case 0: fprintf(stderr, "not a valid network address\n"); return false;
                    case -1: myerror("inet_pton"); return false;
                }
                i++;
                continue;
            }
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return false;
        }
        return true;
    }
} param;

void recvThread(int sd) {
    {
        std::lock_guard<std::mutex>lock(MutexclientSockets);
        clientSockets.push_back(sd);
        printf("New client connected. Total clients: %zu\n", clientSockets.size());
    }

    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    char msg_with_sender[BUFSIZE + 32];
    
    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %zd", res);
            myerror(" ");
            break;
        }
        buf[res] = '\0';
        printf("Received from client %d: %s", sd, buf);
        fflush(stdout);

        if (param.echo) {
            ssize_t sendRes = ::send(sd, buf, res, 0);
            if (sendRes == 0 || sendRes == -1) {
                fprintf(stderr, "echo send failed to client %d\n", sd);
            } else {
                printf("Message echoed to client %d\n", sd);
            }
        }

        if (param.broadcast) {
            std::lock_guard<std::mutex> lock(MutexclientSockets);
            for (int clientSd : clientSockets) {
                if (clientSd != sd) {
                    snprintf(msg_with_sender, sizeof(msg_with_sender), "Client %d: %s", sd, buf);
                    size_t new_len = strlen(msg_with_sender);
                    ssize_t sendRes = ::send(clientSd, msg_with_sender, new_len, 0);
                    if (sendRes == 0 || sendRes == -1) {
                        fprintf(stderr, "broadcast send failed to client %d\n", clientSd);
                    } else {
                        printf("Message broadcasted to client %d\n", clientSd);
                    }
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(MutexclientSockets);
        auto it = std::find(clientSockets.begin(), clientSockets.end(), sd);
        if (it != clientSockets.end()) {
            clientSockets.erase(it);
            printf("Client %d disconnected. Remaining clients: %zu\n", sd, clientSockets.size());
        }
    }

    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

#ifdef __linux__
	{
		int optval = 1;
		int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (res == -1) {
			myerror("setsockopt");
			return -1;
		}
	}
#endif

	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = param.srcIp;
		addr.sin_port = htons(param.port);

		ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	{
		int res = listen(sd, 5);
		if (res == -1) {
			myerror("listen");
			return -1;
		}
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
		if (newsd == -1) {
			myerror("accept");
			break;
		}
		std::thread* t = new std::thread(recvThread, newsd);
		t->detach();
	}
	::close(sd);
}
