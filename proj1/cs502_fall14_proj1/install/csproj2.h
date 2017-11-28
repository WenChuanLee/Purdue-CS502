#ifndef _CS_PROJ2_H_
#define _CS_PROJ2_H_

#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cgraph.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "tree.h"

#define TREE_NAME(t)	\
	tree_code_name[TREE_CODE(t)]

#define MAX_CASE_NUM	256	
#define MAX_LABEL_SIZE	256

typedef enum ActionType {
	ATCFG = 0,
	ATOPD,
	ATLHS,
	ATRHS,
} ActionType;

typedef enum NodeType {
	NTNORMAL = 0,
	NTIF,
	NTSWITCH,
	NTBIND,
	NTLABEL,
	NTGOTO,
} NodeType;

typedef struct Operand {
	/* Operand's name */
	char *name;
	/* Pointer to next operand */
	struct Operand *next;
} Operand;

typedef struct CFGNode {
	NodeType nType;
	/* Operands defined in this node */
	Operand *defOp;
	/* Operands used in this node */
	Operand *useOp;
	/* In set */
	Operand *in;
	/* Out set */
	Operand *out;
	/* Node info, for debugging */
	char *info; 
	/* Next CFG node in cfg list */
	struct CFGNode *nextCFG;
	/* Next CFG node in the flow */
	struct CFGNode *next;
	/* Used by if and while node */
	struct CFGNode *bTrue;
	struct CFGNode *bFalse;
	/* Used by switch node */
	struct CFGNode **caseTable;
	int caseNum;
	/* Used by bind node */
	tree decls;			
	char *bindID;
	struct CFGNode *parentBind;
	int childNum;
	/* Used by label and goto node */
	unsigned int labelID;
	/* For debugging use */
	bool dumped;
} CFGNode;

typedef struct SwitchData {
	CFGNode *switchEntry;
	CFGNode *switchExit;
	CFGNode *pBind;
	unsigned int exitID;
	struct SwitchData *next;
	bool hasDefault;
} SwitchData;

void doDFA(FILE *file, CFGNode *dfaList, int bindNum, char *funcName);
Operand *newOperand(char *name, CFGNode *pBind);

#endif
