#include "tema3.h"

/*
    This is the main function for the download thread.
    Steps:
    1. Get the list of seeds/peers for files from tracker
    2. Check missing chunks and search for them in the list of seeds
    3. Foreach missing chunk, follow the steps:
        3.1. Pick a seed/peer
        3.2. Request the chunk from the seed/peer
        3.3. Wait for the chunk and verify if correct
        3.4. Mark as received

    Tag for download: 100
    Tag for upload: 200
    Tag for getting a chunk: 300
*/
void *download_thread_func(void *arg)
{
    client *client_data = (client *)arg;

    // Wait for ACK from tracker
    char ack[3];
    MPI_Recv(&ack, 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    std::string ack_str(ack, 3);
    if (ack_str != "ACK")
    {
        std::cout << "Error receiving ACK from tracker\n";
        return NULL;
    }

    // Check if the client has any wanted files
    if (client_data->wantedFilesNo == 0)
    {
        MPI_Send("ALL", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        return NULL;
    }

    // Foreach wanted file
    for (int i = 0; i < client_data->wantedFilesNo; ++i)
    {
        // Request the list of peers
        std::string wantedFile = client_data->wantedFiles[i];
        auto peers = request(client_data, wantedFile);

        // Gets the size of file
        int wanted_file_size = 0;
        for (auto it = peers.begin(); it != peers.end(); ++it)
        {
            wanted_file_size = std::max(wanted_file_size, (int)it->second.size()); // at least one has the full size
        }

        int needed_chunks = wanted_file_size - client_data->files[wantedFile].chunks.size();

        int counter = 0;             // Counter for the chunks
        int seeds_no = peers.size(); // Used to alternate between seeds

        auto it = peers.begin();
        while (needed_chunks > 0)
        {
            if (it == peers.end())
            {
                it = peers.begin();
            }

            // Send request for chunk from current seed
            MPI_Send("REQ", 3, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
            MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
            MPI_Send(&counter, 1, MPI_INT, it->first, 200, MPI_COMM_WORLD);

            // Wait for chunk
            char chunk[HASH_SIZE];
            MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, it->first, 300, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string chunk_str(chunk, HASH_SIZE);

            // Check if the chunk is in the list from the tracker.
            if (!isChunkInList(peers, chunk))
            {
                // Request the chunk until it is received correctly
                while (1)
                {
                    MPI_Send("REQ", 3, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
                    MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, it->first, 200, MPI_COMM_WORLD);
                    MPI_Send(&counter, 1, MPI_INT, it->first, 200, MPI_COMM_WORLD);

                    MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, it->first, 300, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    std::string chunk_str(chunk, HASH_SIZE);

                    if (isChunkInList(peers, chunk))
                    {
                        break;
                    }
                }
            }

            // Mark as received
            client_data->files[wantedFile].chunks.push_back(chunk_str);
            // Get to the next hash
            --needed_chunks;
            ++counter;

            // Send update to tracker every 10 chunks
            if (counter % 10 == 0)
            {
                MPI_Send("UPD", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

                MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

                int n = 10;
                MPI_Send(&n, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
                for (int i = 0; i < 10; ++i)
                {
                    MPI_Send(client_data->files[wantedFile].chunks[counter - (10 - i)].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
                }
            }
            ++it;
        }

        // Send FIN to tracker and update the tracker with the last chunks if needed
        MPI_Send("FIN", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
        if (counter % 10 != 0)
        {
            MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            int n = counter % 10;
            MPI_Send(&n, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
            for (int i = 0; i < counter % 10; ++i)
            {
                MPI_Send(client_data->files[wantedFile].chunks[counter - (10 - i)].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            }
        }
        else
        { // No need for update
            MPI_Send(wantedFile.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
            int n = 0;
            MPI_Send(&n, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD);
        }

        // Write the file
        std::ofstream output("client" + std::to_string(client_data->rank) + "_" + wantedFile);
        if (!output.is_open())
        {
            std::cout << "Error opening file\n";
            return NULL;
        }
        for (int i = 0; i < client_data->files[wantedFile].chunks.size(); ++i)
        {
            output << client_data->files[wantedFile].chunks[i];
            if (i != client_data->files[wantedFile].chunks.size() - 1)
            {
                output << std::endl;
            }
        }
    }
    // Finished all download for all files
    MPI_Send("ALL", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

    return NULL;
}

/*
    This is the main function for the upload thread.
    Steps:
    - Wait for a request
    - If the request is for a hash, send the hash
    - If the request is for closing, close the thread
    Tag for upload: 200
    Tag for sending a chunk: 300
*/
void *upload_thread_func(void *arg)
{
    client *client_data = (client *)arg;

    while (1)
    {
        // Wait for a request
        MPI_Status status;
        char mess[3];
        MPI_Recv(&mess, 3, MPI_CHAR, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, &status);
        std::string message(mess, 3);

        // Request for a hash
        if (message == "REQ")
        {
            char fil[MAX_FILENAME];
            MPI_Recv(&fil, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename(fil);

            int chunk_no;
            MPI_Recv(&chunk_no, 1, MPI_INT, status.MPI_SOURCE, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Send the requested hash
            std::string chunk = client_data->files[filename].chunks[chunk_no];
            MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 300, MPI_COMM_WORLD);
        }
        else if (message == "DON")
        { // Close the upload thread
            break;
        }
    }

    return NULL;
}

/*
    This is the main function for the tracker.
    Stepts:
    1. Receive the list of files and hashes from the clients
    2. Checks the recieved message
    3. Multiple types of messages:
        3.1. Request for a chunk: Sends a list of seeds/peers that have the chunk and what other chunks they have.
        3.2. Update from client: Mark the client in the swarm of files it has and updates the list of chunks owned by the client.
        Also sends the list from 2.1 to the client.
        3.3. Finish download of a file: Marks the client as seed.
        3.4. Finish download of all files: Marks the client as finished, but it keeps it in the swarm.
        3.5. All clients finished downloading all the files: Sends a message to all clients to close and closes itself.

*/
void tracker(int numtasks, int rank)
{

    std::map<std::string, std::map<int, std::vector<std::string>>> swarm; // filename -> rank -> chunks

    // Receive the list of files and the hashes from the clients
    for (int i = 1; i < numtasks; ++i)
    {
        int files_no;
        int current_rank;
        MPI_Recv(&current_rank, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&files_no, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < files_no; ++j)
        {
            char filename[100];
            MPI_Recv(&filename, 100, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            swarm[filename][current_rank] = std::vector<std::string>();
            int chunks_no;
            MPI_Recv(&chunks_no, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int k = 0; k < chunks_no; ++k)
            {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][current_rank].push_back(chunk_str);
            }
        }
    }

    int finished = 0;

    // Send signal to start downloading
    for (int i = 1; i < numtasks; ++i)
    {
        MPI_Send("ACK", 3, MPI_CHAR, i, 100, MPI_COMM_WORLD);
    }

    while (finished < numtasks - 1)
    {
        // Wait for signal from a client
        MPI_Status status;
        char mess[3];
        MPI_Recv(&mess, 3, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        std::string message(mess, 3);
        if (message == "REQ")
        { // Recieved a request for list of peers
            char fil[MAX_FILENAME];
            MPI_Recv(&fil, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string filename(fil);

            // Search the wanted file in the swarm
            std::map<int, std::vector<std::string>> response;
            for (auto it = swarm.begin(); it != swarm.end(); ++it)
            {
                if (it->first == filename)
                {
                    response = it->second;
                    break;
                }
            }

            // Send the response list
            int respsize = response.size();
            MPI_Send(&respsize, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD);
            for (auto it = response.begin(); it != response.end(); ++it)
            {
                MPI_Send(&it->first, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // Rank
                int size = it->second.size();
                MPI_Send(&size, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // No of chunks
                for (int i = 0; i < it->second.size(); ++i)
                {
                    MPI_Send(it->second[i].c_str(), HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD); // Chunk
                }
            }
        }
        else if (message == "UPD")
        { // A peer downloaded 10 chunks
            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int n;
            MPI_Recv(&n, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Update the swarm
            for (int i = 0; i < n; ++i)
            {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][status.MPI_SOURCE].push_back(chunk_str);
            }
        }
        else if (message == "FIN")
        { // A peer finished downloading a file
            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int n;
            MPI_Recv(&n, 1, MPI_INT, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Update the swarm
            for (int i = 0; i < n; ++i)
            {
                char chunk[HASH_SIZE];
                MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string chunk_str(chunk, HASH_SIZE);
                swarm[filename][status.MPI_SOURCE].push_back(chunk_str);
            }
        }
        else if (message == "ALL")
        { // A peer finished downloading ALL files
            ++finished;
            if (finished == numtasks - 1)
            { // All peers finished downloading ALL files
                for (int i = 1; i < numtasks; ++i)
                {
                    MPI_Send("DON", 3, MPI_CHAR, i, 200, MPI_COMM_WORLD);
                }
            }
        }
    }
}

/*
    It has the segments of the file. It handles the download and upload of the file.
    It's main priority: Shares the owned segments with other peers and gets the missing ones.
*/
void peer(int numtasks, int rank)
{
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    client cv = readInput(rank);
    cv.sendDataToTracker();

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *)&cv);
    if (r)
    {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *)&cv);
    if (r)
    {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    int numtasks, rank;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE)
    {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK)
    {
        tracker(numtasks, rank);
    }
    else
    {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
