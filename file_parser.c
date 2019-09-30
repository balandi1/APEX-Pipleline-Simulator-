/*
 *  file_parser.c
 *  Contains functions to parse input file and create
 *  code memory, you can edit this file to add new instructions
 *
 *  Author :
 *  Gaurav Kothari (gkothar1@binghamton.edu)
 *  State University of New York, Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

/*
 * This function is related to parsing input file
 *
 * Note : You are not supposed to edit this function
 */
static int
get_num_from_string(char* buffer)
{
  char str[16];
  int j = 0;
  for (int i = 1; buffer[i] != '\0'; ++i) {
    str[j] = buffer[i];
    j++;
  }
  str[j] = '\0';
  return atoi(str);
}

/*
 * This function is related to parsing input file
 *
 * Note : you can edit this function to add new instructions
 */
static void
create_APEX_instruction(APEX_Instruction* ins, char* buffer)
{
  char* token = strtok(buffer, ",");
  int token_num = 0;
  char tokens[6][128];
  while (token != NULL) {
    strcpy(tokens[token_num], token);
    token_num++;
    token = strtok(NULL, ",");
  }

  strcpy(ins->opcode, tokens[0]);

  if (strcmp(ins->opcode, "MOVC") == 0) {
    ins->rd = get_num_from_string(tokens[1]);
    ins->imm = get_num_from_string(tokens[2]);
  }

  if (strcmp(ins->opcode, "STORE") == 0) {
    ins->rs1 = get_num_from_string(tokens[1]);
    ins->rs2 = get_num_from_string(tokens[2]);
    ins->imm = get_num_from_string(tokens[3]);
  }
  
  if (strcmp(ins->opcode, "LOAD") == 0) {
    ins->rd = get_num_from_string(tokens[1]);
    ins->rs1 = get_num_from_string(tokens[2]);
    ins->imm = get_num_from_string(tokens[3]);
  }
  
  if (strcmp(ins->opcode, "ADD") == 0 || strcmp(ins->opcode, "SUB") == 0
      || strcmp(ins->opcode, "AND") == 0 || strcmp(ins->opcode, "OR") == 0
	  || strcmp(ins->opcode, "EX-OR") == 0 || strcmp(ins->opcode, "MUL") == 0) {
    ins->rd = get_num_from_string(tokens[1]);
	ins->rs1 = get_num_from_string(tokens[2]);
    ins->rs2 = get_num_from_string(tokens[3]);
  }
  
  if(strcmp(ins->opcode, "BZ") == 0 || strcmp(ins->opcode, "BNZ") == 0) {
    ins->imm = get_num_from_string(tokens[1]);
  }
  
  if (strcmp(ins->opcode, "JUMP") == 0) {
    ins->rs1 = get_num_from_string(tokens[1]);
    ins->imm = get_num_from_string(tokens[2]);
  }
  
  if (strcmp(ins->opcode, "JAL") == 0) {
    ins->rd = get_num_from_string(tokens[1]);
    ins->rs1 = get_num_from_string(tokens[2]);
    ins->imm = get_num_from_string(tokens[3]);
  }

  if (strcmp(ins->opcode, "ADDL") == 0 || strcmp(ins->opcode, "SUBL") == 0)
  {
	  ins->rd = get_num_from_string(tokens[1]);
	  ins->rs1 = get_num_from_string(tokens[2]);
	  ins->imm = get_num_from_string(tokens[3]);
  }

  
  
}

/*
 * This function is related to parsing input file
 *
 * Note : You are not supposed to edit this function
 */
APEX_Instruction*
create_code_memory(const char* filename, int* size)
{
  if (!filename) {
    return NULL;
  }

  FILE* fp = fopen(filename, "r");
  if (!fp) {
    return NULL;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t nread;
  int code_memory_size = 0;

  while ((nread = getline(&line, &len, fp)) != -1) {
    code_memory_size++;
  }
  *size = code_memory_size;
  if (!code_memory_size) {
    fclose(fp);
    return NULL;
  }

  APEX_Instruction* code_memory =
    malloc(sizeof(*code_memory) * code_memory_size);
  if (!code_memory) {
    fclose(fp);
    return NULL;
  }

  rewind(fp);
  int current_instruction = 0;
  while ((nread = getline(&line, &len, fp)) != -1) {
    create_APEX_instruction(&code_memory[current_instruction], line);
    current_instruction++;
  }

  free(line);
  fclose(fp);
  return code_memory;
}
