#include <vector>
#ifndef opornik_hpp
#define opornik_hpp
#include"resource.hpp"
#include"dvd.hpp"
#include"book.hpp"


class Opornik{
public:
    Opornik(int i,Opornik* p);
    void makeKids(int count);
    void run(int rank);
	int initBooks(int c_books, Opornik* o);
	int initDvds(int c_dvds, Opornik* o);

	void createDvd(int id,Opornik* owner);
	void createBook(int id,Opornik* owner);

	void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
	struct Message reveive_mpi_message(int tag);

private:
    int id;
    std::vector<Opornik*> childs;
	std::vector<Resource*> resources;
    Opornik *parent;
};

#endif
