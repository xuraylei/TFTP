

//opcode for TFTP 
#define TFTP_OP_RRQ 	1
#define TFTP_OP_WRQ 	2
#define TFTP_OP_DATA 	3
#define TFTP_OP_ACK 	4
#define TFTP_OP_ERROR 	5

//error code for error message
#define TFTP_ERR_NOTDEFINED 0
#define TFTP_ERR_NOTFOUND 	1


typedef struct _header
{
	u_short opcode;
	u_short num;		//block number for DATA msg and error number for error msg
}tftp_header;

typedef struct _packet
{
	tftp_header header;
	char payload[512];	
}tftp_packet;