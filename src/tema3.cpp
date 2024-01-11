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

    Tag for download: 100
*/
void *download_thread_func(void *arg)
{
    client *client_data = (client*) arg;

    if(client_data->wantedFilesNo == 0) {
        return NULL;
    }

    // foreach wanted file
    for(int i = 0; i < client_data->wantedFilesNo; ++i) {
        std::string wantedFile = client_data->wantedFiles[i];
        int signal = 1;
        MPI_Send(&signal, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&client_data->rank, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
        MPI_Send(&wantedFile, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

        int seeds_no;
        MPI_Recv(&seeds_no, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        std::vector<int> seeds;
        for(int j = 0; j < seeds_no; ++j) {
            int seed_rank;
            MPI_Recv(&seed_rank, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            seeds.push_back(seed_rank);
        }

        // I got the list of peers that have my file
        int counter = 0;

        int first_person = seeds[0];

        int upload_signal = 100;
        MPI_Send(&signal, 1, MPI_INT, first_person, 200, MPI_COMM_WORLD);
        request req;
        req.source = client_data->rank;
        strcpy(req.filename, wantedFile.c_str());
        req.chunk_no = -1;
        MPI_Send(&req, sizeof(request), MPI_CHAR, first_person, 200, MPI_COMM_WORLD);

        int chunk_no;
        MPI_Recv(&chunk_no, 1, MPI_INT, first_person, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        std::vector<std::string> chunks;
        int peers_no = seeds.size();
        int divided_chunks = chunk_no / peers_no;

        // divide the chunks between the peers. The first divided_chunks number of chunks will come from the first peer and so on
        std::vector<std::string> chunks_received;
        for(int j = 0; j < chunk_no; ++j) {
            MPI_Send(&upload_signal, 1, MPI_INT, seeds[j / divided_chunks], 200, MPI_COMM_WORLD);
            request new_req;
            new_req.source = client_data->rank;
            strcpy(new_req.filename, wantedFile.c_str());
            new_req.chunk_no = j;
            MPI_Send(&new_req, sizeof(request), MPI_CHAR, seeds[j / divided_chunks], 200, MPI_COMM_WORLD);

            std::string chunk;
            MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, seeds[j / divided_chunks], 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            chunks_received.push_back(chunk);
        }

        client_data->files[wantedFile].chunks = chunks_received;
        client_data->files[wantedFile].no_chunks = chunks_received.size();
        client_data->files[wantedFile].filename = wantedFile;
        
        // send to tracker that I finished downloading the file
        signal = 2;
        MPI_Send(&signal, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&client_data->rank, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&wantedFile, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

    }

    int signal = 3;
    MPI_Send(&signal, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

    return NULL;
}

/*
    Tag for upload: 200
*/
void *upload_thread_func(void *arg)
{
    // int rank = *(int*) arg;
    client *client_data = (client*) arg;

    // signals: 100 - request, 200 - done
    while(1) {
        int signal;
        MPI_Recv(&signal, 1, MPI_INT, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(signal == 100) {
            request req;
            MPI_Recv(&req, sizeof(request), MPI_CHAR, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if(req.chunk_no == -1) {
                // send the number of chunks
                int chunks_no = client_data->files[req.filename].no_chunks;
                MPI_Send(&chunks_no, 1, MPI_INT, req.source, 200, MPI_COMM_WORLD);
            } else {
                std::string chunk = client_data->files[req.filename].chunks[req.chunk_no];
                MPI_Send(&chunk, HASH_SIZE, MPI_CHAR, req.source, 200, MPI_COMM_WORLD);
            }
        } else if(signal == 200) { // all peers finished downloading
            break;
        }
    }


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
    // Recieves from each peer a list of the files it has
    std::map<int, std::vector<std::string>> peerFiles; // rank -> files
    
    for(int i = 1; i < numtasks; ++i) { // Gets the list of owned files from each peer
        int files_no;
        int current_rank;
        MPI_Recv(&current_rank, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&files_no, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int j = 0; j < files_no; ++j) {
            char filename[100];
            MPI_Recv(&filename, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            peerFiles[current_rank].push_back(filename);
        }   
    }

    for(auto it = peerFiles.begin(); it != peerFiles.end(); ++it) {
        std::cout << it->first << ":\n";
        for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            std::cout << *it2 << "\n";
        }
    }


    int finished = 0;

    while(1) {
        int signal;
        MPI_Recv(&signal, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // signals: 1 - request, 2 - finished download, 3 - finished all downloads
        if(signal == 1) {
            int source;
            MPI_Recv(&source, 1, MPI_INT, MPI_ANY_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename;
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            std::vector<int> seeds; // ranks of seeds that have the file
            for(auto it = peerFiles.begin(); it != peerFiles.end(); ++it) {
                for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                    if(*it2 == filename) {
                        seeds.push_back(it->first);
                    }
                }
            }

            int seeds_no = seeds.size();

            MPI_Send(&seeds_no, 1, MPI_INT, source, 100, MPI_COMM_WORLD);
            for(int i = 0; i < seeds_no; ++i) {
                int seed_rank = seeds[i];
                MPI_Send(&seed_rank, 1, MPI_INT, source, 100, MPI_COMM_WORLD);
            }
        } else if(signal == 2) {
            // a peer finished downloading a file
            int source;
            MPI_Recv(&source, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename;
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // mark the peer as seed
            peerFiles[source].push_back(filename);

        } else if (signal == 3) { // a peer finished downloading all the files
            finished++;
            if(finished == numtasks - 1) {
                // send stop signal to uploaders
                int signal = 200;
                for(int i = 1; i < numtasks; ++i) {
                    MPI_Send(&signal, 1, MPI_INT, i, 200, MPI_COMM_WORLD);
                }
                break;
            }

        }
    }
    
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
    // cv.printFiles();
    cv.sendDataToTracker();

    r = pthread_create(&download_thread, NULL, download_thread_func, (void*) &cv);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &cv);
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
