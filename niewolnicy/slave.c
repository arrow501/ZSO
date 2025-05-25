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
static int master_fd = -1;
static int slave_fd = -1;
static char slave_fifo_path[256];
static volatile int should_exit = 0;
static volatile int cleanup_done = 0;

static void cleanup_and_exit() {
    // Zapobiegnij wielokrotnemu cleanup
    if (cleanup_done) {
        return;
    }
    cleanup_done = 1;
    
    #if ENABLE_PRINTING
    printf("Slave %d: Starting cleanup...\n", slave_id);
    #endif
    
    // Wyrejestruj się jeśli połączenie jest aktywne
    if (master_fd >= 0 && !should_exit) {
        message_t msg = {
            .type = MSG_UNREGISTER,
            .slave_id = slave_id,
            .payload = 0
        };
        
        // Ignore write errors - master might be gone
        write(master_fd, &msg, sizeof(msg));
        
        #if ENABLE_PRINTING
        printf("Slave %d: Sent unregister message\n", slave_id);
        #endif
    }
    
    // Cleanup
    if (slave_fd >= 0) {
        close(slave_fd);
        slave_fd = -1;
    }
    
    if (master_fd >= 0) {
        close(master_fd);
        master_fd = -1;
    }
    
    // Usuń FIFO
    if (strlen(slave_fifo_path) > 0) {
        if (unlink(slave_fifo_path) == 0) {
            #if ENABLE_PRINTING
            printf("Slave %d: Removed FIFO %s\n", slave_id, slave_fifo_path);
            #endif
        } else {
            #if ENABLE_PRINTING
            perror("unlink slave fifo");
            #endif
        }
    }
}

static void signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
    
    // Jeśli czekamy na read(), przerwij go
    if (slave_fd >= 0) {
        int tmp = slave_fd;
        slave_fd = -1;
        close(tmp);
    }
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

static int send_unregister() {
    if (master_fd < 0) {
        return 0;
    }
    
    message_t msg = {
        .type = MSG_UNREGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    
    ssize_t ret = write(master_fd, &msg, sizeof(msg));
    if (ret < 0 && errno != EPIPE && errno != EBADF) {
        perror("write unregister");
        return -1;
    }
    
    #if ENABLE_PRINTING
    printf("Slave %d: Sent unregister message\n", slave_id);
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
    
    ssize_t ret = write(master_fd, &response, sizeof(response));
    if (ret < 0) {
        if (errno == EPIPE || errno == EBADF) {
            #if ENABLE_PRINTING
            printf("Slave %d: Master disconnected\n", slave_id);
            #endif
            should_exit = 1;
        } else {
            perror("write response");
        }
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
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    
    // Rejestruj cleanup przy wyjściu
    if (atexit(cleanup_and_exit) != 0) {
        fprintf(stderr, "Failed to register cleanup function\n");
        return 1;
    }
    
    // Stwórz FIFO dla slave'a
    if (create_slave_fifo(slave_id) < 0) {
        return 1;
    }
    
    // Otwórz FIFO mastera do pisania
    master_fd = open(MASTER_FIFO, O_WRONLY);
    if (master_fd < 0) {
        perror("open master fifo");
        return 1;
    }
    
    // Zarejestruj się w masterze
    if (register_with_master() < 0) {
        return 1;
    }
    
    // Otwórz własne FIFO do czytania
    slave_fd = open(slave_fifo_path, O_RDONLY);
    if (slave_fd < 0) {
        perror("open slave fifo");
        return 1;
    }
    
    #if ENABLE_PRINTING
    printf("Slave %d: Started (pid=%d)\n", slave_id, getpid());
    #endif
    
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
            #if ENABLE_PRINTING
            printf("Slave %d: Master closed connection\n", slave_id);
            #endif
            break;
        } else if (bytes < 0) {
            if (errno == EINTR || errno == EBADF) {
                // Przerwane przez sygnał lub fd zamknięty
                break;
            }
            perror("read slave fifo");
            break;
        }
    }
    
    // Wyślij unregister jeśli kończymy normalnie
    if (!should_exit) {
        send_unregister();
    }
    
    #if ENABLE_PRINTING
    printf("Slave %d: Exiting\n", slave_id);
    #endif
    
    return 0;
}