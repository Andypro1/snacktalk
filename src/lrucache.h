#ifndef __LRUCACHE_H_
#define __LRUCACHE_H_

 // A Queue Node (Queue is implemented using Doubly Linked List)
 typedef struct QNode
 {
     struct QNode *prev, *next;

         //  Key (and data)
             unsigned pageNumber;  // the page number stored in this QNode

                 //  Data
	char pairNumber;
                     bool isFlipped;
                     } QNode;

                     // A Queue (A FIFO collection of Queue Nodes)
                     typedef struct Queue
                     {
                         unsigned count;  // Number of filled frames
                             unsigned numberOfFrames; // total number of frames
                                 QNode *front, *rear;
                                 } Queue;

                                 // A hash (Collection of pointers to Queue Nodes)
                                 typedef struct Hash
                                 {
                                     int capacity; // how many pages can be there
                                         QNode* *array; // an array of queue nodes
                                         } Hash;


extern QNode* newQNode(unsigned pageNumber, char pairNumber, bool isFlipped);
extern Queue* createQueue(int numberOfFrames);
extern Hash* createHash(int capacity);
extern int AreAllFramesFull(Queue* queue);
extern int isQueueEmpty(Queue* queue);
extern char deQueue(Queue* queue);
extern void Enqueue(Queue* queue, Hash* hash, unsigned pageNumber, bool isFlipped);
extern QNode* ReferencePage(Queue* queue, Hash* hash, unsigned pageNumber, bool isFlipped);
extern unsigned short CombineHashFromPair(char A, char B);
extern char GetFirstFromCombineHash(unsigned short C);
extern char GetSecondFromCombineHash(unsigned short C);

#endif
