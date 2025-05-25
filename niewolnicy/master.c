#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <poll.h>
#include "parameters.h"
#include "message.h"
#include "stats.h"

typedef struct {
    int pid;
    int fd;
    int active;
} slave_info_t;

static slave_info_t slaves[MAX_SLAVES];
static int master_fd = -1;
static stats_t *stats = NULL;
static sem_t *stats_sem = SEM_FAILED;
static volatile int should_exit = 0;
static volatile int show_stats = 0;

static void cleanup_resources() {
    #if ENABLE_PRINTING
    printf("Master: Cleaning up resources...\n");
    #endif
    
    // Zamknij połączenia ze slave'ami
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (slaves[i].fd >= 0) {
            close(slaves[i].fd);
            slaves[i].fd = -1;
        }
    }
    
    // Zamknij master FIFO
    if (master_fd >= 0) {
        close(master_fd);
        master_fd = -1;
    }
    
    // Usuń FIFO mastera
    unlink(MASTER_FIFO);
    
    // Cleanup shared memory
    if (stats != NULL) {
        pthread_mutex_destroy(&stats->mutex);
        munmap(stats, sizeof(stats_t));
        stats = NULL;
    }
    shm_unlink(SHM_NAME);
    
    // Cleanup semaphore
    if (stats_sem != SEM_FAILED) {
        sem_close(stats_sem);
        stats_sem = SEM_FAILED;
    }
    sem_unlink(SEM_NAME);
}

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
        // Przerwij poll() przez zamknięcie fd
        if (master_fd >= 0) {
            close(master_fd);
            master_fd = -1;
        }
    } else if (sig == SIGUSR1) {
        show_stats = 1;
    }
}

static int init_shared_memory() {
    // Cleanup old resources first
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
    
    // Utworzenie pamięci dzielonej
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }
    
    // Ustawienie rozmiaru
    if (ftruncate(shm_fd, sizeof(stats_t)) < 0) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    // Mapowanie pamięci
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, 
                 MAP_SHARED, shm_fd, 0);
    if (stats == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    close(shm_fd);
    
    // Inicjalizacja mutexu
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&stats->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    // Wyzerowanie statystyk
    memset(stats->messages_sent, 0, sizeof(stats->messages_sent));
    memset(stats->messages_received, 0, sizeof(stats->messages_received));
    stats->total_sent = 0;
    stats->total_received = 0;
    
    // Utworzenie semafora
    stats_sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (stats_sem == SEM_FAILED) {
        perror("sem_open");
        munmap(stats, sizeof(stats_t));
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    return 0;
}

static void handle_register(message_t *msg) {
    int id = msg->slave_id;
    
    if (id < 0 || id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave_id in register: %d\n", id);
        return;
    }
    
    // Jeśli już jest zarejestrowany, zamknij stare połączenie
    if (slaves[id].fd >= 0) {
        close(slaves[id].fd);
    }
    
    // Otwórz FIFO slave'a
    char slave_fifo[256];
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, id);
    
    int fd = open(slave_fifo, O_WRONLY);
    if (fd < 0) {
        perror("open slave fifo");
        return;
    }
    
    slaves[id].fd = fd;
    slaves[id].active = 1;
    
    #if ENABLE_PRINTING
    printf("Master: Registered slave %d (fd=%d)\n", id, fd);
    #endif
}

static void handle_unregister(message_t *msg) {
    int id = msg->slave_id;
    
    if (id < 0 || id >= MAX_SLAVES) {
        return;
    }
    
    if (slaves[id].fd >= 0) {
        close(slaves[id].fd);
        slaves[id].fd = -1;
    }
    slaves[id].active = 0;
    
    #if ENABLE_PRINTING
    printf("Master: Unregistered slave %d\n", id);
    #endif
}

static void handle_response(message_t *msg) {
    int id = msg->slave_id;
    
    if (id < 0 || id >= MAX_SLAVES) {
        return;
    }
    
    // Aktualizacja statystyk
    pthread_mutex_lock(&stats->mutex);
    stats->messages_received[id]++;
    stats->total_received++;
    pthread_mutex_unlock(&stats->mutex);
    
    #if ENABLE_PRINTING
    printf("Master: Received response from slave %d: %d\n", 
           id, msg->payload);
    #endif
}

static void send_query(int slave_id, int payload) {
    if (slave_id < 0 || slave_id >= MAX_SLAVES || !slaves[slave_id].active || slaves[slave_id].fd < 0) {
        return;
    }
    
    message_t msg = {
        .type = MSG_QUERY,
        .slave_id = slave_id,
        .payload = payload
    };
    
    ssize_t written = write(slaves[slave_id].fd, &msg, sizeof(msg));
    if (written == sizeof(msg)) {
        pthread_mutex_lock(&stats->mutex);
        stats->messages_sent[slave_id]++;
        stats->total_sent++;
        pthread_mutex_unlock(&stats->mutex);
        
        #if ENABLE_PRINTING
        printf("Master: Sent query to slave %d: %d\n", slave_id, payload);
        #endif
    } else if (written < 0) {
        // Slave może być niedostępny
        if (errno == EPIPE || errno == EBADF) {
            slaves[slave_id].active = 0;
            if (slaves[slave_id].fd >= 0) {
                close(slaves[slave_id].fd);
                slaves[slave_id].fd = -1;
            }
            #if ENABLE_PRINTING
            printf("Master: Slave %d disconnected (EPIPE/EBADF)\n", slave_id);
            #endif
        } else {
            #if ENABLE_PRINTING
            perror("write to slave");
            #endif
        }
    }
}

