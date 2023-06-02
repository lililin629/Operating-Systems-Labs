#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <set>
#include <unistd.h>
#include <tuple>
#include <typeinfo>
#include <algorithm>
using namespace std;

bool DETAIL_OUT = false;
bool PAGE_TABLE_OUT = false;
bool FRAME_TABLE_OUT = false;
bool PROC_SUM_OUT = false;
const int MAX_VPAGES = 64;
const int MAX_FRAMES = 128;
int INSTRUCTION_SIZE;
int INSTRUCTION_COUNT = 0;
int FRAME_SIZE;
bool IS_A = false;
bool IS_W = false;

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
    randNums.erase(randNums.begin());
}

typedef struct // physical memory: 16 or 32
{
    int pid;
    int page;
    int mapped;
    unsigned int age : 32;
} Frame;

// virtual memory: total 32 bits
typedef struct
{
    unsigned int mapped : 1;
    unsigned int write_protect : 1;
    unsigned int paged_out : 1;
    unsigned int file_mapped : 1;
    unsigned int zeros : 17;
    unsigned int referenced : 1;
    unsigned int modified : 1;
    unsigned int frame_id : 7;
} PTE;

// several PTEs grouped together
typedef struct
{
    int start_vpage;
    int end_vpage;
    int write_protect;
    int file_mapped;
} VMA;

// each proc has a page table that contains 64 pages
typedef struct
{
    vector<VMA> vmas;
    vector<PTE> page_table;
    int pid;
    unsigned long unmap = 0;
    unsigned long map = 0;
    unsigned long in = 0;
    unsigned long fin = 0;
    unsigned long out = 0;
    unsigned long fout = 0;
    unsigned long zero = 0;
    unsigned long segv = 0;
    unsigned long segprot = 0;
    unsigned long context_switch = 0;
    unsigned long exit = 0;
    unsigned long rw_instruction = 0;
} Process;

Process *CURRENT_PROC;
vector<Process> PROCESSES;
vector<Frame> FRAME_TABLE;
deque<unsigned int> FREE_FRAMES;
bool inVMA(int vpage)
{
    for (VMA vma : CURRENT_PROC->vmas)
    {
        if (vma.start_vpage <= vpage && vpage <= vma.end_vpage)
        {
            CURRENT_PROC->page_table[vpage].file_mapped = vma.file_mapped;
            CURRENT_PROC->page_table[vpage].write_protect = vma.write_protect;
            return true;
        };
    };
    return false;
};
vector<std::tuple<char, int>> INSTRUCTIONS;
int instCount;
char operation;
int vpage;
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
    int process_count, vma_count;
    file.open(infile);
    getNextLine();
    // procs, pagetables, vmas
    line >> process_count;
    // cout << "process_c: " << process_count << '\n';

    for (int i = 0; i < process_count; i++)
    {
        Process proc;
        proc.pid = i;
        getNextLine();
        // cout << "line: " << line.str() << '\n';
        line >> vma_count;
        // cout << "vma_c: " << vma_count << '\n';
        for (int j = 0; j < vma_count; j++)
        {
            getNextLine();
            int start_vpage, end_vpage;
            bool write_protect, file_mapped;
            line >> start_vpage >> end_vpage >> write_protect >> file_mapped;
            VMA vma;
            vma.start_vpage = start_vpage;
            vma.end_vpage = end_vpage;
            vma.write_protect = write_protect;
            vma.file_mapped = file_mapped;
            proc.vmas.push_back(vma);
        }
        vector<PTE> new_page_table(MAX_VPAGES);
        proc.page_table = new_page_table;
        PROCESSES.push_back(proc);
    }
    // INSTRUCTIONS vector
    while (getNextLine())
    {
        line >> operation >> vpage;
        INSTRUCTIONS.push_back(std::tuple<char, int>{operation, vpage});
    }
    INSTRUCTION_SIZE = INSTRUCTIONS.size();
    // cout << "instructions size: " << instructions.size() << endl;
    // cout << "inst:" << get<0>(instructions[1]) << endl;
}
bool get_next_instruction(char &operation, int &vpage)
{
    while (INSTRUCTION_COUNT < INSTRUCTION_SIZE)
    {
        operation = get<0>(INSTRUCTIONS[INSTRUCTION_COUNT]);
        vpage = get<1>(INSTRUCTIONS[INSTRUCTION_COUNT]);
        cout << INSTRUCTION_COUNT << ": ==> " << operation << " " << vpage << endl;
        INSTRUCTION_COUNT++;
        return true;
    }
    return false;
}

class PagerBase
{
public:
    virtual int select_victim_frame() { return 0; };
};
PagerBase *THE_PAGER;

