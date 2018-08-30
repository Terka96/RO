#ifndef MESSAGES_H
#define MESSAGES_H
#include"constants.hpp"

#define ORDER_MAKEKIDS 1
#define ACK_MAKEKIDS 2
#define INIT_RESOURES 3

/*zamówienie u konspiratora zrobienia dzieci w ilości count, przyjęcia postawy konspiratora i zapamiętania przełożonego i sąsiadów*/
struct order_makekids{
    int parent;
    int count;
    int neighbors[MAX_CHILDREN];
};

/*potwierdzenie ukończenia tworzenia dzieci*/
struct ack_makekids {
    int count;
};

/*inicjalizacja/dystrybucja roli akceptora i zasobów*/
struct init_resources{
    int acceptorTokenId;
    int resourceCount;
    int resourceIds[NUM_RESOURCES];
};

#endif // MESSAGES_H
