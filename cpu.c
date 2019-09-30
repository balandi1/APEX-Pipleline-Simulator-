/*
 *  cpu.c
 *  Contains APEX cpu pipeline implementation
 *
 *  Author :
 *  Bhargavi Hanumant Alandikar (balandi1@binghamton.edu)
 *  State University of New York, Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

/* Set this flag to 1 to enable debug messages */
int ENABLE_DEBUG_MESSAGES=0;

int inputClockCycles=0;
int haltExec=0;
int forwardIndex=0;
int prevLoad=0;
int bTaken=0;
int intFuBusy=0;
int memFuBusy=0;
int mulFuBusy=0;
int mulClock=0;
int memClock=0;

int robHead=-1;
int robTail=-1;

int lsqHead=-1;
int lsqTail=-1;

int cfidHead=-1;
int cfidTail=-1;
int ctrlOccur=0;

int haltAtRobHead=0;
int crossOver=0;

CPU_Stage tempRobStage;
CPU_Stage tempRobStage_1;
int instRetired;
int instRetired_1;
/*
 * This function creates and initializes APEX cpu.
 */
APEX_CPU*
APEX_cpu_init(const char* filename)
{
  if (!filename) {
    return NULL;
  }

  APEX_CPU* cpu = malloc(sizeof(*cpu));
  if (!cpu) {
    return NULL;
  }

  /* Initialize PC, Registers and all pipeline stages */
  cpu->pc = 4000;
  memset(cpu->stage, 0, sizeof(CPU_Stage) * NUM_STAGES);
  memset(cpu->data_memory, 0, sizeof(int) * 4000);
  memset(cpu->urf_regs,0,sizeof(CPU_Register) * 40);
  memset(cpu->rob_list,0,sizeof(CPU_ROB) * 32);
  memset(cpu->lsq_list,0,sizeof(CPU_LSQ) * 20);
  memset(cpu->iq_list,0,sizeof(CPU_IQ) * 16);
  memset(cpu->rat,0,sizeof(front_rename_table) * 17);
  memset(cpu->rRat,0,sizeof(bak_rename_table) * 17);
  
  /* Parse input file and create code memory */
  cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);

  if (!cpu->code_memory) {
    free(cpu);
    return NULL;
  }

  if (ENABLE_DEBUG_MESSAGES) {
    fprintf(stderr,
            "APEX_CPU : Initialized APEX CPU, loaded %d instructions\n",
            cpu->code_memory_size);
    fprintf(stderr, "APEX_CPU : Printing Code Memory\n");
    printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode", "rd", "rs1", "rs2", "imm");

    for (int i = 0; i < cpu->code_memory_size; ++i) {
      printf("%-9s %-9d %-9d %-9d %-9d\n",
             cpu->code_memory[i].opcode,
             cpu->code_memory[i].rd,
             cpu->code_memory[i].rs1,
             cpu->code_memory[i].rs2,
             cpu->code_memory[i].imm);
    }
  }

  /* Make all stages busy except Fetch stage, initally to start the pipeline */
  for (int i = 1; i < NUM_STAGES; ++i) {
    cpu->stage[i].busy = 1;
  }
  
  for (int i = 0; i < 40; i++) {
    cpu->urf_regs[i].isFree = 1;
	cpu->urf_regs[i].valid=1;
  }

  for (int i = 0; i < 16; i++) {
    cpu->rat[i].urf_reg = 100;
  }
  
  cpu->data_memory[37]=0;
  return cpu;
}

/*
 * This function de-allocates APEX cpu.
 */
void
APEX_cpu_stop(APEX_CPU* cpu)
{
  free(cpu->code_memory);
  free(cpu);
}

/* Converts the PC(4000 series) into
 * array index for code memory
 */
int
get_code_index(int pc)
{
  return (pc - 4000) / 4;
}

static void
print_instruction(CPU_Stage* stage,APEX_CPU* cpu)
{
  if (strcmp(stage->opcode, "STORE") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rs1, stage->rs2, stage->imm);
	  printf("\t[%s,U%d,U%d,#%d] ", stage->opcode,stage->urf_rs1_reg,stage->urf_rs2_reg,stage->imm);
  }

  if (strcmp(stage->opcode, "LOAD") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1, stage->imm);
	   printf("\t[%s,U%d,U%d,#%d] ",stage->opcode,stage->urf_dest_reg,stage->urf_rs1_reg,stage->imm);
  }
  
  if (strcmp(stage->opcode, "ADDL") == 0 || strcmp(stage->opcode, "SUBL") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1, stage->imm);
	   printf("\t[%s,U%d,U%d,#%d] ",stage->opcode,stage->urf_dest_reg,stage->urf_rs1_reg,stage->imm);
  }
  
  if (strcmp(stage->opcode, "MOVC") == 0) {
    printf("%s,R%d,#%d ", stage->opcode, stage->rd, stage->imm);
	printf("\t[%s,U%d,#%d] ", stage->opcode, stage->urf_dest_reg, stage->imm);
  }
  if (strcmp(stage->opcode, "ADD") == 0 || strcmp(stage->opcode, "SUB") == 0
      || strcmp(stage->opcode, "MUL") == 0 || strcmp(stage->opcode, "AND") == 0
	  || strcmp(stage->opcode, "OR") == 0 || strcmp(stage->opcode, "EX-OR") == 0) {
    printf("%s,R%d,R%d,R%d ", stage->opcode, stage->rd, stage->rs1, stage->rs2);
	printf("\t[%s,U%d,U%d,U%d] ", stage->opcode, stage->urf_dest_reg, stage->urf_rs1_reg,stage->urf_rs2_reg);
  }
  if (strcmp(stage->opcode, "BZ") == 0 || strcmp(stage->opcode, "BNZ") == 0) {
    printf("%s,#%d ", stage->opcode, stage->imm);
	printf("\t[%s,#%d] ",stage->opcode,stage->imm);
  }
  if (strcmp(stage->opcode, "JUMP") == 0) {
    printf("%s,R%d,#%d ", stage->opcode, stage->rs1, stage->imm);
	printf("\t[%s,U%d,#%d] ",stage->opcode,stage->urf_rs1_reg,stage->imm);
  }
  
  if (strcmp(stage->opcode, "JAL") == 0) {
    printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rd ,stage->rs1, stage->imm);
	printf("\t[%s,U%d,U%d,#%d] ",stage->opcode,stage->urf_dest_reg,stage->urf_rs1_reg,stage->imm);
  }
  
  if (strcmp(stage->opcode, "HALT") == 0) {
    printf("%s,", stage->opcode);
  }
}


static void print_fetch(CPU_Stage* stage)
{
  if (strcmp(stage->opcode, "STORE") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rs1, stage->rs2, stage->imm);
  }

  if (strcmp(stage->opcode, "LOAD") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1, stage->imm);
  }
  
  if (strcmp(stage->opcode, "ADDL") == 0 || strcmp(stage->opcode, "SUBL") == 0) {
    printf(
      "%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1, stage->imm);
  }
  
  if (strcmp(stage->opcode, "MOVC") == 0) {
    printf("%s,R%d,#%d ", stage->opcode, stage->rd, stage->imm);
  }
  if (strcmp(stage->opcode, "ADD") == 0 || strcmp(stage->opcode, "SUB") == 0
      || strcmp(stage->opcode, "MUL") == 0 || strcmp(stage->opcode, "AND") == 0
	  || strcmp(stage->opcode, "OR") == 0 || strcmp(stage->opcode, "EX-OR") == 0) {
    printf("%s,R%d,R%d,R%d ", stage->opcode, stage->rd, stage->rs1, stage->rs2);
  }
  if (strcmp(stage->opcode, "BZ") == 0 || strcmp(stage->opcode, "BNZ") == 0) {
    printf("%s,#%d ", stage->opcode, stage->imm);
  }
  if (strcmp(stage->opcode, "JUMP") == 0) {
    printf("%s,R%d,#%d ", stage->opcode, stage->rs1, stage->imm);
  }
  
  if (strcmp(stage->opcode, "JAL") == 0) {
    printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rd ,stage->rs1, stage->imm);
  }
  if (strcmp(stage->opcode, "HALT") == 0) {
    printf("%s,", stage->opcode);
  }
}
/* Debug function which dumps the cpu stage content
 */
