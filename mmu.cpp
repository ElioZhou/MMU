#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;

const int MAX_VPAGE = 64;
const int MAX_FRAMES = 0;
ifstream infile;
char operation;
int vPage = -1;



class PTE{
public:
    unsigned int present: 1;
    unsigned int referenced: 1;
    unsigned int modified: 1;
    unsigned int writeProtect: 1;
    unsigned int pagedout: 1;
    unsigned int fileMapped: 1;
    // The maximum number of frames is 128
    unsigned int frameNumber: 7;
    // You first determine that the vpage can be accessed, i.e. it is part of one of the VMAs.
    unsigned int inVma: 1;
    void reset();
    PTE() {
        present = 0;
        modified = 0;
        writeProtect = 0;
        referenced = 0;
        pagedout = 0;
        fileMapped = 0;
        frameNumber = 0;
        inVma = 0;
    };
};


class Frame{
public:
    PTE* pte;
//    int id;

    unsigned int pid, vPage;
    bool isFree, isMapped;
//    bitset<32> bitcount;
//    unsigned int last_time;
//    Frame():pte(nullptr),is_free(false),is_mapped(false),process_id(-1),vpage(-1),bitcount(0),last_time(0){};
    Frame() {
        pte = nullptr;
        pid = -1;
        vPage = -1;
        isFree = false;
    }
//    void free_frame();
};


class VMA{
public:
    unsigned int startVpage;
    unsigned int endVpage;
    bool writeProtected;
    bool fileMapped;
};


class Process{
public:
    int pid;
    vector<VMA*> VMAList;
    PTE pageTable[MAX_VPAGE];

    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;
    bool is_vpage_filemapped(int vpage);
    bool is_vpage_protected(int vpage);
    void print_table();
    VMA* get_vpage(int vpage);
//    Process();
};


class Pager{
public:
    vector<Frame*> queue;
    unsigned int hand;
    vector<Frame*> frame_table;
    Pager():hand(0){};
    virtual Frame* selectVictimFrame() = 0;
    virtual void update(Frame *f);
};


///---------------Initialize Data Structures Needed---------------///


Frame* frameTable[MAX_FRAMES];
vector<Frame*> freeFrameList;
vector<Process*> processList;
PTE pageTable[MAX_VPAGE];
Pager* pager;


Frame* allocateFrameFromFreeList() {
    if(freeFrameList.empty()) return nullptr;
    else {
        Frame* frame = freeFrameList.front();
        frame->isFree = false;
        freeFrameList.erase(freeFrameList.begin());
        return frame;
    }
}


Frame* getFrame() {
    Frame* frame = allocateFrameFromFreeList();
    if (frame == nullptr) {
        frame = pager->selectVictimFrame();
    }
    return frame;

}


bool getNewLine(string& line) {
    if (infile.peek() == EOF) {
        return false;
    }
    getline(infile, line);
    return true;
}


void readFile(string file) {
    infile.open(file);
    string line;
    getNewLine(line);
//    cout << line[0] << endl;
    while(line[0] == '#') {
        getNewLine(line);
    }
//    cout << line;
    int processCount = stoi(line);
    for(int i = 0; i < processCount; i++) {
        Process* process = new Process();
        process->pid = i;
        getNewLine(line);
        while(line[0] == '#') {
            getNewLine(line);
        }
//        cout <<line;
        int VMACount = stoi(line);
        for(int j = 0; j < VMACount; j++) {
            getNewLine(line);
            while(line[0] == '#') {
                getNewLine(line);
            }
//            cout <<line;
            VMA* vma = new VMA();
            const char* DELIMITER = " \t\n\r\v\f";
            char* line_c = (char*)line.c_str();
            vma->startVpage = atoi(strtok(line_c, DELIMITER));
            vma->endVpage = atoi(strtok(NULL, DELIMITER));
            vma->writeProtected = atoi(strtok(NULL, DELIMITER));
            vma->fileMapped = atoi(strtok(NULL, DELIMITER));
            process->VMAList.push_back(vma);
        }
        processList.push_back(process);
        cout << processList.at(0)->pid << endl;
    }
//    infile.close();
}

bool getNextInstruction() {
    string line;
    if(!getNewLine(line)) {
        return false;
    }
    getNewLine(line);
    while(line[0] == '#') {
        if(!getNewLine(line)) {
            return false;
        }
    }
//    cout << line << endl;
    const char *DELIMITER = " \t\n\r\v\f";
    char *line_c = (char *) line.c_str();
    operation = strtok(line_c, DELIMITER)[0];
    vPage = atoi(strtok(NULL, DELIMITER));
    return true;
}

int main(int argc, char *argv[]) {

//    readFile(argv[0]);
    readFile("/Users/ethan/Documents/NYU/22 Spring/Operating Systems/Labs/Lab3/lab3_assign/inputs/in11");
    Process* currentProcess;
    while (getNextInstruction()) {
        // handle special case of “c” and “e” instruction
//        cout << operation << ":" << vPage << endl;
        if (operation == 'c'){
            currentProcess = processList[vPage];
//            Context_Switch = Context_Switch + 1;
//            cost = cost + 130;
        }
        else if(operation == 'e'){
//            cout << "EXIT current process " << currentProcess->pid << '\n';
//            after_exit(CURRENT_PROCESS);
//            process_exits = process_exits + 1;
//            cost = cost + 1250;
            // code to free all frames and everything for that process
        }
//        // now the real instructions for read and write
//        PTE *pte = &currentProcess->pageTable[vpage];
//        if (!pte->present) {
//            // this in reality generates the page fault exception and now you execute
//            // verify this is actually a valid page in a vma if not raise error and next inst
//            Frame *newFrame = getFrame();
//                //-> figure out if/what to do with old frame if it was mapped
//                // see general outline in MM-slides under Lab3 header and writeup below
//                // see whether and how to bring in the content of the access page.
    }
//    cout << "";
//// check write protection
//// simulate instruction execution by hardware by updating the R/M PTE bits
////        update_pte(read/modify) bits based on operations.
//    }
}
