/* Georgios Gerasimos Leventopoulos csd4152 
   Konstantinos Anemozalis csd4149      
   Theofanis Tsesmetzis csd4142             */

#include "microtcp.h"
#include "../utils/crc32.h"
#include "util.h"

#define TRUE 1
#define ACK htons(4096)
#define SYN htons(16384)
#define FIN htons(32768)
#define SYN_ACK htons(20480)
#define FIN_ACK htons(36864)
/* 
ACK    	 4096	 0001000000000000  2^12
RST		 8192    0010000000000000  2^13
SYN    	 16384   0100000000000000  2^14
FIN      32768   1000000000000000  2^15
SYN_ACK  20480   0101000000000000  2^14 + 2^12
FIN_ACK  36864	 1001000000000000  2^15 + 2^12
*/
microtcp_sock_t microtcp_socket(int domain, int type, int protocol){
    microtcp_sock_t new_socket;
    new_socket.sd = socket(domain, type, protocol); /* sd is the underline UDP socket descriptor */
    if (new_socket.sd != -1){
        new_socket.state = UNKNOWN;                   /* Initialize the socket state as UNKNOWN */
        new_socket.init_win_size = MICROTCP_WIN_SIZE; /* The window size negotiated at the 3-way handshake */
        new_socket.curr_win_size = MICROTCP_WIN_SIZE; /* The current window size */
        new_socket.cwnd = MICROTCP_INIT_CWND;         /* Congestion Window = 4200 */
        new_socket.ssthresh = MICROTCP_INIT_SSTHRESH; /* ssthresh = 8192 */
    }
    else{
        perror("Error in mircotcp_socket()\n");
        new_socket.state = INVALID;
    }
    return new_socket;
}

/* returns 0 on success or -1 on failure. */
int microtcp_bind(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len){
    int result = bind(socket->sd, address, address_len); /* Result is either 0 or -1 */
    if (result == -1){
        perror("Error in microtcp_bind()\n");
        socket->state = INVALID;
    }
    return result;
}

/* Client. Attempts to connect to server. Returns 0 on success or -1 on failure. */
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len){
    // if(socket->state != UNKNOWN) return -1;
    microtcp_header_t sendToServer, receiveFromServer;
    int isPacketSent, isPacketReceived;
    socket->address = address;
    socket->size = address_len;

    /* Creating and sending the first SYN packet to the server */
    int N = getRandom(500);
    initializeHeader(&sendToServer, htons(N), 0, SYN, 0, 0, 0, 0, 0);
    isPacketSent = sendto(socket->sd, (void *)&sendToServer, sizeof(microtcp_header_t), 0, address, address_len);
    if (isPacketSent == -1){
        perror("Error in microtcp_connect(), while sending the 1st packet.\n");
        socket->state = INVALID;
        return -1;
    }

    printf("\nClient: We just sent an SYN to server\n");

    /* Waiting a response SYN_ACK packet from server */
    isPacketReceived = recvfrom(socket->sd, &receiveFromServer, sizeof(microtcp_header_t), 0, address, &address_len);
    if (isPacketReceived == -1){
        perror("Error in microtcp_connect(), while receiving packet from server.\n");
        socket->state = INVALID;
        return -1;
    }
    if (hasValidCheckSum(&receiveFromServer) != 0){
        socket->state = INVALID;
        perror("Error on checksum in microtcp_connect()\n");
        return -1;
    }
    if (receiveFromServer.control != SYN_ACK){
        socket->state = INVALID;
        perror("The recieved packet should be SYN_ACK.\n");
        return -1;
    }

    /* Sending the last ACK packet to establish te connection.  */
    initializeHeader(&sendToServer, receiveFromServer.ack_number, htons(ntohl(receiveFromServer.seq_number) + 1), ACK, 0, 0, 0, 0, 0); //check the window!!!
    isPacketSent = sendto(socket->sd, (void *)&sendToServer, sizeof(microtcp_header_t), 0, address, address_len);
    if (isPacketSent == -1){
        perror("Error in microtcp_connect(), while sending the 2nd packet.\n");
        socket->state = INVALID;
        return -1;
    }
    else{
        printf("Client: We just sent an ACK to server as an answer to SYN,ACK\n");
        socket->ack_number = ntohl(sendToServer.ack_number);
        socket->seq_number = ntohl(sendToServer.seq_number);
        socket->state = ESTABLISHED;
        printf("Connection set to established (done from client)!\n\n");
        return 0; /* success */
    }
}

