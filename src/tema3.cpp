#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

typedef struct {
    std::string filename;
    int chunks_count;
    std::vector<std::string> chunks = std::vector<std::string>();
} file;

typedef struct {
    int files_count;
    std::vector<file*> files = std::vector<file*>();
} files_list;

files_list *readInput(char *filename);
void printFiles(files_list *files);

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

/*
    Stepts:
    1. Checks the recieved message
    2. Multiple types of messages:
        2.1. Request for a chunk: Sends a list of seeds/peers that have the chunk and what other chunks they have.
        2.2. Update from client: Mark the client in the swarm of files it has and updates the list of chunks owned by the client.
        Also sends the list from 2.1 to the client.
        2.3. Finish download of a file: Marks the client as seed.
        2.4. Finish download of all files: Marks the client as finished, but it keeps it in the swarm.
        2.5. All clients finished downloading all the files: Sends a message to all clients to close and closes itself.
*/
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

    files_list *files = readInput("tests/test1/in1.txt");
    printFiles(files);
    free(files);

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
    fscanf(f, "%d", &files->files_count);
    files->files.resize(files->files_count);

    std::cout << "UNU\n";

    for (int i = 0; i < files->files_count; ++i) {
        file *cur = (file*)malloc(sizeof(file));
        char* name = (char*)malloc(MAX_FILENAME * sizeof(char));
        fscanf(f, "%s %d", name, &cur->chunks_count);
        std::cout << "CVn\n";
        cur->chunks.resize(cur->chunks_count);
        std::cout << "NAME\n";
        cur->filename = name;
        std::cout << "DOI\n";
        for (int j = 0; j < cur->chunks_count; ++j) {
            std::cout << "TREI\n";
            char *chunk = (char*)malloc(HASH_SIZE * sizeof(char));
            fscanf(f, "%s", chunk);
            cur->chunks[j] = chunk;
        }
        files->files[i] = cur;
    }
    std::cout << "FIN\n";
    fclose(f);
    return files;
}

/*
    Prints the files list
*/
void printFiles(files_list *files) {
    printf("Files count: %d\n", files->files_count);
    for (int i = 0; i < files->files_count; ++i) {
        file *cur = files->files[i];
        printf("File %d: %s %d\n", i, cur->filename.c_str(), cur->chunks_count);
        for (int j = 0; j < cur->chunks_count; ++j) {
            printf("%s ", cur->chunks[j].c_str());
        }
        printf("\n");
    }
}
