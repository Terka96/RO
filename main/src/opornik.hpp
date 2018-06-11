#include <vector>
#ifndef opornik_hpp
#define opornik_hpp


class Opornik{
public:
    Opornik(int i,Opornik* p);
    void makeKids(int count);
    void run(int rank);

	void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
	struct Message reveive_mpi_message(int tag);

private:
    int id;
    std::vector<Opornik*> childs;
    Opornik *parent;
};

#endif
