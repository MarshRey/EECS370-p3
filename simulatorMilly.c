/*
 * EECS 370, University of Michigan, Fall 2023
 * Project 3: LC-2K Pipeline Simulator
 * Instructions are found in the project spec: https://eecs370.github.io/project_3_spec/
 * Make sure NOT to modify printState or any of the associated functions
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Machine Definitions
#define NUMMEMORY 65536 // maximum number of data words in memory
#define NUMREGS 8 // number of machine registers

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 // will not implemented for Project 3
#define HALT 6
#define NOOP 7

const char* opcode_to_str_map[] = {
    "add",
    "nor",
    "lw",
    "sw",
    "beq",
    "jalr",
    "halt",
    "noop"
};

#define NOOPINSTR (NOOP << 22)

typedef struct IFIDStruct {
	int pcPlus1;
	int instr;
} IFIDType;

typedef struct IDEXStruct {
	int pcPlus1;
	int valA;
	int valB;
	int offset;
	int instr;
} IDEXType;

typedef struct EXMEMStruct {
	int branchTarget;
    int eq;
	int aluResult;
	int valB;
	int instr;
} EXMEMType;

typedef struct MEMWBStruct {
	int writeData;
    int instr;
} MEMWBType;

typedef struct WBENDStruct {
	int writeData;
	int instr;
} WBENDType;

typedef struct stateStruct {
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int reg[NUMREGS];
	unsigned int numMemory;
	IFIDType IFID;
	IDEXType IDEX;
	EXMEMType EXMEM;
	MEMWBType MEMWB;
	WBENDType WBEND;
	unsigned int cycles; // number of cycles run so far
} stateType;

static inline int opcode(int instruction) {
    return instruction>>22;
}

static inline int field0(int instruction) {
    return (instruction>>19) & 0x7;
}

static inline int field1(int instruction) {
    return (instruction>>16) & 0x7;
}

static inline int field2(int instruction) {
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num) {
    return num - ( (num & (1<<15)) ? 1<<16 : 0 );
}

void printState(stateType*);
void printInstruction(int);
void readMachineCode(stateType*, char*);
int isDataHazard(int);

int main(int argc, char *argv[]) {

    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    // State is the current state of the pipeline
    // newState is the state of the pipeline AT THE END OF CYCLE?

    static stateType state, newState;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // Initialize state here
    state.IFID.instr = NOOPINSTR;
    state.IDEX.instr = NOOPINSTR;
    state.EXMEM.instr = NOOPINSTR;
    state.MEMWB.instr = NOOPINSTR;
    state.WBEND.instr = NOOPINSTR;
    state.cycles = 0;
    state.pc = 0;


    newState = state;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        int instr = 0; // Keeping the current instruction

        newState = state;
        newState.cycles += 1;

        /* ---------------------- IF stage --------------------- */
        // instr = state.instrMem[state.pc];
        newState.IFID.instr = state.instrMem[state.pc];
        newState.IFID.pcPlus1 = state.pc + 1;
        newState.pc++;
        

        /* ---------------------- ID stage --------------------- */
        // instr = state.IFID.instr;
        newState.IDEX.instr = state.IFID.instr;
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1;

        // Stalling for lw hazards
        if (opcode(state.IDEX.instr) == LW && (field0(newState.IDEX.instr) == field1(state.IDEX.instr) 
                || field1(newState.IDEX.instr) == field1(state.IDEX.instr))){
            newState.IDEX.instr = NOOPINSTR;
            newState.pc = state.pc;
            newState.IFID = state.IFID; // Replace the entire IFID with the previous state
        }
        else{ // No hazards
            newState.IDEX.valA = state.reg[field0(state.IFID.instr)];
            newState.IDEX.valB = state.reg[field1(state.IFID.instr)];
            newState.IDEX.offset = convertNum(field2(state.IFID.instr));
        }

        /* ---------------------- EX stage --------------------- */
        instr = state.IDEX.instr;
        newState.EXMEM.instr = state.IDEX.instr;
        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;
        // newState.EXMEM.valB = state.IDEX.valB;
        // newState.EXMEM.eq = (state.IDEX.valA == state.IDEX.valB);
        
        int wbend = (opcode(state.WBEND.instr) == LW) ? field1(state.WBEND.instr) : field2(state.WBEND.instr);
        int regA = state.IDEX.valA;
        int regB = state.IDEX.valB;

        // Data hazards
        if (isDataHazard(instr)){
            if (field0(newState.EXMEM.instr) == wbend){
                regA = state.WBEND.writeData;
            }
            // else{
            //     regA = state.IDEX.valA;
            // }
            if (field1(newState.EXMEM.instr) == wbend){
                regB = state.WBEND.writeData;
            }
            // else{
            //     regB = state.IDEX.valB;
            // }
        }

        //MEMWB detect and forward
        int memwb = (opcode(state.MEMWB.instr) == LW) ? field1(state.MEMWB.instr) : field2(state.MEMWB.instr);

        // Two instruction away data hazard
        if (isDataHazard(state.MEMWB.instr)){
            if (field0(newState.EXMEM.instr) == memwb){
                regA = state.MEMWB.writeData;
            }
            if (field1(newState.EXMEM.instr) == memwb){
                regB = state.MEMWB.writeData;
            }
        }


        int exem = (opcode(state.EXMEM.instr) == LW) ? field1(state.EXMEM.instr) : field2(state.EXMEM.instr);
        if (isDataHazard(state.EXMEM.instr)){
            if (field0(newState.EXMEM.instr) == exem){
                regA = state.EXMEM.aluResult;
            }
            if (field1(newState.EXMEM.instr) == exem){
                regB = state.EXMEM.aluResult;
            }
        }

        // Do calcs and update the ALU
        if (opcode(instr) == ADD){
            newState.EXMEM.aluResult = regA + regB;
        }
        else if (opcode(instr) == NOR){
            newState.EXMEM.aluResult = ~(regA | regB);
        }
        else if (opcode(instr) == LW || opcode(instr) == SW){
            newState.EXMEM.aluResult = regA + state.IDEX.offset;
        }
        else if (opcode(instr) == BEQ){
            newState.EXMEM.eq = (regA == regB);
            newState.EXMEM.aluResult = state.IDEX.pcPlus1 + state.IDEX.offset;
        }

        if (opcode(instr) == NOOP){
            newState.EXMEM.aluResult = 0;
        }
        else{
            newState.EXMEM.valB = regB;
        }
        /* --------------------- MEM stage --------------------- */
        instr = state.EXMEM.instr;
        newState.MEMWB.instr = state.EXMEM.instr;

        if (opcode(instr) == LW){
            // printf("MEMWB: %d\n", state.dataMem[state.EXMEM.aluResult]);
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
        }
        else if (opcode(instr) == SW){
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.valB;
        }
        else if (opcode(instr) == BEQ){
            // Check if the branch is taken
            if (newState.EXMEM.eq){
                newState.pc = state.EXMEM.branchTarget;
                newState.IFID.instr = NOOPINSTR;
                newState.IDEX.instr = NOOPINSTR;
                newState.EXMEM.instr = NOOPINSTR;
            //     newState.MEMWB.instr = NOOPINSTR; 
            //     newState.WBEND.instr = NOOPINSTR;
            }
        }
        else if (!(opcode(instr) == NOOP || opcode(instr) == HALT)){
            newState.MEMWB.writeData = state.EXMEM.aluResult;
        }
        else{
            newState.MEMWB.writeData = 0;
        }

        /* ---------------------- WB stage --------------------- */
        instr = state.MEMWB.instr;
        newState.WBEND.instr = state.MEMWB.instr;
        newState.WBEND.writeData = state.MEMWB.writeData;

        if (opcode(instr) == LW){
            newState.reg[field1(instr)] = state.MEMWB.writeData;
            printf("Wrote LW writeData to reg[%d] = %d\n", field2(instr), state.MEMWB.writeData);

        }
        if (opcode(instr) == ADD || opcode(instr) == NOR){
            newState.reg[field2(instr)] = state.MEMWB.writeData;
            printf("Wrote ADD/NOR writeData to reg[%d] = %d\n", field2(instr), state.MEMWB.writeData);
        }

        /* ------------------------ END ------------------------ */
        state = newState; /* this is the last statement before end of the loop. It marks the end
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("Machine halted\n");
    printf("Total of %d cycles executed\n", state.cycles);
    printf("Final state of machine:\n");
    printState(&state);
}


int isDataHazard(int instr){
    return (opcode(instr) == ADD || opcode(instr) == NOR || opcode(instr) == LW);
}

/*
* DO NOT MODIFY ANY OF THE CODE BELOW.
*/

