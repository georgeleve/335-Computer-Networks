/* Georgios Gerasimos Leventopoulos csd4152 
   Konstantinos Anemozalis csd4149      
   Theofanis Tsesmetzis csd4142             */
   
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <unistd.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))

/* Change the check sum of a microtcp header */
void insertToBuffer(void* buffer, microtcp_header_t *h, size_t headerSize, void* dataBuffer, size_t insertFrom, size_t dataSize){
  memcpy(buffer, h, headerSize);
  memcpy(buffer+headerSize, dataBuffer+insertFrom, dataSize);
  h->checksum = htonl(crc32(buffer, sizeof(buffer)));
  memcpy(buffer, h, headerSize);
}

/* Validate the check sum of a microtcp header */
int hasValidCheckSum(microtcp_header_t *h){
	uint8_t buff[8192];
	memset(buff, 0, sizeof(buff));
	memcpy(buff, h, sizeof(microtcp_header_t));
	return (h->checksum == crc32(buff, sizeof(buff)));
}

/* Initialize a microtcp header */
void initializeHeader(microtcp_header_t *h, uint32_t seq_number, uint32_t ack_number, uint16_t control, uint16_t window, uint32_t data_len, uint32_t future_use0, uint32_t future_use1, uint32_t future_use2){
  h->seq_number = seq_number;
  h->ack_number = ack_number;
  h->control = control; 
  h->window = window;
  h->data_len = data_len;
  h->future_use0 = future_use0;
  h->future_use1 = future_use1;
  h->future_use2 = future_use2;
}

int getRandom(int max){
  srand(time(NULL));
  return rand()%(max+1);
}