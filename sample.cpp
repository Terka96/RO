#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include "struct_const.h"

MPI_Datatype mpi_message_type;
int COMPANIES_COUNT;
bool DEBUG = false;

/**
   Metoda tworzy customowy typ wiadomosci wysyłanej przez MPI
**/
void create_custom_message_type();
/**
   F-cja inicjalizacyjna która losuje startowe wartości
**/
void init_companies(int tid,int size,int companyCount,struct Company *companies);
/**
   Tworzenie nowego wątku
**/
struct ThreadParams* createNewThread(int tid,int size,struct Company* companies, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition, int*curr_time, bool* allReserved);
/**
   Wrapper na MPI_Send któremu podajemy wszystkie parametry i in już buduje strukturę do przesłania
**/
void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver);
/**
   Wrapper na MPI_recv który od razu zwraca nam naszą strukturę wiadomości
**/
struct Message reveive_mpi_message(int tag);
/**
   Usuwa klienta z kolejki danej firmy
**/
void remove_client_from_queue(struct Company *companies, int size, int sender_id, int company_id);
/**
   Dodaje klienta do kolejki sortując ją po timestampie. Jeżeli czasy są takie same pierwszeństwo ma niższe ID
   Sprawdza także czy klient jest już w kolejce, jeżeli tak to pomija wszystko
**/
void add_client_to_queue(struct Company * companies, int size, int sender_id, int company_id, int timestamp);
/**
   Zwraca najelpszą firmę spośród firm do których njuż nie requestowaliśmy - te które są w tablicy requested. Jeżeli nie ma już firmy to zwraca -1
**/
int select_best_company(struct Company* companies, int companies_size, int*requested, int requested_size);
/**
   Zmiania lokalnie reputację firmy oraz rozsyła polecenie zmiany do innych firm
**/
void update_company_reputation_request(struct Company * companies, int tid, int size, int company_id, int reputation_change, pthread_mutex_t *mutexCompany, int*curr_time);
/**
   Krytyczna sekcja
**/
void critical_sention(int tid, int company, int *curr_time, int requested_time);
/**
   Metoda wysyła żądania zwolnienia z kolejek do wszystkich innych klientów oraz zwalnia z lokalnej kolejki dla danej firmy
**/
void free_request_to_company(int tid, int size, int company, struct Company * companies, pthread_mutex_t *mutexCompany, int*curr_time);
/**
   Sprawdza pozycję klienta w danej kolejce firmy.
**/
int check_position(int tid, int size, int company, struct Company * companies);
/**
   Dodaje wybraną firmę do lokalnej kolejki oraz wysyła requesty do innych firm oraz odbiera ACK
   Zwraca listę firm które są przed nami
**/
void request_company(int tid, int size, int company, struct Company * companies, pthread_mutex_t *mutexCompany, int*curr_time);
/**
   F-cja zwalnia wszystkie firmy oprócz wybranej, odpala sekcję krytyczną, zwalnia wybraną f-cję
**/
void use_killer_and_free_others(int tid, int size, int company, struct Company * companies, int requested_count, int*requested, pthread_mutex_t *mutexCompany, int *curr_time){
   int i=0;
   for(i=0;i<requested_count;i++){
      if(requested[i] != company)
         free_request_to_company(tid, size, requested[i], companies, mutexCompany, curr_time);
   }
   int requested_time = -1;
   for (i=0;i<size;i++)
      if(companies[company].queue[i][0] ==tid)
         requested_time = companies[company].queue[i][1];
   critical_sention(tid, company, curr_time, requested_time);
   free_request_to_company(tid, size, company, companies, mutexCompany, curr_time);
}

