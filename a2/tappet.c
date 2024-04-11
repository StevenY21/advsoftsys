#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

#define MAX_STRING_SIZE 2056
#define MAX_THREADS 100
#define BUFFER_SIZE   2

typedef struct {
    char **buffer;   // Shared buffer
    int in;        // Index for inserting into the buffer
    int out;       // Index for removing from the buffer
    int size;      // Size of the buffer
    pthread_mutex_t mutex;		/* Mutex for shared buffer.                        */
} shared_data_t;
typedef struct {
  char *data;			/* Slot data.                            */
  size_t size;			/* Size of data.                         */
  pthread_t id;			/* ID of destination thread.             */

} slot_t;
typedef struct {
    char *shm_id_str_read;
    char *shm_id_str_write;
    char *input_file;
    char *buffer_type;
    char *argn;
} fn_args; // all of the arguments needed for observe, reconstruct, and tapplot
int main(int argc, char *argv[]) {
    char *task_names[MAX_THREADS];
    int num_tasks = 0;
    char *buffer_type = NULL;
    int buffer_size = 0;
    char *argn = "1\0";
    char *input_file = NULL;
    char *last_program = NULL;
    int c;
    while ((c = getopt (argc, argv, "b:s:p:")) != -1) {// checking the options and sending errors if invalid options found
        switch (c)
        {
        case 'p':
            if(optind < argc) {
                task_names[num_tasks] = optarg;
                printf("%s\n", task_names[num_tasks]);
                num_tasks++;
                last_program = optarg;
                // if there's an input filename given after observe, get it
                if (strcmp(optarg, "observe") == 0 && optind < argc && argv[optind][0] != '-') {
                    input_file = argv[optind];
                    optind++;
                }
                break;
            }
        case 'b':
            buffer_type = optarg;
        case 's':
            buffer_size = atoi(optarg);
            break;
        }
    }
    if (last_program != NULL) { //for getting argn value
        for (int i = optind; i < argc; i++) {
            if (strcmp(argv[i], last_program) == 0 && i + 1 < argc) {
                argn = argv[i + 1];
                break;
            }
        }
    }
    printf("%d\n", num_tasks);
    printf("%s\n", input_file);
    printf("%s\n", buffer_type);
    int shm_ids[num_tasks];
    shared_data_t *shared_data[num_tasks];
    pthread_t task_threads[num_tasks];
    for (int i = 0; i < num_tasks; i++) {
        size_t shm_size = sizeof(shared_data_t) + buffer_size * MAX_STRING_SIZE;
        shm_ids[i] = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
        if (shm_ids[i] < 0) {
            perror("shmget");
            exit(1);
        }
        if (buffer_type == "sync") {
            pthread_mutex_init (&shared_data[i]->mutex, NULL);
        }
        shared_data[i] = (shared_data_t *)shmat(shm_ids[i], NULL, 0);
        if ((void *)shared_data[i] == (void *)-1) {
            perror("shmat");
            exit(1);
        }
        printf("Shared memory segment %d created with ID %d\n", i, shm_ids[i]);
        fprintf(stderr, "%s", "setting up shared data\n");
        shared_data[i]->in = 0;
        shared_data[i]->out = 0;
        shared_data[i]->size = buffer_size;
        printf("%d\n", shared_data[i]->size);
        fprintf(stderr, "%s", "allocating memory\n");
        // Allocate memory for the buffer within the shared memory segment
        shared_data[i]->buffer = (char **)((char *)shared_data[i] + sizeof(shared_data_t));
        // Initialize each element of the buffer
        fprintf(stderr, "%s", "initializing elements\n");
        for (int j = 0; j < shared_data[i]->size; j++) {
            shared_data[i]->buffer[j] = (char *)(shared_data[i]->buffer + shared_data[i]->size) + j * MAX_STRING_SIZE;
        }
        fprintf(stderr, "%s", "getting thread args\n");
        fn_args *arguments = (fn_args *)malloc(sizeof(fn_args));
        char shm_id_str_read[10];
        char shm_id_str_write[10];
        arguments->buffer_type = buffer_type;
        arguments->argn = argn;
        void (*fn)(void*);
        //problem here
        void *libtap =dlopen("libtap.so", RTLD_LAZY); 
        if (libtap == NULL) {
            fprintf(stderr, "%s\n", "cannot open library");
            exit(1);
        }
        *(void**)(&fn) = dlsym(libtap, task_names[i]);
        if (fn == NULL) {
            fprintf(stderr, "%s\n", "cannot get function");
            exit(1);
        }
        arguments->shm_id_str_read = NULL;
        arguments->shm_id_str_write = NULL;
        arguments->input_file = input_file;
        if (i == 0) { //first task, reads from stdin/file and writes to buffer
            sprintf(shm_id_str_write, "%d", shm_ids[i]);
            arguments->shm_id_str_write = shm_id_str_write;
            fprintf(stderr, "Executing first task: %s\n", task_names[i]);
        } else if (i == -1) { // last task, only reads from buffer and writes to stdout
            sprintf(shm_id_str_read, "%d", shm_ids[i - 1]);
            arguments->shm_id_str_read = shm_id_str_read;
            fprintf(stderr, "Executing last task: %s\n", last_program);
        } else { //read from previous buffer, write to next
            sprintf(shm_id_str_read, "%d", shm_ids[i - 1]);
            sprintf(shm_id_str_write, "%d", shm_ids[i]);
            fprintf(stderr, "Executing: %s\n", task_names[i]);
            arguments->shm_id_str_write = shm_id_str_write;
            arguments->shm_id_str_read = shm_id_str_read;
        }
        //printf("creating thread %s\n", task_names[i]);
        if (pthread_create(&task_threads[i], NULL, (void *(*)(void *))fn, (void *)arguments) != 0) {
            perror("thread creation failed");
            exit(1);
        }
    }
    fprintf(stderr, "%s", "joining threads\n");
    for(int i = 0; i < num_tasks; i++) {
        pthread_join(task_threads[i], NULL);
    }
    fprintf(stderr, "%s", "detaching and segments\n");
    for (int i = 0; i < num_tasks; i++) {
        fprintf(stderr, "%s", "detaching shared mem\n");
        if (shmdt(shared_data[i]) == -1) {
            perror("shmdt");
            exit(1);
        }
        fprintf(stderr, "%s", "shared mem control\n");
        if (shmctl(shm_ids[i], IPC_RMID, NULL) == -1) {
            perror("shmctl");
            exit(1);
        }
    }
    return 0;

}
