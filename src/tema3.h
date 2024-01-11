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
    int source;
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
        std::cout << "===================" << rank << "====================" << std::endl;
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
    std::string line;

    f >> c.files_no;
    
    for(int i = 0; i < c.files_no; ++i) {
        std::getline(f, line);
        file cur;
        f >> cur.filename >> cur.no_chunks;
        for(int j = 0; j < cur.no_chunks; ++j) {
            std::getline(f, line);
            cur.chunks.push_back(line);
        }
        c.files[cur.filename] = cur;
    }

    std::getline(f, line);
    f >> c.wantedFilesNo;

    for(int i = 0; i < c.wantedFilesNo; ++i) {
        std::getline(f, line);
        c.wantedFiles.push_back(line);
    }

    f.close();
    return c;
}
