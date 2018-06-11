#include <pthread.h>

//Const to companies generator
#define COMPANY_COUNT 4
#define COMPANY_KILLERS_MIN 2
#define COMPANY_KILLERS_MAX 6
#define COMPANY_REPUTATION_MIN 10
#define COMPANY_REPUTATION_MAX 30

//Const for communication
#define SERVICE_RATING_MAX 5
#define ACK_TAG 11
#define MESSAGE_TAG 22

#define CHANGE_REPUTATION_TYPE 100
#define REMOVE_FROM_COMPANY_QUEUE 200
#define REQUEST_KILLER 300
#define ACK_TYPE 400

#define SLEEP_RAND_MAX 8
#define REPUTATION_CHANGE_MIN_MAX 5
#define CRITICAL_SLEEP_MAX 10
#define CRITICAL_SLEEP_MIN 10

struct Message {
  int sender_id;
  int company_id;
  int info_type;
  int timestamp;
  int data;
};

struct Company{
  int **queue;
  int killers;
  int reputation;
};

struct ThreadParams{
  pthread_mutex_t *mutexCompany;
  pthread_cond_t *changeCondition;
  struct Company *companies;
   int tid;
   int size;
   bool* threadIsAlive;
  int* curr_time;
  bool *allReserved;
};
