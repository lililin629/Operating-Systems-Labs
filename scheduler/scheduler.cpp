#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <list>
#include <deque>
#include <queue>
#include <algorithm>
#include <vector>
// main implementation + DES layer

using namespace std;

int maxprio = 4;
int v, t, e, p;
int *randvals;
int ofs;
int rand_num;

// read rfile --> store random numbers in randNums vector
vector<int> randNums;
int randCount = 0;

void readRandFile(string rfile)
{
    double r;
    ifstream in_rfile;
    in_rfile.open(rfile);
    while (in_rfile >> r)
    {
        randNums.push_back(r);
    }
    in_rfile.close();
}

int myrandom(int burst)
{
    if (randCount == randNums.size())
        randCount = 0;
    return 1 + (randNums[++randCount] % burst);
}

// process
int current_time;
int last_cpu_use; // timestamp of the latest process finishing using CPU
bool CALL_SCHEDULER = false;
int total_io_time; // not neccesarily the sum of each processes io time because 1+ processes can be in IO simultaneously
int last_io_use;   // timestamp of the latest process finishing IO

typedef enum
{
    TRANS_TO_READY,
    TRANS_TO_PREEMPT,
    TRANS_TO_RUN,
    TRANS_TO_BLOCK
} Transition;
class Process
{
public:
    int pid;
    int arr_time;
    int total_cpu;
    int cpu_brust;
    int io_brust;
    int static_priority;
    int dynamic_priority;
    int remain_time;
    int curr_cpu_brust;
    int curr_io_brust;
    int finish_time; // prior to finish: total time spent in the system so far
    int total_io_time;
    int cpu_wait_time;
    Transition transition;
};

class Event
{
public:
    Process *evtProcess = new Process();
};

list<Process *> finish_list;

bool comp(const Event *a, const Event *b)
{
    return a->evtProcess->finish_time < b->evtProcess->finish_time;
}

class DES
{
public:
    deque<Event *> eventQ;
    Event *get_from_eventQ()
    {
        Event *temp = eventQ.front();
        eventQ.pop_front();
        return temp;
    };
    void add_to_eventQ(Event *evt)
    {
        if (eventQ.size() == 0)
        {
            eventQ.push_back(evt);
        }
        else
        {
            deque<Event *>::iterator it;
            it = upper_bound(eventQ.begin(), eventQ.end(), evt, comp);
            eventQ.insert(it, evt);
        }
    };
};
DES des;

// read input file --> store process info in Procs vector
void readInputFile(string infile)
{
    int id = 0;
    ifstream file;
    string input;
    file.open(infile);
    while (file >> input)
    {
        Event *evt = new Event();
        evt->evtProcess->pid = id;
        evt->evtProcess->static_priority = myrandom(maxprio);
        evt->evtProcess->dynamic_priority = evt->evtProcess->static_priority - 1;
        evt->evtProcess->arr_time = stoi(input);
        evt->evtProcess->finish_time = stoi(input);
        file >> input;
        evt->evtProcess->total_cpu = stoi(input);
        evt->evtProcess->remain_time = stoi(input);
        file >> input;
        evt->evtProcess->cpu_brust = stoi(input);
        file >> input;
        evt->evtProcess->io_brust = stoi(input);
        // put processes arrival into event queue
        des.add_to_eventQ(evt);
        id++;
    }
    file.close();
};

class Scheduler
{
public:
    // TODO:
    bool is_E = false;
    int quantum;
    deque<Process *> ready_queue;
    deque<Process *> expired_queue;
    virtual Process *get_from_q()
    {
        Process *temp = ready_queue.front();
        ready_queue.pop_front();
        return temp;
    };
    virtual void add_to_q(Process *proc){};
    virtual Process *peek_next_proc(){};
};

bool comp_F(const Process *a, const Process *b)
{
    return a->finish_time < b->finish_time;
}

class Scheduler_F : public Scheduler
{
public:
    virtual void add_to_q(Process *proc)
    {
        if (ready_queue.size() == 0)
        {
            ready_queue.push_back(proc);
        }
        else
        {
            deque<Process *>::iterator it;
            it = upper_bound(ready_queue.begin(), ready_queue.end(), proc, comp_F);
            // it = the position of the element(b) that makes compF(proc, b) true
            ready_queue.insert(it, proc);
        }
    }
};

bool comp_L(const Process *a, const Process *b)
{
    return a->finish_time < b->finish_time;
}

