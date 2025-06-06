# Master-Slave IPC System

Minimalny system komunikacji inter-proces z architekturą master-slave używający named pipes, shared memory i sygnałów.

## Architektura

- **Master Process**: Zarządza rejestracją slave'ów, wysyła zapytania, przechowuje statystyki
- **Slave Processes**: Rejestrują się z masterem, przetwarzają zapytania, wysyłają odpowiedzi  
- **Main Launcher**: Unified process który uruchamia i monitoruje cały system + wbudowane stats viewing

## Kompilacja

```bash
make            # Build debug version (domyślna)
make release    # Build release version (bez printów/asercji)
make clean      # Wyczyść pliki
```

### Parametryzacja kompilacji

```bash
make NUM_SLAVES=5 NUM_MESSAGES_PER_SLAVE=20    # Niestandardowe wartości
make small      # Mała konfiguracja (2 slaves, 5 messages)
make large      # Duża konfiguracja (10 slaves, 20 messages)
```

## Użycie

### Main Launcher
```bash
./main 3        # Uruchom z 3 slave'ami (parametr wymagany)
./main 10       # Uruchom z 10 slave'ami (maksimum)
./main          # Pokaże help i wyjdzie
```

Po uruchomieniu:
- **SIGUSR1** do main → wyświetl statystyki
- **Ctrl+C** → graceful shutdown całego systemu

### Ręczne uruchamianie (do debugowania)
```bash
# Terminal 1
./master

# Terminal 2-4  
./slave 0
./slave 1
./slave 2
```

## Testowanie

```bash
make test           # Comprehensive test suite
./valgrind-suite.sh # Memory safety testing
```

## Kluczowe zmiany w architekturze

### ✅ Zrealizowane wymagania:
- **Brak time dependency**: Zero `#include <time.h>`, zero `sleep()` w C
- **UUID w nazwach IPC**: Wszystkie FIFO/shm/sem mają suffix `2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f`
- **Unified main**: Jeden proces uruchamia wszystko i obsługuje stats viewing
- **Mandatory parameters**: `./main` bez parametrów pokazuje help
- **Asercje zamiast defensive**: `DEBUG_ASSERT()` wszędzie
- **Unified testing**: Jeden skrypt testuje wszystkie scenariusze
- **Absorbed stats reader**: Stats viewing wbudowane w main (simplified)

### 🏗️ Struktura plików:
```
├── src/
│   ├── main.c          # Unified launcher + stats viewing
│   ├── master.c        # Master process  
│   └── slave.c         # Slave process
├── include/
│   ├── parameters.h    # Konfiguracja + UUID
│   └── common.h        # Struktury + asercje
└── test.sh            # Comprehensive test suite
```

## Protokół komunikacji

1. **Main** forkuje master i N slave'ów
2. **Main** czeka na inicjalizację shared memory przez blocking semaphore
3. **Slaves** rejestrują się z masterem przez named pipes
4. **Master** wysyła okresowe zapytania do slave'ów  
5. **Slaves** przetwarzają (podwajają wartość) i odsyłają odpowiedzi
6. **SIGUSR1** → main wysyła sygnał do mastera → statystyki

## Mechanizmy IPC

### **Named Pipes (FIFO)**:
- `/tmp/master_fifo_UUID` - komunikacja slave → master
- `/tmp/slave_fifo_UUID_N` - komunikacja master → slave N

### **Shared Memory**:
- `/dev/shm/master_stats_UUID` - statystyki dostępne cross-process

### **Semafory**:
- `/dev/shm/sem.stats_init_UUID` - synchronizacja inicjalizacji  
- `/dev/shm/sem.stats_ready_UUID` - sygnalizacja nowych statystyk

### **Sygnały**:
- `SIGUSR1` - wyzwala dump statystyk
- `SIGTERM/SIGINT` - graceful shutdown

## Testowanie

Test suite sprawdza:
- ✅ Single slave operation
- ✅ Multiple slaves (3)
- ✅ Maximum slaves (10) 
- ✅ Signal handling
- ✅ Rapid fire signals (kolejkowanie)
- ✅ Graceful shutdown
- ✅ Invalid parameters

## Przykład działania

```bash
$ make && ./main 3
Main: Starting IPC system with 3 slaves
Main: Master started (PID=1234)
Stats monitoring setup for master PID 1234
Main: All processes started
Main: Send SIGUSR1 to this process (PID=1200) to display stats

# W innym terminalu:
$ kill -USR1 1200

Stats request sent to master (PID 1234)
=== Master Statistics ===
Master PID: 1234
Slave Status:
  Slave 0: ACTIVE, sent=5, received=5
  Slave 1: ACTIVE, sent=5, received=5  
  Slave 2: ACTIVE, sent=5, received=5
Totals: 3 active slaves, 15 sent, 15 received
```# Master-Slave IPC System

Prosty system komunikacji inter-proces z architekturą master-slave używający named pipes, shared memory i sygnałów.

## Architektura

