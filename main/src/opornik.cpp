#include "opornik.hpp"
#include "struct_const.h"
#include <mpi.h>
#include <stdio.h>

Opornik::Opornik(int i,Opornik* p){
	id=i;
	parent=p;
}
void Opornik::makeKids(int count){
    int max_childs = (count<4) ? count : 4;
    int rand_childs = (random()%max_childs) + 1;
    count-=rand_childs;
    int childNodes[4]={0,0,0,0};
    for(int i=0;i<count;i++)
        childNodes[random()%rand_childs]++;
    int currId=id+1;
    for(int i=0;i<rand_childs;i++){
        Opornik *op = new Opornik(currId,this);
        if(childNodes[i]>0)
            op->makeKids(childNodes[i]);
        childs.push_back(op);
        currId+=childNodes[i]+1;
    }
}
void Opornik::run(int rank){
	for(int i=0;i<childs.size();i++)
	    childs[i]->run(rank);

	if(rank==id){
	    printf("Hello from node: %d\n",id);
	}
}

void Opornik::send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver){
   struct Message send_data;
  		 //todo
   MPI_Send(&send_data, 1, MPI_INT, receiver, tag, MPI_COMM_WORLD);
}

struct Message Opornik::reveive_mpi_message(int tag){
   struct Message data;
		//todo
   return data;
}

