#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "csproj2.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cgraph.h"
#include "hashtab.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "pretty-print.h"

struct Output {
	char *name;
	char *bindID;
	int order;
} Output;

/* 
 * Table for recording bind scopes. After backward dfa, all
 * bind scopes will be analysed for uninitialized variables.
 */
CFGNode **bindTable;
char tmpBuf[4096];

/* Output table */
struct Output out[1024];
int outNum;

int cmpFunc(const void *a, const void *b)
{
	struct Output *pA = (struct Output *)a;
	struct Output *pB = (struct Output *)b;

	int ccStr = strcmp(pA->bindID, pB->bindID);
	
	if (ccStr != 0)
		return ccStr;
	else
		return pA->order - pB->order;
}

/* Initialization */
void initDFA(CFGNode *dfaList, int numBind)
{
	CFGNode *ptr = dfaList;
	int i;

	/* Initialize bind table */
	bindTable = (CFGNode **) xmalloc(sizeof(CFGNode *) * numBind);
	for (i=0; ptr; ptr=ptr->nextCFG) {
		if (ptr->nType == NTBIND) {
			bindTable[i++] = ptr;
		}
	}
}

/* Global declaration, in case of stack overflow */
Operand *gSucIn[1024];
int sInNum;

bool checkSetDup(Operand *pChk, Operand **set, int setNum)
{
	int i;

	for (i=0; i<setNum; i++) {
		if (pChk->name == set[i]->name)
			return true;
	}

	return false;
}

bool checkListDup(Operand *pChk, Operand *pList)
{
	while (pList) {
		if (pChk->name == pList->name)
			return true;
		pList = pList->next;
	}

	return false;
}

/* 
 * Get the in set from a successor node. For memory space,
 * we consider {in} as {in} U {use}.
 */
int getSucIn(CFGNode *pCur, CFGNode *pSuc)
{
	Operand *ptr;
	int inNum = 0;

	if (!pSuc)
		return 0;

	/*
	 * Scan for current successor's use set duplicate in 
	 * current node's out and successors' in.
	 */
	for (ptr=pSuc->useOp; ptr; ptr=ptr->next) {
		if (checkSetDup(ptr, gSucIn, sInNum) || 
			checkListDup(ptr, pCur->out)) 
		{
			continue;
		} else {
			gSucIn[sInNum + inNum] = ptr;
			inNum++;
		}
	}
	
	/*
	 * Scan for current successor's in set duplicate in 
	 * current node's out and successors' in.
	 */
	for (ptr=pSuc->in; ptr; ptr=ptr->next) {
		if (checkSetDup(ptr, gSucIn, sInNum) || 
			checkListDup(ptr, pCur->out)) 
		{
			continue;
		} else {
			gSucIn[sInNum + inNum] = ptr;
			inNum++;
		}
	}

	return inNum;
}

/* Generate the in set and out set from gSucIn */
void genInOut(CFGNode *pCur)
{
	bool inChange = false;
	int i;

	for (i=0; i<sInNum; i++) {
		/* 
		 * Checked with current out before, so add it to cur out
		 * directly.
		 */
		Operand *pOut = newOperand(gSucIn[i]->name, NULL);

		if (pCur->out)
			pOut->next = pCur->out;
		pCur->out = pOut;

		if (!checkListDup(gSucIn[i], pCur->defOp) &&
			!checkListDup(gSucIn[i], pCur->useOp) && 
			!checkListDup(gSucIn[i], pCur->in)) 
		{
			Operand *pIn = newOperand(gSucIn[i]->name, NULL);
			if (pCur->in)
				pIn->next = pCur->in;
			pCur->in = pIn;
		}
	}
}

/* Running DFA analysis */
void beginDFA(CFGNode *dfaList)
{
	CFGNode *ptr;
	bool converge;

	do {
		ptr = dfaList;
		converge = true;

		for (ptr=dfaList; ptr; ptr=ptr->nextCFG) {
	
			sInNum = 0;
			
			switch (ptr->nType) {
				case NTIF:
					sInNum += getSucIn(ptr, ptr->bTrue);
					sInNum += getSucIn(ptr, ptr->bFalse);
					break;
				case NTSWITCH:
					{
						if (ptr->next && ptr->next->nType == NTBIND) {
							sInNum = getSucIn(ptr, ptr->next);
						} else {
							int i;

							for (i=0; i<ptr->caseNum; i++) {
								sInNum += getSucIn(ptr, ptr->caseTable[i]);
							}
						}
					}
					break;
				case NTBIND:
					if (ptr->caseTable) {
						int i;

						for (i=0; i<ptr->caseNum; i++) {
							sInNum += getSucIn(ptr, ptr->caseTable[i]);
						}
					} else {
						sInNum = getSucIn(ptr, ptr->next);

					}
					break;
				case NTGOTO:
				case NTLABEL:
				case NTNORMAL:
					sInNum = getSucIn(ptr, ptr->next);
					break;
			}

			if (sInNum) {
				converge = false;
				genInOut(ptr);
			} 
		}
	} while (!converge);
}

void handleOut(char *bindID, int order, char *name)
{
	out[outNum].bindID = bindID;
	out[outNum].order = order;
	out[outNum].name = name;
	outNum++;
}

/* 
 * Do final analysis on all bind scopes and 
 * print out all uninitialized variables. 
 * For all elements in out set, check whether it is
 * in the in set. If yes, we have to check whether
 * it is declared in this scope. If yes, then it
 * is an uninitialized but used variable.
 */
void finDFA(FILE *file, int numBind, char *funcName)
{
	int i;

	for (i=0, outNum=0; i<numBind; i++) {
		CFGNode *pBind = bindTable[i];
		Operand *pOut = pBind->out;

		while (pOut) {
			if (checkListDup(pOut, pBind->in) || 
				checkListDup(pOut, pBind->useOp)) 
			{
				/* 
				 * Operand occurs in and out sets, check
				 * for declaration
				 */
				tree decl = pBind->decls;
				int j;

				for (j=0; decl; decl=DECL_CHAIN(decl), j++) {
					if (TREE_CODE(decl) == VAR_DECL) {
						sprintf(tmpBuf, "%s%s-%d", 
							IDENTIFIER_POINTER((DECL_NAME(decl))),
							pBind->bindID, j);
						
						if (strcmp(tmpBuf, pOut->name) == 0) {
							handleOut(pBind->bindID, j, 
								IDENTIFIER_POINTER((DECL_NAME(decl))));
							break;
						}
					}
				}
			}
			pOut = pOut->next;
		}
	}

	if (!outNum)
		return;

	qsort(out, outNum, sizeof(struct Output), cmpFunc);

	fprintf(file, "%s:%s", funcName, out[0].name);
	for (i=1; i<outNum; i++) {
		fprintf(file, ",%s", out[i].name);
	}
	fprintf(file, "\n");
}

void doDFA(FILE *file, CFGNode *dfaList, int numBind, char *funcName)
{
	initDFA(dfaList, numBind);
	beginDFA(dfaList);
	finDFA(file, numBind, funcName);
}
