#include <stdio.h>
#include <assert.h>
#include "csproj2.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cgraph.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "pretty-print.h"
#include "hashtable.h"

extern GTY(()) struct cgraph_node *cgraph_nodes;

/* The list of switch information */
SwitchData *pSwitchList = NULL;
/* The table to keep track of label node */
CFGNode *labelTable[MAX_LABEL_SIZE];
int labelNum;
/* The table to keep track of goto node */
CFGNode *gotoTable[MAX_LABEL_SIZE];
int gotoNum;
/* Recording number of bind nodes */
int numBindNode;
/* Recording number of live CFGNodes */
int numCFGNode;
/* The list of all CFGNodes */
CFGNode *cfgList;
/* The hash table for storing variables */
HashTable *gVarTable;
/* Temporary buffer */
char tmpBuf[4096] = {0};

static unsigned int auxTable[] = {
	0x67452301,
	0xEFCDAB89,
	0x547B7452,
	0x98BADCFE,
	0x10325476,
	0xC3D2E1F0,
	0xA6C3D452,
	0xDAEFCDB7,
};

static unsigned int hashing(char *name)
{
	char *ptr;
	unsigned int hashVal;
	int i;

	for (hashVal=0, ptr=name; *ptr; ptr++)
		hashVal += *ptr;

	for (i=0, ptr=name; *ptr; ptr++, i=(i+1) % 7)
		hashVal += *ptr * auxTable[i];

	return hashVal;
}

static void delData(void *data)
{
	char *ptr = (char *)data;

	free(ptr);
}

static bool cmpEqual(void *a, void *b)
{
	char *pStrA = ((char *)a);
	char *pStrB = ((char *)b);

	if (strcmp(pStrA, pStrB)) {
		return false;
	} else
		return true;
}

/* Helper function to free an operand */
void freeOperand(Operand *pOp, bool isList)
{
	if (!isList)
		free(pOp);
	else {
		Operand *ptr;

		while (pOp) {
			ptr = pOp;
			pOp = pOp->next;
			free(ptr);
		}
	}
}

/* Helper function to free a CFGNode */
void freeCFGNode(CFGNode *pCFG)
{
	/* Decrease live CFGNode number and delete it from cfgList */
	numCFGNode--;
	if (cfgList == pCFG)
		cfgList = cfgList->nextCFG;
	else {
		CFGNode *ptr = cfgList;

		while (ptr->nextCFG != pCFG && ptr->nextCFG) {
			ptr = ptr->nextCFG;
		}

		assert(ptr->nextCFG);

		ptr->nextCFG = pCFG->nextCFG;
	}

	/* Delete node */
	switch (pCFG->nType) {
		case NTSWITCH:
			/* Switch may has bind */
			if (pCFG->caseTable)
				free(pCFG->caseTable);
			break;
		case NTBIND:
			/* Bind may be in a switch */
			if (pCFG->caseTable)
				free(pCFG->caseTable);
			numBindNode--;
			break;
	}

	/* Free its information */
	free(pCFG->info);

	/* Free all operands */
	if (pCFG->defOp)
		freeOperand(pCFG->defOp, true);
	if (pCFG->useOp)
		freeOperand(pCFG->useOp, true);
	if (pCFG->in)
		freeOperand(pCFG->in, true);
	if (pCFG->out)
		freeOperand(pCFG->out, true);

	/* Final free */
	free(pCFG);
}

CFGNode *newCFGNode(char *info, NodeType type)
{
	CFGNode *pNode = (CFGNode *) xmalloc(sizeof(CFGNode));
	memset(pNode, 0, sizeof(CFGNode));

	pNode->info = xstrdup(info);
	pNode->nType = type;

	/* Create case branch table for it */
	switch (type) {
		case NTSWITCH:
			pNode->caseTable = 
				(CFGNode **) xmalloc(sizeof(CFGNode *) * MAX_CASE_NUM);
			memset(pNode->caseTable, 0, sizeof(CFGNode *) * MAX_CASE_NUM);
			pNode->caseNum = 0;
			break;
		case NTBIND:
			numBindNode++;
			break;
	}

	/* Add the live count of CFGNodes and add it to dfa list */
	numCFGNode++;
	if (cfgList)
		pNode->nextCFG = cfgList;
	cfgList = pNode;

	return pNode;
}

