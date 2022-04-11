#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;

const int MAX_VPAGE = 64;
const int MAX_FRAMES = 128;
int vPage;
char operation;
bool O = true, P = false, F = false, S = false;
ifstream infile;



class PTE{
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
    unsigned int inVma: 1;
    PTE() {
        present = 0;
        modified = 0;
        writeProtected = 0;
        referenced = 0;
        pagedout = 0;
        fileMapped = 0;
        fid = 0;
        inVma = 0;
    };
    //Note once the PAGEDOUT flag is set it will
    //never be reset as it indicates there is content on the swap device)
    //and then the PTE’s frame must be set and the valid bit can be set.
    void reset() {
        if(pagedout) {

        }
        else {

        }
    };
    void toString() {
        printf("present: %d, referenced: %d, modified: %d, writeProtected: %d, pagedout: %d, fileMapped: %d, fid: %d, inVma: %d\n",
               present, referenced, modified, writeProtected, pagedout, fileMapped, fid, inVma);
    }
};


class VMA{
public:
    unsigned int startVpage;
    unsigned int endVpage;
    unsigned int writeProtected;
    unsigned int fileMapped;
};


class Process{
public:
    int pid;
    vector<VMA*> VMAList;
    PTE pageTable[MAX_VPAGE];
    // stats
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;
//    bool is_vpage_filemapped(int vpage);
//    bool is_vpage_protected(int vpage);
//    void print_table();
//    VMA* get_vpage(int vpage);
    Process() {

    };
    void printPageTable() {
        printf("pid: %d\n", pid);
        for(PTE pte : pageTable) {
            pte.toString();
        }
    }
};



vector<Process*> processList;


class Frame{
public:
    PTE* pte;
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
    }
    void unMap() {
        if(!isMapped) return;
        //Its entry in the owning process’s page_table must be removed
        pte->fid = 0;
        pte->present = 0;
        processList[pid]->unmaps += 1;
        //You must inspect the state of the R and M bits. If the page was modified, then the page frame must be
        //paged out to the swap device (“OUT”) or in case it was file mapped it has to be written back to the file (“FOUT”).
        if(pte->modified && pte->fileMapped) {
            if(O) {
                cout << " UNMAP" << " " << pid << ":" << vPage << endl;
                cout << "FOUT" << endl;
            }
            processList[pid]->fouts += 1;
//            pte->referenced =false;
            pte->modified=false;
        }
        if(pte->modified && !pte->fileMapped) {
            if(O) {
                cout << " UNMAP" << " " << pid << ":" << vPage << endl;
                cout << "OUT" << endl;
            }
            pte->pagedout=true;
            processList[pid]->outs += 1;
            pte->modified=false;
        }
        cout << " UNMAP" << " " << pid << ":" << vPage << endl;
        isMapped = false;
        pid = 0;
        vPage = 0;
        pte = nullptr;
    }
    void map(PTE * pteParsed, int pidParsed, int v_page) {
        pte = pteParsed;
        pid = pidParsed;
        vPage = v_page;
        isMapped = true;
        pte->fid = fid;
    }
};

class Pager{
public:
//    vector<Frame*> queue;
    unsigned int hand;

//    vector<Frame*> frame_table;
    Pager() {
        hand = 0;
    };
    virtual Frame* selectVictimFrame() = 0;
//    virtual void update(Frame *f) = 0;
};

Frame* frameTable[MAX_FRAMES];

class FIFO: public Pager {
public:
    Frame* selectVictimFrame() {
        if(hand == MAX_FRAMES) hand = 0;
        Frame* victimFrame = frameTable[hand];
        hand++;
        return victimFrame;
    }
};
///---------------Initialize Data Structures Needed---------------///


void printPTE(PTE pte);
//You must define a global frame_table that each operating system maintains to describe the usage of each of its physical
//frames and where you maintain reverse mappings to the process and the vpage that maps a particular frame. Note, that in this
//assignment a frame can only be mapped by at most one PTE at a time, which simplifies things significantly
Pager* pager;
vector<Frame*> freeFrameList;
//vector<Process*> processList;

void initFrameTables() {
    for(unsigned int i = 0; i < MAX_FRAMES; i++ ) {
        frameTable[i] = new Frame(i);
        freeFrameList.push_back(frameTable[i]);
    }
}


void printFrameTable() {
    vector<Frame*>::iterator it = freeFrameList.begin();
    while(it != freeFrameList.end()){
        printf("fid: %d\n", (*it)->fid);
        (*it)->pte->toString();
        it++;
    }
}

///---------------Sub Functions about frame---------------///


/**
 * Allocate frame from free frame list
 * @return
 */
Frame* allocateFrameFromFreeList() {
    if(freeFrameList.empty()) return nullptr;
    else {
        Frame* frame = freeFrameList.front();
//        frame->isMapped = true;
        freeFrameList.erase(freeFrameList.begin());
        return frame;
    }
}


/**
 * Get a free frame
 * @return
 */
Frame* getFrame() {
    Frame* frame = allocateFrameFromFreeList();
    if (frame == nullptr) {
        frame = pager->selectVictimFrame();
    }
    return frame;
}


/**
 * Initialize the page table of the process
 * @param proc
 * @param start_vpage
 * @param end_vpage
 * @param write_protected
 * @param file_mapped
 */
