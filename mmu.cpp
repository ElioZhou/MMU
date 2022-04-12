#include <iostream>
#include <vector>
#include <fstream>

using namespace std;

const int MAX_VPAGE = 64;
const int MAX_FRAMES = 32;
const int INSERT = 1;
const int ERASE = 0;

int vPage;
char operation;
bool O = true, P = true, F = true, S = true, x = false, f = true;
unsigned long long cost = 0, instrCount = 0, rwCount = 0;
unsigned long long contextSwitchCount = 0, processExitCount = 0;
unsigned long ofs = 0;
//bool O = false, P = true, F = false, S = false;

ifstream infile;
vector<int> randList;


class PTE {
public:
    unsigned int present: 1;
    unsigned int referenced: 1;
    unsigned int modified: 1;
    unsigned int writeProtected: 1;
    unsigned int pagedout: 1;
    unsigned int fileMapped: 1;
    // The maximum number of frames is 128
    unsigned int fid: 7;
    // You first determine that the vpage can be accessed, i.e. it is part of one of the VMAs.
    unsigned int inVMA: 1;

    PTE() {
        present = 0;
        modified = 0;
        writeProtected = 0;
        referenced = 0;
        pagedout = 0;
        fileMapped = 0;
        fid = 0;
        inVMA = 0;
    };

    //Note once the PAGEDOUT flag is set it will
    //never be reset as it indicates there is content on the swap device)
    //and then the PTE’s frame must be set and the valid bit can be set.
    void reset() {
        present = 0;
        referenced = 0;
        modified = 0;
        writeProtected = 0;
        pagedout = 0;
        fileMapped = 0;
        fid = 0;
        inVMA = 0;
    };

//    void toString() {
//        printf("present: %lu, referenced: %d, modified: %d, writeProtected: %d, pagedout: %d, fileMapped: %d, fid: %d, inVMA: %d\n",
//               present, referenced, modified, writeProtected, pagedout, fileMapped, fid, inVMA);
//    }
};


class VMA {
public:
    unsigned int startVpage;
    unsigned int endVpage;
    unsigned int writeProtected;
    unsigned int fileMapped;
};


class Process {
public:
    int pid;
    vector<VMA *> VMAList;
    PTE pageTable[MAX_VPAGE];
    // stats
    unsigned long unmaps = 0, maps = 0, ins = 0, outs = 0;
    unsigned long fins = 0, fouts = 0, zeros = 0, segv = 0, segprot = 0;

//    bool is_vpage_filemapped(int vpage);
//    bool is_vpage_protected(int vpage);
//    void print_table();
//    VMA* get_vpage(int vpage);

    Process() = default;


};


vector<Process *> processList;

void printPageTable() {
    for (Process *process: processList) {
        // print table
        printf("PT[%d]:", process->pid);
        int i = 0;
        for (PTE pte: process->pageTable) {
            if (pte.present) {
                cout << " " << i << ":";
                if (pte.referenced) cout << "R";
                else cout << "-";
                if (pte.modified) cout << "M";
                else cout << "-";
                if (pte.pagedout) cout << "S";
                else cout << "-";
            } else if (!pte.present) {
                if (pte.pagedout) cout << " #";
                else cout << " *";
            }
            i++;
        }
        cout << endl;
    }
}


class Frame {
public:
    PTE *pte;
    unsigned int fid;
    //Redundant
    int pid, vPage;
    bool isMapped;

//    bitset<32> bitcount;
//    unsigned int last_time;
//    Frame():pte(nullptr),is_free(false),is_mapped(false),process_id(-1),vpage(-1),bitcount(0),last_time(0){};
    Frame(unsigned int frame_id) {
        pte = nullptr;
        pid = -1;
        vPage = -1;
        isMapped = false;
        fid = frame_id;
    }

    void unMap() {
        if (!isMapped) return;
        //Its entry in the owning process’s page_table must be removed
        pte->fid = 0;
        pte->present = 0;
        processList[pid]->unmaps += 1;
        //You must inspect the state of the R and M bits. If the page was modified, then the page frame must be
        //paged out to the swap device (“OUT”) or in case it was file mapped it has to be written back to the file (“FOUT”).
        if (pte->modified && pte->fileMapped) {
            if (O) {
                cout << " UNMAP" << " " << pid << ":" << vPage << endl;
                cout << " FOUT" << endl;
            }
            processList[pid]->fouts += 1;
//            pte->referenced =false;
            pte->modified = false;
        } else if (pte->modified && !pte->fileMapped) {
            if (O) {
                cout << " UNMAP" << " " << pid << ":" << vPage << endl;
                cout << " OUT" << endl;
            }
            pte->pagedout = true;
            processList[pid]->outs += 1;
            pte->modified = false;
        } else {
            if (O) cout << " UNMAP" << " " << pid << ":" << vPage << endl;
        }
        reset();
    }

