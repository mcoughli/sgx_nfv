/*
 * App.h
 *
 *  Created on: Nov 28, 2016
 *      Author: michael
 */

#ifndef ENCLAVE_ENCLAVE1_APP_APP_H_
#define ENCLAVE_ENCLAVE1_APP_APP_H_

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <assert.h>
#include <string>

#include <unistd.h>
#include <pwd.h>

#include "sgx_urts.h"
#include "App.h"
#include "enclave1_u.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "sgx_tcrypto.h"

#include "stdlib.h"
#include "math.h"
#include "pthread.h"

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif
/*
 To build an example that is linked against the library:
 g++ -Ienclave_enclave1/untrusted -I/opt/intel/sgxsdk/include/ -Lenclave_enclave1/ -L/opt/intel/sgxsdk/lib64 test.cpp -o test.o -lapp -lsgx_urts_sim -lsgx_uae_service_sim -lpthread

 env. variables to build with click:
 export CPPFLAGS="-I/opt/intel/sgxsdk/include/ -I/home/michael/sgx_nfv/NFV_Basic_SGX/sgx/enclave_enclave1/untrusted/"
 export LDFLAGS="-L/home/michael/sgx_nfv/NFV_Basic_SGX/sgx/enclave_enclave1/ -L/opt/intel/sgxsdk/lib64"
 export LIBS="-lapp -lsgx_urts_sim -lsgx_uae_service_sim -lpthread -lcrypto"

 need to set these variables in the shell and then run ./configure again

 if we are using hardware-mode sgx, need to change the variables to:
 export LIBS="-lapp -lsgx_urts -lsgx_uae_service -lpthread -lcrypto"
 */


# define TOKEN_FILENAME   "enclave.token"
# define ENCLAVE1_FILENAME "enclave1.signed.so"

void run_server();
void handle_connection(int socket_fd);
int initialize_enclave();
int call_process_packet_sgx(unsigned char *data, unsigned int length);
void call_process_packet_sgx_sha256(unsigned char *data, unsigned int length);
int call_process_packet_no_sgx_test(unsigned char *data, unsigned int length);

//Disclaimer: SHA256 code taken from http://www.zedwood.com/article/cpp-sha256-function
//License available in License.txt

class SHA256
{
protected:
    typedef unsigned char uint8;
    typedef unsigned int uint32;
    typedef unsigned long long uint64;

    const static uint32 sha256_k[];
    static const unsigned int SHA224_256_BLOCK_SIZE = (512/8);
public:
    void init();
    void update(const unsigned char *message, unsigned int len);
    void final(unsigned char *digest);
    static const unsigned int DIGEST_SIZE = ( 256 / 8);

protected:
    void transform(const unsigned char *message, unsigned int block_nb);
    unsigned int m_tot_len;
    unsigned int m_len;
    unsigned char m_block[2*SHA224_256_BLOCK_SIZE];
    uint32 m_h[8];
};

std::string sha256(std::string input);

#define SHA2_SHFR(x, n)    (x >> n)
#define SHA2_ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define SHA2_ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define SHA2_CH(x, y, z)  ((x & y) ^ (~x & z))
#define SHA2_MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SHA256_F1(x) (SHA2_ROTR(x,  2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
#define SHA256_F2(x) (SHA2_ROTR(x,  6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
#define SHA256_F3(x) (SHA2_ROTR(x,  7) ^ SHA2_ROTR(x, 18) ^ SHA2_SHFR(x,  3))
#define SHA256_F4(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ SHA2_SHFR(x, 10))
#define SHA2_UNPACK32(x, str)                 \
{                                             \
    *((str) + 3) = (uint8) ((x)      );       \
    *((str) + 2) = (uint8) ((x) >>  8);       \
    *((str) + 1) = (uint8) ((x) >> 16);       \
    *((str) + 0) = (uint8) ((x) >> 24);       \
}
#define SHA2_PACK32(str, x)                   \
{                                             \
    *(x) =   ((uint32) *((str) + 3)      )    \
           | ((uint32) *((str) + 2) <<  8)    \
           | ((uint32) *((str) + 1) << 16)    \
           | ((uint32) *((str) + 0) << 24);   \
}


#endif /* ENCLAVE_ENCLAVE1_APP_APP_H_ */
