#include <iostream>
#include <vector>
#include <string>
#include <string.h>
#include <fstream>
#include <sstream>
#include <ctype.h>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <deque>
#include <queue>
#include <limits>

using namespace std;
int cur_time = 0;
int head = 0;
int tot_movement = 0;

typedef struct
{
    int op_id;
    int arr_time;
    int track;
    int start_time;
    int end_time;
} io_operation;

deque<io_operation *> input_list;
deque<io_operation *> io_queue;
deque<io_operation *> completed_list;

ifstream file;
stringstream line;
bool getNextLine()
{
    string s;
    while (getline(file, s))
    { // ignore comment lines
        if (s[0] == '#')
        {
            continue;
        }
        else
        {
            // cout << "s: " << s << '\n';
            line.clear();
            line.str(s);
            return true;
        }
    }
    return false;
}
void readInputFile(string infile)
{
    file.open(infile);
    int oid = 0;

    while (getNextLine())
    {
        int arr_time, track;
        line >> arr_time >> track;
        io_operation *io_op = new io_operation;
        io_op->op_id = oid++;
        io_op->arr_time = arr_time;
        io_op->track = track;
        input_list.push_back(io_op);
        completed_list.push_back(io_op);
    }
    file.close();
}

class SchedBase
{
public:
    virtual io_operation *select_next_op() { return 0; };
};
SchedBase *THE_SCHED;

class FIFO : public SchedBase
{
public:
    io_operation *select_next_op()
    {
        if (io_queue.empty())
        {
            return NULL;
        }
        else
        {
            io_operation *next_op = io_queue.front();
            io_queue.pop_front();
            return next_op;
        }
    }
};
class SSTF : public SchedBase
{
public:
    io_operation *select_next_op()
    {
        if (io_queue.empty())
        {
            return NULL;
        }
        else
        {
            int min_dis = numeric_limits<int>::max();
            io_operation *next_op = new io_operation;
            for (auto op : io_queue)
            {
                int dis = abs(op->track - head);
                if (dis < min_dis)
                {
                    min_dis = dis;
                    next_op = op;
                }
            }
            for (deque<io_operation *>::iterator it = io_queue.begin(); it != io_queue.end(); ++it)
            {
                if (*it == next_op)
                {
                    io_queue.erase(it);
                    break;
                }
            }
            return next_op;
        }
    }
};
class LOOK : public SchedBase
{
private:
    int direction = 0; // -1: down, 1: up, 0: limbo

public:
    io_operation *select_next_op()
    {
        if (io_queue.empty())
        {
            direction = 0;
            return NULL;
        }
        else
        {
            int min_dis = numeric_limits<int>::max();
            io_operation *next_op = new io_operation;
            if (direction == 0)
            {
                next_op = io_queue.front();
                if ((next_op->track - head) > 0)
                {
                    direction = 1;
                }
                else if ((next_op->track - head) < 0)
                {
                    direction = -1;
                }
            }
            else
            {
                bool found = false;
                for (auto op : io_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis && (direction * (op->track - head) >= 0))
                    {
                        min_dis = dis;
                        next_op = op;
                        found = true;
                    }
                }
                if (not found)
                {
                    direction *= -1;
                }
                for (auto op : io_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis && (direction * (op->track - head) > 0))
                    {
                        min_dis = dis;
                        next_op = op;
                    }
                }
            }
            for (deque<io_operation *>::iterator it = io_queue.begin(); it != io_queue.end(); ++it)
            {
                if (*it == next_op)
                {
                    io_queue.erase(it);
                    break;
                }
            }
            return next_op;
        }
    }
};
class CLOOK : public SchedBase
{
private:
    int direction = 0; // -1: down, 1: up, 0: limbo

public:
    io_operation *select_next_op()
    {
        if (io_queue.empty())
        {
            return NULL;
        }
        else
        {
            int min_dis = numeric_limits<int>::max();
            io_operation *next_op = new io_operation;
            if (direction == 0)
            {
                next_op = io_queue.front();
                if ((next_op->track - head) > 0)
                {
                    direction = 1;
                }
                else if ((next_op->track - head) < 0)
                {
                    direction = -1;
                }
            }
            else
            {
                bool found = false;
                for (auto op : io_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis && (direction * (op->track - head) >= 0))
                    {
                        min_dis = dis;
                        next_op = op;
                        found = true;
                    }
                }
                if (not found)
                {
                    int max_dis = 0;
                    for (auto op : io_queue)
                    {
                        int dis = abs(op->track - head);
                        if (dis > max_dis && (direction * (op->track - head) < 0))
                        {
                            max_dis = dis;
                            next_op = op;
                        }
                    }
                }
            }
            for (deque<io_operation *>::iterator it = io_queue.begin(); it != io_queue.end(); ++it)
            {
                if (*it == next_op)
                {
                    io_queue.erase(it);
                    break;
                }
            }
            return next_op;
        }
    }
};