    void map(PTE *pteParsed, int pidParsed, int v_page) {
        pte = pteParsed;
        pid = pidParsed;
        vPage = v_page;
        isMapped = true;
        processList[pid]->maps += 1;
        pte->fid = fid;
        pte->present = 1;
        if (O) cout << " MAP" << " " << fid << endl;
    }

    void reset() {
        pte = nullptr;
        pid = -1;
        vPage = -1;
        isMapped = false;
    }
};


/// When call pager, we can guarantee there's no free frame
class Pager {
public:
//    vector<Frame*> queue;
    unsigned int hand;

//    vector<Frame*> frame_table;
    Pager() {
        hand = 0;
    };

    virtual Frame *selectVictimFrame() = 0;
//    virtual void update(Frame *f) = 0;
};


Frame *frameTable[MAX_FRAMES] = {nullptr};


class FIFO : public Pager {
public:
    Frame *selectVictimFrame() override {
        if (hand == MAX_FRAMES) hand = 0;
        Frame *victimFrame = frameTable[hand];
        hand++;
        return victimFrame;
    }
};


int myRandom();


class Random : public Pager {
    Frame *selectVictimFrame() override {
        Frame *victimFrame = frameTable[myRandom()];
        return victimFrame;
    }
};


class Clock : public Pager {
    Frame *selectVictimFrame() override {
        while (frameTable[hand]->pte->referenced) {
            frameTable[hand]->pte->referenced = false;
            hand++;
            if (hand == MAX_FRAMES) hand = 0;
        }
//        printf("frameTable[%d]: %d\n",hand,processList[frameTable[hand]->pid]->pageTable[frameTable[hand]->vPage].referenced);
        Frame *victimFrame = frameTable[hand];
        // Must do this.
        hand++;
        if (hand == MAX_FRAMES) hand = 0;
        return victimFrame;
    }
};


class ESC : public Pager {
    unsigned long long instAfterReset = 0;
    // store the first frame to each class (class_index = 2*R+M)
    // If instructioncount % 50 == 0, reset reference bit after scanning.
    Frame *selectVictimFrame() override {
        bool reset = false;
        unsigned int handStart = hand;
        printf("start: %d\n", handStart);
        bool firstStart = true;
        Frame *firstFrameOfClass[4] = {nullptr};
        Frame *victimFrame;

//        printf("r: %d, ins: %d\n", reset, instrCount);
        while (firstStart || hand != handStart) {
            firstStart = false;
            Frame *frame = frameTable[hand];
            PTE *pte = frame->pte;
            unsigned int classIndex = 2 * pte->referenced + pte->modified;
            if (classIndex == 0) {
//                printf("Choose fid: %d, startHand = %d\n", frame->fid, handStart);
//                printf("r: %d, s: %d\n",frame->pte->referenced, frame->pte->modified);
                firstFrameOfClass[0] = frame;
//                victimFrame = frame;
                break;
//                return frame;
            }
            if (!firstFrameOfClass[classIndex]) {
                firstFrameOfClass[classIndex] = frame;
            }
            hand++;
            if (hand == MAX_FRAMES) hand = 0;
        }
        //Second cycle, hand == handStart, get the smallest
        for (int i = 3; i >= 0; i--) {
            Frame *currentFrame = firstFrameOfClass[i];
            if (currentFrame) {
//                printf("Choose fid: %d, startHand = %d\n", frame->fid, handStart);
//                Once a victim frame is determined, the hand is set to the next position
//                after the victim frame for the next select_victim_frame() invocation.
//                hand = frame->fid + 1;
//                if (hand == MAX_FRAMES) hand = 0;
                victimFrame = currentFrame;
            }
        }
        //Reset after finding the victim.
        if (instrCount - instAfterReset >= 50) {
            printf("RESET!");
            resetRBit();
            instAfterReset = instrCount;
        }
        hand = victimFrame->fid + 1;
        if (hand == MAX_FRAMES) hand = 0;
        return victimFrame;
    }

    static void resetRBit() {
        for (Frame *frame: frameTable) {
            frame->pte->referenced = false;
        }
    }
};