void mainThread(bool* threadIsAlive, int *curr_time, pthread_mutex_t *mutexCompany, pthread_cond_t *changeCondition, int tid,int size, struct Company *companies,bool* allReserved) {
   int i=0,k=0,j=0, USED_COMPANY=-1;
   srand(tid*1000);
   int distance = 2;
   while(*threadIsAlive){
      bool enoughPosition = false;
      int * requested = (int*)malloc(sizeof(int)*COMPANIES_COUNT); //To mamy informację o firmach do których po kolei się kolejkowaliśmy
      for(i=0;i<COMPANIES_COUNT;i++) requested[i]=-1;
      int sendRes = 0;
      int requested_size=0;
      int loop=1;
      while(loop){
         //Sprawdzamy czy w poprzednich firmach zwolniło się jakieś miejsce Jeżeli jest zabójca to używamy jeżeli nie to zwalniamy firmy które są dalej i dłużej w nich czekamy
         if(requested_size>0){
            int lowest = -1;
            for(k=0;k<requested_size;k++){
               pthread_mutex_lock(mutexCompany);
               int position = check_position(tid, size, requested[k], companies);
               pthread_mutex_unlock(mutexCompany);

               if ((position < companies[requested[k]].killers) && (position>=0) ) {
                  USED_COMPANY = requested[k];
                  use_killer_and_free_others(tid, size, requested[k], companies, requested_size, requested, mutexCompany, curr_time);
                  loop=0;
                  break;
               }
             }
             if(!loop){
               break;
             }
             // Jeżeli jesteśmy dostatecznie blisko konca kolejki to rezygnujemy z reszty o mniejszej reputacji
               pthread_mutex_lock(mutexCompany);
               int maxRep = -1;
               int closest = -1;
               int reputations[requested_size];
               for(k=0;k<requested_size;k++){
                 int position = check_position(tid, size, requested[k], companies);
                 if(position < (companies[requested[k]].killers+distance) && position>=0){
                   if(maxRep < companies[requested[k]].reputation){
                     closest = k;
                     maxRep = companies[requested[k]].reputation;
                   }
                 }
                 reputations[k] = companies[requested[k]].reputation;
               }
               pthread_mutex_unlock(mutexCompany);
               if (closest != -1){
                 enoughPosition = true;
                 for(k=0;k<requested_size && k!=closest;k++){
                   if(reputations[k] < maxRep){
                     free_request_to_company(tid, size, requested[k], companies, mutexCompany, curr_time);
                     requested[k] = -1;
                     int tmp = requested[k];
                     for(j=k;j<requested_size-1;j++){
                       requested[j] = requested[j+1];
                       requested[j+1] = requested[j];
                       }
                     requested_size--;
                   }
                 }
               }
             }
         //Jeżeli są jakieś firmy do których się jeszcze nie zakolejkowaliśmy, to to robimy
         if(sendRes<COMPANIES_COUNT && !enoughPosition){
            pthread_mutex_lock(mutexCompany);
            int best_company = select_best_company(companies, COMPANIES_COUNT, requested, requested_size);
            pthread_mutex_unlock(mutexCompany);
            requested[requested_size] = best_company;
            requested_size++;
            request_company(tid, size, best_company, companies, mutexCompany, curr_time);
            sendRes++;
         }
         else {
            pthread_mutex_lock(mutexCompany);
            *allReserved = true;
            while(*allReserved){
              pthread_cond_wait(changeCondition,mutexCompany);
            }
            pthread_mutex_unlock(mutexCompany);
         }

      }
      update_company_reputation_request(companies, tid, size, USED_COMPANY, (rand()%(REPUTATION_CHANGE_MIN_MAX*2)-REPUTATION_CHANGE_MIN_MAX), mutexCompany, curr_time);
      free(requested);
   }
}