static void
print_stage_content(char* name, CPU_Stage* stage,APEX_CPU* cpu)
{
	if(!stage->stalled)
	{
		if(stage->pc==0)
			printf("%-15s: ", name);
		else if(strcmp(name,"")!=0)
			printf("%-15s: pc(%d) ", name, stage->pc);
		else
			printf("pc(%d) ",stage->pc);
		
		if(strcmp(name,"Fetch")!=0)
			print_instruction(stage,cpu);
		else
			print_fetch(stage);
	}
	else if(strcmp(name,"")!=0)
		printf("%-15s: ", name);
		
  printf("\n");
}

/*
 *  Fetch Stage of APEX Pipeline
 */
int
fetch(APEX_CPU* cpu)
{
  CPU_Stage* stage = &cpu->stage[F];
  if (!stage->busy && !stage->stalled) { 
	stage->busy=1;
    
	/* Store current PC in fetch latch , handle the old pc value for branch instruction*/
	if(cpu->old_pc > 0)
		stage->pc = cpu->old_pc;
	else
		stage->pc=cpu->pc;

    /* Index into code memory using this pc and copy all instruction fields into
     * fetch latch
     */
    APEX_Instruction* current_ins = &cpu->code_memory[get_code_index(stage->pc)];
    strcpy(stage->opcode, current_ins->opcode);
    stage->rd = current_ins->rd;
    stage->rs1 = current_ins->rs1;
    stage->rs2 = current_ins->rs2;
    stage->imm = current_ins->imm;
    
	/* Update PC for next instruction, if there is not stalling due to mul instruction in EX stage*/
	if(cpu->old_pc==0)
		cpu->pc += 4;
	stage->busy=0;
		
	/* Copy data from fetch latch to decode latch*/
	cpu->stage[DRF] = cpu->stage[F];
	
  }
  else if(cpu->mulClock==0)
  {
	  /* Copy data from fetch latch to decode latch*/
		cpu->stage[DRF] = cpu->stage[F];
  }


  if (ENABLE_DEBUG_MESSAGES) {
      print_stage_content("Fetch", stage,cpu);
    }
  return 0;
}

/*
 *  Decode Stage of APEX Pipeline
 */
int
decode(APEX_CPU* cpu)
{
  CPU_Stage* stage = &cpu->stage[DRF];
  if(stage->pc>0)
  {  
		if (!stage->busy && !stage->stalled) {
		
			if(strcmp(stage->opcode,"")!=0)
			{
				stage->busy=1;
			
				// read the source values from urf and rename the destination register
				readRegValue(cpu);
				int conditionTrue=regRename(cpu);
				stage->setIq=conditionTrue;
				
				stage->cfidIndex=cfidTail;
				printf("cfid assigned=%d\n",stage->cfidIndex);
				
				if(strcmp(stage->opcode,"JUMP")==0 || strcmp(stage->opcode,"JAL")==0 
				|| strcmp(stage->opcode,"BZ")==0 || strcmp(stage->opcode,"BNZ")==0)
				{
					if(cfidHead==-1 && cfidTail==-1)
					{
						cfidHead=0;
						cfidTail=0;
					}
					else if(cfidTail==7)
						cfidTail=0;
					else
						cfidTail++;
				}
				
				stage->busy=0;
			}
			cpu->stage[IQ] = cpu->stage[DRF];
		}
  }
	if (ENABLE_DEBUG_MESSAGES) {
      print_stage_content("Decode", stage,cpu);
    }
	return 0;
}

int iqStage(APEX_CPU* cpu)
{
	
	CPU_Stage* stage = &cpu->stage[IQ];
	 int lsqIndex,iqIndex,robIndex;
	 
	if (stage->stalled) {
		return 0;
	}
	 if(strcmp(stage->opcode,"HALT")==0)
	 {
		robIndex=setRobEntry(cpu);

		(&cpu->rob_list[robIndex])->status=1;
		return 0;
	 }
	if(stage->setIq)
	{
		if(strcmp(stage->opcode,"")==0)
			return 0;
		
		iqIndex=setIQEntry(cpu);
		if(iqIndex>-1)
		{
			robIndex=setRobEntry(cpu);
			(&cpu->iq_list[iqIndex])->robIndex=robIndex;
			(&cpu->rob_list[robIndex])->iqIndex=iqIndex;
			
			if(strcmp(stage->opcode,"LOAD")==0 || strcmp(stage->opcode,"STORE")==0)
			{
				lsqIndex=setLSQEntry(cpu,iqIndex);
				(&cpu->lsq_list[lsqIndex])->robIndex=robIndex;
				(&cpu->iq_list[iqIndex])->lsqIndex=lsqIndex;
				(&cpu->rob_list[robIndex])->lsqIndex=lsqIndex;
			}
		}
	}
	
	return 0;
}

/* New code */
int readRegValue(APEX_CPU* cpu)
{
	CPU_Stage* decodeStage = &cpu->stage[DRF];
	
	int urf_index=(&cpu->rat[decodeStage->rs1])->urf_reg;
	decodeStage->urf_rs1_reg=urf_index;
	
	if((&cpu->urf_regs[urf_index])->valid)
	{
		decodeStage->rs1_value=(&cpu->urf_regs[urf_index])->value;
		decodeStage->rs1_value_valid=1;
		decodeStage->urf_rs1_reg=urf_index;
	}
	else
	{
		
		CPU_Forward_Bus fwdEntry=readFrmFwdBus(cpu,urf_index);
		if(fwdEntry.valid>0)
		{
			decodeStage->rs1_value=fwdEntry.rs_value;
			decodeStage->rs1_value_valid=1;
		}
	}
	
	int urf_index_2=(&cpu->rat[decodeStage->rs2])->urf_reg;
	decodeStage->urf_rs2_reg=urf_index_2;
	
	if((&cpu->urf_regs[urf_index_2])->valid)
	{
		decodeStage->rs2_value=(&cpu->urf_regs[urf_index_2])->value;
		decodeStage->rs2_value_valid=1;
		decodeStage->urf_rs2_reg=urf_index_2;
	}
	else
	{
		
		CPU_Forward_Bus fwdEntry=readFrmFwdBus(cpu,urf_index_2);
		if(fwdEntry.valid>0)
		{
			decodeStage->rs2_value=fwdEntry.rs_value;
			decodeStage->rs2_value_valid=1;
		}
		
	}
	
	if(strcmp(decodeStage->opcode,"MOVC")==0 || strcmp(decodeStage->opcode,"BZ")==0 || strcmp(decodeStage->opcode,"BNZ")==0)
	{
		decodeStage->rs1_value_valid=1;
		decodeStage->rs2_value_valid=1;
	}
	if(strcmp(decodeStage->opcode,"LOAD")==0 || strcmp(decodeStage->opcode,"ADDL")==0 
		|| strcmp(decodeStage->opcode,"SUBL")==0 || strcmp(decodeStage->opcode,"JUMP")==0 || strcmp(decodeStage->opcode,"JAL")==0)
		decodeStage->rs2_value_valid=1;
	
	return 0;
}

int regRename(APEX_CPU* cpu)
{
	int freeRegFound=0;
	CPU_Stage* decodeStage=&cpu->stage[DRF];
	if(strcmp(decodeStage->opcode,"STORE")!=0 && strcmp(decodeStage->opcode,"")!=0 
	&& strcmp(decodeStage->opcode,"JUMP")!=0 && strcmp(decodeStage->opcode,"BZ")!=0 
	&& strcmp(decodeStage->opcode,"BNZ")!=0 && strcmp(decodeStage->opcode,"HALT")!=0)
	{
		for(int i=0;i<40;i++)
		{
		
			
			if((&cpu->urf_regs[i])->isFree)
			{
				freeRegFound=1;
				int archDest=decodeStage->rd;
				decodeStage->last_saved_urf_reg=(&cpu->rat[archDest])->urf_reg;
				decodeStage->last_saved_urf_allocated=(&cpu->rat[archDest])->allocated;
				
				(&cpu->rat[archDest])->urf_reg=i;
				(&cpu->rat[archDest])->allocated=1;
				
				if(strcmp(decodeStage->opcode,"LOAD")!=0)
				{
					if(!(&cpu->rat[16])->branch_available)
					{
						(&cpu->rat[16])->urf_reg=i;
						(&cpu->rat[16])->allocated=1;
					}
				}
				
				
				(&cpu->urf_regs[i])->isFree=0;
				
				decodeStage->urf_dest_reg=i;
				(&cpu->urf_regs[i])->valid=0;
				decodeStage->urf_dest_valid=1;
				break;
			}
		}
		
	}
	else
	{
		if(strcmp(decodeStage->opcode,"BZ")==0  || strcmp(decodeStage->opcode,"BNZ")==0)
		{
			(&cpu->rat[16])->branch_available=1;
		}			
		freeRegFound=1;
	}
	
	
	return freeRegFound;
}