void printInstruction(int instr) {
    const char* instr_opcode_str;
    int instr_opcode = opcode(instr);
    if(ADD <= instr_opcode && instr_opcode <= NOOP) {
        instr_opcode_str = opcode_to_str_map[instr_opcode];
    }

    switch (instr_opcode) {
        case ADD:
        case NOR:
        case LW:
        case SW:
        case BEQ:
            printf("%s %d %d %d", instr_opcode_str, field0(instr), field1(instr), convertNum(field2(instr)));
            break;
        case JALR:
            printf("%s %d %d", instr_opcode_str, field0(instr), field1(instr));
            break;
        case HALT:
        case NOOP:
            printf("%s", instr_opcode_str);
            break;
        default:
            printf(".fill %d", instr);
            return;
    }
}

void printState(stateType *statePtr) {
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i=0; i<statePtr->numMemory; ++i) {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i=0; i<NUMREGS; ++i) {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if(opcode(statePtr->IFID.instr) == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");

    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if(idexOp == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.valA);
    if (idexOp >= HALT || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.valB);
    if(idexOp == LW || idexOp > BEQ || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.valB);
    if (exmemOp != SW) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
	int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // WB/END
	int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
    fflush(stdout);
}

// File
#define MAXLINELENGTH 1000 // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType *state, char* filename) {
    char line[MAXLINELENGTH];
    FILE *filePtr = fopen(filename, "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory) {
        if (sscanf(line, "%d", state->instrMem+state->numMemory) != 1) {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ]\t= 0x%08x\t= %d\t= ", state->numMemory, 
            state->instrMem[state->numMemory], state->instrMem[state->numMemory]);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}