/* Server. Waits for client to connect. Returns 0 on success or -1 on failure. */
int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len){
    int isPacketSent, isPacketReceived;
    microtcp_header_t sendToClient;
    microtcp_header_t receiveFromClient;
    socket->address = address;
    socket->size = address_len;

    /* Recieves the first SYN packet from client. */
    isPacketReceived = recvfrom(socket->sd, &receiveFromClient, sizeof(microtcp_header_t), 0, address, &address_len);
    if (isPacketReceived == -1){
        perror("Error in microtcp_accept(), while receiving packet from client.\n");
        socket->state = INVALID;
        return -1;
    }
    if (hasValidCheckSum(&receiveFromClient) != 0){
        socket->state = INVALID;
        perror("Error on checksum");
        return -1;
    }
    if (receiveFromClient.control != SYN){
        socket->state = INVALID;
        perror("The received packet should be SYN");
        return -1;
    }

    /* Creates and sends back the SYN_ACK packet to the client. */
    int N = getRandom(500);
    initializeHeader(&sendToClient, htons(N), htons(ntohl(receiveFromClient.seq_number) + 1), SYN_ACK, 0, 0, 0, 0, 0); //check the window!!!
    isPacketSent = sendto(socket->sd, (void *)&sendToClient, sizeof(microtcp_header_t), 0, address, address_len);
    if (isPacketSent == -1){
        perror("Error in microtcp_connect(), while sending the 1st packet.\n");
        socket->state = INVALID;
        return -1;
    }
    printf("\nServer: We just sent an SYN,ACK to client as an answer to the SYN\n");

    /* Recieves the ACK packet from the client. That is the end of our connection. */
    isPacketReceived = recvfrom(socket->sd, &receiveFromClient, sizeof(microtcp_header_t), 0, address, &address_len);
    if (isPacketReceived == -1) {
        perror("Error in microtcp_connect(), while receiving packet from server.\n");
        socket->state = INVALID;
        return -1;
    }
    if (hasValidCheckSum(&receiveFromClient) != 0){
        socket->state = INVALID;
        perror("Error on checksum");
        return -1;
    }
    if (receiveFromClient.control != ACK){
        socket->state = INVALID;
        perror("The recieved packet should be ACK");
        return -1;
    }
    printf("Connection set to established (done from server)\n\n");
    return 0;
}