Operand *newOperand(char *name, CFGNode *pBind)
{
	Operand *pOp = (Operand *) xmalloc(sizeof(Operand));
	char *pName;
	int len;

	len = sprintf(tmpBuf, "%s", name);

	/* Variable renaming */
	while (pBind) {
		tree decl = pBind->decls;
		int i;

		for (i=0; decl; decl=DECL_CHAIN(decl), i++) {
			if (TREE_CODE(decl) == VAR_DECL) {
				char *pName = IDENTIFIER_POINTER((DECL_NAME(decl)));
				if (strcmp(pName, tmpBuf) == 0) {
					sprintf(tmpBuf+len, "%s-%d", pBind->bindID, i);
					goto done;
				}
			}
		}
		pBind = pBind->parentBind;
	}
done:

	pName = hashLookupTable(gVarTable, hashing(tmpBuf), cmpEqual, tmpBuf, false);

	if (!pName) {
		pName = hashLookupTable(gVarTable, hashing(tmpBuf), 
			cmpEqual, xstrdup(tmpBuf), true);
	}

	pOp->name = pName;
	pOp->next = NULL;

	return pOp;
}

void freeSwitchData()
{
	SwitchData *pHead;
	CFGNode **pCaseTable;
	int *pCaseNum;

	assert(pSwitchList);
	
	pHead = pSwitchList;

	pSwitchList = pSwitchList->next;

	free(pHead);
}

void newSwitchData(CFGNode *pEntry, CFGNode *pExit, unsigned int exitID)
{
	SwitchData *pNew;

	pNew = (SwitchData *) xmalloc(sizeof(SwitchData));
	memset(pNew, 0, sizeof(SwitchData));

	pNew->switchEntry = pEntry;
	pNew->switchExit = pExit;
	pNew->exitID = exitID;

	if (pSwitchList)
		pNew->next = pSwitchList;

	pSwitchList = pNew;
}

void handleGoto(CFGNode *pGoto)
{
	int i;

	/* Check for any label it can go */
	for (i=0; i<labelNum; i++) {
		if (pGoto->labelID == labelTable[i]->labelID) {
			pGoto->next = labelTable[i];
			return;
		}
	}

	/* Put it in goto table for later handling */
	gotoTable[gotoNum++] = pGoto;
}

void handleLabel(CFGNode *pLabel)
{
	int i;

	labelTable[labelNum++] = pLabel;

	/* Handle any un-handled goto */
	for (i=0; i<gotoNum; i++) {
		if (gotoTable[i]->labelID == pLabel->labelID) {
			
			gotoTable[i]->next = pLabel;
			gotoTable[i] = gotoTable[--gotoNum];
			i--;
		}
	}
}

void dumpOperands(Operand *pOp)
{
	printf("%s", pOp->name);
	pOp = pOp->next;

	for (;pOp; pOp=pOp->next) {
		printf(" %s", pOp->name);
	}
}

/*
 * Insert an operand to a CFGNode. Before insertion, we
 * have to check whether there exists the same operand.
 * If it is, do not insert, free it, and return.
 */
void insertOperand(CFGNode *pCFG, Operand *pOp, ActionType type)
{
	Operand *ptr;

	assert(type == ATLHS || type == ATRHS);

	if (type == ATLHS) {
		if (!(ptr = pCFG->defOp)) {
			pCFG->defOp = pOp;
			return;
		}
	} else {
		if (!(ptr = pCFG->useOp)) {
			pCFG->useOp = pOp;
			return;
		}
	}

	/* Compare first operand */
	if (pOp->name == ptr->name) {
		freeOperand(pOp, false);
		return;
	}

	while (ptr->next) {
		if (pOp->name == ptr->name) {
			freeOperand(pOp, false);
			return;
		}
		ptr = ptr->next;
	}

	ptr->next = pOp;	
}

