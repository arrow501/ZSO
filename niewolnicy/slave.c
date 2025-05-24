#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include "parameters.h"
#include "message.h"

static int slave_id;
static int master_fd;
static int slave_fd;
static char slave_fifo_path[256];
static volatile int should_exit = 0;

static void signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
}

static int create_slave_fifo(int id) {
    snprintf(slave_fifo_path, sizeof(slave_fifo_path), 
             "%s%d", SLAVE_FIFO_PREFIX, id);
    
    // Usuń stare FIFO jeśli istnieje
    unlink(slave_fifo_path);
    
    if (mkfifo(slave_fifo_path, 0666) < 0) {
        perror("mkfifo slave");
        return -1;
    }
    
    return 0;
}

static int register_with_master() {
    message_t msg = {
        .type = MSG_REGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    
    if (write(master_fd, &msg, sizeof(msg)) != sizeof(msg)) {
        perror("write register");
        return -1;
    }
    
    #if ENABLE_PRINTING
    printf("Slave %d: Registered with master\n", slave_id);
    #endif
    
    return 0;
}

static int unregister_from_master() {
    message_t msg = {
        .type = MSG_UNREGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    
    if (write(master_fd, &msg, sizeof(msg)) != sizeof(msg)) {
        perror("write unregister");
        return -1;
    }
    
    #if ENABLE_PRINTING
    printf("Slave %d: Unregistered from master\n", slave_id);
    #endif
    
    return 0;
}

static void process_query(message_t *query) {
    #if ENABLE_PRINTING
    printf("Slave %d: Received query with payload %d\n", 
           slave_id, query->payload);
    #endif
    
    // Prosta odpowiedź - zwróć payload * 2
    message_t response = {
        .type = MSG_RESPONSE,
        .slave_id = slave_id,
        .payload = query->payload * 2
    };
    
    if (write(master_fd, &response, sizeof(response)) != sizeof(response)) {
        perror("write response");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slave_id>\n", argv[0]);
        return 1;
    }
    
    slave_id = atoi(argv[1]);
    if (slave_id < 0 || slave_id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave_id: %d\n", slave_id);
        return 1;
    }
    
    // Ustawienie obsługi sygnałów
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // Stwórz FIFO dla slave'a
    if (create_slave_fifo(slave_id) < 0) {
        return 1;
    }
    
    // Otwórz FIFO mastera do pisania
    master_fd = open(MASTER_FIFO, O_WRONLY);
    if (master_fd < 0) {
        perror("open master fifo");
        unlink(slave_fifo_path);
        return 1;
    }
    
    // Zarejestruj się w masterze
    if (register_with_master() < 0) {
        close(master_fd);
        unlink(slave_fifo_path);
        return 1;
    }
    
    // Otwórz własne FIFO do czytania
    slave_fd = open(slave_fifo_path, O_RDONLY);
    if (slave_fd < 0) {
        perror("open slave fifo");
        close(master_fd);
        unlink(slave_fifo_path);
        return 1;
    }
    
    // Główna pętla
    while (!should_exit) {
        message_t msg;
        ssize_t bytes = read(slave_fd, &msg, sizeof(msg));
        
        if (bytes == sizeof(msg)) {
            if (msg.type == MSG_QUERY) {
                process_query(&msg);
            }
        } else if (bytes == 0) {
            // EOF - master zamknął połączenie
            break;
        } else if (bytes < 0 && errno != EINTR) {
            perror("read slave fifo");
            break;
        }
    }
    
    // Wyrejestruj się
    unregister_from_master();
    
    // Cleanup
    close(slave_fd);
    close(master_fd);
    unlink(slave_fifo_path);
    
    #if ENABLE_PRINTING
    printf("Slave %d: Exiting\n", slave_id);
    #endif
    
    return 0;
}