/**
   Implementacja dodatkowego wątku który przyjmuje wiadomości i odpowiada innym klientom.
   *************************Implementacja prawie skończona, został tylko ten wait condition do zrobiernia zamiast aktywnego czekania*********************
**/
void *additionalThread(void *thread){
  int i=0;
  struct ThreadParams *pointer = (struct ThreadParams*) thread;
  struct Company *companies = pointer->companies;
  int tid = pointer->tid;
  int size = pointer->size;
  int *curr_time = pointer->curr_time;
  bool* threadIsAlive = pointer->threadIsAlive;
  pthread_mutex_t *mutexCompany = pointer->mutexCompany;
  pthread_cond_t *changeCondition = pointer->changeCondition;
  bool *allReserved = pointer->allReserved;
  while(*threadIsAlive){
    //WAIT ON MASSAGE
    struct Message data = reveive_mpi_message(MESSAGE_TAG);
    switch (data.info_type) {
       case CHANGE_REPUTATION_TYPE:
         if(DEBUG)printf("\tClient %d received reputation change (%d) from client %d, company: %d\n", tid, data.data, data.sender_id, data.company_id);
         pthread_mutex_lock(mutexCompany);
         if(data.timestamp > *curr_time) *curr_time = data.timestamp;
         companies[data.company_id].reputation += data.data;
         pthread_mutex_unlock(mutexCompany);
         break;
       case REMOVE_FROM_COMPANY_QUEUE:
         if(DEBUG)printf("\tClient %d received remove request from client %d, company: %d\n", tid, data.sender_id, data.company_id);
         pthread_mutex_lock(mutexCompany);
         if(data.timestamp > *curr_time) *curr_time=data.timestamp;
         remove_client_from_queue(companies, size, data.sender_id, data.company_id);
         *allReserved = false;
         pthread_cond_signal(changeCondition);
         pthread_mutex_unlock(mutexCompany);
         break;
       case REQUEST_KILLER: //Dodajemy do kolejki
         if(DEBUG)printf("\tClient %d received killer request from client %d, company: %d\n", tid, data.sender_id, data.company_id);
         pthread_mutex_lock(mutexCompany);
         add_client_to_queue(companies, size, data.sender_id, data.company_id, data.timestamp);
         //Jeżeli nie ubiegamy się o firmę lub nie ubiegamy się o tą konktretną wysyłamy -1. W przeciwnym wypadku wysyłamy czas w którym robiliśmy request.
         if(data.timestamp > *curr_time) *curr_time=data.timestamp;
         int myRequestTimeStamp = -1;
         for(i=0;i<size;i++){
            if(companies[data.company_id].queue[i][0] == tid){
               myRequestTimeStamp = companies[data.company_id].queue[i][1];
               break;
            }
         }
         pthread_mutex_unlock(mutexCompany);
         send_mpi_message(tid, data.company_id,ACK_TYPE,myRequestTimeStamp,-1, ACK_TAG, data.sender_id);
         break;
    }
  }
  pthread_exit(NULL);
}

