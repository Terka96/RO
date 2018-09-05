#ifndef constants_hpp
#define constants_hpp

#define MAX_CHILDREN 4
#define MAX_BUFFER_SIZE 64

#define NUM_RESOURCES 6
#define NUM_ACCEPTORS 4
#define NUM_CONSPIR 16

/*value used to indicate null ids*/
#define NONE -1

struct msgBcastInfo{
    int uniqueTag;
    int respondTo;
    int waitingForResponse;
    int msgSize;
    int buffer[MAX_BUFFER_SIZE];
    bool operator ==(const msgBcastInfo& x) {
        return x.uniqueTag==uniqueTag;
    }
};


#endif