class ESC1 : public Pager
{
public:
    ESC1(){
        int hand = 0;
    }
    int threshold = 49;
    Frame* selectVictimFrame(){
        int lowest_class  = 10;
        Frame* victim_frame = NULL;
        int curr_hand_position = hand;
        //cout << "curr_hand_position" << curr_hand_position << endl;
        do{
            //cout << "hand" << hand;
            Frame* frame = frameTable[hand];
            PTE* pte = &((processList.at(frame->pid))->pageTable[frame->vPage]);
            int curr_class = 2 * pte->referenced + pte->modified;
            if(curr_class < lowest_class){
                lowest_class = curr_class;
                victim_frame = frame;
            }
            hand = hand + 1;
            if(hand>= MAX_FRAMES)
                hand = 0;
            if(lowest_class == 0)
                break;
        }
        while(hand != curr_hand_position);
        if (instrCount >= this->threshold)
        {
            this->threshold = instrCount + 50;
            resetRBit();
        }
        hand = victim_frame->fid + 1;
        if(hand >= MAX_FRAMES) hand = 0;
//        printf("Choose fid: %d, startHand = %d\n", victim_frame->fid, curr_hand_position);
//        printf("r: %d, s: %d\n",victim_frame->pte->referenced, victim_frame->pte->modified);
        return victim_frame;
    }
    static void resetRBit() {
        for (Frame *frame: frameTable) {
            frame->pte->referenced = false;
        }
    }
};



class Aging : public Pager {
    Frame *selectVictimFrame() override {

    }
};


class WorkingSet : public Pager {
    Frame *selectVictimFrame() override {

    }
};


///---------------Initialize Data Structures Needed---------------///


void printPTE(PTE pte);

//You must define a global frame_table that each operating system maintains to describe the usage of each of its physical
//frames and where you maintain reverse mappings to the process and the vpage that maps a particular frame. Note, that in this
//assignment a frame can only be mapped by at most one PTE at a time, which simplifies things significantly
Pager *pager;
deque<Frame *> freeFrameList;
//vector<Process*> processList;


///---------------Sub Functions about frame---------------///


void printFreeFrameList() {
    auto it = freeFrameList.begin();
    printf("Free List:\n");
    while (it != freeFrameList.end()) {
        printf("fid: %d\n", (*it)->fid);
//        (*it)->pte->toString();
        it++;
    }
    printf("Free List End\n");
}


void printFrameTable() {
    cout << "FT:";
    int i = 0;
    for (Frame *frame: frameTable) {
        if (frame->isMapped) {
            cout << " " << frame->pid << ":" << frame->vPage;

        } else cout << " *";
        i++;
    }
    cout << endl;
}


void printFrameTableR() {
    cout << "FT:";
    int i = 0;
    for (Frame *frame: frameTable) {
        if (frame->isMapped) {
            cout << " " << i << ":" << frame->pid << ":" << frame->vPage;
//            cout << " " << frame->pid << ":" << frame->vPage;
            if (processList[frame->pid]->pageTable[frame->vPage].referenced)
                cout << 'r';
            if (processList[frame->pid]->pageTable[frame->vPage].modified)
                cout << 'm';
        } else cout << " *";
        i++;
//        printf("fid: %d, isMapped: %d\n", frame->fid, frame->isMapped);
    }
    cout << endl;
}


void initFrameTables() {
    for (unsigned int i = 0; i < MAX_FRAMES; i++) {
//        printf("i = %d\n", i);
        frameTable[i] = new Frame(i);
        freeFrameList.push_back(frameTable[i]);
    }
//    printFrameTable();
//    printFreeFrameList();
}


/**
 * Insert to or erase frame from free list after unmapping or mapping
 * @param op
 * @param frame
 * @return
 */
bool updateFreeFrameList(int op, Frame *frame) {
    if (op == INSERT) {
        freeFrameList.push_back(frame);
        return true;
    } else if (op == ERASE) {
        auto itr = find(freeFrameList.begin(), freeFrameList.end(), frame);
        if (itr != freeFrameList.end()) {
            freeFrameList.erase(itr);
            return true;
        } else return false;
    }
    return false;
}


/**
 * Allocate frame from free frame list
 * @return
 */
Frame *allocateFrameFromFreeList() {
    if (freeFrameList.empty()) return nullptr;
    else {
        Frame *frame = freeFrameList.front();
//        frame->isMapped = true;
        freeFrameList.erase(freeFrameList.begin());
        return frame;
    }
}


/**
 * Get a free frame
 * @return
 */
Frame *getFrame() {
    Frame *frame = allocateFrameFromFreeList();
    if (frame == nullptr) {
        frame = pager->selectVictimFrame();
    }
    return frame;
}


///---------------Sub Functions about process and page table---------------///


