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
        MPI_Send("ALL", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        return NULL;
    }

    // Foreach wanted file
    for(int i = 0; i < client_data->wantedFilesNo; ++i) {
        // Request the list of peers
        std::string wantedFile = client_data->wantedFiles[i];
        auto peers = request(client_data, wantedFile);

        int wanted_file_size = 0;
        for(auto it = peers.begin(); it != peers.end(); ++it) {
            wanted_file_size = std::max(wanted_file_size, (int) it->second.size());
        }
        
        int needed_chunks = wanted_file_size - client_data->files[wantedFile].chunks.size();

        int counter = 0;
        int seeds_no = peers.size();
        int chunk_send = 0;
        
        auto it = peers.begin();
        while(needed_chunks > 0) {
            if(it == peers.end()) {
                it = peers.begin();
            }
                        
            MPI_Send("REQ", 3, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
            MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
            MPI_Send(&counter, 1, MPI_INT, it->first, 200, MPI_COMM_WORLD);

            char chunk[HASH_SIZE];
            MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, it->first, 300, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string chunk_str(chunk, HASH_SIZE);

            // Check if the chunk is already in the list
            client_data->files[wantedFile].chunks.push_back(chunk_str);
            --needed_chunks;
            ++counter;
            if(counter % 10 == 0) {
                MPI_Send("TEN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

                MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

                int n = 10;
                MPI_Send(&n, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
                for(int i = 0; i < 10; ++i) {
                    MPI_Send(client_data->files[wantedFile].chunks[chunk_send].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
                    ++chunk_send;
                }
            }
            ++it;
        }

        if(counter % 10 != 0) {
            MPI_Send("TEN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            int n = counter % 10;
            MPI_Send(&n, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
            for(int i = 0; i < counter % 10; ++i) {
                MPI_Send(client_data->files[wantedFile].chunks[chunk_send].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
                ++chunk_send;
            }
        }

        // Send "FIN" to tracker
        MPI_Send("FIN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        
        MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

        std::ofstream output("client"+std::to_string(client_data->rank)+"_"+wantedFile);
        if(!output.is_open()) {
            std::cout << "Error opening file\n";
            return NULL;
        }
        for(int i = 0; i < client_data->files[wantedFile].chunks.size(); ++i) {
            output << client_data->files[wantedFile].chunks[i];
            if(i != client_data->files[wantedFile].chunks.size()-1) {
                output << std::endl;
            }
        }
    }
    std::cout << "Client " << client_data->rank << " finished downloading\n";
    MPI_Send("ALL", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);


    return NULL;
}

/*
    Tag for upload: 200
*/
void *upload_thread_func(void *arg)
{
    // int rank = *(int*) arg;
    client *client_data = (client*) arg;

    while(1) {
        MPI_Status status;
        char mess[3];
        MPI_Recv(&mess, 3, MPI_CHAR, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, &status);
        std::string message(mess, 3);
        
        // Request for a hash
        if(message == "REQ") {
            char fil[MAX_FILENAME];
            MPI_Recv(&fil, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename(fil);

            int chunk_no;
            MPI_Recv(&chunk_no, 1, MPI_INT, status.MPI_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            std::string chunk = client_data->files[filename].chunks[chunk_no];
            MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 300, MPI_COMM_WORLD);
        } else if(message == "FIN") {
            return NULL;
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

    std::map<std::string, std::map<int, std::vector<std::string>>> swarm; // filename -> rank -> chunks

    std::cout << numtasks << "\n";

    // Receive the list of files and the hashes from the clients
    for(int i = 1; i < numtasks; ++i) {
        int files_no;
        int current_rank;
        MPI_Recv(&current_rank, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&files_no, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int j = 0; j < files_no; ++j) {
            char filename[100];
            MPI_Recv(&filename, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            swarm[filename][current_rank] = std::vector<std::string>();
            int chunks_no;
            MPI_Recv(&chunks_no, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for(int k = 0; k < chunks_no; ++k) {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][current_rank].push_back(chunk_str);
            }
        }
    }

    // Print swarm
    for(auto it = swarm.begin(); it != swarm.end(); ++it) {
        std::cout << "Filename: " << it->first << "\n";
        for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            std::cout << "Rank: " << it2->first << " ";
            for(int i = 0; i < it2->second.size(); ++i) {
                std::cout << it2->second[i] << " ";
            }
            std::cout << "\n";
        }
    }

    int finished = 0;

    while(finished < numtasks - 1) {
        // Wait for signal from a client
        MPI_Status status;
        char mess[3];
        MPI_Recv(&mess, 3, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        std::string message(mess, 3);
        if(message == "REQ") { // Recieved a request for list of peers
            char fil[MAX_FILENAME];
            MPI_Recv(&fil, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename(fil);

            // Search the wanted file in the swarm
            std::map<int, std::vector<std::string>> response;
            for(auto it = swarm.begin(); it != swarm.end(); ++it) {
                if(it->first == filename) {
                    response = it->second;
                    break;
                }
            }

            // Send the response list
            int respsize = response.size();
            MPI_Send(&respsize, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD);
            for(auto it = response.begin(); it != response.end(); ++it) {
                MPI_Send(&it->first, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // Rank
                int size = it->second.size();
                MPI_Send(&size, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // No of chunks
                for(int i = 0; i < it->second.size(); ++i) {
                    MPI_Send(it->second[i].c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // Chunk
                }
            }


        } else if(message == "TEN") { // A peer downloaded 10 chunks
            
            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int n;
            MPI_Recv(&n, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for(int i = 0; i < n; ++i) {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][status.MPI_SOURCE].push_back(chunk_str);  
            }

        } else if(message == "FIN") { // A peer finished downloading a file
            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


            // Send "ACK" to the peer that finished downloading the file (only download thread)
            // MPI_Send("ACK", 3, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD);
        } else if(message == "ALL") { // A peer finished downloading ALL files
            ++finished;
            if(finished == numtasks - 1) {
                std::cout << "All clients finished downloading\n";
                for(int i = 1; i < numtasks; ++i) {
                    MPI_Send("FIN", 3, MPI_CHAR, i, 200, MPI_COMM_WORLD);
                }
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
