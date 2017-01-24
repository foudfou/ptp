#ifndef SERVER_H
#define SERVER_H

#define SERVER_BUFLEN 10

int server_applysockopts(const int sock, const int family);
int server_start(const char bind_addr[], const char bind_port[]);
int server_stop(int sock);
int server_conn_accept(const int listenfd);
int server_conn_close(int client);

#endif /* SERVER_H */
