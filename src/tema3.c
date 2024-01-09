#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

typedef struct {
    char filename[MAX_FILENAME];
    int chunks_count;
    char chunks[MAX_CHUNKS][HASH_SIZE];
} file;

typedef struct {
    int files_count;
    file files[MAX_FILES];
} files_list;

readInput(char *filename);

/*
    Steps:
    1. Get the list of seeds/peers for files from tracker
    2. Check missing chunks and search for them in the list of seeds
    3. Foreach missing chunk, follow the steps:
        3.1. Pick a seed/peer
        3.2. Request the chunk from the seed/peer
        3.3. Wait for the chunk
        3.4. Mark as received

*/
void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;

    return NULL;
}

void tracker(int numtasks, int rank) {



}

/*
    It has the segments of the file. It handles the download and upload of the file.
    It's main priority: Shares the owned segments with other peers and gets the missing ones.
*/
void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}

/*
    Reads the input. Structure of the file:
    - first line: No. of files
    - next line: filname hash chunks_count
    - next lines: chunk_1, chunk_2, ..., chunk_n
    - next line: filname hash chunks_count
    - next lines: chunk_1, chunk_2, ..., chunk_n
    ...
*/
files_list *readInput(char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        printf("Eroare la deschiderea fisierului %s\n", filename);
        exit(-1);
    }

    files_list *files = (files_list *) malloc(sizeof(files_list));
    if (files == NULL) {
        printf("Eroare la alocarea memoriei pentru files\n");
        exit(-1);
    }

    fscanf(f, "%d", &files->files_count);
    for (int i = 0; i < files->files_count; i++) {
        fscanf(f, "%s", files->files[i].filename);
        fscanf(f, "%d", &files->files[i].chunks_count);
        for (int j = 0; j < files->files[i].chunks_count; j++) {
            fscanf(f, "%s", files->files[i].chunks[j]);
        }
    }

    fclose(f);
    return files;
}
