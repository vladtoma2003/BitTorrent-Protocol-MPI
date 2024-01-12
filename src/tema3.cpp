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

    std::string ack;
    MPI_Recv(&ack, 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if(ack != "ACK") {
        std::cout << "Error receiving ACK from tracker" << std::endl;
        return NULL;
    }

    // Foreach wanted file
    while(client_data->wantedFilesNo > 0) {
        // Request the list of peers
        std::string wantedFile = client_data->wantedFiles[0];
        std::map<int, std::vector<std::string>> peers = request(client_data, wantedFile);

        int wanted_file_size = 0;
        for(auto it = peers.begin(); it != peers.end(); ++it) {
            wanted_file_size = std::max(wanted_file_size, (int) it->second.size());
        }
        
        int needed_chunks = wanted_file_size - client_data->files[wantedFile].chunks.size();
        std::vector<std::string> new_chunks;
        // Check missing chunks
        



        if(needed_chunks == 0) {
            // Send "FIN" to tracker
            MPI_Send("FIN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            MPI_Recv(&ack, 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            client_data->wantedFiles.erase(client_data->wantedFiles.begin());
            --client_data->wantedFilesNo;
            continue;
        }

        // After downloading 10 chunks, send a message to tracker
        MPI_Send("TEN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        for(int i = 0; i < 10; ++i) {
            MPI_Send(new_chunks[i].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        }
    }

    // Send "ALL" to tracker
    MPI_Send("ALL", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

    MPI_Recv(&ack, 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    return NULL;
}

/*
    Tag for upload: 200
*/
void *upload_thread_func(void *arg)
{
    // int rank = *(int*) arg;
    client *client_data = (client*) arg;

    std::string ack;
    MPI_Recv(&ack, 3, MPI_CHAR, TRACKER_RANK, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if(ack != "ACK") {
        std::cout << "Error receiving ACK from tracker" << std::endl;
        return NULL;
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

    // Print the list of files and the chunks
    for(auto it = swarm.begin(); it != swarm.end(); ++it) {
        std::cout << it->first << std::endl;
        for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            std::cout << it2->first << ":\n";
            for(int i = 0; i < it2->second.size(); ++i) {
                std::cout << it2->second[i] << "\n";
            }
            std::cout << std::endl;
        }
    }

    // Sends "ACK" to all clients so they can start downloading
    for(int i = 1; i < numtasks; ++i) {
        MPI_Send("ACK", 3, MPI_CHAR, i, 100, MPI_COMM_WORLD);
        MPI_Send("ACK", 3, MPI_CHAR, i, 200, MPI_COMM_WORLD);
    }

    int finished = 0;

    while(finished < numtasks - 1) {
        // Wait for signal from a client
        MPI_Status status;
        std::string message;
        MPI_Recv(&message, 100, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if(message == "REQ") { // Recieved a request for list of peers
            std::string filename;
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Search the wanted file in the swarm
            auto response = searchFile(swarm, filename);

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
            MPI_Status status;
            std::string filename;
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for(int i = 0; i < 10; ++i) {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][status.MPI_SOURCE].push_back(chunk_str);                
            }

            MPI_Send("ACK", 3, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD);

        } else if(message == "FIN") { // A peer finished downloading a file

        } else if(message == "ALL") { // A peer finished downloading ALL files
            ++finished;
            // Send "ACK" to the peer that finished all the files (only download thread)
            MPI_Send("ACK", 3, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD);
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