class FIFO : public PagerBase
{
private:
    int hand = 0;
    int victim_frame;

public:
    virtual int select_victim_frame()
    {
        victim_frame = hand;
        hand = (hand + 1) % FRAME_SIZE;
        return victim_frame;
    }
};

class RANDOM : public PagerBase
{
private:
    int i = 0;
    int hand;
    int RAND_SIZE = randNums.size();

public:
    virtual int select_victim_frame()
    {
        hand = i % RAND_SIZE;
        i++;
        return randNums[hand] % FRAME_SIZE;
    }
};

class CLOCK : public PagerBase
{
private:
    int hand = 0;
    int victim_frame;

public:
    virtual int select_victim_frame()
    {
        while (true)
        {
            Frame &cur_frame = FRAME_TABLE[hand];
            PTE &cur_pte = PROCESSES[cur_frame.pid].page_table[cur_frame.page];
            if (!cur_pte.referenced)
            {
                victim_frame = hand;
                hand = ++hand % FRAME_SIZE;
                return victim_frame;
            }
            else
            {
                cur_pte.referenced = false;
                hand = ++hand % FRAME_SIZE;
                continue;
            }
        }
    }
};

class NRU : public PagerBase
{
private:
    int hand = 0;
    int previous_reset = 0;
    std::vector<int> nru_class;

    void reset_nru_class()
    {
        nru_class.assign(4, -1);
        update_nru_class(hand, true);
    }

    void update_nru_class(int start, bool reset_references)
    {
        for (int i = 0; i < FRAME_SIZE; i++)
        {
            Process &cur_proc = PROCESSES[FRAME_TABLE[start].pid];
            PTE &cur_pte = cur_proc.page_table[FRAME_TABLE[start].page];
            int frame_class = cur_pte.referenced * 2 + cur_pte.modified;
            if (nru_class[frame_class] == -1)
            {
                nru_class[frame_class] = start;
            }
            if (reset_references)
            {
                cur_pte.referenced = false;
            }
            start = (start + 1) % FRAME_SIZE;
        }
    }

public:
    int select_victim_frame() override
    {
        if (INSTRUCTION_COUNT - previous_reset >= 50)
        {
            reset_nru_class();
            previous_reset = INSTRUCTION_COUNT;
        }
        else
        {
            nru_class.assign(4, -1);
            update_nru_class(hand, false);
        }

        int victim_frame = *std::find_if(nru_class.begin(), nru_class.end(), [](int frame)
                                         { return frame != -1; });
        hand = (victim_frame + 1) % FRAME_SIZE;
        return victim_frame;
    }
};

class AGING : public PagerBase
{
private:
    int victim_frame = 0;
    int hand = 0;

public:
    virtual int select_victim_frame()
    {
        unsigned int min_age = 0xFFFFFFFF;

        for (int i = 0; i < FRAME_SIZE; i++)
        {
            Frame &cur_frame = FRAME_TABLE[hand];
            cur_frame.age = cur_frame.age >> 1;
            if (PROCESSES[cur_frame.pid].page_table[cur_frame.page].referenced)
            {
                cur_frame.age = (cur_frame.age | 0x80000000);
                PROCESSES[cur_frame.pid].page_table[cur_frame.page].referenced = false;
            }
            if (cur_frame.age < min_age)
            {
                min_age = cur_frame.age;
                victim_frame = hand;
            }
            hand = (hand + 1) % FRAME_SIZE;
        }
        hand = (victim_frame + 1) % FRAME_SIZE;
        return victim_frame;
    }
};

class WORKING_SET : public PagerBase
{
private:
    int TAU = 49;
    int victim_frame = 0;
    int hand = 0;

public:
    virtual int select_victim_frame()
    {
        int min_time = INSTRUCTION_COUNT;
        for (int i = 0; i < FRAME_SIZE; i++)
        {
            Frame &cur_frame = FRAME_TABLE[hand];
            PTE &cur_pte = PROCESSES[cur_frame.pid].page_table[cur_frame.page];

            if (cur_pte.referenced)
            {
                cur_frame.age = INSTRUCTION_COUNT;
            }
            else if ((INSTRUCTION_COUNT - cur_frame.age) > TAU)
            {
                victim_frame = hand;
                break;
            }
            if (cur_frame.age < min_time)
            {
                min_time = cur_frame.age;
                victim_frame = hand;
            }
            hand = (hand + 1) % FRAME_SIZE;
            cur_pte.referenced = false;
        }

        FRAME_TABLE[victim_frame].age = INSTRUCTION_COUNT;
        hand = (victim_frame + 1) % FRAME_SIZE;
        return victim_frame;
    }
};

