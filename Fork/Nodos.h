#ifndef NODOS_H
#define NODOS_H

typedef struct Node {
    struct Node *next;
    struct Node *left;
    struct Node *right;
    unsigned char symbol;
    int count;
}Node;
void insertInOrder(Node **head, Node *element);  // 👈 Prototipo
//void insertSymbol(Node *List, Node *element, Node *head, Node *aux);
/*
    insertSymbol: Inserta un nuevo elemento en la lista
    Entradas:
        -list = Node* : Es la lista donde se va a insertar el nuevo elemento
        -element = Node* : Elemento a insertar
        -head = Node*: Es la cabeza de la lista
        -aux = Node* : Es el siguiente elemento en la lista
    Salidas:
        void
*/
static inline void insertSymbol(Node *list, Node *element, Node *head, Node *aux){    
    element->next = aux;
    if(!list) head = element;
    else list->next = element;
}
/*
    inserNewSymbol: Inserta un nuevo simbolo en la lista
    Entradas:
        -actual = Node*: Es el nodo actual donde se va a insertar el nuevo simbolo
        -next = Node*: Es el siguiente nodo en la lista
        -list = Node*: Es la lista donde se va a insertar el nuevo simbolo
        -c = unsigned char : Es el simbolo a insertar
    Salidas:
        void
*/
static inline void insertNewSymbol(Node *actual, Node *next, Node *list, unsigned char c){
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->symbol = c;
    newNode->left = newNode->right = NULL;
    newNode-> count = 1;
    insertSymbol(actual, newNode, list, next);

    
}
// void sortList(Node **head);
static inline void sortList(Node **head){
    Node *listAux;
    Node *aux;
    listAux = *head;
    *head = NULL;
    
    while(listAux){
        aux = listAux;
        listAux = aux->next;
        
        insertInOrder(head, aux);
    }
}
// void insertInOrder(Node **head, Node *element);
/*
    inserInOrder: Función que inserta un elemento en una lista ordenada
    Entradas:
        -head = Node** : La cabeza de la lista
        -element = Node*: El elemento a insertar en la lista
    Salidas:
        - void
*/
void insertInOrder(Node **head, Node *element){
    Node *aux = NULL;
    Node *auxNext = NULL;

    if(!*head){
        *head = element;
        (*head)->next = NULL;
    }else{
        aux = *head;
        auxNext = NULL;
        while(aux && aux->count < element->count){
            auxNext = aux;
            aux = aux->next;
            
        }
        element->next = aux;
        if(auxNext) auxNext->next = element;
        else *head = element;
    }
}
// void freeNode(Node *head);
/*
    freeNode: Función que libera la memoria de la lista
    Entradas:
        -head = Node* : Es la cabeza de la lista
    Salidas:
        -void 
    
*/
static inline void freeNode(Node *head){
    if(head->left) freeNode(head->left);
    if(head->right) freeNode(head->right);
    free(head);
}
// void printNode(Node **head);
/*
    printNode: Función que imprime la lista
    Entradas:
        -head = Node* : Es la cabeza de la lista
    Salidas:
        -void
*/
static inline void printNode(Node **head){
    Node *aux;
    aux = *head;
    while(aux){
        printf("Simbolo: %c cuenta: %i\n", aux->symbol, aux->count);
        if(aux->left)
            printf("izquierda Simbolo: %c cuenta: %i\n", aux->left->symbol, aux->left->count);
        if(aux->right)
            printf("derecha Simbolo: %c cuenta: %i\n", aux->right->symbol, aux->right->count);
        aux = aux->next;
    }
}

#endif
