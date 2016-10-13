#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <stdbool.h>

#include "packet.h"


#define TIMEOUT 1
#define WINDOW_SIZE 1

#define MAX_CLIENT_NUM 100      // 

#define MAX_SOCKET_BUFFER 1000   //the max buffer for incomming packet
#define MAX_FILE_BUFFER 1024*1024 //we support max data 1M
#define FILE_NAME_LEN 100       //the len for file name


//for concurrent request, we assume each client can only run single thread TFTP client
//hence, we use sockadd_in  as the ID of the client
//structure for TFTP client profile
typedef struct _client{
  struct sockaddr_in id;

  int cur_seq;  //current sequence number
  int done;     //if the file is transmitted
 //remaining data to transfer
  int data_len;
  char data[MAX_FILE_BUFFER];


  struct _client *next;
}tftp_client;

//add and initialize a client profile 
tftp_client*  addClient(tftp_client* list, struct sockaddr_in addr, char *file, int len){

  tftp_client *client = (tftp_client*) malloc(sizeof (tftp_client)); 

  client->id = addr;
  client->cur_seq = 1;
  client->done = 0;
  //check if the len less than MAX_FILE_BUFFER
  memcpy(client->data, file, len);
  client->data_len = len;
  client->next = NULL;

  //add the client to the tail of the client list
  tftp_client *tmp = list->next;

  if (tmp == NULL){
    list->next = client;
    return client;
  }
  
  while (tmp->next != NULL){
    tmp = tmp->next;
  }
  tmp->next = client;

  return client;
}



//compare if two address is equal
bool addrIsEqual(struct sockaddr_in a, struct sockaddr_in b){
  return ((a.sin_addr.s_addr == b.sin_addr.s_addr ) && (a.sin_port = b.sin_port));
}

//return the client profile
tftp_client* getClient(tftp_client* list, struct sockaddr_in addr){

  tftp_client *tmp = list->next;

  while(tmp != NULL && !addrIsEqual(tmp->id, addr)){
    tmp = tmp->next;
  }
  return tmp;
}

void removeClient(tftp_client* list, struct sockaddr_in addr){
  
  tftp_client *tmp = list->next;
  tftp_client *prev = list;

  while(tmp != NULL && !addrIsEqual(tmp->id, addr)){
    prev = tmp;
    tmp = tmp->next;
  }

  //double check if the file transmission for the client is done
  if (!tmp->done){
    perror("Terminating unfinished file transmission!");
  }
  else{
    prev->next = tmp->next;
  }

}