void get_frame(PTE &pte, int vpage)
{
    // frame
    int frame_id;
    if (FREE_FRAMES.empty()) //(unmap -> page replacement)
    {
        frame_id = THE_PAGER->select_victim_frame();
        printf(" UNMAP %d:%d\n", FRAME_TABLE[frame_id].pid, FRAME_TABLE[frame_id].page);
        PROCESSES[FRAME_TABLE[frame_id].pid].unmap++;

        // cout << CURRENT_PROC->pid << ": unmap+" << endl;
        Process &victim_proc = PROCESSES[FRAME_TABLE[frame_id].pid];
        PTE &victim_pte = victim_proc.page_table[FRAME_TABLE[frame_id].page];
        victim_pte.mapped = false;
        if (victim_pte.modified)
        {
            if (victim_pte.file_mapped)
            {
                victim_proc.fout++;
                victim_pte.modified = false;
                cout << " FOUT" << endl;
            }
            else
            {
                victim_pte.paged_out = true;
                victim_pte.modified = false;
                victim_proc.out++;
                cout << " OUT" << endl;
            }
        }
    }
    else // MAP
    {
        frame_id = FREE_FRAMES.front();
        FRAME_TABLE[frame_id].mapped = true;
        FREE_FRAMES.pop_front();
    }
    FRAME_TABLE[frame_id].pid = CURRENT_PROC->pid;
    FRAME_TABLE[frame_id].page = vpage;
    // pte
    pte.frame_id = frame_id;
    pte.mapped = true;
    if (pte.paged_out)
    {
        CURRENT_PROC->in++;
        cout << " IN" << endl;
    }
    else if (pte.file_mapped)
    {
        CURRENT_PROC->fin++;
        cout << " FIN" << endl;
    }
    else
    {
        CURRENT_PROC->zero++;
        cout << " ZERO" << endl;
    }
    CURRENT_PROC->map++;
    cout << " MAP " << pte.frame_id << endl;
    if (IS_W)
    {
        FRAME_TABLE[frame_id].age = INSTRUCTION_COUNT;
    }
    if (IS_A)
    {
        FRAME_TABLE[frame_id].age = 0;
    }
};
void read_operation(PTE &pte, int vpage)
{
    // CURRENT_PROC->rw_instruction++;
    pte.referenced = true;
    if (!pte.mapped)
        get_frame(pte, vpage);
};
void write_operation(PTE &pte, int vpage)
{
    // CURRENT_PROC->rw_instruction++;
    pte.referenced = true;
    if (!pte.mapped)
    {
        get_frame(pte, vpage);
    }

    if (pte.write_protect)
    {
        CURRENT_PROC->segprot++;
        cout << " SEGPROT" << endl;
    }
    else
    {
        pte.modified = true;
    }
};
void exit_operation(Process *proc)
{
    printf("EXIT current process %d\n", proc->pid);
    proc->exit++;
    // scan cur proc page table, UNMAP mapped pages (pid: page)
    for (int i = 0; i < MAX_VPAGES; i++)
    {
        PTE &cur_pte = proc->page_table[i];
        cur_pte.paged_out = false;
        if (cur_pte.mapped)
        {
            cur_pte.mapped = false;
            printf(" UNMAP %d:%d\n", proc->pid, i);
            proc->unmap++;
            // FOUT modified, file_mapped pages
            if (cur_pte.modified && cur_pte.file_mapped)
            {
                proc->fout++;
                cout << " FOUT" << endl;
            }
            // return frames to FREE_FRAMES in order
            FREE_FRAMES.push_back(cur_pte.frame_id);

            // Reset frame age
            if (IS_A)
            {
                FRAME_TABLE[cur_pte.frame_id].age = 0;
            }
            else if (IS_W)
            {
                FRAME_TABLE[cur_pte.frame_id].age = INSTRUCTION_COUNT;
            }
        }
    }
};
void simulation()
{
    char operation;
    int vpage;

    while (get_next_instruction(operation, vpage))
    { // handle c,e
        if (operation == 'c')
        {
            CURRENT_PROC = &PROCESSES[vpage];
            CURRENT_PROC->context_switch++;
            continue;
        }
        else if (operation == 'e')
        {
            exit_operation(CURRENT_PROC);
            continue;
        }
        else
        {
            CURRENT_PROC->rw_instruction++;
            if (!inVMA(vpage))
            {
                CURRENT_PROC->segv++;
                cout << " SEGV" << endl;
                continue;
            }
            else
            {
                PTE &pte = CURRENT_PROC->page_table[vpage]; // seg fault
                // pte->present = true;                         // TODO: maybe delete?
                // pte->frame_id = get_frame(vpage);            // (unmap -> page replacement) and remap
                switch (operation)
                {
                case 'r':
                {
                    read_operation(pte, vpage);
                    break;
                }
                case 'w':
                {
                    write_operation(pte, vpage);
                    break;
                }
                }
            }
        }
    }
};