int setIQEntry(APEX_CPU* cpu)
{
	int iqIndex=-1;
	CPU_Stage decodeStage=cpu->stage[IQ];
	for(int i=0;i<16;i++)
	{
		if(!(&cpu->iq_list[i])->allocated)
		{
			(&cpu->iq_list[i])->allocated=1;
			(&cpu->iq_list[i])->clockCycle=cpu->clock;
		
			
			(&cpu->iq_list[i])->stage=decodeStage;
			strcpy((&cpu->iq_list[i])->fuType,getfuType(decodeStage));
			
			
		
			(&cpu->iq_list[i])->src1_valid=(&decodeStage)->rs1_value_valid;
			(&cpu->iq_list[i])->src2_valid=(&decodeStage)->rs2_value_valid;
			iqIndex=i;
			
			
			// set lsq and rob index when implemented
			break;
		}
	}
	
	return iqIndex;
}

int FwdToIssueQueue(APEX_CPU* cpu)
{
	for(int i=0;i<16;i++)
	{
		if((&cpu->iq_list[i])->allocated)
		{
			if(!((&cpu->iq_list[i])->stage.rs1_value_valid))
			{
				CPU_Forward_Bus fwdEntry=readFrmFwdBus(cpu,(&cpu->iq_list[i])->stage.urf_rs1_reg);
				if(fwdEntry.valid>0)
				{
					(&cpu->iq_list[i])->stage.rs1_value=fwdEntry.rs_value;
					(&cpu->iq_list[i])->stage.rs1_value_valid=1;
					(&cpu->iq_list[i])->src1_valid=1;
				}
			}
			
			if(!((&cpu->iq_list[i])->stage.rs2_value_valid))
			{
				CPU_Forward_Bus fwdEntry_2=readFrmFwdBus(cpu,(&cpu->iq_list[i])->stage.urf_rs2_reg);
				if(fwdEntry_2.valid>0)
				{
					(&cpu->iq_list[i])->stage.rs2_value=fwdEntry_2.rs_value;
					(&cpu->iq_list[i])->stage.rs2_value_valid=1;
					(&cpu->iq_list[i])->src2_valid=1;
				}
			}
		}
	}
	return 0;
}

int FwdToLSQ(APEX_CPU* cpu)
{
	for(int i=0;i<20;i++)
	{
		if((&cpu->lsq_list[i])->allocated)
		{
			if(!((&cpu->lsq_list[i])->stage.rs1_value_valid))
			{
				CPU_Forward_Bus fwdEntry=readFrmFwdBus(cpu,(&cpu->lsq_list[i])->stage.urf_rs1_reg);
				if(fwdEntry.valid>0)
				{
					(&cpu->lsq_list[i])->stage.rs1_value=fwdEntry.rs_value;
					(&cpu->lsq_list[i])->stage.rs1_value_valid=1;
					(&cpu->lsq_list[i])->src1_valid=1;
				}
			}
		}
	}
	return 0;
}

int setLSQEntry(APEX_CPU* cpu,int iqIndex)
{
	int lsqIndex=-1;
	
	if(lsqHead==-1) 
	{
		lsqHead=0;
	}
	
	if(lsqTail==-1)
	{
		lsqTail=0;
	}
	else if(lsqTail==19)
		lsqTail=0;
	else
		lsqTail++;
	
	
	
	CPU_Stage decodeStage=cpu->stage[IQ];
	//for(int i=0;i<20;i++)
	//{
		
		if(!(&cpu->lsq_list[lsqTail])->allocated)
		{
			lsqIndex=lsqTail;
			(&cpu->lsq_list[lsqTail])->allocated=1;
			(&cpu->lsq_list[lsqTail])->stage=decodeStage;
			(&cpu->lsq_list[lsqTail])->iqIndex=iqIndex;
			(&cpu->lsq_list[lsqTail])->address_valid=0;
			
			(&cpu->lsq_list[lsqTail])->src1_valid=(&decodeStage)->rs1_value_valid;
			//(&cpu->lsq_list[lsqTail])->src2_valid=(&decodeStage)->rs2_value_valid;
			//lsqTail++;
		}
	//}
	
	return lsqIndex;
}


int setRobEntry(APEX_CPU* cpu)
{
	CPU_Stage decodeStage=cpu->stage[IQ];
	if(robHead==-1)
		robHead=0;
	if(robTail==-1)
		robTail=0;
	else if(robTail==31)
		robTail=0;
	else
		robTail++;
	
	(&cpu->rob_list[robTail])->stage=decodeStage;
	(&cpu->rob_list[robTail])->status=0;
	
	return robTail;
			
}

char* getfuType(CPU_Stage decodeStage)
{
	if(strcmp((&decodeStage)->opcode,"MUL")!=0)
	{
		return "IntFu";
	}
	else
		return "MulFu";
}

int intFuncUnit(APEX_CPU* cpu)
{
	int entrySelected=0;
	CPU_IQ *iqSelectedEntry;
	CPU_ROB *robSelectedEntry;
	CPU_Stage* dummyStage=malloc(sizeof(*dummyStage));
	dummyStage->stalled=1;
	CPU_LSQ *lsqEntry;
	if(!intFuBusy)
	{
		
		int readyIqIndex=getReadyIQIndex(cpu,"IntFu");
		// select an entry that satisfies all conditions for issue
		//for(int i=0;i<16;i++)
		//{
			if(readyIqIndex>-1)
			{
			CPU_IQ *iqEntry=(&cpu->iq_list[readyIqIndex]);
			//if(strcmp(iqEntry->fuType,"IntFu")==0)
			//{
				//if((cpu->clock-iqEntry->clockCycle)>=1)
				//{
					if(strcmp((&iqEntry->stage)->opcode,"STORE")==0)
					{
						if(iqEntry->allocated && iqEntry->src2_valid)
						{
							entrySelected=1;
							iqSelectedEntry=iqEntry;
							robSelectedEntry=(&cpu->rob_list[iqSelectedEntry->robIndex]);
							lsqEntry=(&cpu->lsq_list[iqSelectedEntry->lsqIndex]);
						}
					}
					else if(iqEntry->allocated && iqEntry->src1_valid && iqEntry->src2_valid)
					{
						entrySelected=1;
						iqSelectedEntry=iqEntry;
						robSelectedEntry=(&cpu->rob_list[iqSelectedEntry->robIndex]);
						lsqEntry=(&cpu->lsq_list[iqSelectedEntry->lsqIndex]);
						//break;
					}
				}
				//}
			//}
		//}
		

		// perform the operation for the selected issue queue entry
		if(entrySelected)
		{
			intFuBusy=1;
			if (strcmp((&iqSelectedEntry->stage)->opcode, "ADD") == 0) {
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value + (&iqSelectedEntry->stage)->rs2_value;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "ADDL") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value+(&iqSelectedEntry->stage)->imm;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "SUB") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value - (&iqSelectedEntry->stage)->rs2_value;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "SUBL") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value - (&iqSelectedEntry->stage)->imm;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "EX-OR") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value ^ (&iqSelectedEntry->stage)->rs2_value;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "OR") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value | (&iqSelectedEntry->stage)->rs2_value;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "AND") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value & (&iqSelectedEntry->stage)->rs2_value;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "LOAD") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value + (&iqSelectedEntry->stage)->imm;
				lsqEntry->stage=iqSelectedEntry->stage;
				lsqEntry->address_valid=1;
				
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "STORE") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs2_value + (&iqSelectedEntry->stage)->imm;
				lsqEntry->stage=iqSelectedEntry->stage;
				lsqEntry->address_valid=1;
				//robSelectedEntry->status=1;
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "MOVC") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->imm + 0;
			}
			
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "JUMP") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value + (&iqSelectedEntry->stage)->imm;
				cpu->old_pc=cpu->pc;
				cpu->pc=(&iqSelectedEntry->stage)->buffer;
				bTaken=1;
				ctrlOccur=1;
							
				
			}
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "JAL") == 0) {
				
				(&iqSelectedEntry->stage)->mem_address = (&iqSelectedEntry->stage)->rs1_value + (&iqSelectedEntry->stage)->imm;
				cpu->old_pc=cpu->pc;
				cpu->pc=(&iqSelectedEntry->stage)->mem_address;
				(&iqSelectedEntry->stage)->buffer=(&iqSelectedEntry->stage)->pc+4;
				bTaken=1;
				ctrlOccur=1;
							
				
			}
			
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "BZ") == 0) {
				
				// get the latest instance of zero flag from RAT or from forward bus
				int urf_reg=(&cpu->rat[16])->urf_reg;
				
				
				int zFlag=0;
				if((&cpu->urf_regs[urf_reg])->valid)
				{
					zFlag=(&cpu->urf_regs[urf_reg])->zFlag;
				
				}
				else
				{
					CPU_Forward_Bus fwdZFlag=readFrmFwdBus(cpu,urf_reg);
					if(fwdZFlag.valid)
						zFlag=fwdZFlag.zFlag;
				}
				
				if(zFlag)
				{
					(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->pc + (&iqSelectedEntry->stage)->imm;
					cpu->old_pc=cpu->pc;
					cpu->pc=(&iqSelectedEntry->stage)->buffer;
					bTaken=1;
					ctrlOccur=1;
				}

				(&cpu->rat[16])->branch_available=0;
				
			}
			
			else if (strcmp((&iqSelectedEntry->stage)->opcode, "BNZ") == 0) {
				
				// get the latest instance of zero flag from RAT or from forward bus
				int urf_reg=(&cpu->rat[16])->urf_reg;
				int zFlag=0;
				
				if((&cpu->urf_regs[urf_reg])->valid)
					zFlag=(&cpu->urf_regs[urf_reg])->zFlag;
				else
				{
					CPU_Forward_Bus fwdZFlag=readFrmFwdBus(cpu,urf_reg);
					if(fwdZFlag.valid)
						zFlag=fwdZFlag.zFlag;
				}
				
				if(!zFlag)
				{
					(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->pc + (&iqSelectedEntry->stage)->imm;
					cpu->old_pc=cpu->pc;
					cpu->pc=(&iqSelectedEntry->stage)->buffer;
					bTaken=1;
					ctrlOccur=1;
					
				}
				
				(&cpu->rat[16])->branch_available=0;
				
			}
			
			robSelectedEntry->stage=iqSelectedEntry->stage;
			
			if (strcmp((&iqSelectedEntry->stage)->opcode, "LOAD") != 0 )
			//&& strcmp((&iqSelectedEntry->stage)->opcode, "STORE") != 0) 
			{
			
			robSelectedEntry->status=1;
			
			if (strcmp((&iqSelectedEntry->stage)->opcode, "JUMP") != 0 && strcmp((&iqSelectedEntry->stage)->opcode, "BZ") != 0 
			    && strcmp((&iqSelectedEntry->stage)->opcode, "BNZ") != 0 && strcmp((&iqSelectedEntry->stage)->opcode, "STORE") != 0)
				writeOnFwdBus(cpu,iqSelectedEntry->stage);

			}
			intFuBusy=0;
			
			iqSelectedEntry->allocated=0;
			
			
		}
			
	}
	
	if (ENABLE_DEBUG_MESSAGES) {
		print_stage_content("EX_INT_FU",(entrySelected?(&(iqSelectedEntry->stage)):dummyStage),cpu);
    }
	return 0;
}

