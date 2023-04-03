#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h> //コンパイル時に -pthreadオプションをつける

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEVICE_DRIVER "/dev/lrimo"
#define LOGFILE "/home/pi/BackApp/applog"
#define SERVER_PORT 8080
#define BUF_SIZE 3

pthread_mutex_t FILE_ACCESS;
unsigned char LightMode = 0; //証明のon/off状態 //最後に書き込んだ値(あくまでも予測) //FILE_ACCESS所有時のみ変更可

int put_log(char *message){
	time_t now;
	struct tm *local_time;
	struct tm thread_local_time;
	time(&now);
	local_time = localtime_r(&now, &thread_local_time); //各スレッドから呼び出す時は構造体定義してから //localtime関数はグローバル変数使うのでスレッドセーフでない
	char now_print[20];
	memset(now_print, 0, sizeof(char)*20);
	sprintf(now_print, "%02d-%02d-%02d %02d:%02d:%02d", local_time->tm_year%100,local_time->tm_mon+1,local_time->tm_mday,local_time->tm_hour,local_time->tm_min,local_time->tm_sec);

	FILE *fp_log = fopen(LOGFILE, "a");
	fprintf(fp_log,"%s RimoconServer : %s\n", now_print, message);
	fclose(fp_log);
	return 0;
}

int put_signal(char *signal, char *executer){
	char message[50];
	sprintf(message, "signal %s from %s", signal, executer);
	return put_log(message);
}

void *button_read(void *arg){
	FILE *dd;
	if((dd = fopen(DEVICE_DRIVER, "r+")) == NULL){
		put_log("device driver open error at button_read");
		return NULL;
	}
	unsigned char read_val;
	unsigned char write_val;
	char signal_name[][4] = {"off", "on"};
	int pushing = 0;

	while(fread(&read_val, sizeof(char), 1, dd) == 1){
		if(!pushing && read_val==1){ //スイッチの押下を検知
			pushing = 1;
		}
		if(pushing && read_val==0){ //スイッチの開放を検知
			if(pthread_mutex_trylock(&FILE_ACCESS)==0){
				write_val = (LightMode+1)%2;
				if(fwrite(&write_val, sizeof(char), 1, dd)==1){
					LightMode = write_val;
					put_signal(signal_name[write_val], "Button");
				}else{
					put_log("device driver write error at button_read");
				}
				pthread_mutex_unlock(&FILE_ACCESS);
			}else{
				put_log("failed to get device driver write access at button_read");
			}
			pushing = 0;
		}
	}
	fclose(dd);
	put_log("exit button_read");
	return NULL;
}

int getsignal(int sock, char *signal, int len){
	char send_buf=0;
	char recv_buf[len];

	memset(recv_buf, 0, sizeof(char)*len+1);
	if(recv(sock, recv_buf, sizeof(char)*len, 0) <= 0){ //受信
		put_log("recv error\n");
		send_buf = 1;
	}else{
		for(int i=0; i<len; i++){
			*(signal+i) = recv_buf[i];
		}
		*(signal+len) = 0;
	}
	// 返答
	if(send(sock, &send_buf, 1, 0)==-1){
		put_log( "send error");
		return -1;
	}
	return -send_buf;
}

void *socket_server(void *arg){
	int w_addr, c_sock;
	struct sockaddr_in a_addr;

	w_addr = socket(AF_INET, SOCK_STREAM, 0);
	if(w_addr == -1){
		put_log("socket error");
		return NULL;
	}

	/*構造体を全て0にセット*/
	memset(&a_addr, 0, sizeof(struct sockaddr_in));

	/*サーバーのipとポート設定*/
	a_addr.sin_family = AF_INET;
	a_addr.sin_port = htons((unsigned short)SERVER_PORT);
	a_addr.sin_addr.s_addr = INADDR_ANY; //通信相手は任意

	/*ソケットに情報設定*/
	if(bind(w_addr, (const struct sockaddr *)&a_addr, sizeof(a_addr)) == -1){
		put_log("bind error");
		return NULL;
	}

	/*ソケットを接続待ちに設定*/
	if(listen(w_addr, 2) == -1){
		put_log("listen error");
		close(w_addr);
		return NULL;
	}

	char signal[BUF_SIZE];
	while(1){
		// waiting connection...
		if((c_sock = accept(w_addr, NULL, NULL))==-1){
			put_log( "accept error");
			close(w_addr);
		}
		// connected
		if(getsignal(c_sock, signal, BUF_SIZE)==-1){
			continue;
		}

		unsigned char sig = -1;
		if(strcmp(signal, "off") == 0){
			sig = 0;
		}else if(strcmp(signal, "on") == 0){
			sig = 1;
		}else{
			continue;
		}

		if(pthread_mutex_trylock(&FILE_ACCESS)==0){
			FILE *dd;
			if((dd = fopen(DEVICE_DRIVER, "w")) == NULL){
				put_log("device driver open error at socket_server");
			}else{
				if(fwrite(&sig, sizeof(unsigned char), 1, dd)==1){
					LightMode = sig;
					put_signal(signal, "App");
				}else{
					put_log( "device driver write error at socket_server");
				}
				fclose(dd);
			}
			pthread_mutex_unlock(&FILE_ACCESS);
		}else{
			put_log("failed to get device driver write access at socket_server");
		}
	}
	close(w_addr);

	put_log("exit socket_server");
	return NULL;
}

int main(){
	pthread_mutex_init(&FILE_ACCESS, NULL);

	pthread_t button_thread, socket_thread;
	pthread_create(&button_thread, NULL, button_read, NULL);
	pthread_create(&socket_thread, NULL, socket_server, NULL);

	pthread_join(button_thread, NULL);
	pthread_join(socket_thread, NULL);

	pthread_mutex_destroy(&FILE_ACCESS);
	
	return 0;
}
