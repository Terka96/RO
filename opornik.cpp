#include "opornik.hpp"
#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "constants.hpp"

#define DEBUGLOG 1

int inline Opornik::debug_log(const char* format, ...){
    int res;
    #ifdef DEBUGLOG
    printf("Node %d [%d]:",id,clock);
    va_list args;
    va_start (args, format);
    res = vprintf(format, args);
    va_end (args);
    #endif
    return res;
};

Opornik::Opornik(){
    int rank,size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    id=rank;

    srand(time(NULL)+id*size);//give every process (even on same machine) different random seed
    if(id==0)
    {
        parent=-1;
        makeKids(size-1);
        //initBooks(0,me);
    }
    else
    {
        MPI_Status status;
        order_makekids order;
        MPI_Recv(&order,2+MAX_CHILDREN,MPI_INT,MPI_ANY_SOURCE,ORDER_MAKEKIDS,MPI_COMM_WORLD,&status);
        parent=order.parent;
        for(int i=0;i<MAX_CHILDREN;i++)
            if(order.neighbors[i]!=-1 && order.neighbors[i]!=id)
                neighbors.push_back(order.neighbors[i]);
        makeKids(order.count);
    }
    clock=0;
}

/*int Opornik::initBooks(int c_books, Opornik* o){
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
}*/

void Opornik::makeKids(int count){
    int ackCount=count;
    int grandchildrenCount=0;
    if(count>0)
    {
        int max_childs = (count<MAX_CHILDREN) ? count : MAX_CHILDREN;
        int rand_childs = (random()%max_childs) + 1;
        grandchildrenCount=count-rand_childs;
        //WARNING: STUPID STATIC CLEAR
        int grandchildrenInNodes[MAX_CHILDREN]={0,0,0,0};
        int childrenNodes[MAX_CHILDREN]={-1,-1,-1,-1};
        //Distribution of grandchildren
        for(int i=0;i<grandchildrenCount;i++)
            grandchildrenInNodes[random()%rand_childs]++;
        //calculating children nodes ids
        childrenNodes[0]=id+1;
        for(int i=1;i<rand_childs;i++)
            childrenNodes[i]=childrenNodes[i-1]+grandchildrenInNodes[i-1]+1;

        order_makekids order;
        order.parent=id;
        for(int i=0;i<MAX_CHILDREN;i++)
            order.neighbors[i]=childrenNodes[i];

        for(int i=0;i<rand_childs;i++){
            order.count=grandchildrenInNodes[i];
            debug_log("Sending order(%d) to node %d\n",grandchildrenInNodes[i],childrenNodes[i]);
            MPI_Bsend(&order,2+MAX_CHILDREN,MPI_INT,childrenNodes[i],ORDER_MAKEKIDS,MPI_COMM_WORLD);
            childs.push_back(childrenNodes[i]);
        }
        MPI_Status status;
        for(int i=0;i<rand_childs;i++){
            ack_makekids ack;
            ack.count=-1;
            MPI_Recv(&ack,1,MPI_INT,MPI_ANY_SOURCE,ACK_MAKEKIDS,MPI_COMM_WORLD,&status);
            ackCount-=(ack.count+1);
            debug_log("Recv ack(%d) from %d node\n",ack.count,status.MPI_SOURCE);
        }
    }

    if(ackCount==0){
        if(parent!=-1)
        {
            ack_makekids ack;
            ack.count=count;
            MPI_Send(&ack,1,MPI_INT,parent,ACK_MAKEKIDS,MPI_COMM_WORLD);
        }
    }
    else
        printf("ERROR: node %d ordered %d remaining acks %d from childs %d\n",id,count,ackCount,childs.size());
}

void Opornik::run(){
    printf("Node %d [%d]:Hello, my parent is: %d",id,clock,parent);
    if(neighbors.size()>0)
        printf(" and my neighbors are ");
    for(int i=0; i<neighbors.size();i++)
        printf(" %d",neighbors[i]);
    printf("\n");

    for(int i=0;i<100;i++)//or while(true)
    {
        sleep(1);//1 sec- or use nanosleep instead
        int actionRand=rand()%1001;          //promilowy podział prawdopodobieństwa dla pojedynczego procesu co sekundę
        if(actionRand>=975)
            debug_log("Chcę zorganizować spotkanie!\n");//+send info
        else if(actionRand>=950 && true)//drugi warunek- jestem akceptorem (not implemented yet)
            debug_log("Nie chcę już być akceptorem!\n");//+send info

        //non-blocking recv (Brecv czy coś)
        //switch(tag)
        //{
        //  tutaj syf związany z obsługą komunikatów
        //}
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
/*
void Opornik::send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver){
   struct Message send_data;
  		 //todo
   MPI_Send(&send_data, 1, MPI_INT, receiver, tag, MPI_COMM_WORLD);
}

struct Message Opornik::reveive_mpi_message(int tag){
   struct Message data;
		//todo
   return data;
}*/

