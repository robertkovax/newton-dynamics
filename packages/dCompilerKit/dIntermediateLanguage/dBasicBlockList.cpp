/* Copyright (c) <2009> <Newton Game Dynamics>
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/


#include "dCILstdafx.h"
#include "dDataFlowGraph.h"
#include "dCILInstrBranch.h"
#include "dBasicBlockList.h"
#include "dCILInstrLoadStore.h"
#include "dConstantPropagationSolver.h"


void dStatementBlockDictionary::BuildUsedVariableWorklist(dBasicBlocksGraph& list)
{
	RemoveAll();

	for (dBasicBlocksGraph::dListNode* node = list.GetFirst(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();

		for (dCIL::dListNode* node = block.m_end; node != block.m_begin; node = node->GetPrev()) {
			dCILInstr* const instruction = node->GetInfo();
			//instruction->Trace();

			dList<dCILInstr::dArg*> variablesList;
			instruction->GetUsedVariables(variablesList);
			for (dList<dCILInstr::dArg*>::dListNode* varNode = variablesList.GetFirst(); varNode; varNode = varNode->GetNext()) {
				const dCILInstr::dArg* const variable = varNode->GetInfo();
				dTreeNode* entry = Find(variable->m_label);
				if (!entry) {
					entry = Insert(variable->m_label);
				}
				dStatementBlockBucket& buckect = entry->GetInfo();
				buckect.Insert(&block, node);
			}
		}
	}
}



dBasicBlock::dBasicBlock (dCIL::dListNode* const begin)
	:m_mark (0)
	,m_begin (begin)
	,m_end(NULL)
	,m_idom(NULL)
{
}

dBasicBlock::dBasicBlock(const dBasicBlock& src)
	:m_mark(0)
	,m_begin(src.m_begin)
	,m_end(NULL)
	,m_idom(NULL)
{
	for (m_end = m_begin; m_end && !m_end->GetInfo()->IsBasicBlockEnd(); m_end = m_end->GetNext()) {
		m_end->GetInfo()->m_basicBlock = this;
	}
	m_end->GetInfo()->m_basicBlock = this;
}


void dBasicBlock::Trace() const
{
	bool terminate = false;
	for (dCIL::dListNode* node = m_begin; !terminate; node = node->GetNext()) {
		terminate = (node == m_end);
		node->GetInfo()->Trace();
	}
/*
	dCILInstrLabel* const label = m_begin->GetInfo()->GetAsLabel();
	dTrace ((" block-> %s\n", label->GetArg0().m_label.GetStr()));

	if (m_idom) {
		dTrace(("   immediateDominator-> %s\n", m_idom->m_begin->GetInfo()->GetAsLabel()->GetArg0().m_label.GetStr()));
	} else {
		dTrace(("   immediateDominator->\n"));
	}

	dTrace(("   dominatorTreeChildren-> "));
	for (dList<const dBasicBlock*>::dListNode* childNode = m_children.GetFirst(); childNode; childNode = childNode->GetNext()) {
		const dBasicBlock* const childBlock = childNode->GetInfo();
		dCILInstrLabel* const label = childBlock->m_begin->GetInfo()->GetAsLabel();
		dTrace(("%s ", label->GetArg0().m_label.GetStr()));
	}
	dTrace(("\n"));


	dTrace (("   dominators-> "));
	dTree<int, const dBasicBlock*>::Iterator iter (m_dominators);
	for (iter.Begin(); iter; iter ++) {
		const dBasicBlock& domBlock = *iter.GetKey();
		dCILInstrLabel* const label = domBlock.m_begin->GetInfo()->GetAsLabel();
		dTrace (("%s ", label->GetArg0().m_label.GetStr()));
	}
	dTrace (("\n"));

	dTrace(("   dominanceFrontier-> "));
	for (dList<const dBasicBlock*>::dListNode* childFrontierNode = m_dominanceFrontier.GetFirst(); childFrontierNode; childFrontierNode = childFrontierNode->GetNext()) {
		const dBasicBlock* const childBlock = childFrontierNode->GetInfo();
		dCILInstrLabel* const label = childBlock->m_begin->GetInfo()->GetAsLabel();
		dTrace(("%s ", label->GetArg0().m_label.GetStr()));
	}
	dTrace (("\n"));
*/
	dTrace (("\n"));
}


