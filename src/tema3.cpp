#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <fstream>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

typedef struct {
    std::string filename;
    int no_chunks;
    std::vector<std::string> chunks;
} file;

typedef struct {
    int no_files;
    std::vector<file> files;
} filelist;

filelist readInput(char *filename);
void printFiles(filelist files);

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
filelist readInput(char *filename) {
    std::ifstream in;
    in.open(filename);
    if(!in) {
        printf("Error reading input file\n");
        exit(-1);
    }
    std::string line;

    filelist files;

    std::getline(in, line);
    files.no_files = std::stoi(line);

    for(int i = 0; i < files.no_files; ++i) {
        file cur;

        std::getline(in, line);
        int pos = line.find(" ");
        cur.filename = line.substr(0, pos);
        cur.no_chunks = std::stoi(line.substr(pos + 1, line.length() - pos - 1));

        std::cout << cur.no_chunks << "\n";
        for(int j = 0; j < cur.no_chunks; ++j) {
            std::getline(in, line);
            std::cout << line << "\n";
            cur.chunks.push_back(line);
        }
        files.files.push_back(cur);
    }

    in.close();
    return files;
}

/*
    Prints the files list
*/
void printFiles(filelist files) {
    std::cout << "Number of files: " << files.no_files << "\n";
    for(int i = 0; i < files.no_files; ++i) {
        std::cout << "File: " << files.files[i].filename << " \nNumber of hashes: " << files.files[i].no_chunks << "\n";
        for(int j = 0; j < files.files[i].no_chunks; ++j) {
            std::cout << files.files[i].chunks[j] << "\n";
        }
    }
}