class Scheduler_L : public Scheduler
{
public:
    Process *get_from_q()
    {
        int idx = -1;
        int count = 0;
        Process *temp = ready_queue.front();
        for (auto i : ready_queue)
        {
            if (i->finish_time > last_cpu_use)
            {
                if (idx == -1)
                {
                    idx = 0;
                }
                break;
            }
            if (temp->finish_time != i->finish_time)
            {
                temp = i;
                count = 0;
            }
            else if (idx != -1)
            {
                count++;
            }
            idx++;
        }
        ready_queue.erase(ready_queue.begin() + idx - count);
        return temp;
    }
    void add_to_q(Process *proc)
    {
        if (ready_queue.size() == 0)
        {
            ready_queue.push_back(proc);
        }
        else
        {
            deque<Process *>::iterator it;
            it = lower_bound(ready_queue.begin(), ready_queue.end(), proc, comp_L);
            ready_queue.insert(it, proc);
        }
    }
};

bool comp_S(const Process *a, const Process *b)
{
    return a->remain_time < b->remain_time;
}

class Scheduler_S : public Scheduler
{
public:
    Process *get_from_q()
    {
        Process *temp = ready_queue.front();
        int idx = 0;
        for (auto i : ready_queue)
        {
            if (i->finish_time <= last_cpu_use)
            {
                temp = i;
                ready_queue.erase(ready_queue.begin() + idx);
                return i;
                break;
            }
            idx++;
        }
        int min_finish_time = temp->finish_time;
        int erase_idx = 0;
        idx = 0;
        for (auto i : ready_queue)
        {
            if (i->finish_time < min_finish_time)
            {
                min_finish_time = i->finish_time;
                temp = i;
                erase_idx = idx;
            }
            idx++;
        }
        ready_queue.erase(ready_queue.begin() + erase_idx);
        return temp;
    }

    void add_to_q(Process *proc)
    {
        if (ready_queue.size() == 0)
        {
            ready_queue.push_back(proc);
        }
        else
        {
            deque<Process *>::iterator it;
            it = upper_bound(ready_queue.begin(), ready_queue.end(), proc, comp_S);
            ready_queue.insert(it, proc);
        }
    }
};

class Scheduler_R : public Scheduler_F
{
};

bool comp_P(const Process *a, const Process *b)
{
    if (a->dynamic_priority == b->dynamic_priority)
    {
        return a->finish_time < b->finish_time;
    }
    return a->dynamic_priority > b->dynamic_priority;
}

class Scheduler_P : public Scheduler
{
public:
    void add_to_q(Process *proc)
    {
        if (proc->dynamic_priority == -1)
        {
            proc->dynamic_priority = proc->static_priority - 1;
            deque<Process *>::iterator it;
            it = upper_bound(expired_queue.begin(), expired_queue.end(), proc, comp_P);
            expired_queue.insert(it, proc);
        }

        else
        {
            deque<Process *>::iterator it;
            it = upper_bound(ready_queue.begin(), ready_queue.end(), proc, comp_P);
            ready_queue.insert(it, proc);
        }
    }

    Process *get_from_q()
    {
        // expiredQ:processes that have exceeded their allotted CPU time, waiting for the next round
        if (ready_queue.size() == 0)
        {
            ready_queue.swap(expired_queue);
        }
        Process *temp = ready_queue.front();
        int highest_priority = temp->dynamic_priority; // when added to queue, procs with higher prios (and smaller finish time) are added to the front
        int idx = 0;
        int erase_idx = 0;
        for (auto i : ready_queue)
        {
            if (i->dynamic_priority == highest_priority)
            {
                if (i->finish_time <= last_cpu_use)
                {
                    temp = i;
                    erase_idx = idx;
                    ready_queue.erase(ready_queue.begin() + erase_idx);
                    return temp;
                }
            }
            else
            {
                highest_priority = i->dynamic_priority;
                if (i->finish_time <= last_cpu_use)
                {
                    temp = i;
                    erase_idx = idx;
                    ready_queue.erase(ready_queue.begin() + erase_idx);
                    return temp;
                }
            }
            idx++;
        }

        // no procs in readayQ arrives before last CPU use -> swap expiredQ to readyQ
        for (auto i : expired_queue)
        {
            add_to_q(i);
        }
        expired_queue.clear();

        temp = ready_queue.front();
        highest_priority = temp->dynamic_priority;
        idx = 0;
        erase_idx = 0;
        for (auto i : ready_queue)
        {
            if (i->dynamic_priority == highest_priority)
            {
                if (i->finish_time <= last_cpu_use)
                {
                    temp = i;
                    erase_idx = idx;
                    ready_queue.erase(ready_queue.begin() + erase_idx);
                    return temp;
                }
            }
            else
            {
                highest_priority = i->dynamic_priority;
                if (i->finish_time <= last_cpu_use)
                {
                    temp = i;
                    erase_idx = idx;
                    ready_queue.erase(ready_queue.begin() + erase_idx);
                    return temp;
                }
            }
            idx++;
        }

        int min_finish_time = temp->finish_time;
        erase_idx = 0;
        idx = 0;
        for (auto i : ready_queue)
        {
            if (i->finish_time < min_finish_time)
            {
                min_finish_time = i->finish_time;
                temp = i;
                erase_idx = idx;
            }
            idx++;
        }
        ready_queue.erase(ready_queue.begin() + erase_idx);
        return temp;
    }
};