/**
 * Initialize the page table of the process
 * @param proc
 * @param start_vpage
 * @param end_vpage
 * @param write_protected
 * @param file_mapped
 */
void setPageTable(Process *proc, unsigned int start_vpage, unsigned int end_vpage,
                  unsigned int write_protected, unsigned int file_mapped) {
    for (unsigned int i = start_vpage; i <= end_vpage; i++) {
        proc->pageTable[i].writeProtected = write_protected;
        proc->pageTable[i].fileMapped = file_mapped;
        proc->pageTable[i].inVMA = 1;
    }
}


/**
 * Things to do after process exits
 * @param process
 */
void processExits(Process *process) {
    for (int i = 0; i < MAX_VPAGE; i++) {
        // Must use a pointer, cannot just get the PTE itself
        PTE *pte = &process->pageTable[i];
        if (pte->present) {
            Frame *frame = frameTable[pte->fid];
            updateFreeFrameList(INSERT, frame);
            if (O) printf(" UNMAP %d:%d\n", frame->pid, frame->vPage);
            process->unmaps += 1;
            frame->reset();
        }
        //Note that dirty non-fmapped (anonymous) pages are not written
        //back (OUT) as the process exits.
        if (pte->modified && pte->fileMapped) {
            if (O) cout << " FOUT" << endl;
            process->fouts += 1;
        }
        pte->reset();
    }
}


/**
 * Verify this is actually a valid page in a vma if not raise error and next inst
 * @param proc, page
 */
VMA *getVMAofPage(Process *proc, int page) {
    auto it = proc->VMAList.begin();
    while (it != proc->VMAList.end()) {
        if (page >= (*it)->startVpage && page <= (*it)->endVpage) {
            return *it;
        }
        it++;
    }
    return nullptr;
}


///---------------Sub Functions about reading file---------------///


/**
 * Get next line from input file
 * @param line
 * @return
 */
bool getNewLine(string &line) {
    if (infile.peek() == EOF) {
        return false;
    }
    getline(infile, line);
    return true;
}


/**
 * Read the input file, get the processes
 * @param file
 */
void readFile(string file) {
    infile.open(file);
    string line;
    getNewLine(line);
//    cout << line[0] << endl;
    while (line[0] == '#') {
        getNewLine(line);
    }
//    cout << line;
    int processCount = stoi(line);
    for (int i = 0; i < processCount; i++) {
        Process *process = new Process();
        process->pid = i;
        getNewLine(line);
        while (line[0] == '#') {
            getNewLine(line);
        }
//        cout <<line;
        int VMACount = stoi(line);
        for (int j = 0; j < VMACount; j++) {
            getNewLine(line);
            while (line[0] == '#') {
                getNewLine(line);
            }
//            cout <<line;
            VMA *vma = new VMA();
            char *line_c = (char *) line.c_str();
            vma->startVpage = atoi(strtok(line_c, " \t\n"));
            vma->endVpage = atoi(strtok(nullptr, " \t\n"));
            vma->writeProtected = atoi(strtok(nullptr, " \t\n"));
            vma->fileMapped = atoi(strtok(nullptr, " \t\n"));
            process->VMAList.push_back(vma);
//            setPageTable(process, vma->startVpage, vma->endVpage,
//                         vma->writeProtected, vma->fileMapped);
        }
        processList.push_back(process);
//        cout << processList.at(0)->pid << endl;
    }
//    infile.close();
}


/**
 * Get next instruction from input file
 * @return
 */
bool getNextInstruction() {
    string line;
    if (!getNewLine(line)) {
        return false;
    }
//    getNewLine(line);
    while (line[0] == '#') {
        if (!getNewLine(line)) {
            return false;
        }
    }
//    cout << line << endl;
    const char *DELIMITER = " \t\n\r\v\f";
    char *line_c = (char *) line.c_str();
    operation = strtok(line_c, " \t\n")[0];
    vPage = atoi(strtok(nullptr, " \t\n"));
    return true;
}


int myRandom() {
    if (ofs == randList.size()) ofs = 0;
    else ofs++;
    return randList[ofs] % MAX_FRAMES;
}


void readRandFile() {
    ifstream randFile("/Users/ethan/Documents/NYU/22 Spring/Operating Systems/Labs/Lab3/lab3_assign/inputs/rfile");
    string randstr;
    while (randFile >> randstr)
        randList.push_back(stoi(randstr, nullptr, 10));
    randFile.close();
}


