#ifndef MESSAGES_H
#define MESSAGES_H
#include"constants.hpp"

#define ORDER_MAKEKIDS 1
#define ACK_MAKEKIDS 2

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

#endif // MESSAGES_H