class Scheduler_E : public Scheduler_P
{
public:
    bool is_E = true;
    Process *peek_next_proc()
    {
        if (ready_queue.empty())
            return NULL;
        return ready_queue.front();
    }
};

void Simulation(Scheduler *sched)
{
    while (des.eventQ.size() != 0)
    {
        Event *evt = des.get_from_eventQ();
        Process *proc = evt->evtProcess;
        current_time = proc->finish_time;
        if (current_time < last_cpu_use)
        {
            proc->cpu_wait_time += last_cpu_use - current_time;
            proc->finish_time = last_cpu_use;
        }

        int transition = proc->transition;
        delete evt;
        evt = nullptr;
        switch (transition)
        {
        case TRANS_TO_READY:
        {
            sched->add_to_q(proc);
            CALL_SCHEDULER = true;
            break;
        }
        case TRANS_TO_BLOCK:
        {
            sched->add_to_q(proc);
            CALL_SCHEDULER = true;
            break;
        }
        }
        while (CALL_SCHEDULER)
        {
            if (des.eventQ.size() != 0 && (des.eventQ.front()->evtProcess->finish_time == current_time || des.eventQ.front()->evtProcess->finish_time <= last_cpu_use))
            {
                break;
            }
            CALL_SCHEDULER = false;
            if (sched->ready_queue.size() != 0 || sched->expired_queue.size() != 0)
            {
                Process *running_process = sched->get_from_q();
                if (running_process->curr_cpu_brust == 0)
                {
                    running_process->curr_cpu_brust = myrandom(running_process->cpu_brust);
                }
                if (running_process->curr_cpu_brust <= sched->quantum) // cpu burst will be completed w/o preemption
                {
                    // TODO:
                    if (sched->is_E && (running_process != NULL))
                    {
                        Process *next_proc = sched->peek_next_proc();
                        if ((next_proc != NULL) && (next_proc->dynamic_priority > running_process->dynamic_priority))
                        {
                            // if (check_do_preempt(event_list, cur_run_proc, CURRENT_TIME))
                            // {
                            Event *evt = new Event();
                            evt->evtProcess = next_proc;
                            des.add_to_eventQ(evt);

                            // }
                        }
                    }
                    // TODO:
                    else if (running_process->remain_time <= running_process->curr_cpu_brust) // process will complete in this cpu burst
                    {
                        if (running_process->finish_time < last_cpu_use)
                        {
                            running_process->cpu_wait_time += last_cpu_use - running_process->finish_time;
                            running_process->finish_time = last_cpu_use;
                        }
                        running_process->finish_time += running_process->remain_time;
                        last_cpu_use = running_process->finish_time;
                        finish_list.push_back(running_process);
                        CALL_SCHEDULER = true;
                    }
                    else // process will not complete in this cpu burst, trans to block(IO)
                    {
                        // check if running proc needs to wait for cpu
                        if (running_process->finish_time < last_cpu_use)
                        {
                            running_process->cpu_wait_time += last_cpu_use - running_process->finish_time;
                            running_process->finish_time = last_cpu_use;
                        }
                        // update remain time, finish time, last cpu use after running proc completes cur cpu burst
                        running_process->remain_time -= running_process->curr_cpu_brust;
                        running_process->finish_time += running_process->curr_cpu_brust;
                        last_cpu_use = running_process->finish_time;
                        // enters into io burst(trans to block)
                        running_process->curr_io_brust = myrandom(running_process->io_brust);
                        running_process->total_io_time += running_process->curr_io_brust;
                        running_process->finish_time += running_process->curr_io_brust;

                        if (last_cpu_use > last_io_use) // running proc enters IO (finishes cpu burst) after the last process finishes IO, no overlap in IO
                        {
                            total_io_time += running_process->curr_io_brust;
                            last_io_use = running_process->finish_time;
                        }
                        else // at least 2 processes in IO
                        {
                            if (running_process->finish_time > last_io_use) // running proc stays in IO longer
                            {
                                total_io_time += running_process->finish_time - last_io_use;
                                last_io_use = running_process->finish_time;
                            }
                        }
                        running_process->curr_cpu_brust = 0;
                        running_process->transition = TRANS_TO_BLOCK;
                        running_process->dynamic_priority = running_process->static_priority - 1;
                        Event *evt = new Event();
                        evt->evtProcess = running_process;
                        des.add_to_eventQ(evt);
                    }
                }
                else //  cur cpu burst > quantum => preempted
                {
                    if (running_process->remain_time <= sched->quantum)
                    {
                        if (running_process->finish_time < last_cpu_use) // in ready queue wating for cpu to free up
                        {
                            running_process->cpu_wait_time += last_cpu_use - running_process->finish_time;
                            running_process->finish_time = last_cpu_use;
                        }
                        running_process->finish_time += running_process->remain_time;
                        last_cpu_use = running_process->finish_time;
                        finish_list.push_back(running_process);
                        CALL_SCHEDULER = true;
                    }
                    else // trans to preempt
                    {
                        running_process->remain_time -= sched->quantum;
                        running_process->curr_cpu_brust -= sched->quantum;
                        if (running_process->finish_time < last_cpu_use)
                        {
                            running_process->cpu_wait_time += last_cpu_use - running_process->finish_time;
                            running_process->finish_time = last_cpu_use;
                        }
                        last_cpu_use = running_process->finish_time + sched->quantum;
                        running_process->finish_time += sched->quantum;
                        running_process->dynamic_priority -= 1;
                        running_process->transition = TRANS_TO_READY;
                        Event *evt = new Event();
                        evt->evtProcess = running_process;
                        des.add_to_eventQ(evt);
                    }
                }
            }
        }
    }
}