void setupBind(CFGNode *curBind, CFGNode *parentBind)
{
	if (parentBind == NULL) { /* Root bind */
		curBind->bindID = xstrdup("#0");
	} else {
		sprintf(tmpBuf, "%s#%d", 
			parentBind->bindID, parentBind->childNum++);
		curBind->parentBind = parentBind;
		curBind->bindID = xstrdup(tmpBuf);
	}
}

void dumpCFG(CFGNode *ptr)
{
	if (!ptr)
		return;

	if (ptr->dumped) {
		printf("[0x%x] dumped\n", ptr);
		return;
	}

	int i;

	ptr->dumped = true;

	printf("[0x%x][%s", ptr, ptr->info);

	for (i=13-strlen(ptr->info); i>0; i--) {
		printf(" ");
	}
	printf("]");

	if (ptr->defOp) {
		printf("[DEF:");
		dumpOperands(ptr->defOp);
		printf("]");
	}
	if (ptr->useOp) {
		printf("[USE:");
		dumpOperands(ptr->useOp);
		printf("]");
	}

	if (ptr->in) {
		printf("[IN:");
		dumpOperands(ptr->in);
		printf("]");
	}

	if (ptr->out) {
		printf("[OUT:");
		dumpOperands(ptr->out);
		printf("]");
	}

	printf("\n");

	if (ptr->nType == NTIF) {
		dumpCFG(ptr->bTrue);
		dumpCFG(ptr->bFalse);
	} else if (ptr->nType == NTSWITCH) {
		int i;

		dumpCFG(ptr->next);
		for (i=0; i<ptr->caseNum; i++) {
			dumpCFG(ptr->caseTable[i]);
		}
	} else {
		dumpCFG(ptr->next);
	}
}

