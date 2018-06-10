#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <time.h>

class opornik{
public:
    opornik(int i,opornik* p){
        id=i;
        parent=p;
    }
    void makeKids(int count){
            int max_childs = (count<4) ? count : 4;
            int rand_childs = (random()%max_childs) + 1;
            count-=rand_childs;
            int childNodes[4]={0,0,0,0};
            for(int i=0;i<count;i++)
                childNodes[random()%rand_childs]++;
            int currId=id+1;
            for(int i=0;i<rand_childs;i++){
                opornik *op = new opornik(currId,this);
                if(childNodes[i]>0)
                    op->makeKids(childNodes[i]);
                childs.push_back(op);
                currId+=childNodes[i]+1;
            }
    }
    void run(int rank){
        for(int i=0;i<childs.size();i++)
            childs[i]->run(rank);

        if(rank==id){
            printf("Hello from node: %d\n",id);
        }
    }

private:
    int id;
    std::vector<opornik*> childs;
    opornik *parent;
};



int main(int argc, char **argv)
{
    srand(time(NULL));
    opornik root = opornik(0,NULL);
    root.makeKids(16);

    int rank, size;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //Populate tree with threads
    root.run(rank);

    MPI_Finalize();
}