void setPageTable(Process* proc, unsigned int start_vpage, unsigned int end_vpage,
                  unsigned int write_protected, unsigned int file_mapped) {
    for(unsigned int i = start_vpage; i <= end_vpage; i++) {
        proc->pageTable[i].writeProtected = write_protected;
        proc->pageTable[i].fileMapped = file_mapped;
        proc->pageTable[i].inVma = 1;
    }
}


/**
 * Verify this is actually a valid page in a vma if not raise error and next inst
 * @param proc, page
 */
VMA* getVMAofPage(Process* proc, int page) {
    auto it = proc->VMAList.begin();
    while(it != proc->VMAList.end()){
        if(page >= (*it)->startVpage && page <= (*it)->endVpage){
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
bool getNewLine(string& line) {
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
            vma->endVpage = atoi(strtok(nullptr, DELIMITER));
            vma->writeProtected = atoi(strtok(nullptr, DELIMITER));
            vma->fileMapped = atoi(strtok(nullptr, DELIMITER));
            process->VMAList.push_back(vma);
            setPageTable(process, vma->startVpage, vma->endVpage,
                         vma->writeProtected, vma->fileMapped);
        }
        processList.push_back(process);
        cout << processList.at(0)->pid << endl;
    }
//    infile.close();
}


/**
 * Get next instruction from input file
 * @return
 */
bool getNextInstruction() {
    string line;
    if(!getNewLine(line)) {
        return false;
    }
//    getNewLine(line);
    while(line[0] == '#') {
        if(!getNewLine(line)) {
            return false;
        }
    }
//    cout << line << endl;
    const char *DELIMITER = " \t\n\r\v\f";
    char *line_c = (char *) line.c_str();
    operation = strtok(line_c, DELIMITER)[0];
    vPage = atoi(strtok(nullptr, DELIMITER));
    return true;
}


int main(int argc, char *argv[]) {

//    readFile(argv[0]);
    readFile("/Users/ethan/Documents/NYU/22 Spring/Operating Systems/Labs/Lab3/lab3_assign/inputs/in1");
    pager = new FIFO();
    initFrameTables();
    Process *currentProcess;
    int instrCount = 0;
    while (getNextInstruction()) {
        // handle special case of “c” and “e” instruction
//        cout << operation << ":" << vPage << endl;
        if (O) {
            printf("%d: ==> %c %d\n", instrCount, operation, vPage);
        }
        instrCount++;
        if (operation == 'c') {
            currentProcess = processList[vPage];
//            Context_Switch = Context_Switch + 1;
//            cost = cost + 130;
        } else if (operation == 'e') {
//            cout << "EXIT current process " << currentProcess->pid << '\n';
//            after_exit(CURRENT_PROCESS);
//            process_exits = process_exits + 1;
//            cost = cost + 1250;
            // code to free all frames and everything for that process
        } else {
            // now the real instructions for read and write

            PTE *pte = &currentProcess->pageTable[vPage];
            // Not in VMA or not initialized
            if (!pte->inVma) {
                VMA* vma = getVMAofPage(currentProcess, vPage);
                if(!vma) {
                    // This in reality generates the page fault exception
                    cout << "SEGV" << endl;
                    currentProcess->segv += 1;
                    continue;
                }
                else {
                    pte->inVma = true;
                    setPageTable(currentProcess, vma->startVpage,
                                 vma->endVpage, vma->writeProtected, vma->fileMapped);
                }
            };
            if (!pte->present) {
                // and now you execute
                // verify this is actually a valid page in a vma if not raise error and next inst
                Frame *allocatedFrame = getFrame();
                //-> figure out if/what to do with old frame if it was mapped
                // see general outline in MM-slides under Lab3 header and writeup below
                // see whether and how to bring in the content of the access page.
                //Once a victim frame has been determined, the victim frame must
                //be unmapped from its user ( <address space:vpage>)
                allocatedFrame->unMap();
                //Now the frame can be reused for the faulting instruction.
                //First the PTE must be reset
//                pte->reset();
                allocatedFrame->map(pte, currentProcess->pid, vPage);
//                a frame must be allocated, assigned to the PTE belonging to the vpage of this instruction and then populated with the proper content.
//                The population depends whether this page was previously paged out (in which case the page must be brought
//                back from the swap space (“IN”) or (“FIN” in case it is a memory mapped file). If the vpage was
//                never swapped out and is not file mapped, then by definition it still has a zero filled content and you issue the “ZERO” output
                pte->fid = allocatedFrame->fid;
                if(pte->pagedout) {
                    if(O) cout << "IN" << endl;
                    currentProcess->ins += 1;
                }
                else if(pte->fileMapped) {
                    if(O) cout << "FIN" << endl;
                    currentProcess->fins += 1;
                }
                // Not modified before
                else {
                    if(O) cout << "ZERO" << endl;
                    currentProcess->zeros += 1;
                }
            }
            else if(pte->present) {
                Frame* frame;
            }
        }

        // check write protection
        // simulate instruction execution by hardware by updating the R/M PTE bits
        // update_pte(read/modify) bits based on operations.
    }
    printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
           currentProcess->pid,
           currentProcess->unmaps, currentProcess->maps, currentProcess->ins, currentProcess->outs,
           currentProcess->fins, currentProcess->fouts, currentProcess->zeros,
           currentProcess->segv, currentProcess->segprot);
}


