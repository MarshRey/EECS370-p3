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


int main(int argc, char *argv[]) {

    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    static stateType state, newState;
    
    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // All registers in the processor should be initialized to 0, alongside the program counter.
    // Initialize registers to 0
    for (int i = 0; i < NUMREGS; i++){
        state.reg[i] = 0;
    }
    // program counter is initialized to 0
    state.pc = 0;

    // The instruction field in all pipeline registers should be initialized to the noop instruction
    // Initialize pipeline registers to NOOP
    state.IFID.instr = NOOPINSTR;
    state.IDEX.instr = NOOPINSTR;
    state.EXMEM.instr = NOOPINSTR;
    state.MEMWB.instr = NOOPINSTR;
    state.WBEND.instr = NOOPINSTR;

    // Initialize state here
    state.cycles = 0; // set cycles to 0

    // detect and forward destinations
    int EXEM_det_and_forward = 0;
    int MEMWB_det_and_forward = 0;
    int WBEND_det_and_forward = 0;




    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState = state;
        newState.cycles += 1;
        

        /* ---------------------- IF stage --------------------- */
        // IF = Instruction Fetch
        newState.IFID.instr = state.instrMem[state.pc];  // new state stage gets instruction from memory
        newState.IFID.pcPlus1 = state.pc + 1; 
        newState.pc++; // increment pc


        /* ---------------------- ID stage --------------------- */
        // ID = Instruction Decode
        newState.IDEX.instr = state.IFID.instr; // new state stage gets instruction from previous stage
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1; // new state stage gets pcPlus1 from previous stage

        // hazard potential LW
        if (opcode(state.IDEX.instr) == LW){
            // check if field0 newstate and field1 state are the same
            // then check if field1 newstate and field1 state are the same
            // also check if field0 newstate and field1 state are the same
            // if either are the same, then stall with noop
            if (field1(newState.IDEX.instr) == field1(state.IDEX.instr) || field0(newState.IDEX.instr) == field1(state.IDEX.instr)){
                // if they are the same, then stall with noop
                newState.pc = state.pc; // unincriment pc
                newState.IFID = state.IFID; // set state instruction
                newState.IDEX.instr = NOOPINSTR; // give noop this cycle
            }
            else{ // no data hazard
            // get register values and offset for LW and send them to next stage
            newState.IDEX.valA = state.reg[field0(state.IFID.instr)];
            newState.IDEX.valB = state.reg[field1(state.IFID.instr)];
            newState.IDEX.offset = convertNum(field2(state.IFID.instr));
            }
        }
        else{ // no data hazard
            // get register values and offset for LW and send them to next stage
            newState.IDEX.valA = state.reg[field0(state.IFID.instr)];
            newState.IDEX.valB = state.reg[field1(state.IFID.instr)];
            newState.IDEX.offset = convertNum(field2(state.IFID.instr));
        }


        /* ---------------------- EX stage --------------------- */
        // EX = Execute
        newState.EXMEM.instr = state.IDEX.instr; // new state stage gets instruction from previous stage
        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset; // set branch target if needed
        // int writeBackDestReg = 0; // set destination register to 0
        // int memDestReg = 0; // set destination register to 0
        // int exmemDestReg = 0; // set destination register to 0
        // NOW DECLARED OUTSIDE LOOP
        int reg0Value = state.IDEX.valA; // set reg0Value so that the value can be used and not be overwritten
        int reg1Value = state.IDEX.valB; // set reg1Value so that the value can be used and not be overwritten

        // if LW for write back state instruction set destination register to field1
        if (opcode(state.WBEND.instr) == LW){
            WBEND_det_and_forward = field1(state.WBEND.instr);
        }
        else { // instruction is anythting but LW so set destination register to field2
            WBEND_det_and_forward = field2(state.WBEND.instr);
        }

        // repeat for MEMWB
        if (opcode(state.MEMWB.instr) == LW){
            MEMWB_det_and_forward = field1(state.MEMWB.instr);
        }
        else {
            MEMWB_det_and_forward = field2(state.MEMWB.instr);
        }

        // repeat for EXMEM
        if (opcode(state.EXMEM.instr) == LW){
            EXEM_det_and_forward = field1(state.EXMEM.instr);
        }
        else {
            EXEM_det_and_forward = field2(state.EXMEM.instr);
        }

        // data hazard branching time
        // idea is forward and correct later
        // add, nor, lw can cause
        if (opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NOR || opcode(state.WBEND.instr) == LW){
            // if write back destination matches field0 or field1 of new state instruction
            if (field0(newState.EXMEM.instr) == WBEND_det_and_forward){
                // set reg0Value to write back value
                reg0Value = state.WBEND.writeData;
            }
            if (field1(newState.EXMEM.instr) == WBEND_det_and_forward){
                // set reg1Value to write back value
                reg1Value = state.WBEND.writeData;
            }
        }

        // repeate for MEMWB
        if (opcode(state.MEMWB.instr) == ADD || opcode(state.MEMWB.instr) == NOR || opcode(state.MEMWB.instr) == LW){
            // if write back destination matches field0 or field1 of new state instruction
            if (field0(newState.EXMEM.instr) == MEMWB_det_and_forward){
                // set reg0Value to write back value
                reg0Value = state.MEMWB.writeData;
            }
            if (field1(newState.EXMEM.instr) == MEMWB_det_and_forward){
                // set reg1Value to write back value
                reg1Value = state.MEMWB.writeData;
            }
        }

        // repeate for EXMEM
        if (opcode(state.EXMEM.instr) == ADD || opcode(state.EXMEM.instr) == NOR || opcode(state.EXMEM.instr) == LW){
            // if write back destination matches field0 or field1 of new state instruction
            if (field0(newState.EXMEM.instr) == EXEM_det_and_forward){
                // set reg0Value to write back value
                reg0Value = state.EXMEM.aluResult;
            }
            if (field1(newState.EXMEM.instr) == EXEM_det_and_forward){
                // set reg1Value to write back value
                reg1Value = state.EXMEM.aluResult;
            }
        }


        // determine aluResult based on opcode
        if (opcode(newState.EXMEM.instr) == ADD){ // is ADD
            newState.EXMEM.aluResult = reg0Value + reg1Value; // set aluResult to reg0Value + reg1Value (aka add)
        }
        else if (opcode(newState.EXMEM.instr) == LW || opcode(newState.EXMEM.instr) == SW){ // is LW
            newState.EXMEM.aluResult = reg0Value + state.IDEX.offset; // set aluResult to reg0Value + current state offset
        }
        else if (opcode(newState.EXMEM.instr) == BEQ){ // is BEQ
            newState.EXMEM.aluResult = reg0Value - reg1Value; // set aluResult to reg0Value - reg1Value (cause beq magic)
            // if reg0Value == reg1Value
            if (reg0Value == reg1Value){
                newState.EXMEM.eq = 1; // set eq to 1
            }
            else{ // reg0Value != reg1Value
                newState.EXMEM.eq = 0; // set eq to 0
            }

        }
        else if (opcode(newState.EXMEM.instr) == NOR){ // is NOR
            newState.EXMEM.aluResult = ~(reg0Value | reg1Value); // set aluResult to ~(reg0Value | reg1Value) (aka bitwise nor)
            // print result
            //printf("nor result: %d\n", newState.EXMEM.aluResult);
        }

        if (opcode(newState.EXMEM.instr) != NOOP){ // not a NOOP
            newState.EXMEM.valB = reg1Value; // set valB to reg1Value
        }
        else{
            newState.EXMEM.aluResult = 0; // reset valB to 0
        }

        /* --------------------- MEM stage --------------------- */
        // MEM = Memory access
        newState.MEMWB.instr = state.EXMEM.instr; // new state stage gets instruction from previous stage
        // newState.MEMWB.writeData = state.EXMEM.aluResult; // new state stage gets aluResult from previous stage

        // opcode operations
        if (opcode(newState.MEMWB.instr) == LW){
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult]; // set writeData to dataMem at aluResult
        }
        else if (opcode(newState.MEMWB.instr) == SW){ 
            // memory is changed in this stage and the location was calculated in the previous stage through the alu
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.valB; // set dataMem at aluResult to valB
        }
        else if (opcode(newState.MEMWB.instr) == BEQ){
            // branch could have already been taken in the previous stage
            // if (state.EXMEM.aluResult == 0){ // if aluResult is 0
            //     // fill pipline with noops so control hazard doesnt occur
            //     newState.IFID.instr = NOOPINSTR; // set IFID instruction to NOOP
            //     newState.IDEX.instr = NOOPINSTR; // set IDEX instruction to NOOP
            //     newState.EXMEM.instr = NOOPINSTR; // set EXMEM instruction to NOOP
            //     newState.pc = state.EXMEM.branchTarget; // set pc to branchTarget
            // }

            // check using eq 
            if (state.EXMEM.eq == 1){ // if eq is true
                // fill pipline with noops so control hazard doesnt occur
                newState.IFID.instr = NOOPINSTR; // set IFID instruction to NOOP
                newState.IDEX.instr = NOOPINSTR; // set IDEX instruction to NOOP
                newState.EXMEM.instr = NOOPINSTR; // set EXMEM instruction to NOOP
                newState.pc = state.EXMEM.branchTarget; // set pc to branchTarget
            }
        }
        else if (opcode(newState.MEMWB.instr) != NOOP && opcode(newState.MEMWB.instr) != HALT){ // all instructions except noop and halt
            newState.MEMWB.writeData = state.EXMEM.aluResult; // set writeData to aluResult
        }
        else{
            newState.MEMWB.writeData = 0; // reset writeData to 0
        }

        /* ---------------------- WB stage --------------------- */
        // WB = Register write back
        newState.WBEND.instr = state.MEMWB.instr; // new state stage gets instruction from previous stage
        newState.WBEND.writeData = state.MEMWB.writeData; // new state stage gets writeData from previous stage


        // two write back cases
        // add and nor (effectivly the same)
        // lw
        // fuck me i had exmem for nor instesad of wbend
        if (opcode(newState.WBEND.instr) == ADD || opcode(newState.WBEND.instr) == NOR){
            newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData; // set reg at field2 to writeData
        }
        // changed to if instead of else if
        if (opcode(newState.WBEND.instr) == LW){
            newState.reg[field1(state.MEMWB.instr)] = state.MEMWB.writeData; // set reg at field1 to writeData
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