CFGNode* walkStmt(tree node, tree nextNode, CFGNode *pPrev, 
	ActionType type, CFGNode *pBind)
{
	CFGNode *pRet = NULL;

	printf("%s %x\n", TREE_NAME(node), pBind);
try:
	switch (TREE_CODE(node)) {
		case STATEMENT_LIST:
			{
				tree_stmt_iterator i;
				CFGNode *pCur = pPrev;

				for (i=tsi_start(node); !tsi_end_p (i); tsi_next (&i)) {
					tree stmt = tsi_stmt(i);
					tree nextStmt;

					if (i.ptr->next)
						nextStmt = i.ptr->next->stmt;

					pCur = 
						walkStmt(stmt, nextStmt, pCur, ATCFG, pBind);
				}

				pRet = pCur;
			}
			break;
		case NOP_EXPR:
		case ADDR_EXPR:
			node = TREE_OPERAND(node, 0);
			goto try;
		case BIND_EXPR:
			{
				CFGNode *pHead = newCFGNode("BIND", NTBIND);
				CFGNode *pTail = newCFGNode("BIND_END", NTNORMAL);
				CFGNode *pLastExpr;

				if (pPrev->nType != NTGOTO)
					pPrev->next = pHead;
		
				if (pPrev->nType == NTSWITCH) {
					pSwitchList->pBind = pHead;
				}

				/* Get the declaration tree directly */
				pHead->decls = BIND_EXPR_VARS(node);

				/* Handling bind expression*/
				setupBind(pHead, pBind);

				pLastExpr = 
					walkStmt(BIND_EXPR_BODY(node), NULL, pHead, ATCFG, pHead);

				if (pLastExpr->nType != NTGOTO)
					pLastExpr->next = pTail;
				
				pRet = pTail;
			}
			break;
		case DECL_EXPR:
			{
				tree declNode = DECL_EXPR_DECL(node);
				
				/* Only handle initialized variables */
				if (DECL_INITIAL(declNode)) {
					CFGNode *pCur = newCFGNode(TREE_NAME(node), NTNORMAL);
					
					pPrev->next = pCur;
			
					walkStmt(declNode, NULL, pCur, ATLHS, pBind);
					walkStmt(DECL_INITIAL(declNode), 
						NULL, pCur, ATRHS, pBind);

					pRet = pCur;
				} else
					pRet = pPrev;
			}
			break;
		case MODIFY_EXPR:
			{
				if (type == ATCFG) {
					CFGNode *pCur = newCFGNode(TREE_NAME(node), NTNORMAL);

					if (pPrev->nType != NTGOTO)
						pPrev->next = pCur;
					pRet = pCur;
				} else 
					pRet = pPrev;

				walkStmt(TREE_OPERAND(node, 0), 
					NULL, pRet, ATLHS, pBind);
				walkStmt(TREE_OPERAND(node, 1), 
					NULL, pRet, ATRHS, pBind);
			}
			break;
		case COND_EXPR:
			if (TREE_TYPE (node) == NULL || 
				TREE_TYPE (node) == void_type_node) 
			{
				CFGNode *pCond = newCFGNode("IF_BEG", NTIF);
				CFGNode *pEnd = newCFGNode("IF_END", NTNORMAL);

				if (pPrev->nType != NTGOTO)
					pPrev->next = pCond;

				walkStmt(COND_EXPR_COND(node), 
					NULL, pCond, ATRHS, pBind);

				if (COND_EXPR_THEN(node)) {
					/* Last expression of then body */
					CFGNode *pLastExpr;
					CFGNode *pTrue = newCFGNode("IF_TRUE", NTNORMAL);
					
					pLastExpr = walkStmt(COND_EXPR_THEN(node), 
						NULL, pTrue, ATCFG, pBind);
					
					if (pLastExpr->nType != NTGOTO)
						pLastExpr->next = pEnd;

					pCond->bTrue = pTrue;
				} 

				/* If may has ELSE or not */
				if (COND_EXPR_ELSE(node)) {
					/* Last expression of else body */
					CFGNode *pLastExpr;
					CFGNode *pFalse = newCFGNode("IF_FALSE", NTNORMAL);
					
					pLastExpr =	walkStmt(COND_EXPR_ELSE(node), 
						NULL, pFalse, ATCFG, pBind);

					if (pLastExpr->nType != NTGOTO)
						pLastExpr->next = pEnd;

					pCond->bFalse = pFalse;
				} else {
					pCond->bFalse = pEnd;
				}

				pRet = pEnd;
			} else {

				walkStmt(COND_EXPR_COND(node), NULL, pPrev, ATRHS, pBind);
				walkStmt(COND_EXPR_THEN(node), NULL, pPrev, ATRHS, pBind);
				walkStmt(COND_EXPR_ELSE(node), NULL, pPrev, ATRHS, pBind);
				pRet = pPrev;
			}
			break;
		case CALL_EXPR:
			{
				/*
				 * Function call can be a statement or part of expression
				 * Example as stmt:		 foo(a);
				 * Example as expr part: int a = i + foo(i);
				 */
				if (type == ATCFG) {
					CFGNode *pCur = newCFGNode(TREE_NAME(node), NTNORMAL);
					char *funcName = 
						IDENTIFIER_POINTER(
							DECL_NAME(TREE_OPERAND(CALL_EXPR_FN(node), 0)));

					/* Handling special case to make it as defined */
					if (strcmp(funcName, "scanf") == 0) {
						type = ATLHS;
					} else
						type = ATRHS;

					if (pPrev->nType != NTGOTO)
						pPrev->next = pCur;
					pRet = pCur;
				} else
					pRet = pPrev;
					
				tree arg;
				call_expr_arg_iterator iter;

				FOR_EACH_CALL_EXPR_ARG (arg, iter, node) {
					walkStmt(arg, NULL, pRet, type, pBind);
				}
			}
			break;
		case RETURN_EXPR:
			{
				CFGNode *pCur = newCFGNode(TREE_NAME(node), NTNORMAL);
				tree op0 = TREE_OPERAND(node, 0);

				if (pPrev->nType != NTGOTO)
					pPrev->next = pCur;
	
				if (op0) {
					/* Don't know why */
					if (TREE_CODE(op0) == MODIFY_EXPR) {
						op0 = TREE_OPERAND(op0, 1);
					}

					walkStmt(op0, NULL, pCur, ATRHS, pBind);
				}
				pRet = pCur;
			}
			break;
		case COMPOUND_EXPR:
			{
				tree *tp;

				walkStmt(TREE_OPERAND(node, 0), 
					NULL, pPrev, ATRHS, pBind);
				
				for (tp=&TREE_OPERAND(node, 1);
					 TREE_CODE(*tp)==COMPOUND_EXPR;
					 tp=&TREE_OPERAND(*tp, 1))
				{
					walkStmt(TREE_OPERAND(*tp, 0), 
						NULL, pPrev, ATRHS, pBind);
				}
			}
			break;
		case SWITCH_EXPR: 
			{
				CFGNode *pCur = newCFGNode("SWITCH_COND", NTSWITCH);
				CFGNode *pEnd = newCFGNode("SWITCH_END", NTNORMAL);
				bool hasBreak = nextNode && TREE_CODE(nextNode) == LABEL_EXPR;

				if (pPrev->nType != NTGOTO)
					pPrev->next = pCur;

				/* Switch may have break or not */
				if (hasBreak)
					newSwitchData(pCur, pEnd, DECL_UID(TREE_OPERAND(nextNode, 0)));
				else
					newSwitchData(pCur, pEnd, 0);

				walkStmt(SWITCH_COND(node), 
					NULL, pCur, ATRHS, pBind);

				walkStmt(SWITCH_BODY(node), 
					NULL, pCur, ATCFG, pBind)->next = pEnd;

				/* If there is no default label, switch may act as IF */
				if (!pSwitchList->hasDefault) {
					pCur->caseTable[pCur->caseNum++] = pEnd;
				}

				/* This switch has bind scope */
				if (pSwitchList->pBind) 
				{
					pCur->next = pSwitchList->pBind;
					pSwitchList->pBind->caseTable = pCur->caseTable;
					pSwitchList->pBind->caseNum = pCur->caseNum;
					pCur->caseTable = pCur->caseNum = 0;
				}

				/* For switch has no break, switchDat should be freed here */
				if (!hasBreak)
					freeSwitchData();
				
				pRet = pEnd;
			}
			break;
		case GOTO_EXPR:
			{
				unsigned int gotoID = DECL_UID(GOTO_DESTINATION(node));

				sprintf(tmpBuf, "GOTO <D.%d>", gotoID);

				CFGNode *pCur = newCFGNode(tmpBuf, NTGOTO);
				pPrev->next = pCur;
				pRet = pCur;
				pCur->labelID = gotoID;
		
				/* Check for switch goto */
				if (pSwitchList && gotoID == pSwitchList->exitID)
					pCur->next = pSwitchList->switchExit;
				else
					handleGoto(pCur);

			}
			break;
		case LABEL_EXPR:
			pRet = pPrev;

			if (pSwitchList &&
				pSwitchList->exitID == DECL_UID(TREE_OPERAND(node, 0))) 
			{
				freeSwitchData();
			} else {
				sprintf(tmpBuf, "<D.%d>", DECL_UID(TREE_OPERAND(node, 0)));
				CFGNode *pCur = newCFGNode(tmpBuf, NTLABEL);
				pCur->labelID = DECL_UID(TREE_OPERAND(node, 0));
				pRet = pCur;
				
				if (pPrev->nType != NTGOTO)
					pPrev->next = pCur;
				
				handleLabel(pCur);
			}
			break;
		case CASE_LABEL_EXPR:
			assert(pSwitchList);
			{
				CFGNode *pSwitchEntry = pSwitchList->switchEntry;
				CFGNode *pCur = newCFGNode("CASE", NTNORMAL);
				pSwitchEntry->caseTable[pSwitchEntry->caseNum++] = pCur;
				pRet = pCur;
				if (pPrev->nType != NTGOTO)
					pPrev->next = pCur;

				/* It is default case label */
				if (!CASE_LOW(node))
					pSwitchList->hasDefault = true;
			}

			break;
		case FLOAT_EXPR:
		case PLUS_EXPR:
		case MINUS_EXPR:
		case MULT_EXPR:
		case RDIV_EXPR:
		case TRUNC_DIV_EXPR:
		case TRUTH_ANDIF_EXPR:
		case TRUTH_ORIF_EXPR:
		case TRUTH_AND_EXPR:
		case TRUTH_OR_EXPR:
		case LT_EXPR:
		case LE_EXPR:
		case GT_EXPR:
		case GE_EXPR:
		case EQ_EXPR:
		case NE_EXPR:
		case POSTINCREMENT_EXPR:
		case POSTDECREMENT_EXPR:
		case PREINCREMENT_EXPR:
		case PREDECREMENT_EXPR:
			{
				/* 
				 * Expression without any effect is perfact legal in gcc 
				 * Ex. a + 1;
				 */
				if (type == ATCFG) {
					CFGNode *pCur = newCFGNode(TREE_NAME(node), NTNORMAL);
				
					pPrev->next = pCur;
					pRet = pCur;
				} else {
					pRet = pPrev;
				}
				if (TREE_OPERAND(node, 0))
					walkStmt(TREE_OPERAND(node, 0), 
						NULL, pRet, ATRHS, pBind);
				if (TREE_OPERAND(node, 1) && TREE_CODE(node) != FLOAT_EXPR)
					walkStmt(TREE_OPERAND(node, 1), 
						NULL, pRet, ATRHS, pBind);
			}
			break;
		case VAR_DECL:
		case PARM_DECL: 
			{
				Operand *pOp = 
					newOperand(IDENTIFIER_POINTER((DECL_NAME(node))), pBind);

				assert(type == ATLHS || type == ATRHS);

				insertOperand(pPrev, pOp, type);
			}
			break;

		case INTEGER_CST:
			/* Integer is not a variable */
			break;
		default:
			break;
	}
	return pRet;
}

void init()
{
	labelNum = gotoNum = 0;
	numBindNode = numCFGNode = 0;
	cfgList = NULL;
	gVarTable = hashCreateTable(256);
}

void fin()
{
	hashDeleteTable(gVarTable, delData);

	while (cfgList) {
		CFGNode *ptr = cfgList;
		freeCFGNode(ptr);
	}
}
void cs502_proj2()
{
	struct cgraph_node *node;
	FILE *file = fopen("output.txt", "w");

	/* Walk through all functions */
	for (node=cgraph_nodes; node; node=node->next) {
		tree fn = node->decl;
		tree body = DECL_SAVED_TREE((fn));
	
		init();
		CFGNode *pEntry = newCFGNode("Entry", NTNORMAL), *pTmp = pEntry;
		
		print_c_tree(stdout, body);
		
		walkStmt(body, NULL, pEntry, ATCFG, NULL);
		pEntry = pEntry->next;
		freeCFGNode(pTmp);

		doDFA(file, cfgList, numBindNode, 
			IDENTIFIER_POINTER(DECL_NAME(fn)));

		dumpCFG(pEntry);

		fin();
	}
	fclose(file);
}
