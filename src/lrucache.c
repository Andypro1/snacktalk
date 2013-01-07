// A C program to show implementation of LRU cache
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lrucache.h"

// A utility function to create a new Queue Node. The queue Node
// will store the given 'pageNumber'
QNode* newQNode( unsigned pageNumber, unsigned char pairNumber, bool isFlipped)
{
    // Allocate memory and assign 'pageNumber'
    QNode* temp = (QNode *)malloc( sizeof( QNode ) );
    temp->pageNumber = pageNumber;

    //  Add data
    temp->pairNumber = pairNumber;
    temp->isFlipped  = isFlipped;

    // Initialize prev and next as NULL
    temp->prev = temp->next = NULL;

    return temp;
}

// A utility function to create an empty Queue.
// The queue can have at most 'numberOfFrames' nodes
Queue* createQueue( int numberOfFrames )
{
    Queue* queue = (Queue *)malloc( sizeof( Queue ) );

    // The queue is empty
    queue->count = 0;
    queue->front = queue->rear = NULL;

    // Number of frames that can be stored in memory
    queue->numberOfFrames = numberOfFrames;

    return queue;
}

// A utility function to create an empty Hash of given capacity
Hash* createHash( int capacity )
{
    // Allocate memory for hash
    Hash* hash = (Hash *) malloc( sizeof( Hash ) );
    hash->capacity = capacity;

    // Create an array of pointers for refering queue nodes
    hash->array = (QNode **) malloc( hash->capacity * sizeof( QNode* ) );

    // Initialize all hash entries as empty
    int i;
    for( i = 0; i < hash->capacity; ++i )
        hash->array[i] = NULL;

    return hash;
}

// A function to check if there is slot available in memory
int AreAllFramesFull( Queue* queue )
{
    return queue->count == queue->numberOfFrames;
}

// A utility function to check if queue is empty
int isQueueEmpty( Queue* queue )
{
    return queue->rear == NULL;
}

// A utility function to delete a frame from queue
//  The pair number will be returned so the next init_pair()
//  call knows which pair to overwrite
unsigned char deQueue( Queue* queue )
{
    unsigned char pairNum;

    if( isQueueEmpty( queue ) )
        return;

    // If this is the only node in list, then change front
    if (queue->front == queue->rear)
        queue->front = NULL;

    // Change rear and remove the previous rear
    QNode* temp = queue->rear;
    queue->rear = queue->rear->prev;

    if (queue->rear)
        queue->rear->next = NULL;

    pairNum = temp->pairNumber;  //  Save for return
    free( temp );

    // decrement the number of full frames by 1
    queue->count--;

    return pairNum;
}

// A function to add a page with given 'pageNumber' to both queue
// and hash
void Enqueue( Queue* queue, Hash* hash, unsigned pageNumber, bool isFlipped)
{
    unsigned char newPairNum;  //  pair number assigned internally for this new node
    short newFg, newBg;

    // If all frames are full, remove the page at the rear
    if ( AreAllFramesFull ( queue ) )
    {
        // remove page from hash
        hash->array[ queue->rear->pageNumber ] = NULL;
        newPairNum = deQueue( queue );
    }
    else {
        newPairNum = queue->count + 1;
    }

    newFg = isFlipped ? GetSecondFromCombineHash(pageNumber) : GetFirstFromCombineHash(pageNumber);
    newBg = isFlipped ? GetFirstFromCombineHash(pageNumber) : GetSecondFromCombineHash(pageNumber);

    //  Run curses init_pair() with new color info
    init_pair(newPairNum, newFg, newBg);

    // Create a new node with given page number,
    // And add the new node to the front of queue
    QNode* temp = newQNode( pageNumber, newPairNum, isFlipped );
    temp->next = queue->front;

    // If queue is empty, change both front and rear pointers
    if ( isQueueEmpty( queue ) )
        queue->rear = queue->front = temp;
    else  // Else change the front
    {
        queue->front->prev = temp;
        queue->front = temp;
    }

    // Add page entry to hash also
    hash->array[ pageNumber ] = temp;

    // increment number of full frames
    queue->count++;
}

// This function is called when a page with given 'pageNumber' is referenced
// from cache (or memory). There are two cases:
// 1. Frame is not there in memory, we bring it in memory and add to the front
//    of queue
// 2. Frame is there in memory, we move the frame to front of queue
QNode* ReferencePage( Queue* queue, Hash* hash, unsigned pageNumber, bool isFlipped)
{
    QNode* reqPage = hash->array[ pageNumber ];

    // the page is not in cache, bring it
    if ( reqPage == NULL )
        Enqueue( queue, hash, pageNumber, isFlipped );

    // page is there and not at front, change pointer
    else if (reqPage != queue->front)
    {
        // Unlink rquested page from its current location
        // in queue.
        if (reqPage->prev)
           reqPage->prev->next = reqPage->next;
        if (reqPage->next)
           reqPage->next->prev = reqPage->prev;

        // If the requested page is rear, then change rear
        // as this node will be moved to front
        if (reqPage == queue->rear)
        {
           queue->rear = reqPage->prev;
           queue->rear->next = NULL;
        }

        // Put the requested page before current front
        reqPage->next = queue->front;
        reqPage->prev = NULL;

        // Change prev of current front
        reqPage->next->prev = reqPage;

        // Change front to the requested page
        queue->front = reqPage;
    }

    return queue->front;  //  Return the accessed element
}

//  Combine hash a combinatorial pair (without regard to order)
unsigned int CombineHashFromPair(short A, short B) {
	short temp;

	//  Sort the pair, biggest first
	if(A < B) {
		temp = A;
		A = B;
		B = temp;
	}

	return ((A+1)*258)+(B+1);
}

short GetFirstFromCombineHash(unsigned int C) {
	return (C/258)-1;
}

short GetSecondFromCombineHash(unsigned int C) {
	return (C%258)-1;
}
