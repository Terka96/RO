#include "opornik.hpp"
#include "struct_const.h"
#include <mpi.h>
#include <stdio.h>
#include "constants.hpp"

Opornik::Opornik(int i,Opornik* p){
	id=i;
	parent=p;
}

int Opornik::initBooks(int c_books, Opornik* o){
	//todo powinno przydzielac w losowych miejscach?
	for (Opornik* child : o->childs)
	{  
	    if(c_books<NUM_BOOK)
			createBook(c_books++, child);
		c_books = initBooks(c_books, child);
	}
	return c_books;
}

int Opornik::initDvds(int c_dvds, Opornik* o){
	//todo powinno przydzielac w losowych miejscach?
	for (Opornik* child : o->childs)
	{  
		if(c_dvds<NUM_DVD)
			createDvd(c_dvds++, child);
		c_dvds = initDvds(c_dvds, child);
	}
	return c_dvds;
}

void Opornik::makeKids(int count){
    int max_childs = (count<MAX_CHILDREN) ? count : MAX_CHILDREN;
    int rand_childs = (random()%max_childs) + 1;
    count-=rand_childs;
    int childNodes[MAX_CHILDREN]={0};
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
	    printf("Hello from node: %d, my parent is: %d\n",id, parent!=NULL? parent->id: -1);
	}
}

void Opornik::createDvd(int id, Opornik* owner){
	Dvd *r = new Dvd(id,owner);
	owner->resources.push_back(r);
	printf("Konspirator %d spiracił nową płytę DVD o id: %d\n",owner->id, id);
}

void Opornik::createBook(int id, Opornik* owner){
	Book *r = new Book(id,owner);
	owner->resources.push_back(r);
	printf("Konspirator %d przepisał książkę o id: %d\n",owner->id, id);
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