int main(int argc, char *argv[]) {
//    readFile(argv[0]);
    readFile("/Users/ethan/Documents/NYU/22 Spring/Operating Systems/Labs/Lab3/lab3_assign/inputs/in7");
//    pager = new FIFO();
    readRandFile();
//    pager = new Random();
//    pager = new Clock();
    pager = new ESC();

    initFrameTables();
//    unsigned long instrCount = 0;

    Process *currentProcess;
    while (getNextInstruction()) {
        // handle special case of “c” and “e” instruction
//        cout << operation << ":" << vPage << endl;
        if (O) {
            printf("%llu: ==> %c %d\n", instrCount, operation, vPage);
        }
        instrCount++;
        if (operation == 'c') {
            currentProcess = processList[vPage];
            contextSwitchCount += 1;
        } else if (operation == 'e') {
            cout << "EXIT current process " << currentProcess->pid << '\n';
            processExits(currentProcess);
            processExitCount += 1;
            currentProcess = nullptr;
            continue;
            // code to free all frames and everything for that process
        } else {
            rwCount += 1;
            // now the real instructions for read and write
            PTE *pte = &currentProcess->pageTable[vPage];
            // Not in VMA or not initialized
            if (!pte->inVMA) {
                VMA *vma = getVMAofPage(currentProcess, vPage);
                if (!vma) {
                    // This in reality generates the page fault exception
                    cout << "SEGV" << endl;
                    currentProcess->segv += 1;
                    continue;
                } else {
                    pte->inVMA = true;
                    setPageTable(currentProcess, vma->startVpage,
                                 vma->endVpage, vma->writeProtected, vma->fileMapped);
                }
            };
            if (!pte->present) {
                // and now you execute
                // verify this is actually a valid page in a vma if not raise error and next inst
//                printFreeFrameList();
//                cout << freeFrameList.front()->fid << endl;
                Frame *allocatedFrame = getFrame();
                //-> figure out if/what to do with old frame if it was mapped
                // see general outline in MM-slides under Lab3 header and writeup below
                // see whether and how to bring in the content of the access page.
                //Once a victim frame has been determined, the victim frame must
                //be unmapped from its user ( <address space:vpage>)
                allocatedFrame->unMap();
                updateFreeFrameList(INSERT, allocatedFrame);
                //Now the frame can be reused for the faulting instruction.
                //First the PTE must be reset
//                pte->reset();
                if (pte->pagedout) {
                    if (O) cout << " IN" << endl;
                    currentProcess->ins += 1;
                } else if (pte->fileMapped) {
                    if (O) cout << " FIN" << endl;
                    currentProcess->fins += 1;
                }
                    // Not modified before
                else {
                    if (O) cout << " ZERO" << endl;
                    currentProcess->zeros += 1;
                }

                allocatedFrame->map(pte, currentProcess->pid, vPage);
                updateFreeFrameList(ERASE, allocatedFrame);
//                a frame must be allocated, assigned to the PTE belonging to the vpage of this instruction and then populated with the proper content.
//                The population depends whether this page was previously paged out (in which case the page must be brought
//                back from the swap space (“IN”) or (“FIN” in case it is a memory mapped file). If the vpage was
//                never swapped out and is not file mapped, then by definition it still has a zero filled content and you issue the “ZERO” output
                pte->fid = allocatedFrame->fid;

            } else if (pte->present) {
                Frame *frame;
            }
            // simulate instruction execution by hardware by updating the R/M PTE bits
            // update_pte(read/modify) bits based on operations.
            pte->referenced = true;
            if (operation == 'w') {
                // check write protection
                if (pte->writeProtected) {
                    cout << " SEGPROT" << endl;
                    currentProcess->segprot++;
                } else {
                    pte->modified = true;
                }
            }
        }
//        if(x) printPageTable();
        if (f && instrCount <3000 ) printFrameTableR();
//        cout << "hand:";
//        cout << pager->hand << endl;
    }
    if (P) printPageTable();
    if (F) printFrameTable();
    if (S) {
        cost += rwCount + 130 * contextSwitchCount + 1250 * processExitCount;
        for (Process *process: processList) {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   process->pid, process->unmaps, process->maps, process->ins,
                   process->outs, process->fins, process->fouts, process->zeros,
                   process->segv, process->segprot);
            cost += 300 * process->maps + 400 * process->unmaps + 3100 * process->ins
                    + 2700 * process->outs + 2800 * process->fins + 2400 * process->fouts
                    + 140 * process->zeros + 340 * process->segv + 420 * process->segprot;
        }
        printf("TOTALCOST %llu %llu %llu %llu %lu\n",
               instrCount, contextSwitchCount, processExitCount, cost, sizeof(PTE));
    }
}


