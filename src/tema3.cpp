#include "tema3.h"

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
    // Recieves from each peer a list of the chunks
    
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

    client cv = readInput(rank);

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