bool dBasicBlock::ComparedDominator(const dTree<int, const dBasicBlock*>& newdominators) const
{
	if (m_dominators.GetCount() != newdominators.GetCount()) {
		return true;
	}

	dTree<int, const dBasicBlock*>::Iterator iter0 (m_dominators);
	dTree<int, const dBasicBlock*>::Iterator iter1 (newdominators);
	for (iter0.Begin(), iter1.Begin(); iter0 && iter1; iter0++, iter1++) {
		if (iter0.GetKey() != iter1.GetKey()) {
			return true;
		}
	}
	return false;
}

void dBasicBlock::ReplaceDominator (const dTree<int, const dBasicBlock*>& newdominators)
{
	m_dominators.RemoveAll();
	dTree<int, const dBasicBlock*>::Iterator iter1(newdominators);
	for (iter1.Begin(); iter1; iter1++) {
		m_dominators.Insert(0, iter1.GetKey());
	}
}


dBasicBlocksGraph::dBasicBlocksGraph()
	:dList<dBasicBlock> ()
	,m_dominatorTree(NULL)
	,m_mark(0)
{
}


void dBasicBlocksGraph::Trace() const
{
	for (dList<dBasicBlock>::dListNode* blockNode = GetFirst(); blockNode; blockNode = blockNode->GetNext()) {
		dBasicBlock& block = blockNode->GetInfo();
		block.Trace();
	}
}

void dBasicBlocksGraph::Build (dCIL::dListNode* const functionNode)
{
	//RemoveAll();
	dAssert (!GetCount());

	m_begin = functionNode->GetNext();
	dAssert (m_begin->GetInfo()->GetAsLabel());
	for (m_end = functionNode->GetNext(); !m_end->GetInfo()->GetAsFunctionEnd(); m_end = m_end->GetNext());

	// find the root of all basic blocks leaders
	for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
		if (node->GetInfo()->IsBasicBlockBegin()) {
			dAssert (node->GetInfo()->GetAsLabel());
			Append(dBasicBlock(node));
		}
	}
	CalculateSuccessorsAndPredecessors ();
	BuildDominatorTree ();
//Trace();
}


void dBasicBlocksGraph::CalculateSuccessorsAndPredecessors ()
{
	m_mark += 1;
	dList<dBasicBlock*> stack;
	stack.Append(&GetFirst()->GetInfo());

	while (stack.GetCount()) {
		dBasicBlock* const block = stack.GetLast()->GetInfo();

		stack.Remove(stack.GetLast()->GetInfo());
		if (block->m_mark < m_mark) {

			block->m_mark = m_mark;
			//m_traversalBlocksOrder.Addtop(block);
//block->Trace();

			dCILInstr* const instruction = block->m_end->GetInfo();
			dAssert(instruction->IsBasicBlockEnd());
			if (instruction->GetAsIF()) {
				dCILInstrConditional* const ifInstr = instruction->GetAsIF();

				dAssert (ifInstr->GetTrueTarget());
				dAssert (ifInstr->GetFalseTarget());

				dCILInstrLabel* const target0 = ifInstr->GetTrueTarget()->GetInfo()->GetAsLabel();
				dCILInstrLabel* const target1 = ifInstr->GetFalseTarget()->GetInfo()->GetAsLabel();

				dBasicBlock* const block0 = target0->m_basicBlock;
				dAssert (block0);
				block->m_successors.Append (block0);
				block0->m_predecessors.Append(block);
				stack.Append (block0);

				dBasicBlock* const block1 = target1->m_basicBlock;
				dAssert(block1);
				block->m_successors.Append(block1);
				block1->m_predecessors.Append(block);
				stack.Append(block1);

			} else if (instruction->GetAsGoto()) {
				dCILInstrGoto* const gotoInst = instruction->GetAsGoto();

				dAssert(gotoInst->GetTarget());
				dCILInstrLabel* const target = gotoInst->GetTarget()->GetInfo()->GetAsLabel();
				dBasicBlock* const block0 = target->m_basicBlock;

				dAssert(block0);
				block->m_successors.Append(block0);
				block0->m_predecessors.Append(block);
				stack.Append(block0);
			}
		}
	}

	DeleteUnreachedBlocks();
}

