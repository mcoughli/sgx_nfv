#include "App.h"

sgx_enclave_id_t global_eid = 0;
volatile unsigned char* data_buffer_in[50];
volatile int buffer_in_write;
volatile int buffer_in_read;
volatile unsigned int data_buffer_out[50];
volatile int buffer_out_write;
volatile int buffer_out_read;
volatile unsigned int packet_length;
volatile unsigned int valid = 0;
volatile unsigned int iterations = 0;
volatile int count = -1;
volatile packet_queue head_in;
volatile packet_queue head_out;
volatile packet_queue *head_in_tail;
volatile packet_queue *head_out_tail;
unsigned int iterations_sum = 0;
bool initial_call = false;
pthread_t sgx_thread, output_thread;

using namespace std;

void ocall_print(const char* str) {
    printf("%s", str);
}

void ocall_print_hexbyte(unsigned char byteval){
    printf("%02x", byteval);
}

void ocall_print_int(int val){
    printf("%i", val);
}

void print_error_message(sgx_status_t ret)
{
    std::cout << "Encountered SGX error: " << ret << std::endl;
}

void ocall_print_int_message(char *message, unsigned int val){
    printf(message, val);
}

void test_process_packet(unsigned char *data, unsigned int length){
  int index=0, count = 0, sequence_len = ceil(length/500);
  unsigned char *search_seq = data;
  unsigned char *curr = data;
  while(index<(length-sequence_len)){
    if(memcmp(curr, search_seq, sequence_len) == 0){
      curr += sequence_len;
      index += sequence_len;
      count ++;
    } else{
      curr++;
      index++;
    }
  }
}

