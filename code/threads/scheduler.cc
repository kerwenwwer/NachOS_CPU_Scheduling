// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"



// ---------------------------------------------------------------------
//  Scheduling algorithm
//  Three algroithm SJF, Priority and FIFO
//
// ---------------------------------------------------------------------
int SJFCompare(Thread *a, Thread *b) {
    if((a->getApproxBurstTime() - a->getBurstTime()) == (b->getApproxBurstTime() - b->getBurstTime()))
        return 0;
    return (a->getApproxBurstTime() - a->getBurstTime()) > (b->getApproxBurstTime() - b->getBurstTime()) ? 1 : -1;
}

int PriorityCompare(Thread *a, Thread *b) {
    if(a->getPriority() == b->getPriority())
        return 0;
    return a->getPriority() > b->getPriority() ? 1 : -1;
}

int FIFOCompare(Thread *a, Thread *b) {
    return 1;
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{
    L1 = new SortedList<Thread *> (SJFCompare);
    L2 = new SortedList<Thread *> (PriorityCompare);
    L3 = new List<Thread *>;
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
    delete L1;
    delete L2;
    delete L3;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    //cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    
    int priority = thread->getPriority();

    if(priority >= 100 && priority <= 149) {
        L1->Insert(thread);
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[1]");
    } else if (priority >= 50 && priority <= 99) {
        L2->Insert(thread);
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[2]");
    } else if (priority >= 0 && priority <= 49) {
        L3->Append(thread);
        DEBUG(dbgSchedule, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[3]");
    } else {
        cout << "Pritory is not in any ranges. ERROR!!!\n";
        ASSERTNOTREACHED();
    }

}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Thread *thread = NULL;

    if(!L1->IsEmpty()){
        thread = L1->RemoveFront();
        DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is removed from queue L[1]");
    }else if (!L2->IsEmpty()){
        thread = L2->RemoveFront();
        DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is removed from queue L[2]");
    }else if (!L3->IsEmpty()){
        thread = L3->RemoveFront();
        DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is removed from queue L[3]");
    }

    return thread;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing)
    { // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL)
    {                               // if this thread is a user program,
        oldThread->SaveUserState(); // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow(); // check if the old thread
                                // had an undetected stack overflow

    kernel->currentThread = nextThread; // switch to the next thread
    nextThread->setStatus(RUNNING);     // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // setStartTime to the next thread before context switch
    nextThread->setStartTime();

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    DEBUG(dbgSchedule, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() 
                        << "] is now selected for execution, thread [" << oldThread->getID() 
                        << "] is replaced, and it has executed [" << oldThread->getBurstTime() << "] ticks");
    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed(); // check if thread we were running
                          // before this one has finished
                          // and needs to be cleaned up

    if (oldThread->space != NULL)
    {                                  // if there is an address space
        oldThread->RestoreUserState(); // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL)
    {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print()
{
    cout << "Ready list contents:\n";

   L1->Apply(ThreadPrint);
   L2->Apply(ThreadPrint);
   L3->Apply(ThreadPrint);
}


//----------------------------------------------------------------------
// Scheduler::checkPreemptive()
//----------------------------------------------------------------------

bool Scheduler::checkPreemptive(){
    if(!L1->IsEmpty()){
        Thread *first = L1->RemoveFront();
        L1->Insert(first);

        Thread *now = kernel->currentThread;
        if (first->getApproxBurstTime() < now->getApproxBurstTime()) {
            return true;
        }
    }
    return false;
}


//----------------------------------------------------------------------
// Scheduler::aging
// Handling when aging happened, we need to relocate thread position 
// in queues.
//----------------------------------------------------------------------

void Scheduler::aging() {
    SortedList<Thread *> *newL1 = new SortedList<Thread *> (SJFCompare);
    SortedList<Thread *> *newL2 = new SortedList<Thread *> (PriorityCompare);
    List<Thread *> *newL3 = new List<Thread *>;

    while (!L1->IsEmpty()){
        Thread *t  = L1->RemoveFront();
        t->aging();
        newL1->Insert(t);
    }

    while (!L2->IsEmpty()){
        Thread *t  = L2->RemoveFront();
        t->aging();
        if( t->getPriority() >= 100) {
            newL1->Insert(t);
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is removed from queue L[2]");
        } else {
            newL2->Insert(t);
        }
    }

    while (!L3->IsEmpty()){
        Thread *t  = L3->RemoveFront();
        t->aging();
        if( t->getPriority() >= 50) {
            newL2->Insert(t);
            DEBUG(dbgSchedule, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << t->getID() << "] is removed from queue L[3]");
        } else {
            newL3->Append(t);
        }
    }

    delete L1;
    delete L2;
    delete L3;
    L1 = newL1;
    L2 = newL2;
    L3 = newL3;
}