/* return 0 on success or -1 on failure */
int microtcp_shutdown(microtcp_sock_t *socket, int how){
    if (socket->state != ESTABLISHED && socket->state != CLOSING_BY_PEER){
        perror("You can't call shutdown with this socket state.");
        return -1;
    }
    //printf("\nYou called shutdown\n\n");
    microtcp_header_t send;
    microtcp_header_t receive;
    int isPacketReceived, isPacketSent;
    int X, Y;

    if (socket->state != CLOSING_BY_PEER) { /* client */
        /* Send 1st packet to server */
        X = getRandom(500);
        initializeHeader(&send, htons(X), 0, FIN_ACK, 0, 0, 0, 0, 0); 
        isPacketSent = sendto(socket->sd, (void *)&send, sizeof(microtcp_header_t), 0, socket->address, socket->size); 
        if (isPacketSent == -1){
            perror("Error in microtcp_shutdown(), while sending the FIN_ACK packet to server.\n");
            socket->state = INVALID;
            return -1;
        }
        printf("Client: We just sent a FIN_ACK\n");
        /* receive 1st packet and check it     */
        /* Not sure about the last 2 arguments */ 
        isPacketReceived = recvfrom(socket->sd, &receive, sizeof(microtcp_header_t), 0, socket->address, &socket->size);
        if (isPacketReceived == -1){
            perror("Error in microtcp_shutdown, while receiving 1st packet from server.\n");
            socket->state = INVALID;
            return -1;
        }
        //printf("Client: We just received a ACK after we send the FINACK.\n");
        if (hasValidCheckSum(&receive) != 0){
            socket->state = INVALID;
            perror("Error on checksum");
            return -1;
        }
        if (receive.control != ACK){
            socket->state = INVALID;
            perror("Error, the received packet should be ACK\n");
            return -1;
        }

        socket->state = CLOSING_BY_HOST; /* change state for client */

        /* Receive 2nd packet from server */
        /* Not sure about the last 2 arguments */
        isPacketReceived = recvfrom(socket->sd, &receive, sizeof(microtcp_header_t), 0, socket->address, &socket->size);
        if (isPacketReceived == -1){
            perror("Error in microtcp_shutdown, while receiving the 2nd packet from server.\n");
            socket->state = INVALID;
            return -1;
        }
        if (hasValidCheckSum(&receive) != 0){
            socket->state = INVALID;
            perror("Error on checksum");
            return -1;
        }
        if (receive.control != FIN_ACK){
            socket->state = INVALID;
            perror("The recieved packet should be FIN_ACK\n");
            return -1;
        }
        //printf("Client: We just receiverd a FIN_ACk from server\n");

        
        /* client sends the final packet to server */
        initializeHeader(&send, htons(ntohl(receive.ack_number)), htons(ntohl(receive.seq_number) + 1), ACK, 0, 0, 0, 0, 0);
        isPacketSent = sendto(socket->sd, (void *)&send, sizeof(microtcp_header_t), 0, socket->address, socket->size);
        if (isPacketSent == -1){
            perror("Error in microtcp_connect(), while sending the the final packet to client.\n");
            socket->state = INVALID;
            return -1;
        }
        printf("Client: We just sent an ACK to server as an answer to the FINACK\n");
    }
    else{ /* server */

        
        /* send 1st packet to client */
        initializeHeader(&send, 0, htons(ntohl(receive.seq_number) + 1), ACK, 0, 0, 0, 0, 0); /* window ?? */
        isPacketSent = sendto(socket->sd, (void *)&send, sizeof(microtcp_header_t), 0, socket->address, socket->size);
        if (isPacketSent == -1){
            perror("Error in microtcp_connect(), while sending the 1st packet to client.\n");
            socket->state = INVALID;
            return -1;
        }

        printf("Server: We just sent an ACK as an answer to FINACK\n");

        /* send 2nd packet to client */
        Y = getRandom(500);                                                                               /* is ack the same as before?? */
        initializeHeader(&send, htons(Y), htons(ntohl(receive.seq_number) + 1), FIN_ACK, 0, 0, 0, 0, 0); /* window ?? */
        isPacketSent = sendto(socket->sd, (void *)&send, sizeof(microtcp_header_t), 0, socket->address, socket->size);
        if (isPacketSent == -1){
            perror("Error in microtcp_connect(), while sending the 1st packet to client.\n");
            socket->state = INVALID;
            return -1;
        }
        printf("Server: We just sent a FIN_ACK\n");

        /* server receives the final packet */
        isPacketReceived = recvfrom(socket->sd, &receive, sizeof(microtcp_header_t), 0, socket->address, &socket->size);
        if (isPacketReceived == -1){
            perror("Error in microtcp_shutdown, while receiving the final packet from client.\n");
            socket->state = INVALID;
            return -1;
        }
        //printf("Server: We just received an ACK from client\n");
        if (hasValidCheckSum(&receive) != 0){
            socket->state = INVALID;
            perror("Error on checksum");
            return -1;
        }
        if (receive.control != ACK){
            socket->state = INVALID;
            perror("The recieved packet should be ACK");
            return -1;
        }
    }
    socket->state = CLOSED;
    return 0; /* return 0 on sucess */     
}