void dBasicBlocksGraph::DeleteUnreachedBlocks()
{
	dTree<dBasicBlock*, dCIL::dListNode*> blockMap;
	for (dBasicBlocksGraph::dListNode* blockNode = GetFirst(); blockNode; blockNode = blockNode->GetNext()) {
		dBasicBlock& block = blockNode->GetInfo();
		blockMap.Insert(&block, block.m_begin);
	}

	
	m_mark += 1;
	dList<dBasicBlock*> stack;
	stack.Append(&GetFirst()->GetInfo());
	while (stack.GetCount()) {
		dBasicBlock* const block = stack.GetLast()->GetInfo();

		stack.Remove(stack.GetLast()->GetInfo());
		if (block->m_mark < m_mark) {
			block->m_mark = m_mark;

			dCILInstr* const instruction = block->m_end->GetInfo();
			dAssert(instruction->IsBasicBlockEnd());
			if (instruction->GetAsIF()) {
				dCILInstrConditional* const ifInstr = instruction->GetAsIF();
				stack.Append(blockMap.Find(ifInstr->GetTrueTarget())->GetInfo());
				stack.Append(blockMap.Find(ifInstr->GetFalseTarget())->GetInfo());

			} else if (instruction->GetAsGoto()) {
				dCILInstrGoto* const gotoInst = instruction->GetAsGoto();
				stack.Append(blockMap.Find(gotoInst->GetTarget())->GetInfo());
			}
		}
	}

	dCIL* const cil = m_begin->GetInfo()->GetCil();
	dBasicBlocksGraph::dListNode* nextBlockNode;
	for (dBasicBlocksGraph::dListNode* blockNode = GetFirst(); blockNode; blockNode = nextBlockNode) {
		dBasicBlock& block = blockNode->GetInfo();
		nextBlockNode = blockNode->GetNext();
		if (block.m_mark != m_mark) {
			//block.Trace();
			bool terminate = false;
			dCIL::dListNode* nextNode;
			for (dCIL::dListNode* node = block.m_begin; !terminate; node = nextNode) {
				terminate = (node == block.m_end);
				nextNode = node->GetNext();
				cil->Remove(node);
			}
			Remove(blockNode);
		}
	}
}