bool comp_id(const Process *a, const Process *b)
{
    return a->pid < b->pid;
}

int main(int argc, char *argv[])
{
    // Get Commandline arguments
    int c;
    char sch;
    int quantum = 10000;
    int index;
    string rfile;
    string infile;
    string sched_name;
    Scheduler *scheduler;

    while ((c = getopt(argc, argv, "vtepis:::")) != -1)
    {
        switch (c)
        {
        case 'v':
            break;
        case 't':
            break;
        case 'e':
            break;
        case 'p':
            break;
        case 'i':
            break;
        case 's':
            sscanf(optarg, "%c%d:%d", &sch, &quantum, &maxprio);
            break;
        case '?':
            return 1;
        default:
            abort();
        }
    }

    index = optind;
    infile = argv[index];

    index++;
    rfile = argv[index];

    // read rfile
    readRandFile(rfile);

    // read input file
    readInputFile(infile);

    // select scheduler type
    Scheduler *sched = new (Scheduler);
    switch (sch)
    {
    case 'F':
    {
        printf("FCFS\n");
        sched = new (Scheduler_F);
        sched->quantum = quantum;
        break;
    }
    case 'L':
    {
        printf("LCFS\n");
        sched = new (Scheduler_L);
        sched->quantum = quantum;
        break;
    }
    case 'S':
    {
        printf("SRTF\n");
        sched = new (Scheduler_S);
        sched->quantum = quantum;
        break;
    }
    case 'R':
    {
        printf("RR %d\n", quantum);
        sched = new (Scheduler_R);
        sched->quantum = quantum;
        break;
    }
    case 'P':
    {
        printf("PRIO %d\n", quantum);
        sched = new (Scheduler_P);
        sched->quantum = quantum;
        break;
    }
    case 'E':
    {
        printf("PREPRIO %d\n", quantum);
        sched = new (Scheduler_E);
        sched->quantum = quantum;
        break;
    }
    default:
    {
        return 0;
    }
    }
    // simulation
    Simulation(sched);
    // output
    int finish_time = 0;
    int num = finish_list.size();
    int total_cpu_time = 0;
    int total_around_time = 0;
    int total_cpu_wait = 0;

    for (auto it : finish_list)
    {
        finish_time = it->finish_time;
        total_cpu_time += it->total_cpu;
        total_around_time += it->finish_time - it->arr_time;
        total_cpu_wait += it->cpu_wait_time;
    }
    finish_list.sort(comp_id);
    for (auto it : finish_list)
    {
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", it->pid, it->arr_time, it->total_cpu, it->cpu_brust, it->io_brust, it->static_priority, it->finish_time, it->finish_time - it->arr_time, it->total_io_time, it->cpu_wait_time);
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", finish_time, double(total_cpu_time) / double(finish_time) * 100, double(total_io_time) / double(finish_time) * 100, double(total_around_time) / double(num), double(total_cpu_wait) / double(num), double(num) / double(finish_time) * 100);
    return 0;
}
