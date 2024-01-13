#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include<algorithm>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

struct request {
    // int source;
    char filename[MAX_FILENAME];
    int chunk_no;
};

struct file{
    std::string filename;
    int no_chunks;
    std::vector<std::string> chunks;
};


struct client{

    int rank;
    int files_no;
    std::map<std::string, file> files; // filename -> chunks

    int wantedFilesNo;
    std::vector<std::string> wantedFiles;

    /*
    Prints the files list
    */
    void printFiles() {
        std::cout << "Files: " << std::endl;
        for(auto it = files.begin(); it != files.end(); ++it) {
            std::cout << it->first << " " << it->second.no_chunks << std::endl;
            for(int i = 0; i < it->second.no_chunks; ++i) {
                std::cout << it->second.chunks[i] << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Wanted files: " << std::endl;
        for(int i = 0; i < wantedFilesNo; ++i) {
            std::cout << wantedFiles[i] << std::endl;
        }
    }

    /*
        Sends the list of owned files to the tracker
    */
    void sendDataToTracker() {
        MPI_Send(&rank, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&files_no, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        for(auto it = files.begin(); it != files.end(); ++it) {
            MPI_Send(it->first.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
            MPI_Send(&it->second.no_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
            for(int i = 0; i < it->second.no_chunks; ++i) {
                MPI_Send(it->second.chunks[i].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
            }
        }
    }
};


/*
Reads the input. Structure of the file:
- first line: No. of files
- next line: filname hash chunks_count
- next lines: chunk_1, chunk_2, ..., chunk_n
- next line: filname hash chunks_count
- next lines: chunk_1, chunk_2, ..., chunk_n
...
- next line: No. of wanted files
- next lines: wanted_file_1, wanted_file_2, ..., wanted_file_n
*/
client readInput(int rank) {
    client c;
    c.rank = rank;
    
    std::ifstream f("in" + std::to_string(rank) + ".txt");
    // std::string line;

    f >> c.files_no;
    
    for(int i = 0; i < c.files_no; ++i) {
        file cur;
        f >> cur.filename;
        f >> cur.no_chunks;
        for(int j = 0; j < cur.no_chunks; ++j) {
            char chunk[HASH_SIZE];
            f >> chunk;
            cur.chunks.push_back(chunk);
        }
        c.files[cur.filename] = cur;
    }

    f >> c.wantedFilesNo;
    for(int i = 0; i < c.wantedFilesNo; ++i) {
        std::string filename;
        f >> filename;
        c.wantedFiles.push_back(filename);
    }

    f.close();
    return c;
}

/*
    Requests from the tracker the list of peers that have the wanted file
*/
std::map<int, std::vector<std::string>> request(client *client_data, std::string filename) {

    MPI_Send("REQ", 3, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);
    MPI_Send(filename.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

    int response_size;
    MPI_Recv(&response_size, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    std::map<int, std::vector<std::string>> peers;
    // Receive the list of peers
    for(int j = 0; j < response_size; ++j) {
        int rank;
        MPI_Recv(&rank, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int chunks_no;
        MPI_Recv(&chunks_no, 1, MPI_INT, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        peers[rank] = std::vector<std::string>();
        for(int k = 0; k < chunks_no; ++k) {
            char chunk[HASH_SIZE];
            MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::string chunk_str(chunk, HASH_SIZE);
            peers[rank].push_back(chunk_str);
        }
    }

    return peers;
}