int getReadyIQIndex(APEX_CPU* cpu,char fuType[10])
{
	int minClock=0;
	int iqIndex=-1;
	for(int i=0;i<16;i++)
	{
		CPU_IQ *iqEntry=(&cpu->iq_list[i]);
		if(strcmp(iqEntry->fuType,fuType)==0)
		{
			if(iqEntry->allocated)
			{
				if((cpu->clock-iqEntry->clockCycle)>=1)
				{
					if(minClock==0)
					{
						minClock=iqEntry->clockCycle;
						iqIndex=i;
					}
					else if(iqEntry->clockCycle < minClock)
					{
						minClock=iqEntry->clockCycle;
						iqIndex=i;
					}
				}
			}
		}
	}
	
	return iqIndex;
}
int mulFuncUnit(APEX_CPU* cpu)
{
	int entrySelected=0;
	CPU_IQ *iqSelectedEntry;
	CPU_ROB *robSelectedEntry;
	CPU_Stage* dummyStage=malloc(sizeof(*dummyStage));
	dummyStage->stalled=1;
	if(!mulFuBusy)
	{
		
		int readyIqIndex=getReadyIQIndex(cpu,"MulFu");
		
		// select an entry that satisfies all conditions for issue
		//for(int i=0;i<16;i++)
		//{
			if(readyIqIndex>-1)
			{
			CPU_IQ *iqEntry=(&cpu->iq_list[readyIqIndex]);
			//if(strcmp(iqEntry->fuType,"MulFu")==0)
			//{
			//	if((cpu->clock-iqEntry->clockCycle)>=1)
			//	{
					if(iqEntry->allocated && iqEntry->src1_valid && iqEntry->src2_valid)
					{
						entrySelected=1;
						iqSelectedEntry=iqEntry;
						(&cpu->mulFuncUnit)->robIndex=iqSelectedEntry->robIndex;
						robSelectedEntry=(&cpu->rob_list[iqSelectedEntry->robIndex]);
						//break;
					}
			}
				//}
			//}
		//}
		
		if(entrySelected)
		{
			mulFuBusy=1;
			mulClock++;
			if (strcmp((&iqSelectedEntry->stage)->opcode, "MUL") == 0) {
				
				(&iqSelectedEntry->stage)->buffer = (&iqSelectedEntry->stage)->rs1_value*(&iqSelectedEntry->stage)->rs2_value;
				robSelectedEntry->stage=iqSelectedEntry->stage;
				//robSelectedEntry->status=1;
			}
			
			iqSelectedEntry->allocated=0;
			//writeOnFwdBus(cpu,(&iqSelectedEntry->stage));
		}
	}
	else
	{
		//if(mulClock==2)
		//{
		//	mulFuBusy=0;
		//	mulClock=0;
		//}
		//else
		//{
			entrySelected=1;
			//mulClock++;
			mulFuBusy=0;
			mulClock=0;
			robSelectedEntry=(&cpu->rob_list[(&cpu->mulFuncUnit)->robIndex]);
			robSelectedEntry->status=1;
			writeOnFwdBus(cpu,robSelectedEntry->stage);
		//}
			
		
	}
	
	if (ENABLE_DEBUG_MESSAGES) {
		print_stage_content("EX_MUL_FU",(entrySelected?(&robSelectedEntry->stage):dummyStage),cpu);
	}
	return 0;
}