- **Master Process**: Zarządza rejestracją slave'ów, wysyła zapytania, przechowuje statystyki
- **Slave Processes**: Rejestrują się z masterem, przetwarzają zapytania, wysyłają odpowiedzi  
- **Stats Reader**: Zewnętrzny proces odczytujący statystyki z shared memory

## Kompilacja

```bash
make            # Build debug version (domyślna)
make release    # Build release version (bez printów/asercji)
make clean      # Wyczyść pliki
```

### Parametryzacja kompilacji

```bash
make NUM_SLAVES=5 NUM_MESSAGES_PER_SLAVE=20    # Niestandardowe wartości
make small      # Mała konfiguracja (2 slaves, 5 messages)
make large      # Duża konfiguracja (10 slaves, 20 messages)
```

## Użycie

### Opcja 1: Launcher (Rekomendowane)
```bash
./main          # Uruchom z domyślną liczbą slave'ów
./main 5        # Uruchom z 5 slave'ami
```

### Opcja 2: Ręczne uruchamianie
```bash
# Terminal 1
./master

# Terminal 2-4
./slave 0
./slave 1  
./slave 2

# Terminal 5
./stats_reader
```

### Stats Reader
W stats_reader:
- **ENTER** - Wyślij SIGUSR1 do mastera i wyświetl statystyki
- **q + ENTER** - Wyjście

## Testowanie

```bash
make test           # Test podstawowy
make test-small     # Test z małą konfiguracją  
make test-large     # Test z dużą konfiguracją
```

## Szczegóły implementacji

### Komunikacja
- **Named pipes (FIFO)**: Komunikacja master ↔ slave
- **Shared memory**: Statystyki dostępne dla external readers
- **Semafory**: Sygnalizacja gotowości statystyk
- **Sygnały**: SIGUSR1 wyzwala dump statystyk

### Pliki IPC
- `/tmp/master_fifo` - FIFO mastera
- `/tmp/slave_fifo_N` - FIFO slave'a N  
- `/tmp/master_pid` - PID mastera dla stats_reader
- `/dev/shm/master_stats` - Shared memory ze statystykami
- `/dev/shm/sem.stats_ready` - Semafor gotowości statystyk

### Protokół komunikacji
1. Slave tworzy swój FIFO i rejestruje się z masterem
2. Master otwiera FIFO slave'a do wysyłania zapytań
3. Master wysyła okresowe zapytania z incrementującymi wartościami
4. Slave przetwarza zapytanie (podwaja wartość) i wysyła odpowiedź
5. Slave wyrejestrowuje się po przetworzeniu N wiadomości

### Obsługa sygnałów
- **SIGINT/SIGTERM**: Graceful shutdown
- **SIGUSR1**: Master dumuje statystyki do shared memory
- **SIGPIPE**: Ignorowany (obsługa broken pipes)

## Parametry konfiguracyjne

W `include/parameters.h`:

```c
NUM_SLAVES              // Maksymalna liczba slave'ów (domyślnie 3)
NUM_MESSAGES_PER_SLAVE  // Wiadomości na slave'a przed wyjściem (domyślnie 10)  
ENABLE_PRINTING         // Debug output (1/0)
ENABLE_ASSERTS          // Asercje debug (1/0)
POLL_TIMEOUT_MS         // Timeout poll() (domyślnie 100ms)
QUERY_INTERVAL_MS       // Okres zapytań (domyślnie 1000ms)
```

## Debug Mode

Debug mode (domyślny) włącza:
- Szczegółowe komunikaty printów
- Asercje sprawdzające założenia
- Dodatkowe sprawdzenia błędów

Release mode wyłącza wszystkie printy i asercje dla performance.

## Przykład działania

```bash
$ ./main 2
Main: Starting IPC system with 2 slaves
Main: Master started (PID=1234)
Main: Master is ready
Main: Slave 0 started (PID=1235)
Main: Slave 1 started (PID=1236)
Main: All processes started
Main: You can now run './stats_reader' in another terminal

# W innym terminalu:
$ ./stats_reader
Stats Reader: Master PID is 1234

Commands:
  ENTER - Request and display stats
  q + ENTER - Quit
> [ENTER]
Stats request sent to master (PID 1234)

=== Master Statistics ===
Time: Fri Jun  6 15:30:45 2025
Master PID: 1234

Slave Status:
  Slave 0: ACTIVE, sent=5, received=5
  Slave 1: ACTIVE, sent=5, received=5

Totals: 2 active slaves, 10 sent, 10 received
========================
```

## Rozwiązywanie problemów

### Częste błędy
- **"No such file or directory"**: Master nie jest uruchomiony
- **"Assertion failed"**: Sprawdź czy nie ma leftover plików IPC
- **Slaves nie startują**: Sprawdź czy master_pid file istnieje

### Czyszczenie zasobów
```bash
make clean  # Usuwa pliki IPC automatycznie
```

### Debug
```bash
make DEBUG=1    # Włącz wszystkie asercje i printy
```