void dBasicBlocksGraph::BuildDominatorTree ()
{
	// dominator of the start node is the start itself
	//Dom(n0) = { n0 }
	dBasicBlock& firstBlock = GetFirst()->GetInfo();
	firstBlock.m_dominators.Insert(0, &firstBlock);

	// for all other nodes, set all nodes as the dominators
	for (dListNode* node = GetFirst()->GetNext(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();
		for (dListNode* node1 = GetFirst(); node1; node1 = node1->GetNext()) {
			block.m_dominators.Insert(0, &node1->GetInfo());
		}
	}

	bool change = true;
	while (change) {
		change = false;
		for (dListNode* node = GetFirst()->GetNext(); node; node = node->GetNext()) {
			dBasicBlock& block = node->GetInfo();

//block.Trace();
			dTree<int, const dBasicBlock*> predIntersection;
			const dBasicBlock& predBlock = *block.m_predecessors.GetFirst()->GetInfo();

			dTree<int, const dBasicBlock*>::Iterator domIter (predBlock.m_dominators);
			for (domIter.Begin(); domIter; domIter ++) {
				const dBasicBlock* const block = domIter.GetKey();
				predIntersection.Insert (0, block);
			}

			for (dList<const dBasicBlock*>::dListNode* predNode = block.m_predecessors.GetFirst()->GetNext(); predNode; predNode = predNode->GetNext()) {
				const dBasicBlock& predBlock = *predNode->GetInfo();
				dTree<int, const dBasicBlock*>::Iterator predIter (predIntersection);
				for (predIter.Begin(); predIter; ) {
					const dBasicBlock* block = predIter.GetKey();
					predIter ++;
					if (!predBlock.m_dominators.Find(block)) {
						predIntersection.Remove(block);
					}
				}
			}

			dAssert (!predIntersection.Find(&block));
			predIntersection.Insert(&block);

			bool dominatorChanged = block.ComparedDominator (predIntersection);
			if (dominatorChanged) {
				block.ReplaceDominator (predIntersection);
				//block.Trace();
			}
			change |= dominatorChanged;
		}
	}

	// find the immediate dominator of each block
	for (dListNode* node = GetFirst()->GetNext(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();
//block.Trace();

		dAssert (block.m_dominators.GetCount() >= 2);
		dTree<int, const dBasicBlock*>::Iterator iter(block.m_dominators);

		for (iter.Begin(); iter; iter, iter++) {
			const dBasicBlock* const sblock = iter.GetKey();
			if (sblock != &block) {
				if (sblock->m_dominators.GetCount() == (block.m_dominators.GetCount() - 1)) {
					bool identical = true;
					dTree<int, const dBasicBlock*>::Iterator iter0(block.m_dominators);
					dTree<int, const dBasicBlock*>::Iterator iter1(sblock->m_dominators);
					for (iter0.Begin(), iter1.Begin(); identical && iter0 && iter1; iter0++, iter1++) {
						if (iter0.GetKey() == &block) {
							iter0 ++;
						}
						identical &= (iter0.GetKey() == iter1.GetKey());
					}
					if (identical) {
						if (sblock->m_dominators.GetCount() == (block.m_dominators.GetCount() - 1)) {
							block.m_idom = sblock;
							break;
						}
					}
				}
			}
		}
		dAssert (block.m_idom);
		//block.m_idom->Trace();
	}

	// build dominator tree
	m_dominatorTree = &GetFirst()->GetInfo();
	for (dListNode* node = GetFirst()->GetNext(); node; node = node->GetNext()) {
		const dBasicBlock& block = node->GetInfo();
		block.m_idom->m_children.Append(&block);
	}
	
//Trace();
}


void dBasicBlocksGraph::BuildDomicanceFrontier (const dBasicBlock* const root) const
{
	dTree<int, const dBasicBlock*> frontier;
	for (dList<const dBasicBlock*>::dListNode* succNode = root->m_successors.GetFirst(); succNode; succNode = succNode->GetNext()) {
		const dBasicBlock& succBlock = *succNode->GetInfo();
		if (succBlock.m_idom != root) {
			frontier.Insert(0, &succBlock);
		}
//succBlock.Trace();
	}

	for (dList<const dBasicBlock*>::dListNode* node = root->m_children.GetFirst(); node; node = node->GetNext()) {
		const dBasicBlock* const childBlock = node->GetInfo();
		BuildDomicanceFrontier (childBlock);
		for (dList<const dBasicBlock*>::dListNode* childFrontierNode = childBlock->m_dominanceFrontier.GetFirst(); childFrontierNode; childFrontierNode = childFrontierNode->GetNext()) {
			const dBasicBlock* const childBlock = childFrontierNode->GetInfo();
			if (childBlock->m_idom != root) {
				frontier.Insert(0, childBlock);
			}
		}
	}

	dTree<int, const dBasicBlock*>::Iterator iter (frontier);
	for (iter.Begin(); iter; iter ++) {
		root->m_dominanceFrontier.Append(iter.GetKey());
	}
}

void dBasicBlocksGraph::ConvertToSSA ()
{
	for (dListNode* node = GetFirst(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();
		block.m_dominanceFrontier.RemoveAll();
	}
	BuildDomicanceFrontier (m_dominatorTree);

//Trace();

	dCIL* const cil = m_begin->GetInfo()->GetCil();
	dTree <dStatementBucket, dString> variableList;
	for (dListNode* node = GetFirst(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();

		for (dCIL::dListNode* node = block.m_end; node != block.m_begin; node = node->GetPrev()) {
			dCILInstr* const instruction = node->GetInfo();
			//instruction->Trace();
			const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
			if (variable) {
				dTree <dStatementBucket, dString>::dTreeNode* entry = variableList.Find(variable->m_label);
				if (!entry) {
					entry = variableList.Insert(variable->m_label);
					entry->GetInfo().m_variable = dCILInstr::dArg(variable->m_label, variable->GetType());
				}
				dStatementBucket& buckect = entry->GetInfo();
				buckect.Insert(instruction, &block);
			}
		}
	}

	dTree <dStatementBucket, dString> phyVariables;
	dTree <dStatementBucket, dString>::Iterator iter (variableList);
	for (iter.Begin(); iter; iter ++) {
		dStatementBucket w;
		dStatementBucket::Iterator bucketIter (iter.GetNode()->GetInfo());
		for (bucketIter.Begin(); bucketIter; bucketIter ++) {
			w.Insert(bucketIter.GetNode()->GetInfo(), bucketIter.GetKey());
		}

		//const dString& name = iter.GetKey();
		dCILInstr::dArg name (iter.GetNode()->GetInfo().m_variable);
		dStatementBucket& wPhy = phyVariables.Insert(name.m_label)->GetInfo();
		while (w.GetCount()) {
			const dBasicBlock* const block = w.GetRoot()->GetKey();
			const dCILInstr* const instruction = w.GetRoot()->GetInfo();
			w.Remove(w.GetRoot());

//instruction->Trace();
//block->Trace();
			for (dList<const dBasicBlock*>::dListNode* node = block->m_dominanceFrontier.GetFirst(); node; node = node->GetNext()) {
				const dBasicBlock* const frontier = node->GetInfo();
				if (!wPhy.Find(frontier)) {
					dList<dCILInstr*> sources;

					for (dList<const dBasicBlock*>::dListNode* predNode = frontier->m_predecessors.GetFirst(); predNode; predNode = predNode->GetNext()) {
						const dBasicBlock& predBlock = *predNode->GetInfo();
						for (dCIL::dListNode* node = predBlock.m_end; node != predBlock.m_begin; node = node->GetPrev()) {
							dCILInstr* const instruction = node->GetInfo();
							const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
							if (variable && variable->m_label == name.m_label) {
								sources.Append(instruction);
								break;
							}
						}
					}
					if (sources.GetCount() > 1) {
						dCILInstrPhy* const phyInstruction = new dCILInstrPhy (*cil, name.m_label, name.GetType(), sources, frontier);
						cil->InsertAfter(frontier->m_begin, phyInstruction->GetNode());
	//frontier->Trace();

						wPhy.Insert(instruction, frontier);
						if (!iter.GetNode()->GetInfo().Find(frontier)) {
							w.Insert(instruction, frontier);
						}
					}
				}
			}
		}
	}

	variableList.RemoveAll ();
	for (dListNode* node = GetFirst(); node; node = node->GetNext()) {
		dBasicBlock& block = node->GetInfo();

		for (dCIL::dListNode* node = block.m_end; node != block.m_begin; node = node->GetPrev()) {
			dCILInstr* const instruction = node->GetInfo();
			//instruction->Trace();
			const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
			if (variable) {
				dTree <dStatementBucket, dString>::dTreeNode* entry = variableList.Find(variable->m_label);
				if (!entry) {
					entry = variableList.Insert(variable->m_label);
					entry->GetInfo().m_variable = dCILInstr::dArg(variable->m_label, variable->GetType());
				}
				dStatementBucket& buckect = entry->GetInfo();
				buckect.Insert(instruction, &block);
			}
		}
	}
	
	RenameVariables (&GetFirst()->GetInfo(), variableList);
}


void dBasicBlocksGraph::RenameVariables (const dBasicBlock* const root, dTree <dStatementBucket, dString>& stack) const
{
//root->Trace();
	bool terminate = false;
	for (dCIL::dListNode* node = root->m_begin; !terminate; node = node->GetNext()) {
		terminate = (node == root->m_end);
		dCILInstr* const instruction = node->GetInfo();
//instruction->Trace();
		if (!instruction->GetAsPhy()) {
			dList<dCILInstr::dArg*> variablesList;
			instruction->GetUsedVariables (variablesList);
			for (dList<dCILInstr::dArg*>::dListNode* argNode = variablesList.GetFirst(); argNode; argNode = argNode->GetNext()) {
				dCILInstr::dArg* const variable = argNode->GetInfo();
				dString name (instruction->RemoveSSAPostfix(variable->m_label));
				dAssert(stack.Find(name));
				dStatementBucket& topStack = stack.Find(name)->GetInfo();
				if (topStack.m_stack.GetCount()) {
					int stackLevel = topStack.m_stack.GetLast()->GetInfo();
					variable->m_label = instruction->MakeSSAName(name, stackLevel);
				}
			}
		}
		
		dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
		if (variable) {
			dString name(instruction->RemoveSSAPostfix(variable->m_label));
			dAssert(stack.Find(name));
			dStatementBucket& topStack = stack.Find(name)->GetInfo();

			int i = topStack.m_index;
			variable->m_label = instruction->MakeSSAName(name, i);
			topStack.m_stack.Append(i);
			topStack.m_index = i + 1;
		}
		//instruction->Trace();
	}

	for (dList<const dBasicBlock*>::dListNode* node = root->m_children.GetFirst(); node; node = node->GetNext()) {
		RenameVariables (node->GetInfo(), stack);
	}

	for (dCIL::dListNode* node = root->m_end; node != root->m_begin; node = node->GetPrev()) {
		dCILInstr* const instruction = node->GetInfo();
		const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
		if (variable) {
			dString name (instruction->RemoveSSAPostfix(variable->m_label));
			dStatementBucket& topStack = stack.Find(name)->GetInfo();
			dAssert (topStack.m_stack.GetLast());
			topStack.m_stack.Remove(topStack.m_stack.GetLast());
		}
	}

//root->Trace();
//dataFlow->m_cil->Trace();
}



//void dCIL::OptimizeSSA(dListNode* const functionStart)
void dBasicBlocksGraph::OptimizeSSA ()
{
//	dDataFlowGraph dataFlow(this, functionStart);
	Trace();

	bool pass = true;
	while (pass) {
		pass = false;
		pass |= ApplyConstantPropagationSSA();

		pass |= ApplyConditionalConstantPropagationSSA();

		//Trace();
		//pass |= ApplyDeadCodeEliminationSSA ();
		//Trace();
		
		//Trace();
		//pass |= ApplyConstantConditionalSSA();
		//Trace();
		//pass |= ApplyCopyPropagationSSA ();
		//Trace();
		//pass |= dataFlow.ApplyConditionalConstantPropagationSSA();
	}

Trace();
}

/*
void dDataFlowGraph::GeneratedVariableWorklist(dTree <int, dCIL::dListNode*>& list) const
{
	for (dCIL::dListNode* node = m_function; !node->GetInfo()->GetAsFunctionEnd(); node = node->GetNext()) {
		dCILInstr* const instruction = node->GetInfo();
		const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
		if (variable) {
			list.Insert(0, node);
		}
	}
}
*/

void dBasicBlocksGraph::GetStatementsWorklist(dTree <int, dCIL::dListNode*>& list) const
{
	for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
		list.Insert(0, node);
	}
}


bool dBasicBlocksGraph::ApplyConstantConditionalSSA()
{
	bool anyChanges = false;

	dTree<int, dCIL::dListNode*> phyMap;
	for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
		dCILInstr* const instruction = node->GetInfo();
		if (instruction->GetAsPhy()) {
			phyMap.Insert(0, node);
		}
	}

	dCIL* const cil = m_begin->GetInfo()->GetCil();
	for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
		dCILInstr* const instruction = node->GetInfo();
		if (instruction->GetAsIF()) {
			dCILInstrConditional* const conditinal = instruction->GetAsIF();

			const dCILInstr::dArg& arg0 = conditinal->GetArg0();
			if ((arg0.GetType().m_intrinsicType == dCILInstr::m_constInt) || (arg0.GetType().m_intrinsicType == dCILInstr::m_constFloat)) {
				dAssert(conditinal->GetTrueTarget());
				dAssert(conditinal->GetFalseTarget());
				int condition = arg0.m_label.ToInteger();
				if (conditinal->m_mode == dCILInstrConditional::m_ifnot) {
					condition = !condition;
				}

				dCILInstrLabel* label;
				if (condition) {
					label = conditinal->GetTrueTarget()->GetInfo()->GetAsLabel();
				}
				else {
					label = conditinal->GetFalseTarget()->GetInfo()->GetAsLabel();
				}

				dCILInstrGoto* const jump = new dCILInstrGoto(*cil, label->GetArg0().m_label);
				jump->SetTarget(label);
				conditinal->ReplaceInstruction(jump);
				anyChanges = true;
			}
		}
	}

	return anyChanges;
}



bool dBasicBlocksGraph::ApplyDeadCodeEliminationSSA()
{
	bool anyChanges = false;

	dWorkList workList;
	dStatementBlockDictionary usedVariablesList;
	usedVariablesList.BuildUsedVariableWorklist(*this);

	dTree<dCIL::dListNode*, dString> map;
	for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
		dCILInstr* const instruction = node->GetInfo();
		const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
		if (variable) {
			map.Insert(node, variable->m_label);
		}
	}

	GetStatementsWorklist(workList);
	while (workList.GetCount()) {
		dCIL::dListNode* const node = workList.GetRoot()->GetKey();
		workList.Remove(workList.GetRoot());
		dCILInstr* const instruction = node->GetInfo();
		if (!instruction->GetAsCall()) {
			const dCILInstr::dArg* const variable = instruction->GetGeneratedVariable();
			if (variable) {
				dStatementBlockDictionary::dTreeNode* const usesNodeBuckect = usedVariablesList.Find(variable->m_label);
				dAssert(!usesNodeBuckect || usesNodeBuckect->GetInfo().GetCount());
				if (!usesNodeBuckect) {
					anyChanges = true;
					dList<dCILInstr::dArg*> variablesList;
					instruction->GetUsedVariables(variablesList);
					for (dList<dCILInstr::dArg*>::dListNode* varNode = variablesList.GetFirst(); varNode; varNode = varNode->GetNext()) {
						const dCILInstr::dArg* const variable = varNode->GetInfo();
						dAssert(usedVariablesList.Find(variable->m_label));
						dStatementBlockDictionary::dTreeNode* const entry = usedVariablesList.Find(variable->m_label);
						if (entry) {
							dStatementBlockBucket& buckect = entry->GetInfo();
							buckect.Remove(node);
							dAssert(map.Find(variable->m_label));
							workList.Insert(map.Find(variable->m_label)->GetInfo());
							if (!buckect.GetCount()) {
								usedVariablesList.Remove(usesNodeBuckect);
							}
						}
					}
					instruction->Nullify();
				}
			}
		}
	}
	return anyChanges;
}