int instAtRobHead(APEX_CPU* cpu)
{
	CPU_ROB* headRob=(&cpu->rob_list[robHead]);
	
	int nextRobIndex=robHead==31 ? 0: robHead+1;
	
	CPU_ROB* nextHeadRob=(&cpu->rob_list[nextRobIndex]);
	
	if(headRob->status)
	{
		if(strcmp((&headRob->stage)->opcode,"HALT")==0)
		{
			haltAtRobHead=1;
			//if(robHead==31)
			//	robHead=0;
			//else 
			//	robHead++;
		
			
			flushInstruction_halt(cpu,(&headRob->stage)->cfidIndex,1);
			(&cpu->stage[F])->stalled=1;
			(&cpu->stage[DRF])->stalled=1;
			(&cpu->stage[IQ])->stalled=1;
			tempRobStage=headRob->stage;
			tempRobStage_1.stalled=1;
			instRetired=1;
			
			robHead=-1;
			robTail=-1;
			
			return 0;
		}
		
		if(strcmp((&headRob->stage)->opcode,"STORE")!=0 && strcmp((&headRob->stage)->opcode,"")!=0 
		&& strcmp((&headRob->stage)->opcode,"JUMP")!=0 && strcmp((&headRob->stage)->opcode,"BZ")!=0 
		&& strcmp((&headRob->stage)->opcode,"BNZ")!=0 && strcmp((&headRob->stage)->opcode,"HALT")!=0)
		{
			(&cpu->urf_regs[(&headRob->stage)->urf_dest_reg])->value=(&headRob->stage)->buffer;
			
			
			if((&headRob->stage)->buffer==0)
				(&cpu->urf_regs[(&headRob->stage)->urf_dest_reg])->zFlag=1;
			
			// deallocate the previous urf instance of the architectural register
			int oldUrfReg=(&headRob->stage)->last_saved_urf_reg;
			if(oldUrfReg!=100)
			//&& (&cpu->urf_regs[oldUrfReg])->valid)
			{
				(&cpu->urf_regs[oldUrfReg])->isFree=1;
				//(&cpu->urf_regs[oldUrfReg])->valid=0;
			}
		
			(&cpu->urf_regs[(&headRob->stage)->urf_dest_reg])->valid=1;
		}
		//else
		//{
			if(strcmp((&headRob->stage)->opcode,"JUMP")==0 || strcmp((&headRob->stage)->opcode,"BZ")==0 
			|| strcmp((&headRob->stage)->opcode,"BNZ")==0 || strcmp((&headRob->stage)->opcode,"JAL")==0)
			{
				//crossOver=1;
			}
		//}
		
		tempRobStage=headRob->stage;
		
		if(robHead==31)
			robHead=0;
		else 
			robHead++;
		
		instRetired=1;
	}
	else
	{
		tempRobStage.stalled=1;
		return 0;
	}
	
	
	
	// inst commit for head + 1
	if(nextHeadRob->status)
	{
		if(strcmp((&nextHeadRob->stage)->opcode,"HALT")==0)
		{
			
			if(!bTaken)
			{
				haltAtRobHead=1;
				//if(robHead==31)
				//	robHead=0;
				//else 
				//	robHead++;
			
				flushInstruction_halt(cpu,(&nextHeadRob->stage)->cfidIndex,1);
				(&cpu->stage[F])->stalled=1;
			(&cpu->stage[DRF])->stalled=1;
			(&cpu->stage[IQ])->stalled=1;
				
				tempRobStage_1=nextHeadRob->stage;
				
				instRetired_1=1;
			}
				return 0;
			
		}
		
		if(strcmp((&nextHeadRob->stage)->opcode,"STORE")!=0 && strcmp((&nextHeadRob->stage)->opcode,"")!=0 
		&& strcmp((&nextHeadRob->stage)->opcode,"JUMP")!=0 && strcmp ((&nextHeadRob->stage)->opcode,"BZ")!=0 
		&& strcmp((&nextHeadRob->stage)->opcode,"BNZ")!=0 && strcmp  ((&nextHeadRob->stage)->opcode,"HALT")!=0)
		{
			(&cpu->urf_regs[(&nextHeadRob->stage)->urf_dest_reg])->value=(&nextHeadRob->stage)->buffer;
			
			
			if((&nextHeadRob->stage)->buffer==0)
				(&cpu->urf_regs[(&nextHeadRob->stage)->urf_dest_reg])->zFlag=1;
			
			// deallocate the previous urf instance of the architectural register
			int oldUrfReg=(&nextHeadRob->stage)->last_saved_urf_reg;
			if(oldUrfReg!=100)
			//&& (&cpu->urf_regs[oldUrfReg])->valid)
			{
				(&cpu->urf_regs[oldUrfReg])->isFree=1;
				//(&cpu->urf_regs[oldUrfReg])->valid=0;
			}
		
			(&cpu->urf_regs[(&nextHeadRob->stage)->urf_dest_reg])->valid=1;
		}
		
		tempRobStage_1=nextHeadRob->stage;
		
		if(robHead==31)
			robHead=0;
		else 
			robHead++;
		
		instRetired_1=1;
	}
	else
		tempRobStage_1.stalled=1;
	
	
	return 0;
}

int commitToRrat(APEX_CPU* cpu)
{
	if(instRetired)
	{
		if(strcmp(tempRobStage.opcode,"STORE")!=0 && strcmp(tempRobStage.opcode,"")!=0 
		&& strcmp(tempRobStage.opcode,"JUMP")!=0 && strcmp(tempRobStage.opcode,"BZ")!=0 
		&& strcmp(tempRobStage.opcode,"BNZ")!=0 && strcmp(tempRobStage.opcode,"HALT")!=0)
		{
			(&cpu->rRat[tempRobStage.rd])->urf_reg=tempRobStage.urf_dest_reg;
			(&cpu->rRat[tempRobStage.rd])->allocated=1;
			
			if(strcmp(tempRobStage.opcode,"LOAD")!=0)
			{
				(&cpu->rRat[16])->urf_reg=tempRobStage.urf_dest_reg;
				(&cpu->rRat[16])->allocated=1;
			}
			//instRetired=0;
		}
		
		instRetired=0;
	}
	
	//commitment for inst at rob head + 1
	if(instRetired_1)
	{
		if(strcmp(tempRobStage_1.opcode,"STORE")!=0 && strcmp(tempRobStage_1.opcode,"")!=0 
		&& strcmp(tempRobStage_1.opcode,"JUMP")!=0 && strcmp (tempRobStage_1.opcode,"BZ")!=0 
		&& strcmp(tempRobStage_1.opcode,"BNZ")!=0 && strcmp  (tempRobStage_1.opcode,"HALT")!=0)
		{
			(&cpu->rRat[tempRobStage_1.rd])->urf_reg=tempRobStage_1.urf_dest_reg;
			(&cpu->rRat[tempRobStage_1.rd])->allocated=1;
			
			if(strcmp(tempRobStage_1.opcode,"LOAD")!=0)
			{
				(&cpu->rRat[16])->urf_reg=tempRobStage_1.urf_dest_reg;
				(&cpu->rRat[16])->allocated=1;
			}
			//instRetired=0;
		}
		
		instRetired_1=0;
	}
	
	return 0;
}

int memFuncUnit(APEX_CPU* cpu)
{
	int entrySelected=0;
	CPU_LSQ *lsqSelectedEntry;
	CPU_ROB *robSelectedEntry;
	CPU_Stage* dummyStage=malloc(sizeof(*dummyStage));
	dummyStage->stalled=1;
	if(!memFuBusy)
	{
		// select an entry that satisfies all conditions for issue
		CPU_LSQ *lsqEntry=(&cpu->lsq_list[lsqHead]);
		if(lsqEntry->allocated && lsqEntry->src1_valid && lsqEntry->address_valid)
		{
			//entrySelected=1;
			lsqSelectedEntry=lsqEntry;
			(&cpu->memFuncUnit)->robIndex=lsqSelectedEntry->robIndex;
			robSelectedEntry=(&cpu->rob_list[lsqSelectedEntry->robIndex]);
			
			//if(lsqSelectedEntry->robIndex==robHead)
			//{
				entrySelected=1;
				
				if(lsqHead==19)
					lsqHead=0;
				else
					lsqHead++;
			//}
			
			
		}
		
		if(entrySelected)
		{
			//if(lsqSelectedEntry->robIndex==robHead)
			//{
				
				memFuBusy=1;
				memClock++;
				
				if (strcmp((&lsqSelectedEntry->stage)->opcode, "STORE") == 0) {
					
					cpu->data_memory[(&lsqSelectedEntry->stage)->buffer]=(&lsqSelectedEntry->stage)->rs1_value;
					
					robSelectedEntry->stage=lsqSelectedEntry->stage;
					//robSelectedEntry->status=1;
					lsqSelectedEntry->allocated=0;
					
				}
			//}
			if (strcmp((&lsqSelectedEntry->stage)->opcode, "LOAD") == 0) {
					
				(&lsqSelectedEntry->stage)->buffer=cpu->data_memory[(&lsqSelectedEntry->stage)->buffer];
				
				robSelectedEntry->stage=lsqSelectedEntry->stage;
				lsqSelectedEntry->allocated=0;
				//robSelectedEntry->status=1;
				
				//writeOnFwdBus(cpu,robSelectedEntry->stage);
			}
			
			
		}
	}
	else
	{
		if(memClock==2)
		{
			entrySelected=1;
			memFuBusy=0;
			memClock=0;
			robSelectedEntry=(&cpu->rob_list[(&cpu->memFuncUnit)->robIndex]);
			robSelectedEntry->status=1;
			if (strcmp((&robSelectedEntry->stage)->opcode, "STORE") != 0)
			{
				writeOnFwdBus(cpu,robSelectedEntry->stage);
			}
		}
		else
		{
			entrySelected=1;
			memClock++;
			robSelectedEntry=(&cpu->rob_list[(&cpu->memFuncUnit)->robIndex]);
		}
			
		
	}
	
	if (ENABLE_DEBUG_MESSAGES) {
		print_stage_content("MEM_FU",(entrySelected?(&robSelectedEntry->stage):dummyStage),cpu);
	}
	return 0;
}