deque<io_operation *> active_queue;
deque<io_operation *> add_queue;
class FLOOK : public SchedBase
{
private:
    int direction = 0; // -1: down, 1: up, 0: limbo

public:
    io_operation *select_next_op()
    {
        if (active_queue.empty())
        {
            if (add_queue.empty())
            {
                return NULL;
            }
            else
            {
                active_queue.swap(add_queue);
            }
        }
        if (!active_queue.empty())
        {
            int min_dis = numeric_limits<int>::max();
            io_operation *next_op = new io_operation;
            if (direction == 0)
            {
                for (auto op : active_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis)
                    {
                        min_dis = dis;
                        next_op = op;
                    }
                }
                if ((next_op->track - head) > 0)
                {
                    direction = 1;
                }
                else if ((next_op->track - head) < 0)
                {
                    direction = -1;
                }
            }
            else
            {
                bool found = false;
                for (auto op : active_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis && (direction * (op->track - head) >= 0))
                    {
                        min_dis = dis;
                        next_op = op;
                        found = true;
                    }
                }
                if (not found)
                {
                    direction *= -1;
                }
                for (auto op : active_queue)
                {
                    int dis = abs(op->track - head);
                    if (dis < min_dis && (direction * (op->track - head) > 0))
                    {
                        min_dis = dis;
                        next_op = op;
                    }
                }
            }
            for (deque<io_operation *>::iterator it = active_queue.begin(); it != active_queue.end(); ++it)
            {
                if (*it == next_op)
                {
                    active_queue.erase(it);
                    break;
                }
            }
            return next_op;
        }
    }
};

void simulation()
{
    int distance = 0;

    while (true)
    {
        while (!(input_list.empty()))
        {
            io_operation *op = input_list.front();
            if (op->arr_time <= cur_time)
            {
                io_queue.push_back(op);
                add_queue.push_back(op);
                input_list.pop_front();
            }
            else
            {
                break;
            }
        }
        io_operation *next_op = THE_SCHED->select_next_op();
        if (next_op == NULL)
        {
            if (input_list.empty())
                break;
            else
            {
                cur_time++;
                continue;
            }
        }
        else
        {
            next_op->start_time = cur_time;
            // cout << "t: " << next_op->track << " h:" << head << endl;
            distance = abs(next_op->track - head);
            // cout << distance << endl;
            tot_movement += distance;
            cur_time += distance;
            head = next_op->track;
            next_op->end_time = cur_time;
        }
    }
}

int main(int argc, char *argv[])
{
    int opt;
    string algo_type;
    string options;
    int index;
    string infile;

    while ((opt = getopt(argc, argv, "s:v:q:f:")) != -1)
    {
        switch (opt)
        {
        case 's':
        {
            algo_type = optarg;
            break;
        }
        case 'v':
        {
            break;
        }
        case 'q':
        {
            break;
        }
        case 'f':
        {
            break;
        }
        }
    }

    index = optind;
    infile = argv[index];

    // read input file -> create input_list
    readInputFile(infile);

    // select sched algo
    switch (algo_type[0])
    {
    case 'N':
    {
        THE_SCHED = new FIFO;
        break;
    }
    case 'S':
    {
        THE_SCHED = new SSTF;
        break;
    }
    case 'L':
    {
        THE_SCHED = new LOOK;
        break;
    }
    case 'C':
    {
        THE_SCHED = new CLOOK;
        break;
    }
    case 'F':
    {
        THE_SCHED = new FLOOK;
        break;
    }
    }

    simulation();

    // output
    for (int i = 0; i < completed_list.size(); i++)
    {
        printf("%5d: %5d %5d %5d \n", i, completed_list[i]->arr_time,
               completed_list[i]->start_time, completed_list[i]->end_time);
    }

    // output summary
    int total_time = cur_time;
    double io_utilization = static_cast<double>(tot_movement) / total_time;
    int max_waittime = 0;
    double total_turnaround = 0;
    double total_waittime = 0;
    int op_count = completed_list.size();
    for (auto op : completed_list)
    {
        total_turnaround += (op->end_time - op->arr_time);
        total_waittime += (op->start_time - op->arr_time);
        if ((op->start_time - op->arr_time) > max_waittime)
        {
            max_waittime = (op->start_time - op->arr_time);
        }
    }
    double avg_turnaround = total_turnaround / op_count;
    double avg_waittime = total_waittime / op_count;
    printf("SUM: %d %d %.4lf %.2lf %.2lf %d\n", total_time, tot_movement, io_utilization, avg_turnaround, avg_waittime, max_waittime);

    return 0;
}