int main(int argc,char **argv){
   COMPANIES_COUNT = COMPANY_COUNT;
   if (argc > 1){
      COMPANIES_COUNT = (int) strtol(argv[1], (char **)NULL, 10);
   }
   int size, tid, provided;   //size - number of processes ; tid - current process id ; provided - to check mpi threading
   MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
   if (provided < MPI_THREAD_MULTIPLE){
      printf("ERROR: The MPI library does not have full thread support\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
   }
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   create_custom_message_type();
   MPI_Comm_rank( MPI_COMM_WORLD, &tid );

   struct Company *companies = (struct Company*)malloc(sizeof(struct Company)*COMPANIES_COUNT);
   init_companies(tid,size,COMPANIES_COUNT,companies);

   //  Create additional thread
   pthread_mutex_t *mutexCompany = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
   pthread_mutex_init(mutexCompany,NULL);
   pthread_attr_t *attr = (pthread_attr_t*)malloc(sizeof(pthread_attr_t));
   pthread_attr_init(attr);
   pthread_attr_setdetachstate(attr, PTHREAD_CREATE_JOINABLE);
   pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
   bool *threadIsAlive = (bool*)malloc(sizeof(bool));
   *threadIsAlive = true;
   pthread_cond_t *changeCondition = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
   pthread_cond_init(changeCondition,NULL);
   int *curr_time=(int*)malloc(sizeof(int)*1);
   *curr_time=0;
   bool* allReserved = (bool*)malloc(sizeof(bool));
   *allReserved = false;
   struct ThreadParams* param = createNewThread(tid,size,companies,threadIsAlive,mutexCompany,changeCondition, curr_time,allReserved);
   int err = pthread_create(thread, attr, additionalThread, (void *)param);
   if (err){
      fprintf(stderr,"ERROR Pthread_create: %d\n",err);
      MPI_Abort(MPI_COMM_WORLD, 1);
   }

   mainThread(threadIsAlive, curr_time, mutexCompany,changeCondition, tid, size, companies,allReserved);

   pthread_join(*thread, NULL);
   pthread_attr_destroy(attr);
   pthread_mutex_destroy(mutexCompany);
   pthread_cond_destroy(changeCondition);
   MPI_Type_free(&mpi_message_type);
   MPI_Finalize();
   free(thread);
   free(threadIsAlive);
   free(attr);
   free(changeCondition);
   free(mutexCompany);
   free(allReserved);
   free(curr_time);
   free(companies);
   pthread_exit(NULL);
}





void create_custom_message_type(){
   int blocklengths[5] = {1,1,1,1,1};
   MPI_Datatype types[5] = {MPI_INT,MPI_INT,MPI_INT,MPI_INT,MPI_INT};
   MPI_Aint offsets[5];
   offsets[0] = offsetof(struct Message, sender_id);
   offsets[1] = offsetof(struct Message, company_id);
   offsets[2] = offsetof(struct Message, info_type);
   offsets[3] = offsetof(struct Message, timestamp);
   offsets[4] = offsetof(struct Message, data);

   MPI_Type_create_struct(5, blocklengths, offsets, types, &mpi_message_type);
   MPI_Type_commit(&mpi_message_type);
}

void init_companies(int tid,int size,int companyCount,struct Company *companies){
  srand(time(NULL));
  int i,j;
  for (i = 0; i < companyCount; i++) {
    companies[i].queue = (int**)malloc(sizeof(int*)*size);
    for (j = 0; j < size; j++) {
      companies[i].queue[j] = (int*)malloc(sizeof(int)*2);
      companies[i].queue[j][0] = -1;   //Client ID
      companies[i].queue[j][1] = -1;   //Request time
    }
    companies[i].killers = -1;
    companies[i].reputation = -1;
    int randComp[2];
    if(tid == ROOT){
      randComp[0] = (rand()%(COMPANY_KILLERS_MAX - COMPANY_KILLERS_MIN) + COMPANY_KILLERS_MIN);
      randComp[1] = (rand()%(COMPANY_REPUTATION_MAX - COMPANY_REPUTATION_MIN) + COMPANY_REPUTATION_MIN);
      printf("Company %d\tkillers: %d\treputation: %d\n", i,randComp[0],randComp[1]);
    }
    MPI_Bcast(&randComp, 2, MPI_INT, ROOT, MPI_COMM_WORLD);
    companies[i].killers = randComp[0];
    companies[i].reputation = randComp[1];
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

struct ThreadParams* createNewThread(int tid,int size,struct Company* companies, bool *threadIsAlive, pthread_mutex_t *mutexCompany,pthread_cond_t *changeCondition, int *curr_time, bool* allReserved){
    struct ThreadParams* thread = (struct ThreadParams*)malloc(sizeof(struct ThreadParams));
    thread->tid = tid;
    thread->size = size;
    thread->companies = companies;
    thread->threadIsAlive = threadIsAlive;
    thread->changeCondition = changeCondition;
    thread->mutexCompany = mutexCompany;
    thread->curr_time=curr_time;
    thread->allReserved = allReserved;
    return thread;
}
void send_mpi_message(int sender_id, int company_id, int info_type, int timestamp, int data, int tag, int receiver){
   struct Message send_data;
   send_data.sender_id=sender_id;
   send_data.company_id=company_id;
   send_data.info_type=info_type;
   send_data.timestamp=timestamp;
   send_data.data=data;
   //puts("SENDING\n");
   MPI_Send(&send_data, 1, mpi_message_type, receiver, tag, MPI_COMM_WORLD);
}
struct Message reveive_mpi_message(int tag){
   struct Message data;
   MPI_Status status;
   //TODO HANDLE STATUS
   MPI_Recv(&data, 1, mpi_message_type, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
   //printf("RECEIVED: sender_id:%d company_id:%d info_type:%d time_stamp:%d data:%d\n", data.sender_id, data.company_id, data.info_type, data.timestamp, data.data);
   return data;
}
void remove_client_from_queue(struct Company *companies, int size, int sender_id, int company_id){
   int i;
   for(i=0; i<size; i++){
      if(companies[company_id].queue[i][0] == sender_id){
         companies[company_id].queue[i][0] = -1;
         companies[company_id].queue[i][1] = -1;
         while(i<size-1 && companies[company_id].queue[i+1][0] !=-1){
            companies[company_id].queue[i][0] = companies[company_id].queue[i+1][0];
            companies[company_id].queue[i][1] = companies[company_id].queue[i+1][1];
            companies[company_id].queue[i+1][0]=-1;
            companies[company_id].queue[i+1][1]=-1;
            i++;
         }
         break;
      }
   }
}
void add_client_to_queue(struct Company * companies, int size, int sender_id, int company_id, int timestamp){
   int i=0;
   for(i=0;i<size;i++) if(companies[company_id].queue[i][0] == sender_id) return;
   for(i=0;i<size;i++){
      if((companies[company_id].queue[i][0] == -1) || (timestamp <= companies[company_id].queue[i][1])){
         if((timestamp == companies[company_id].queue[i][1]) && (sender_id>companies[company_id].queue[i][0]))
            continue;

         int tmp1=companies[company_id].queue[i][0];
         int tmp2=companies[company_id].queue[i][1];
         companies[company_id].queue[i][0] = sender_id;
         companies[company_id].queue[i][1] = timestamp;

         while(i<size-1 && tmp1!=-1){
            int tmp3=companies[company_id].queue[i+1][0];
            int tmp4=companies[company_id].queue[i+1][1];
            companies[company_id].queue[i+1][0] = tmp1;
            companies[company_id].queue[i+1][1] = tmp2;
            tmp1 = tmp3;
            tmp2 = tmp4;
            i++;
         }
         break;
      }
   }
}
int select_best_company(struct Company* companies, int companies_size, int*requested, int requested_size){
   int i=0,k=0;
   int best_company=-1, best_reputation=-1;
   for(i=0;i<companies_size;i++){
      int is_requested=0;
      for(k=0;k<requested_size; k++)
         if(requested[k] == i)
            is_requested = 1;
      if(is_requested) continue;

      if(best_company==-1 || best_reputation<companies[i].reputation){
         best_company=i;
         best_reputation=companies[i].reputation;
      }
   }
   return best_company;
}
void update_company_reputation_request(struct Company * companies, int tid, int size, int company_id, int reputation_change, pthread_mutex_t *mutexCompany, int*curr_time){
   int i=0;
   pthread_mutex_lock(mutexCompany);
   (*curr_time)++;
   companies[company_id].reputation+=reputation_change;
   pthread_mutex_unlock(mutexCompany);
   for(i=0;i<size;i++){
      if(i!=tid){
         send_mpi_message(tid,company_id, CHANGE_REPUTATION_TYPE, (*curr_time), reputation_change, MESSAGE_TAG, i);
      }
   }
}
void critical_sention(int tid, int company, int *curr_time, int requested_time){
   printf("%d wchodzi do sekcji krytycznej firmy %d, time: %d, requested_time: %d\n",tid, company, *curr_time, requested_time);
   sleep(rand()%CRITICAL_SLEEP_MAX+CRITICAL_SLEEP_MIN);
   printf("%d wychodzi z sekcji krytycznej firmy %d, time: %d, requested_time: %d\n",tid, company, *curr_time, requested_time);
}
void free_request_to_company(int tid, int size, int company, struct Company * companies, pthread_mutex_t *mutexCompany, int*curr_time){
   int i=0;
   pthread_mutex_lock(mutexCompany);
   (*curr_time)++;
   remove_client_from_queue(companies, size, tid, company);
   pthread_mutex_unlock(mutexCompany);
   for(i=0;i<size;i++)
      if (i!=tid){
         send_mpi_message(tid,company,REMOVE_FROM_COMPANY_QUEUE,(*curr_time),-1,MESSAGE_TAG, i);
      }
}

int check_position(int tid, int size, int company, struct Company * companies){
   int i=0;
   for(i=0;i<size;i++){
      if(companies[company].queue[i][0]==tid) return i;
   }
   puts("WARNING! THIS SHOULD NOT HAPPEN!");
   return -1;
}
void request_company(int tid, int size, int company, struct Company * companies, pthread_mutex_t *mutexCompany, int*curr_time){
   int i=0;
   pthread_mutex_lock(mutexCompany);
   int CURR_TIME = ++(*curr_time);
   add_client_to_queue(companies, size, tid, company, CURR_TIME);
   pthread_mutex_unlock(mutexCompany);
   for(i=0;i<size;i++){
      if(i!=tid){
         send_mpi_message(tid,company, REQUEST_KILLER, CURR_TIME, -1, MESSAGE_TAG, i);
         struct Message data = reveive_mpi_message(ACK_TAG);
         if(DEBUG)printf("\tClient %d received ACK with %d timestamp from client %d, company: %d\n", tid, data.timestamp, data.sender_id, data.company_id);
         if(data.timestamp==-1 || data.timestamp>CURR_TIME) continue;
         if(data.timestamp == CURR_TIME && tid < data.sender_id) continue;
         //Na wypadek gdyby request do nas nie dotarł na podstawie odpowiedzi dodajemy klienta do naszej kolejki. Powtórna próba dodania nic nie zmieni.
         pthread_mutex_lock(mutexCompany);
         add_client_to_queue(companies, size, data.sender_id, company, data.timestamp);
         if(data.timestamp >= *curr_time) *curr_time=data.timestamp;
         pthread_mutex_unlock(mutexCompany);
      }
   }
}