int writeOnFwdBus(APEX_CPU* cpu, CPU_Stage stage)
{
	int fIndex=-1;
	int regExist=checkFReg(cpu,stage.urf_dest_reg);
	fIndex=regExist>-1?regExist:forwardIndex;
	
	
	(&cpu->fBus[fIndex])->rs=stage.urf_dest_reg;
	(&cpu->fBus[fIndex])->rs_value=stage.buffer;
	(&cpu->fBus[fIndex])->valid=1;
	
		if(stage.buffer ==0)
		{
			(&cpu->fBus[fIndex])->zFlag=1;
		}
		else
			(&cpu->fBus[fIndex])->zFlag=0;
		
		
		forwardIndex= regExist==-1 ? (forwardIndex==2 ? 0 : forwardIndex+1) : (regExist==2 ? 0 : forwardIndex);
		
		return 0;
}

CPU_Forward_Bus readFrmFwdBus(APEX_CPU* cpu,int urf_reg)
{
	//int value;
	CPU_Forward_Bus fwdEntry;
	fwdEntry.valid=0;
	
	for(int i=0;i<3;i++)
	{
		if((&cpu->fBus[i])->rs==urf_reg)
		{
			//value=(&cpu->fBus[i])->rs_value;
			fwdEntry=cpu->fBus[i];
		}
	}
	return fwdEntry;
}

/*
 *  Execute Stage of APEX Pipeline
 */
int
execute(APEX_CPU* cpu)
{
  CPU_Stage* stage = &cpu->stage[EX];
  if (!stage->busy && !stage->stalled) {

	stage->busy=1;	
    /* Store */
    if (strcmp(stage->opcode, "STORE") == 0) {
				
		for(int i=0;i<3;i++)
			{
				if((&cpu->fBus[i])->rs==stage->rs2)
					stage->rs2_value=(&cpu->fBus[i])->rs_value;
					
			}
			
		stage->buffer = stage->rs2_value+stage->imm;
		
    }
	/* Load */
    if (strcmp(stage->opcode, "LOAD") == 0) {
		stage->buffer = stage->rs1_value+stage->imm;
    }	
    /* MOVC */
    if (strcmp(stage->opcode, "MOVC") == 0) {
		stage->buffer=stage->imm+0;
    }

	if (strcmp(stage->opcode, "ADD") == 0) {
		
		for(int i=0;i<3;i++)
		{
			if((&cpu->fBus[i])->rs==stage->rs1)
			{
				stage->rs1_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
				
			}
			if((&cpu->fBus[i])->rs==stage->rs2)
			{
				stage->rs2_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
			}
		}
		
		
		stage->buffer = stage->rs1_value+stage->rs2_value;
    }
	
	if (strcmp(stage->opcode, "SUB") == 0) {
		
		for(int i=0;i<3;i++)
		{
			if((&cpu->fBus[i])->rs==stage->rs1)
			{
				stage->rs1_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
				
			}
			if((&cpu->fBus[i])->rs==stage->rs2)
			{
				stage->rs2_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
			}
		}
		
		stage->buffer = stage->rs1_value - stage->rs2_value;
    }
	if (strcmp(stage->opcode, "MUL") == 0) {
		
		if(cpu->mulClock==1)
		{
			for(int i=0;i<3;i++)
			{
				if((&cpu->fBus[i])->rs==stage->rs1)
				{
					stage->rs1_value=(&cpu->fBus[i])->rs_value;
					
						stage->zFlag=(&cpu->fBus[i])->zFlag;
					
				}
				 if((&cpu->fBus[i])->rs==stage->rs2)
				{
					stage->rs2_value=(&cpu->fBus[i])->rs_value;
					
						stage->zFlag=(&cpu->fBus[i])->zFlag;
				}
			}
			
			stage->buffer = stage->rs1_value * stage->rs2_value;
			
		}
		
		
    }
	if (strcmp(stage->opcode, "AND") == 0) {
		
		for(int i=0;i<3;i++)
		{
			if((&cpu->fBus[i])->rs==stage->rs1)
			{
				stage->rs1_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
				
			}
			if((&cpu->fBus[i])->rs==stage->rs2)
			{
				stage->rs2_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
			}
		}
		
		stage->buffer = stage->rs1_value & stage->rs2_value;
    }
	if (strcmp(stage->opcode, "OR") == 0) {
		
		for(int i=0;i<3;i++)
		{
			if((&cpu->fBus[i])->rs==stage->rs1)
			{
				stage->rs1_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
				
			}
			if((&cpu->fBus[i])->rs==stage->rs2)
			{
				stage->rs2_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
			}
		}
		
		stage->buffer = stage->rs1_value | stage->rs2_value;
    }
	if (strcmp(stage->opcode, "EX-OR") == 0) {
		
		for(int i=0;i<3;i++)
		{
			if((&cpu->fBus[i])->rs==stage->rs1)
			{
				stage->rs1_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
				
			}
			if((&cpu->fBus[i])->rs==stage->rs2)
			{
				stage->rs2_value=(&cpu->fBus[i])->rs_value;
				
					stage->zFlag=(&cpu->fBus[i])->zFlag;
			}
		}
		
		int a=stage->rs1_value & stage->rs2_value;
		int b=~stage->rs1_value & ~stage->rs2_value;
		stage->buffer=~a & ~b;
		
    }
	
	// set the zero flag in forward bus if calculated value is zero
	if(strcmp(stage->opcode, "BNZ") != 0 
	   && strcmp(stage->opcode, "BZ") != 0 && strcmp(stage->opcode, "JUMP") != 0 
	   && strcmp(stage->opcode, "LOAD") != 0 && strcmp(stage->opcode, "STORE") != 0)
	{	
	
		int fIndex=-1;
		int regExist=checkFReg(cpu,stage->rd);
		fIndex=regExist>-1?regExist:forwardIndex;
		
		
		(&cpu->fBus[fIndex])->rs=stage->rd;
		(&cpu->fBus[fIndex])->rs_value=stage->buffer;
		
			if(stage->buffer ==0)
			{
				(&cpu->fBus[fIndex])->zFlag=1;
			}
			else
				(&cpu->fBus[fIndex])->zFlag=0;
			
			
			forwardIndex= regExist==-1 ? (forwardIndex==2 ? 0 : forwardIndex+1) : (regExist==2 ? 0 : forwardIndex);
			
		
		
	}
	
	if (strcmp(stage->opcode, "JUMP") == 0) {
		stage->buffer = stage->rs1_value + stage->imm;
		cpu->old_pc=cpu->pc;
		cpu->pc=stage->buffer;
		
	}
	
	if (strcmp(stage->opcode, "BZ") == 0) {
		
		if(stage->zFlag)
		{
			bTaken=1;
			stage->buffer = stage->pc + stage->imm;
			cpu->old_pc=cpu->pc;
			cpu->pc=stage->buffer;
			
		}
	}
	
	if (strcmp(stage->opcode, "BNZ") == 0) {
		if(!stage->zFlag)
		{
			bTaken=1;
			stage->buffer = stage->pc + stage->imm;
			cpu->old_pc=cpu->pc;
			cpu->pc=stage->buffer;
		}
	}
	
	if(cpu->mulClock==0)
	{	
			stage->busy=0;    
					
			/* Copy data from Execute latch to Memory latch*/
			cpu->stage[MEM] = cpu->stage[EX];
			(&cpu->stage[MEM])->stalled=0;
			(&cpu->stage[MEM])->pc=stage->pc;
	}
	else
		(&cpu->stage[MEM])->stalled=1;
    	
  }
  else
  {
	  /* Copy data from Execute latch to Memory latch*/
			cpu->stage[MEM] = cpu->stage[EX];
	(&cpu->stage[MEM])->stalled=1;
  }
  
  if (ENABLE_DEBUG_MESSAGES) {
     // print_stage_content("Execute", stage);
    }
  return 0;
}


/*
 *  Writeback Stage of APEX Pipeline
 */
int
writeback(APEX_CPU* cpu)
{
  CPU_Stage* stage = &cpu->stage[WB];
  if (!stage->busy && !stage->stalled) {

    /* Update register file */  
	if (strcmp(stage->opcode, "STORE") != 0 && strcmp(stage->opcode, "BNZ") != 0 
	    && strcmp(stage->opcode, "BZ") != 0 && strcmp(stage->opcode, "JUMP") != 0 && strcmp(stage->opcode, "HALT") != 0) {
      cpu->regs[stage->rd] = stage->buffer;
	  cpu->regs_valid[stage->rd]=1;
	  	  
	if(strcmp(stage->opcode, "BNZ") != 0 && strcmp(stage->opcode, "BZ") != 0)
	{	
		if(stage->buffer ==0)
			cpu->zeroFlag=1;
		else
			cpu->zeroFlag=0;
	}
	
    }
	
    cpu->ins_completed++;
	   
  }
  
  if (ENABLE_DEBUG_MESSAGES) {
      //print_stage_content("Writeback", stage);
    }
  return 0;
}