void print_page_table()
{
    for (int i = 0; i < PROCESSES.size(); i++)
    {
        cout << "PT[" << i << "]:";
        Process cur_proc = PROCESSES[i];
        for (int j = 0; j < MAX_VPAGES; j++)
        {
            PTE cur_page = cur_proc.page_table[j];
            if (!cur_page.mapped)
            {
                if (cur_page.paged_out)
                {
                    cout << " #";
                }
                else
                {
                    cout << " *";
                }
            }
            else
            {
                cout << " " << j << ":";
                if (cur_page.referenced)
                {
                    cout << "R";
                }
                else
                {
                    cout << "-";
                }
                if (cur_page.modified)
                {
                    cout << "M";
                }
                else
                {
                    cout << "-";
                }
                if (cur_page.paged_out)
                {
                    cout << "S";
                }
                else
                {
                    cout << "-";
                }
            }
        }
        cout << '\n';
    }
}
void print_frame_table()
{
    cout << "FT:";
    for (int i = 0; i < FRAME_SIZE; i++)
    {
        if (FRAME_TABLE[i].mapped)
        {
            cout << " " << FRAME_TABLE[i].pid << ":" << FRAME_TABLE[i].page;
        }
        else
        {
            cout << " *";
        }
    }
    cout << '\n';
}
void print_proc_sum()
{
    unsigned long CTX_SWITCH = 0;
    unsigned long PROC_EXIT = 0;
    unsigned long long COST = 0;
    for (int i = 0; i < PROCESSES.size(); i++)
    {
        Process *proc = &PROCESSES[i];
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
               proc->pid,
               proc->unmap, proc->map, proc->in, proc->out,
               proc->fin, proc->fout, proc->zero,
               proc->segv, proc->segprot);
        COST = COST +
               proc->rw_instruction * 1 +
               proc->context_switch * 130 +
               proc->exit * 1230 +
               proc->map * 350 +
               proc->unmap * 410 +
               proc->in * 3200 +
               proc->out * 2750 +
               proc->fin * 2350 +
               proc->fout * 2800 +
               proc->zero * 150 +
               proc->segv * 440 +
               proc->segprot * 410;

        CTX_SWITCH += proc->context_switch;
        PROC_EXIT += proc->exit;
    }
    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
           INSTRUCTION_COUNT, CTX_SWITCH, PROC_EXIT, COST, sizeof(PTE));
}

int main(int argc, char *argv[])
{
    int opt;
    int frame_size;
    string algo_type;
    string options;
    int index;
    string rfile;
    string infile;

    while ((opt = getopt(argc, argv, "f:a:o:")) != -1)
    {
        switch (opt)
        {
        case 'f':
        {
            FRAME_SIZE = stoi(optarg);
            break;
        }
        case 'a':
        {
            algo_type = optarg;
            break;
        }
        case 'o':
        {
            options = optarg;
            break;
        }
        }
    }

    index = optind;
    infile = argv[index];

    index++;
    rfile = argv[index];

    // read rfile -> create randNums vector
    readRandFile(rfile);

    // read input file -> create procs, pagetables, vmas, instructions vector
    readInputFile(infile);

    // initialize frame table
    for (int i = 0; i < FRAME_SIZE; i++)
    {
        FRAME_TABLE.push_back(Frame());
        FREE_FRAMES.push_back(i);
    }
    // select pager algo
    switch (algo_type[0])
    {
    case 'f':
    {
        THE_PAGER = new FIFO;
        break;
    }
    case 'r':
    {
        THE_PAGER = new RANDOM;
        break;
    }
    case 'c':
    {
        THE_PAGER = new CLOCK;
        break;
    }
    case 'e':
    {
        THE_PAGER = new NRU;
        break;
    }
    case 'a':
    {
        THE_PAGER = new AGING;
        IS_A = true;
        break;
    }
    case 'w':
    {
        THE_PAGER = new WORKING_SET;
        IS_W = true;
        break;
    }
    }
    // select output options
    for (int i = 0; i < options.size(); i++)
    {
        if (options[i] == 'O')
        {
            DETAIL_OUT = true;
        }
        if (options[i] == 'P')
        {
            PAGE_TABLE_OUT = true;
        }
        if (options[i] == 'F')
        {
            FRAME_TABLE_OUT = true;
        }
        if (options[i] == 'S')
        {
            PROC_SUM_OUT = true;
        }
    }

    simulation();

    if (PAGE_TABLE_OUT)
    {
        print_page_table();
    }
    if (FRAME_TABLE_OUT)
    {
        print_frame_table();
    }
    if (PROC_SUM_OUT)
    {
        print_proc_sum();
    }
    return 0;
}
