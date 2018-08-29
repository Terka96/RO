#include <vector>
#ifndef opornik_hpp
#define opornik_hpp
#include"messages.h"
#include"resource.hpp"
#include"dvd.hpp"
#include"book.hpp"

class Opornik{
public:
    Opornik();
    void makeKids(int count);
    void run();
	int initBooks(int c_books, Opornik* o);
    int initDvds(int c_dvds, Opornik* o);

	void createDvd(int id,Opornik* owner);
	void createBook(int id,Opornik* owner);

	void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
    void reveive_mpi_message(int tag);

private:
    int id;
    std::vector<int> neighbors;
    std::vector<int> childs;
	std::vector<Resource*> resources;
    int parent;
    int clock;

    int inline debug_log(const char* format, ...);
};

#endif