bool dBasicBlocksGraph::ApplyCopyPropagationSSA()
{
	bool anyChanges = false;

	dWorkList workList;
	dStatementBlockDictionary usedVariablesList;
	usedVariablesList.BuildUsedVariableWorklist (*this);

	GetStatementsWorklist(workList);
	while (workList.GetCount()) {
		dCIL::dListNode* const node = workList.GetRoot()->GetKey();
		workList.Remove(workList.GetRoot());
		dCILInstr* const instruction = node->GetInfo();
		anyChanges |= instruction->ApplyCopyPropagationSSA(workList, usedVariablesList);
	}
	return anyChanges;
}


bool dBasicBlocksGraph::ApplyConstantPropagationSSA()
{
	bool anyChanges = false;

	dWorkList workList;
	dStatementBlockDictionary usedVariablesList;
	usedVariablesList.BuildUsedVariableWorklist(*this);

	GetStatementsWorklist(workList);
	while (workList.GetCount()) {
		dCIL::dListNode* const node = workList.GetRoot()->GetKey();
		workList.Remove(workList.GetRoot());
		dCILInstr* const instruction = node->GetInfo();
		anyChanges |= instruction->ApplyConstantPropagationSSA(workList, usedVariablesList);
	}

	if (anyChanges) {
		for (dCIL::dListNode* node = m_begin; node != m_end; node = node->GetNext()) {
			dCILInstr* const instruction = node->GetInfo();
			instruction->ApplyConstantFoldingSSA();
		}
	}
	return anyChanges;
}



bool dBasicBlocksGraph::ApplyConditionalConstantPropagationSSA()
{
	dConstantPropagationSolver constantPropagation (this);
	return constantPropagation.Solve();
}