/////////////////////////////////////////////
//main function
int main(int argc, char *argv[])
{

  struct sockaddr_in server_addr, client_addr; 
  int server_sock;
  int block_number;  //indicate block number
  int port;
  socklen_t sock_len = sizeof(client_addr);

  FILE* pfile;
  fd_set readfds; 

  tftp_client *client_list = (tftp_client*) malloc(sizeof (tftp_client));

  char file_name[FILE_NAME_LEN];
  char input_buffer[MAX_SOCKET_BUFFER];
  char file_buffer[MAX_FILE_BUFFER];

  //tftp message
  int opcode, ack;

  //timeout
  time_t tm;
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0; //total time of 1 second

  if (argc != 2){
    printf("Invalid parameter.\nUsage: %s portnum \n", argv[0]);
    exit(-1);
  }

  port = atoi(argv[1]);

  bzero((char*) &server_addr,sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY; 

  server_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(server_sock == -1){
      perror("Cannot create socket. quit...\n");
        exit(-1);
  }

  if(bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
      perror("Cannot bind to the socket. quit... \n");
      exit(-1);
  }


  FD_ZERO (&readfds);
  FD_SET (server_sock, &readfds);

  while(1)
  {


//select (sock+1, &readfds,0,0,&tv);
if (select (server_sock+1, &readfds, NULL, NULL, NULL) < 0){
  perror("Select() error!");
  exit(-1); 
}

int num = recvfrom(server_sock, input_buffer, sizeof(input_buffer), 0, &client_addr, &sock_len);

//debug
printf("Reciving message from client %d:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


if (num < 0){
    perror("Cannot create socket. quit... \n");
    exit(-1);
 }

/*
if (completed == 1 && *(client_data+1) == 4 ) //after all packets have been sent only ack remain
        {
            block_no = *(client_data +3);
            printf(" ACK received for block # %d\n", block_no);
            continue; //back to start of loop
        }
*/

opcode = input_buffer[1];


//debug
//printf("opcode: %d", input_buffer[1]);
//fflush(stdout);

//respone msg
tftp_header res_header;
tftp_packet res_packet; 

switch (opcode)
{
  case TFTP_OP_RRQ:
        
        strcpy(file_name, input_buffer + 2);

        pfile = fopen(file_name, "r");

        //initialize response message
        bzero((char*) &res_header, sizeof(tftp_header));
        bzero((char*) &res_packet, sizeof(tftp_packet));

        //if we cannot locate the file
        if (pfile == NULL) 
        { 
          res_header.opcode = htons(TFTP_OP_ERROR);
          res_header.num =  htons(TFTP_ERR_NOTFOUND);    
          res_packet.header = res_header;

          strcpy(res_packet.payload, "File not Found");

				  perror("Cannot find file");
          
          
          int size = strlen(res_packet.payload) + 5;  

          
          if (sendto(server_sock, &res_packet, size, 0, &client_addr, sock_len) < 0)
          {
            perror("Cannot send out RRQ message");
            exit(-1);
          }
          break;
        }
        else //file is found
        {
            block_number =1;

            int file_size = fread(file_buffer, 1, MAX_FILE_BUFFER, pfile);

            printf("file size %d\n", file_size);
            fflush(stdout);

            if (file_size < 512){
                
                res_header.opcode = htons(TFTP_OP_DATA);
                res_header.num = htons(block_number);

                res_packet.header = res_header;
                memcpy(res_packet.payload, file_buffer,file_size);

                int size = file_size + 4;

                if(sendto(server_sock, &res_packet, size, 0, &client_addr, sock_len) <0)
                {
                  perror("Cannot send out DATA message. quitting...\n");
                  exit(-1);
                }
                  
            }
            else{ //if the file is larger than 512B, we should maitain a client profile 
              tftp_client* client = getClient(client_list, client_addr);

              printf("send out large file");
              fflush(stdout);

              if (client == NULL){
                client = addClient(client_list, client_addr, file_buffer, file_size);
              }

              res_header.opcode = htons(TFTP_OP_DATA);
              res_header.num = htons(block_number);

              res_packet.header = res_header;
              memcpy(res_packet.payload, file_buffer, 512);

              int size =  516;

              if(sendto(server_sock, &res_packet, size, 0, &client_addr, sock_len) <0)
              {
                perror("Cannot send out DATA message. quitting...\n");
                exit(-1);
              }

              //calculate remaining data to send out
              client->data_len -= 512;
              memcpy(client->data, client->data+512, client->data_len);
            }
   break;

  case TFTP_OP_ACK:
    ack = (int)input_buffer[3];

    printf("test\n");
    fflush(stdout);

    tftp_client* client = getClient(client_list, client_addr);

    if (client == NULL){ //ignore out-of-order ack
      printf("break\n");
    fflush(stdout);
      break;
    }

    if (client->data_len < 512){
                block_number = client->cur_seq + 1;
                char* file_buffer = client->data;
                int file_size = client->data_len;

                res_header.opcode = htons(TFTP_OP_DATA);
                res_header.num = htons(block_number);

                res_packet.header = res_header;
                memcpy(res_packet.payload, file_buffer, file_size);

                int size = file_size + 4;

                if(sendto(server_sock, &res_packet, size, 0, &client_addr, sock_len) <0)
                {
                  perror("Cannot send out DATA message. quitting...\n");
                  exit(-1);
                }

                //calculate remaining data to send out
                client->data_len = 0;
                client->done = 1;
                client->cur_seq++;

                removeClient(client_list, client_addr);
    }
    else{
            block_number = client->cur_seq +1;
            char* file_buffer = client->data;
            
    

              res_header.opcode = htons(TFTP_OP_DATA);
              res_header.num = htons(block_number);

              res_packet.header = res_header;
              memcpy(res_packet.payload, file_buffer, 512);

              int size =  516;

              if(sendto(server_sock, &res_packet, size, 0, &client_addr, sock_len) <0)
              {
                perror("Cannot send out DATA message. quitting...\n");
                exit(-1);
              }

              //calculate remaining data to send out
              client->data_len -= 512;
              memcpy(client->data, client->data+512, client->data_len);
              client->cur_seq++;

              printf("remaining data len: %d", client->data_len);
              fflush(stdout);
    }
  	break;

  default :
    perror("Error in parsing TFTP OPCODE.\n");
       break;
   }
}
fclose(pfile);
//close(server_sock);
}
return 0;
}


                                                                                                                     