#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_
/**
 *  cpu.h
 *  Contains various CPU and Pipeline Data structures
 *
 *  Author :
 *  Gaurav Kothari (gkothar1@binghamton.edu)
 *  State University of New York, Binghamton
 */

enum
{
  F,
  DRF,
  IQ,
  EX,
  MEM,
  WB,
  NUM_STAGES
};

/* Format of an APEX instruction  */
typedef struct APEX_Instruction
{
  char opcode[128];	// Operation Code
  int rd;		    // Destination Register Address
  int rs1;		    // Source-1 Register Address
  int rs2;		    // Source-2 Register Address
  int imm;		    // Literal Value
} APEX_Instruction;

/* Model of CPU stage latch */
typedef struct CPU_Stage
{
  int pc;		    // Program Counter
  char opcode[128];	// Operation Code
  int rs1;		    // Source-1 Register Address
  int rs2;		    // Source-2 Register Address
  int rd;		    // Destination Register Address
  int imm;		    // Literal Value
  int rs1_value;	// Source-1 Register Value
  int rs2_value;	// Source-2 Register Value
  int buffer;		// Latch to hold some value
  int mem_address;	// Computed Memory Address
  int busy;		    // Flag to indicate, stage is performing some action
  int stalled;		// Flag to indicate, stage is stalled
  int zFlag;
  
  int urf_dest_reg;
  int urf_dest_valid;
  int rs1_value_valid;
  int rs2_value_valid;
  int urf_rs1_reg;
  int urf_rs2_reg;
  int setIq;
  int cfidIndex;
  
  int last_saved_urf_reg;
  int last_saved_urf_allocated;
  
} CPU_Stage;

/* Model of Forwarding Bus */
typedef struct CPU_Forward_Bus
{
	int rs;
	int rs_value;
	int zFlag;
	int valid;
} CPU_Forward_Bus;

typedef struct CPU_IQ
{
	int allocated;
	int clockCycle;
	CPU_Stage stage;
	char fuType[50];
	int src1_valid;
	int src2_valid;
	
	int robIndex;
	int lsqIndex;
}CPU_IQ;

typedef struct CPU_LSQ
{
	int allocated;
	int clockCycle;
	CPU_Stage stage;
	
	int src1_valid;
	int address_valid;
	int robIndex;
	int iqIndex;
}CPU_LSQ;

typedef struct CPU_ROB
{
	CPU_Stage stage;
	int status;  // status = 1 is valid
	int lsqIndex;
	int iqIndex;
}CPU_ROB;

typedef struct CPU_Register
{
	int isFree;
	int value;
	int renamed;
	int firstUse;
	int valid;
	int zFlag;
}CPU_Register;

typedef struct front_rename_table
{
	int allocated;
	int urf_reg;
	int branch_available;
}front_rename_table;


typedef struct bak_rename_table
{
	int allocated;
	int urf_reg;
}bak_rename_table;

typedef struct multiply_func_unit
{
	int robIndex;
}multiply_func_unit;

typedef struct mem_func_unit
{
	int robIndex;
}mem_func_unit;


typedef struct Branch_CFID_Map
{
	CPU_Stage stage;
	int cfidIndex;
}Branch_CFID_Map;

/* Model of APEX CPU */
typedef struct APEX_CPU
{
  /* Clock cycles elasped */
  int clock;
  
  /* Clock counter for multiply instruction */
  int mulClock;
  
  /* Current program counter */
  int pc;
  
  int old_pc;

  /* Integer register file */
  int regs[16];
  int regs_valid[16];
  
  /* Zero flag */
  int zeroFlag;

  /* Array of 5 CPU_stage */
  CPU_Stage stage[5];

  /* Code Memory where instructions are stored */
  APEX_Instruction* code_memory;
  int code_memory_size;

  /* Data Memory */
  int data_memory[4096];

  /* Some stats */
  int ins_completed;
  
  CPU_Forward_Bus fBus[3];
  
  CPU_Register urf_regs[40];
  CPU_IQ iq_list[16];
  CPU_LSQ lsq_list[20];
  CPU_ROB rob_list[32];
  front_rename_table rat[17];		// 17 entries (last one for zero flag, rest for 16 arch registers)
  bak_rename_table rRat[17];		// 17 entries (last one for zero flag, rest for 16 arch registers)
  multiply_func_unit mulFuncUnit;
  mem_func_unit memFuncUnit;
  
  Branch_CFID_Map b_cfid_map[16];

} APEX_CPU;




APEX_Instruction*
create_code_memory(const char* filename, int* size);

APEX_CPU*
APEX_cpu_init(const char* filename);

int APEX_cpu_start(const char* filename,const char* operation,const char* cycles);

int
APEX_cpu_run(APEX_CPU* cpu);

void
APEX_cpu_stop(APEX_CPU* cpu);

int
fetch(APEX_CPU* cpu);

int
decode(APEX_CPU* cpu);

int
execute(APEX_CPU* cpu);

int
memory(APEX_CPU* cpu);

int
writeback(APEX_CPU* cpu);

int printRegs(APEX_CPU* cpu);

int printMemData(APEX_CPU* cpu);

int checkFReg(APEX_CPU* cpu,int stageRd);

int readRegValue(APEX_CPU* cpu);

int regRename(APEX_CPU* cpu);

int setIQEntry(APEX_CPU* cpu);

int setLSQEntry(APEX_CPU* cpu,int iqIndex);

int setRobEntry(APEX_CPU* cpu);

char* getfuType(CPU_Stage decodeStage);

int intFuncUnit(APEX_CPU* cpu);

int printIQ(APEX_CPU* cpu);

int printRat(APEX_CPU* cpu);

int printRob(APEX_CPU* cpu);

int instAtRobHead(APEX_CPU* cpu);

int commitToRrat(APEX_CPU* cpu);

int printLsq(APEX_CPU* cpu);

int printrRat(APEX_CPU* cpu);

int writeOnFwdBus(APEX_CPU* cpu, CPU_Stage stage);

CPU_Forward_Bus readFrmFwdBus(APEX_CPU* cpu,int urf_reg);

int FwdToLSQ(APEX_CPU* cpu);

int FwdToIssueQueue(APEX_CPU* cpu);

int getReadyIQIndex(APEX_CPU* cpu,char fuType[10]);

int printRetiredInstruction(APEX_CPU* cpu);

int flushInstruction(APEX_CPU* cpu,int cfidIndex,int isHalt);

int checkInRat(APEX_CPU* cpu,int urf_dest_reg);

int flushInstruction_halt(APEX_CPU* cpu,int cfidIndex,int isHalt);
#endif
