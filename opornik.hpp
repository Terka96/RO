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
    	void run();
    	void introduce();
	int initBooks(int c_books, Opornik* o);
    	int initDvds(int c_dvds, Opornik* o); 

	void createDvd(int id,Opornik* owner);
	void createBook(int id,Opornik* owner);

	void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
    	void reveive_mpi_message(int tag);

private:
    	void makeTree();
    	void makeKids(int count);
    	void distributeAcceptorsAndResources();
    	void pass_acceptor();
    	static void *live_starter(void * arg);  
    	static void *listen_starter(void * arg);
    	void live();
    	void listen();

    	int id;
    	int size;
    	int parent;
    	int clock;
    	int acceptorToken;
	bool blocked;
    	std::vector<int> neighbors;
    	std::vector<int> childs;
    	std::vector<int> resources;

    	int inline debug_log(const char* format, ...);
};

#endif