static void display_stats() {
    pthread_mutex_lock(&stats->mutex);
    
    printf("\n=== Master Statistics ===\n");
    printf("Total messages sent: %d\n", stats->total_sent);
    printf("Total messages received: %d\n", stats->total_received);
    
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (stats->messages_sent[i] > 0 || stats->messages_received[i] > 0) {
            printf("Slave %d: sent=%d, received=%d\n", 
                   i, stats->messages_sent[i], stats->messages_received[i]);
        }
    }
    printf("========================\n\n");
    
    pthread_mutex_unlock(&stats->mutex);
    
    // Sygnalizacja przez semafor
    sem_post(stats_sem);
}

int main() {
    // Inicjalizacja
    memset(slaves, 0, sizeof(slaves));
    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].fd = -1;
    }
    
    // Obsługa sygnałów
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    
    // Rejestruj cleanup przy wyjściu
    if (atexit(cleanup_resources) != 0) {
        fprintf(stderr, "Failed to register cleanup function\n");
        return 1;
    }
    
    // Inicjalizacja pamięci dzielonej
    if (init_shared_memory() < 0) {
        return 1;
    }
    
    // Utworzenie FIFO mastera
    unlink(MASTER_FIFO);
    if (mkfifo(MASTER_FIFO, 0666) < 0) {
        perror("mkfifo master");
        cleanup_resources();
        return 1;
    }
    
    // Otwórz FIFO do czytania
    master_fd = open(MASTER_FIFO, O_RDONLY | O_NONBLOCK);
    if (master_fd < 0) {
        perror("open master fifo");
        cleanup_resources();
        return 1;
    }
    
    #if ENABLE_PRINTING
    printf("Master: Started (pid=%d)\n", getpid());
    #endif
    
    // Główna pętla
    int query_counter = 0;
    struct pollfd pfd = {
        .fd = master_fd,
        .events = POLLIN
    };
    
    // Timing dla zapytań
    struct timespec last_query_time;
    clock_gettime(CLOCK_MONOTONIC, &last_query_time);
    
    while (!should_exit) {
        // Sprawdź czy trzeba wyświetlić statystyki
        if (show_stats) {
            display_stats();
            show_stats = 0;
        }
        
        // Poll z timeoutem 100ms
        int ret = poll(&pfd, 1, 100);
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue; // Sygnał, kontynuuj
            }
            perror("poll");
            break;
        }
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            // Czytaj wszystkie dostępne komunikaty
            message_t msg;
            ssize_t bytes;
            
            while ((bytes = read(master_fd, &msg, sizeof(msg))) == sizeof(msg)) {
                switch (msg.type) {
                    case MSG_REGISTER:
                        handle_register(&msg);
                        break;
                    case MSG_UNREGISTER:
                        handle_unregister(&msg);
                        break;
                    case MSG_RESPONSE:
                        handle_response(&msg);
                        break;
                    default:
                        fprintf(stderr, "Unknown message type: %d\n", msg.type);
                }
            }
            
            if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                if (errno != EBADF) { // Nie raportuj EBADF (zamknięty przez sygnał)
                    perror("read master fifo");
                }
            }
        }
        
        // Sprawdź czy upłynęło 500ms od ostatniego zapytania
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        long elapsed_ms = (current_time.tv_sec - last_query_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - last_query_time.tv_nsec) / 1000000;
        
        if (elapsed_ms >= 500) {
            last_query_time = current_time;
            query_counter++;
            
            // Wyślij zapytania do wszystkich aktywnych slave'ów
            int active_count = 0;
            for (int i = 0; i < MAX_SLAVES; i++) {
                if (slaves[i].active && slaves[i].fd >= 0) {
                    send_query(i, query_counter);
                    active_count++;
                }
            }
            
            #if ENABLE_PRINTING
            if (active_count > 0) {
                printf("Master: Sent queries to %d slaves (round %d)\n", 
                       active_count, query_counter);
            }
            #endif
        }
        
        // Sprawdź czy master_fd jest jeszcze ważny
        if (master_fd < 0) {
            break;
        }
    }
    
    #if ENABLE_PRINTING
    printf("Master: Shutting down...\n");
    #endif
    
    // Wyświetl końcowe statystyki
    if (stats != NULL) {
        display_stats();
    }
    
    #if ENABLE_PRINTING
    printf("Master: Exiting\n");
    #endif
    
    return 0;
}