int initialize_enclave() {
    char* token_path = TOKEN_FILENAME;
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;

    /* Step 1: try to retrieve the launch token saved by last transaction
     *         if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    FILE* fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE1_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* Step 3: save the launch token if it is updated */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

void call_process_packet_sgx_sha256(unsigned char *data, unsigned int length){
  sgx_status_t status;
  unsigned char hash[SGX_SHA256_HASH_SIZE];
  memset(hash, 0, SGX_SHA256_HASH_SIZE);
  status = ecall_process_packet_sha256(global_eid, data, length, hash);
  if(status != SGX_SUCCESS){
    printf("Error encountered in calling enclave function");
  }
}

static void *init_sgx_thread(void *args){
    sgx_status_t status = ecall_process_packet(global_eid,
                                               (void*)&head_in,
                                               (void*)&head_out);
    if(status != SGX_SUCCESS){
      printf("Error encountered in calling enclave function");
    }
}

int consume(packet_data *data){
  printf("In consume\n");
  if(head_out.divider != head_out.last){
    data = (packet_data*)head_out.divider->next;
    head_out.divider = head_out.divider->next;
    printf("Out of consume\n");
    return 0;
  } else{
    printf("Out of consume fail\n");
    return -1;
  }
}

void produce(packet_data *data){
  printf("In produce\n");
  head_in.last->next = data;
  head_in.last = head_in.last->next;
  printf("IN produce 2\n");
  while(head_in.first != head_in.divider){
    printf("In produce while\n");
    volatile packet_data *tmp = head_in.first;
    head_in.first = head_in.first->next;
    // free((void*)tmp);
  }
  printf("Out of produce\n");
}

static void *output_thread_call(void *args){
  output_thread_data *data_args = (output_thread_data*)args;
  packet_data *data;
  callback_function callback = data_args->callback;
  while(true){
    if(consume(data) < 0){
      printf("Calling consume in callback thread\n");
      continue;
    }
    printf("Calling callback\n");
    callback(data->result, data->packet_id);
    printf("callback done\n");
  }
}

int initialize_enclave_threads(callback_function callback){
  packet_data *in_divider = (packet_data*)malloc(sizeof(packet_data));
  memset(in_divider, 0, sizeof(packet_data));
  packet_data *out_divider = (packet_data*)malloc(sizeof(packet_data));
  memset(out_divider, 0, sizeof(packet_data));
  head_in.first = in_divider;
  head_in.divider = in_divider;
  head_in.last = in_divider;

  head_out.first = out_divider;
  head_out.divider = out_divider;
  head_out.last = out_divider;
  // ecall_process_packet((void*)&head_in, (void*)&head_out);
  int rc = pthread_create(&sgx_thread, NULL, init_sgx_thread, NULL);
  if(rc){
      printf("Error creating thread: %i\n", rc);
      return -1;
  }
  output_thread_data *data_args = (output_thread_data*)malloc(sizeof(output_thread_data));
  data_args->callback = callback;
  rc = pthread_create(&output_thread, NULL, output_thread_call, (void*)data_args);
  if(rc){
      printf("Error creating 2nd thread: %i\n", rc);
      return -1;
  }
  return 0;
}



int call_process_packet_sgx(unsigned char *data, unsigned int length, long int packet_id){
//     int i, rc;
//     static int iterations = 0;
//     if(initial_call == false){
//         rc = pthread_create(&sgx_thread, NULL, init_sgx_thread, NULL);
//         if(rc){
//             printf("Error creating thread: %i\n", rc);
//         } else{
//             initial_call = true;
//             buffer_in_write = 0;
//             buffer_in_read = 0;
//             buffer_out_write = 0;
//             buffer_out_read = 0;
//         }
//     }
//     while(buffer_in_write == (buffer_in_read - 1) % 50 ){
// //        printf("Stuck here: %i, %i\n", buffer_in_write, buffer_in_read);
//         __asm__ __volatile__("");
//     }
//     data_buffer_in[buffer_in_write] = data;
//     packet_length = length;
//     buffer_in_write = (buffer_in_write + 1) % 50;
//     while(buffer_out_read == buffer_out_write){
// //        printf("Stuck here in %i: %i, %i\n", iterations, buffer_out_read, buffer_out_write);
//         __asm__ __volatile__("");
//     }
//     int current_out = data_buffer_out[buffer_out_read];
//     buffer_out_read = (buffer_out_read + 1) % 50;
//     iterations++;
//     return current_out;
  packet_data *new_item = (packet_data*)malloc(sizeof(packet_data));
  new_item->packet_data = data;
  new_item->length = length;
  new_item->packet_id = packet_id;
  new_item->next = NULL;
  printf("In sgx call 1\n");
  produce(new_item);
  printf("Done with SGX call 1\n");
  // packet_queue *end = &head_in;
  // while(end->next != NULL){
  //   end = end->next;
  // }
  // end->next = new_item;
}

void handle_connection(int socket_fd){
    int recv_size = 1000;
    int recv_offset = 0;
    int bytes_received = 0;
    long int data_length = 0;
    int num_len_chars = 0;
    int header_len = 0;
    int total_data = 0;
    int total_received = 0;
    unsigned char *recv_buff = NULL;
    unsigned char *more_recv_buff = NULL;
    unsigned char *data;
    unsigned char response[100];
    int enclave_return = 0;

    recv_buff = (unsigned char*)malloc(recv_size);
    if(recv_buff == NULL){
        printf("Error allocating receive buffer\n");
        close(socket_fd);
        return;
    }
    memset(recv_buff + recv_offset, 0, 1000);
    bytes_received = recv(socket_fd, recv_buff, 1000, 0);
    if(strncmp("data_len:", (char *)recv_buff, 9) != 0){
        printf("Malformed header. Aborting");
        free(recv_buff);
        close(socket_fd);
        return;
    }
    total_received = bytes_received;
    data_length = strtol((char*)recv_buff+9, (char**)&data, 0);
    if(strncmp("|data:", (char*)data, 6) != 0){
        printf("Malformed header after data length\n");
        close(socket_fd);
        free(recv_buff);
        return;
    }
    num_len_chars = data - recv_buff - 9;
    data = data + 6;
    header_len = 9 + num_len_chars + 6;
    total_data = header_len + data_length;
    while(total_received < total_data){
        recv_size += 1000;
        recv_offset += 1000;
        more_recv_buff = (unsigned char*)realloc(recv_buff, recv_size);
        if(more_recv_buff == NULL){
            printf("Failed to extend buffer to %i", recv_size);
            close(socket_fd);
            free(recv_buff);
            return;
        } else{
            recv_buff = more_recv_buff;
            more_recv_buff = NULL;
        }
        bytes_received = recv(socket_fd, recv_buff + recv_offset, 1000, 0);
        if(bytes_received < 0){
            printf("Error receiving data. Aborting");
            close(socket_fd);
            free(recv_buff);
            return;
        }
        total_received += bytes_received;
    }
    // call_process_packet_sgx(data, data_length);
    memset(response, 0, 100);
    sprintf((char*)response, "OK:%i", enclave_return);
    if(send(socket_fd, response, strlen((char*)response) + 1, 0) < 0){
        printf("Error sending response\n");
    }
    close(socket_fd);
    free(recv_buff);

}

void run_server(){
    int socket_fd, status, new_connection_fd;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(10000);
    char clientIp[INET_ADDRSTRLEN];

    std::cout << "Running server" <<std::endl;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0){
        std::cout << "Error opening socket file descriptor" << std::endl;
        return;
    }

    //set socket reusable from previous program run
    int yes = 1;
    status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    //bind to socket
    status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    if(status < 0){
        std::cout << "Error binding on socket" << std::endl;
        return;
    } else{
        std::cout << "Running server on TCP port: "<<10000<<std::endl;
    }

    //listen on socket with a backlog of 10 connections
    status = listen(socket_fd, 100);
    if(status < 0){
        cout << "Error listening on bound socket" << endl;
        return;
    } else{
        cout << "Listening on TCP socket" << endl;
    }

    while(true){
        cout << "Accepting new connections" << endl;

        struct sockaddr connection_addr;
        socklen_t connection_addr_size = sizeof(sockaddr);

        //block on accept until new connection
        new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);
        if(new_connection_fd < 0){
            cout << "Error accepting connection" <<endl;
        }

        //calculate ip of client
        int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
        inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
        cout << "Accepted new connection from: " << string(clientIp) << " in socket: " << new_connection_fd << endl;

        //handle connection single-threaded for now
        handle_connection(new_connection_fd);
    }
}

//int main(int argc, char const *argv[]) {
//    int i, enclave_return;
//    if (initialize_enclave() < 0) {
//        std::cout << "Enclave initialization failed" << std::endl;
//    }
//
//    unsigned char data[] = {'t','e', 's', 't', '\n'};
//    unsigned int data_len = 5;
//    printf("Expected data:\n0x");
//    for(i=0; i<5; i++){
//        printf("%02x", data[i]);
//    }
//    printf("\n");
//    sgx_status_t status = ecall_process_packet(global_eid, &enclave_return, data, data_len);
//    if (status != SGX_SUCCESS) {
//        std::cout << "SGX error" << std::endl;
//    } else{
//        printf("Received %i from enclave\n", enclave_return);
//    }
//    run_server();
//    return 0;
//}