/* Eπιστρέϕει τον αριθμό των bytes που επιτυχημένα και επιβεβαιωμένα έστειλε στον παραλήπτη. */
ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags){
    struct timeval timeout;
    size_t sentUpTo, dataLeft, payloadSize, totalBytes, chunks, chunkSize, seq;
    microtcp_header_t header;
    int receiveResult;
    void *outBuffer = malloc((MICROTCP_MSS + sizeof(microtcp_header_t)) * sizeof(char));
    void *inBuffer = malloc((MICROTCP_MSS + sizeof(microtcp_header_t)) * sizeof(char));
    int i;

    if(buffer == NULL) {
        perror("Error: Null buffer\n");
        return 0;
    }
    totalBytes = 0;
    timeout.tv_sec = 0;
    timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
    sentUpTo = 0;
    dataLeft = length;
    seq = socket->seq_number;

    while (sentUpTo < length) {
        totalBytes = min(socket->curr_win_size, min(socket->cwnd, dataLeft));
        chunks = totalBytes / MICROTCP_MSS;
        for (i = 0; i < chunks; i++) {
            initializeHeader(&header, htons(seq), 0, 0, 0, htonl(MICROTCP_MSS), 0, 0, 0);
            memcpy(outBuffer, &header, sizeof(microtcp_header_t));
            memcpy(outBuffer + sizeof(microtcp_header_t), buffer + i * MICROTCP_MSS + sentUpTo, MICROTCP_MSS);

            if (sendto(socket->sd, outBuffer, MICROTCP_MSS, 0, (struct sockaddr *)&socket->address, socket->size) < 0) {
                socket->state = INVALID;
                perror("microTCP Send  - while trying to send data\n");
                free(outBuffer);
                return -1;
            }
            sentUpTo += MICROTCP_MSS;
            seq += MICROTCP_MSS;
        }
        /* Check if there is a semi-filled chunk */
        if (totalBytes % MICROTCP_MSS){
            chunks++;
            initializeHeader(&header, htons(seq), 0, 0, 0, htonl(totalBytes), 0, 0, 0);
            memcpy(outBuffer, &header, sizeof(microtcp_header_t));
            memcpy(outBuffer + sizeof(microtcp_header_t), buffer, MICROTCP_MSS); //+i*MICROTCP_MSS+sentUpTo na to alaxeis -- oxi MSS alla total bytes
            if (sendto(socket->sd, outBuffer, MICROTCP_MSS +sizeof(microtcp_header_t), 0, socket->address, socket->size) < 0){
                socket->state = INVALID;
                perror("microTCP Send  - while trying to send");
                free(outBuffer);
                return -1;
            }
            seq += totalBytes;
            sentUpTo += totalBytes % MICROTCP_MSS;
        }

    }
    socket->ack_number += sentUpTo;
    free(outBuffer);
    free(inBuffer);
    return sentUpTo;
}


ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags){
    int receiveResult;
    void *inBuffer = malloc((MICROTCP_MSS + sizeof(microtcp_header_t)) * sizeof(char));
    microtcp_header_t *tmpHeader;
    microtcp_header_t inHeader, outHeader;
    size_t expectedSeq = socket->seq_number;
    size_t seqToSend = socket->ack_number;
    while (TRUE){
        receiveResult = recvfrom(socket->sd, inBuffer, MICROTCP_MSS + sizeof(microtcp_header_t), 0, socket->address, &socket->size);
        if(receiveResult < 0) {
            perror("Server receive error.\n");
            return -1;
        }
        memcpy(buffer, inBuffer+sizeof(microtcp_header_t) , receiveResult-sizeof(microtcp_header_t));
       // printf("\nreceiveResult = ld\n", (receiveResult - sizeof(microtcp_header_t) ));

        tmpHeader = (microtcp_header_t *)inBuffer;
        
        if(tmpHeader->control == FIN_ACK){
            socket->seq_number = ntohl(tmpHeader->seq_number);
            //printf("We just received a FIN_ACK\n");
			socket->state = CLOSING_BY_PEER;
			return -1;
		}	
        break;

        /*    tmpHeader = (microtcp_header_t *)inBuffer;
        initializeHeader(&inHeader, tmpHeader->seq_number, tmpHeader->ack_number, tmpHeader->control, tmpHeader->window, tmpHeader->data_len, 0, 0, 0);

        /// do some finack  here

        if (ntohl(inHeader->checksum) == crc32(inBuffer, sizeof(inBuffer)))
        {
            if (ntohl(inHeader->seq_number) == expectedSeq)
            {
                //good
                memcpy(curBuffPos, inBuffer + sizeof(microtcp_header_t), inHeader->data_len, htons(MICROTCP_RECVBUF_LEN - indexRecvBuff));
                curBuffPos += inHeader->data_len;
                expectedSeq++;
            }
            initializeHeader(&outHeader, htonl(seqToSend++), htonl(expectedSeq), ACK, htons(MICROTCP_RECVBUF_LEN - curBuffPos), inHeader->data_len, 0, 0, 0);

            memcpy(buffer, &outHeader, sizeof(microtcp_header_t));
            outHeader->checksum = htonl(crc32(buffer, sizeof(buffer)));
            memcpy(buffer, &outHeader, sizeof(microtcp_header_t));

            if (sendto(socket->sd, (void *)&send_head_pack, sizeof(microtcp_header_t), 0, (struct sockaddr *)&socket->clientAddress, socket->clientSize) < 0)
            { 
                socket->state = INVALID;
                perror("microTCP rcv connection error - While trying to send ACK\n");
            }
        }
        else
        {
            //checksum error
        }
    }

    if (socket->state != ESTABLISHED)
        perror("Connection is not established");
    ssize_t receivedData = 0;
    void *bufferTosend = malloc(sizeof(microtcp_header_t));
    void *bufferForRecv = malloc(MICROTCP_MSS);
    void *receiveBuffer = malloc(MICROTCP_RECVBUF_LEN);
    int isReceived;

    /* Timeout timer for socket 
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;

    while (TRUE)
    {
        if (setsockopt(socket->sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) < 0)
        {
            socket->state = INVALID;
            perror("Error: Set Timeout\n");
            return 0;
        }
        isReceived = recvfrom(socket->sd, bufferForRecv, MICROTCP_MSS, 0, (struct sockaddr *)&socket->clientAddress, &socket->clientSize);
        if (isReceived > 0)
        {
        }
        else
        { /* Then error (isReceived == -1) 
        }
    */
    }
    return (receiveResult - sizeof(microtcp_header_t));
}