int flushInstruction(APEX_CPU* cpu,int cfidIndex,int isHalt)
{
	for(int i=cfidHead;i<=cfidTail;i++)
	{
		// flush instructions from iq 
		for(int j=0;j<16;j++)
		{
			if(((&cpu->iq_list[j])->stage).cfidIndex==i)
				(&cpu->iq_list[j])->allocated=0;
		}
		
		
	}
	
	
	// flush instructions from lsq
	for(int m=lsqTail;m>=lsqHead;m--)
	{
		if((&cpu->lsq_list[m])->allocated)
		{
			if(((&cpu->lsq_list[m])->stage).cfidIndex>=cfidIndex)
			{
				
				lsqTail--;
				(&cpu->lsq_list[m])->allocated=0;
			}
		}
	}
	
	
	//printf("LSQ flush done\n");
	int flushDone=0;
	
	
	
	// flush instructions from rob
	for(int m=robTail;m>=robHead;m--)
	{
		
		//if(m==robHead && strcmp(((&cpu->rob_list[m])->stage)->opcode,"HALT")==0)
			
		
		if(((&cpu->rob_list[m])->stage).cfidIndex>=cfidIndex)
		{
			flushDone=1;
			(&cpu->rob_list[m])->status=0;
			
			if(strcmp(((&cpu->rob_list[m])->stage).opcode,"STORE")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"JUMP")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"BZ")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"BNZ")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"HALT")!=0)
			{
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->valid=1;
				
				if(strcmp(((&cpu->rob_list[m])->stage).opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
					(&cpu->rat[16])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
			
				int present=checkInRat(cpu,((&cpu->rob_list[m])->stage).urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->valid=1;
				
				
			}
			
			robTail--;
		}
	}
	
	
	/// new code added
	if(!flushDone)
	{
		if(robTail<robHead)
		{
			for(int m=robTail;m!=robHead;)
			{
				
				if(((&cpu->rob_list[m])->stage).cfidIndex>=cfidIndex)
					
			{
			//flushDone=1;
			(&cpu->rob_list[m])->status=0;
			
			if(strcmp(((&cpu->rob_list[m])->stage).opcode,"STORE")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"JUMP")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"BZ")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"BNZ")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"HALT")!=0)
			{
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->valid=1;
				
				if(strcmp(((&cpu->rob_list[m])->stage).opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
					(&cpu->rat[16])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
			
				int present=checkInRat(cpu,((&cpu->rob_list[m])->stage).urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->valid=1;
				
				
			}
			
			if(robTail==0)
				robTail=31;
			else
				robTail--;
						
			m=robTail;
		}
		else
			break;
		
			}
		}
	}
	
	//
	//if(crossOver==1)
	//{
		if((robHead-robTail)==1)
			crossOver=2;
	//}
	
	
	// new code:  to reset the cfid index when all instruction after a taken branch are flushed
	if(!isHalt)
		cfidTail=cfidIndex;
	
	
	// handle renamed registers in previous decode stage
	if(strcmp((&cpu->stage[IQ])->opcode,"STORE")!=0 && strcmp((&cpu->stage[IQ])->opcode,"")!=0 
			&& strcmp((&cpu->stage[IQ])->opcode,"JUMP")!=0 && strcmp((&cpu->stage[IQ])->opcode,"BZ")!=0 
			&& strcmp((&cpu->stage[IQ])->opcode,"BNZ")!=0 && strcmp((&cpu->stage[IQ])->opcode,"HALT")!=0)
			{
				
				(&cpu->rat[(&cpu->stage[IQ])->rd])->urf_reg=(&cpu->stage[IQ])->last_saved_urf_reg;
				(&cpu->rat[(&cpu->stage[IQ])->rd])->allocated=(&cpu->stage[IQ])->last_saved_urf_allocated;
				
				(&cpu->urf_regs[(&cpu->stage[IQ])->last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[(&cpu->stage[IQ])->last_saved_urf_reg])->valid=1;
				
				if(strcmp((&cpu->stage[IQ])->opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=(&cpu->stage[IQ])->last_saved_urf_reg;
					(&cpu->rat[16])->allocated=(&cpu->stage[IQ])->last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->isFree=1;
				
				int present=checkInRat(cpu,(&cpu->stage[IQ])->urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->valid=1;
				
			}
	
	
	//intFuBusy=0;
	//mulFuBusy=0;
	//memFuBusy=0;
	
	return 0;
}


int flushInstruction_halt(APEX_CPU* cpu,int cfidIndex,int isHalt)
{
		for(int j=0;j<16;j++)
		{
				(&cpu->iq_list[j])->allocated=0;
		}
	
	// flush instructions from lsq
	lsqHead=-1;
	lsqTail=-1;
	
	int flushDone=0;
	
	// flush instructions from rob
	for(int m=robTail;m>=robHead;m--)
	{
		
		//if(m==robHead && strcmp(((&cpu->rob_list[m])->stage)->opcode,"HALT")==0)
			
		
		//if(((&cpu->rob_list[m])->stage).cfidIndex>=cfidIndex)
		//{
			flushDone=1;
			(&cpu->rob_list[m])->status=0;
			
			if(strcmp(((&cpu->rob_list[m])->stage).opcode,"STORE")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"JUMP")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"BZ")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"BNZ")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"HALT")!=0)
			{
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->valid=1;
				
				if(strcmp(((&cpu->rob_list[m])->stage).opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
					(&cpu->rat[16])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
			
				int present=checkInRat(cpu,((&cpu->rob_list[m])->stage).urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->valid=1;
				
				
			}
			
			robTail--;
		//}
	}
	
	
	/// new code added
	if(!flushDone)
	{
		if(robTail<robHead)
		{
			for(int m=robTail;m!=robHead;)
			{
				
				//if(((&cpu->rob_list[m])->stage).cfidIndex>=cfidIndex)
					
			//{
			//flushDone=1;
			(&cpu->rob_list[m])->status=0;
			
			if(strcmp(((&cpu->rob_list[m])->stage).opcode,"STORE")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"JUMP")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"BZ")!=0 
			&& strcmp(((&cpu->rob_list[m])->stage).opcode,"BNZ")!=0 && strcmp(((&cpu->rob_list[m])->stage).opcode,"HALT")!=0)
			{
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
				(&cpu->rat[((&cpu->rob_list[m])->stage).rd])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[((&cpu->rob_list[m])->stage).last_saved_urf_reg])->valid=1;
				
				if(strcmp(((&cpu->rob_list[m])->stage).opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=((&cpu->rob_list[m])->stage).last_saved_urf_reg;
					(&cpu->rat[16])->allocated=((&cpu->rob_list[m])->stage).last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
			
				int present=checkInRat(cpu,((&cpu->rob_list[m])->stage).urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[((&cpu->rob_list[m])->stage).urf_dest_reg])->valid=1;
				
				
			}
			
			if(robTail==0)
				robTail=31;
			else
				robTail--;
						
			m=robTail;
		//}
		
			}
		}
	}
	
	//
	//if(crossOver==1)
	//{
		if((robHead-robTail)==1)
			crossOver=2;
	//}
	
	
	
	// handle renamed registers in previous decode stage
	if(strcmp((&cpu->stage[IQ])->opcode,"STORE")!=0 && strcmp((&cpu->stage[IQ])->opcode,"")!=0 
			&& strcmp((&cpu->stage[IQ])->opcode,"JUMP")!=0 && strcmp((&cpu->stage[IQ])->opcode,"BZ")!=0 
			&& strcmp((&cpu->stage[IQ])->opcode,"BNZ")!=0 && strcmp((&cpu->stage[IQ])->opcode,"HALT")!=0)
			{
				
				(&cpu->rat[(&cpu->stage[IQ])->rd])->urf_reg=(&cpu->stage[IQ])->last_saved_urf_reg;
				(&cpu->rat[(&cpu->stage[IQ])->rd])->allocated=(&cpu->stage[IQ])->last_saved_urf_allocated;
				
				(&cpu->urf_regs[(&cpu->stage[IQ])->last_saved_urf_reg])->isFree=0;
				//(&cpu->urf_regs[(&cpu->stage[IQ])->last_saved_urf_reg])->valid=1;
				
				if(strcmp((&cpu->stage[IQ])->opcode,"LOAD")!=0)
				{
					(&cpu->rat[16])->urf_reg=(&cpu->stage[IQ])->last_saved_urf_reg;
					(&cpu->rat[16])->allocated=(&cpu->stage[IQ])->last_saved_urf_allocated;
					(&cpu->rat[16])->branch_available=0;
				}
				
				//if(!isHalt)
				//	(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->isFree=1;
				
				int present=checkInRat(cpu,(&cpu->stage[IQ])->urf_dest_reg);
				if(!present)
					(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->isFree=1;
				
				(&cpu->urf_regs[(&cpu->stage[IQ])->urf_dest_reg])->valid=1;
				
			}
	
	
	//intFuBusy=0;
	//mulFuBusy=0;
	//memFuBusy=0;
	
	return 0;
}

int checkInRat(APEX_CPU* cpu,int urf_dest_reg)
{
	int alreadyPresent=0;
	for(int i=0;i<16;i++)
	{
		if((&cpu->rat[i])->urf_reg==urf_dest_reg)
		{
			alreadyPresent=1;
			break;
		}
		
	}
	
	return alreadyPresent;
}
int
APEX_cpu_run(APEX_CPU* cpu)
{
	
	while (cpu->clock<inputClockCycles && (!haltAtRobHead || memFuBusy)) {
		if (ENABLE_DEBUG_MESSAGES) {
      printf("\n--------------------------------\n");
      printf("Clock Cycle #: %d\n", cpu->clock+1);
      printf("--------------------------------\n");
    }
	
	//commitToRrat(cpu);
	//instAtRobHead(cpu);
	
	if(ctrlOccur && bTaken)
	{
		bTaken=0;
		ctrlOccur=0;
		flushInstruction(cpu,(&cpu->stage[IQ])->cfidIndex,0);
		cpu->old_pc=0;
		CPU_Stage dummyStage;
		dummyStage.stalled=1;
		cpu->stage[DRF]=dummyStage;
		cpu->stage[IQ]=dummyStage;
	}
	
	commitToRrat(cpu);
	instAtRobHead(cpu);
	
	memFuncUnit(cpu);
	intFuncUnit(cpu);
	mulFuncUnit(cpu);

	
	iqStage(cpu);
	
	FwdToIssueQueue(cpu);
	FwdToLSQ(cpu);
	
	decode(cpu);
	fetch(cpu);	
	
	if (ENABLE_DEBUG_MESSAGES) {
		printIQ(cpu);
		printRat(cpu);
		printrRat(cpu);
		printLsq(cpu);
		printRob(cpu);
		printRetiredInstruction(cpu);
	}
	
	//if(strcmp((&cpu->stage[EX])->opcode,"HALT")==0 && !bTaken)
	//{
	//	(&cpu->stage[DRF])->stalled=1;		
	//	(&cpu->stage[F])->stalled=1;
	//}
    cpu->clock++;
	
	}

  return 0;
}

int APEX_cpu_start(const char* filename,const char* operation,const char* cycles)
{
	APEX_CPU* cpu;
	
	if(strstr(operation, "initialize") != NULL)
	{
		 cpu=APEX_cpu_init(filename);
		 
		 if (!cpu) {
			fprintf(stderr, "APEX_Error : Unable to initialize CPU\n");
			exit(1);
		}
	}
	if(strstr(operation, "display") != NULL)
	{
		ENABLE_DEBUG_MESSAGES=1;
		
		cpu=APEX_cpu_init(filename);
		if (!cpu) {
				fprintf(stderr, "APEX_Error : Unable to initialize CPU\n");
				exit(1);
			}
		
		inputClockCycles=atoi(cycles);	
		APEX_cpu_run(cpu);
		printRegs(cpu);
		printMemData(cpu);
	}
	if (strstr(operation, "simulate") != NULL) 
	{
		ENABLE_DEBUG_MESSAGES=0;
			cpu=APEX_cpu_init(filename);
			
			if (!cpu) {
				fprintf(stderr, "APEX_Error : Unable to initialize CPU\n");
				exit(1);
			}
		
		inputClockCycles=atoi(cycles);
		APEX_cpu_run(cpu);
		printRegs(cpu);
		printMemData(cpu);
	}
	
	return 0;
    
}

int printRegs(APEX_CPU* cpu)
{
	printf("\n========== STATE OF ARCHITECTURAL REGISTER FILE ==========\n");
	for(int i=0;i<40;i++)
	{
		if(!(cpu->urf_regs[i]).isFree)
			printf("|    URF[%d]\t|\tValue=%-9d|    Status=%-9s|\n",i,(cpu->urf_regs[i]).value,((cpu->urf_regs[i]).valid?"VALID":"INVALID"));
	}
	
	return 0;
}

int printMemData(APEX_CPU* cpu)
{
	printf("\n========== STATE OF DATA MEMORY ==========\n");
	for(int i=0;i<100;i++)
	{
		printf("|    MEM[%d]\t|\tData Value=%d\t|\n",i,cpu->data_memory[i]);
	}
	
	return 0;
}

int checkFReg(APEX_CPU* cpu,int stageRd)
{
	for(int i=0;i<3;i++)
	{
		if((&cpu->fBus[i])->rs==stageRd)
			return i;
	}
	return -1;
}

int printIQ(APEX_CPU* cpu)
{
	printf("\n========== Details of IQ (Issue Queue) State ==========\n");
	
	for(int i=0;i<16;i++)
	{
		if((&cpu->iq_list[i])->allocated)
		{
			char name[10];
			sprintf(name,"IQ[%d]",i);
			print_stage_content(name,(&(&cpu->iq_list[i])->stage),cpu);
		}
	}
	
	printf("\n=====================================================\n");
	
	return 0;
}

int printRat(APEX_CPU* cpu)
{
	printf("\n========== Details of RENAME TABLE (RAT) State ==========\n");
	for(int i=0;i<16;i++)
	{
		if((&cpu->rat[i])->allocated)
			printf("|    RAT[%d]\t-->\tU%d\t|\n",i,(&cpu->rat[i])->urf_reg);
	}
	
	
	printf("\n=====================================================\n");
	return 0;
}


int printrRat(APEX_CPU* cpu)
{
	printf("\n========== Details of RENAME TABLE (R-RAT) State ==========\n");
	for(int i=0;i<16;i++)
	{
		if((&cpu->rRat[i])->allocated)
			printf("|    R-RAT[%d]\t-->\tU%d\t|\n",i,(&cpu->rRat[i])->urf_reg);
	}
	
	
	printf("\n=====================================================\n");
	return 0;
}


int printRob(APEX_CPU* cpu)
{
	printf("\n========== Details of ROB (Reorder Buffer) State ==========\n");
	
	
	if(robHead<=robTail)
	{
		for(int i=robHead;i<=robTail;i++)
		{
			if(i!=-1)
			{
				char name[10];
				sprintf(name,"ROB[%d]",i);
				print_stage_content(name,(&(&cpu->rob_list[i])->stage),cpu);
			}
		}
	}
	else
	{
		if(crossOver==2)
		{
			if((robHead-robTail)==1)
				crossOver=2;
			else
				crossOver=0;
			printf("\n=====================================================\n");
			return 0;
		}
			
		int i=robHead;
		for(;i<=31;i++)
		{
				char name[10];
				sprintf(name,"ROB[%d]",i);
				print_stage_content(name,(&(&cpu->rob_list[i])->stage),cpu);
		}
		
		i--;
		if(i==31)
			i=0;
		for(;i<=robTail;i++)
		{
				char name[10];
				sprintf(name,"ROB[%d]",i);
				print_stage_content(name,(&(&cpu->rob_list[i])->stage),cpu);
		}
	}
	
	
	printf("\n=====================================================\n");
	
	return 0;
}

int printLsq(APEX_CPU* cpu)
{
	printf("\n========== Details of LSQ (Load-Store Queue) State ==========\n");
	for(int i=0;i<20;i++)
	{
		if((&cpu->lsq_list[i])->allocated)
		{
			char name[10];
			sprintf(name,"LSQ[%d]",i);
			print_stage_content(name,(&(&cpu->lsq_list[i])->stage),cpu);
		}
	}
	
	printf("\n=====================================================\n");
	
	return 0;
}

int printRetiredInstruction(APEX_CPU* cpu)
{
	printf("\n========== Details of ROB Retired Instructions ==========\n");
	print_stage_content("",&tempRobStage,cpu);
	print_stage_content("",&tempRobStage_1,cpu);
	printf("\n=====================================================\n");
	
	return 0;
}