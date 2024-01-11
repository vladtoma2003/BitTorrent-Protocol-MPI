#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

struct file{
    std::string filename;
    int no_chunks;
    std::vector<std::string> chunks;
};

struct client{

    int rank;
    int files_no;
    std::map<std::string, std::vector<file>> files; // filename -> chunks

    int wantedFilesNo;
    std::vector<std::string> wantedFiles;

    /*
    Prints the files list
    */
    void printFiles() {
        std::cout << "=================" << rank << "=================\n";
        std::cout << "Files no: " << files_no << "\n";
        for(auto it = files.begin(); it != files.end(); ++it) {
            std::cout << it->first << " " << it->second.size() << "\n";
            for(int i = 0; i < it->second.size(); ++i) {
                std::cout << it->second[i].filename << " " << it->second[i].no_chunks << "\n";
                for(int j = 0; j < it->second[i].no_chunks; ++j) {
                    std::cout << it->second[i].chunks[j] << "\n";
                }
            }
        }

        std::cout << "Wanted files no: " << wantedFilesNo << "\n";
        for(int i = 0; i < wantedFilesNo; ++i) {
            std::cout << wantedFiles[i] << "\n";
        }
    }

    /*
        Sends the list of owned files to the tracker
    */
    // void sendDataToTracker() {
    //     for(int i = 0; i < files_no; ++i) {
    //         MPI_Send(&(files[i].filename.c_str()), (files[i].filename.size), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    //     }
    // }
};

/*
Reads the input. Structure of the file:
- first line: No. of files
- next line: filname hash chunks_count
- next lines: chunk_1, chunk_2, ..., chunk_n
- next line: filname hash chunks_count
- next lines: chunk_1, chunk_2, ..., chunk_n
...
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
        c.files[cur.filename].push_back(cur);
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
