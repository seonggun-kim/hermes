/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "DeclCollector.h"

using namespace hermes::ESTree;

namespace hermes {
namespace sema {

/* static */ std::unique_ptr<DeclCollector> DeclCollector::run(
    ESTree::FunctionLikeNode *root,
    const sema::Keywords &kw,
    unsigned recursionDepth,
    const std::function<void(ESTree::Node *)> &recursionDepthExceeded) {
  std::unique_ptr<DeclCollector> dc(
      new DeclCollector(root, kw, recursionDepth, recursionDepthExceeded));
  dc->runImpl();
  return dc;
}

void DeclCollector::runImpl() {
  if (auto *func = llvh::dyn_cast<FunctionDeclarationNode>(root_)) {
    newScope();
    // Visit the children of the body, since we don't want to associate a
    // scope with it.
    visitESTreeChildren(*this, llvh::cast<BlockStatementNode>(func->_body));
    closeScope(root_);
    return;
  }
  if (auto *func = llvh::dyn_cast<FunctionExpressionNode>(root_)) {
    newScope();
    // Visit the children of the body, since we don't want to associate a
    // scope with it.
    visitESTreeChildren(*this, llvh::cast<BlockStatementNode>(func->_body));
    closeScope(root_);
    return;
  }

  if (auto *func = llvh::dyn_cast<ArrowFunctionExpressionNode>(root_)) {
    newScope();
    // If there is a BlockStatement, don't visit it, just visit its children.
    if (auto *block = llvh::dyn_cast<BlockStatementNode>(func->_body)) {
      visitESTreeChildren(*this, block);
    } else {
      visitESTreeChildren(*this, root_);
    }
    closeScope(root_);
    return;
  }

  newScope();
  visitESTreeChildren(*this, root_);
  closeScope(root_);
}

void DeclCollector::dump(llvh::raw_ostream &os, unsigned indent) {
#ifndef NDEBUG
  for (const auto &p : scopes_) {
    os << llvh::left_justify("", indent) << p.first->getNodeName() << "["
       << llvh::format("%p", p.first) << "]:";
    for (ESTree::Node *n : p.second) {
      os << " " << n->getNodeName() << "[" << llvh::format("%p", n) << "]";
    }
    os << "\n";
  }
#endif
}

void DeclCollector::setScopeDeclsForNode(ESTree::Node *node, ScopeDecls decls) {
  scopes_[node] = decls;
}
void DeclCollector::addScopeDeclForFunc(ESTree::Node *node) {
  scopes_[root_].push_back(node);
}

void DeclCollector::visit(ESTree::VariableDeclarationNode *node) {
  if (node->_kind == kw_.identVar) {
    addToFunc(node);
  } else {
    addToCur(node);
  }
  visitESTreeChildren(*this, node);
}
void DeclCollector::visit(ESTree::ClassDeclarationNode *node) {
  addToCur(node);
  // Don't descend into class bodies.
}
void DeclCollector::visit(ESTree::ImportDeclarationNode *node) {
  addToCur(node);
  visitESTreeChildren(*this, node);
}
#if HERMES_PARSE_FLOW
void DeclCollector::visit(ESTree::TypeAliasNode *node) {
  addToCur(node);
  visitESTreeChildren(*this, node);
}
#endif
#if HERMES_PARSE_TS
void DeclCollector::visit(ESTree::TSTypeAliasDeclarationNode *node) {
  addToCur(node);
  visitESTreeChildren(*this, node);
}
#endif

void DeclCollector::visit(ESTree::FunctionDeclarationNode *node) {
  // Record but don't descend.
  addToCur(node);
  if (scopeStack_.size() > 1) {
    scopedFuncDecls_.push_back(node);
  }
}

void DeclCollector::visit(ESTree::BlockStatementNode *node) {
  newScope();
  visitESTreeChildren(*this, node);
  closeScope(node);
}
void DeclCollector::visit(ESTree::ForStatementNode *node) {
  newScope();
  visitESTreeChildren(*this, node);
  closeScope(node);
}
void DeclCollector::visit(ESTree::ForInStatementNode *node) {
  newScope();
  visitESTreeChildren(*this, node);
  closeScope(node);
}
void DeclCollector::visit(ESTree::ForOfStatementNode *node) {
  newScope();
  visitESTreeChildren(*this, node);
  closeScope(node);
}
void DeclCollector::visit(ESTree::CatchClauseNode *node) {
  newScope();
  if (node->_param) {
    addToCur(node);
    visitESTreeNode(*this, node->_param, node);
  }
  // Declarations in the catch body are associated with CatchClauseNode,
  // not BlockStatementNode.
  visitESTreeChildren(*this, node->_body);
  closeScope(node);
}
void DeclCollector::visit(ESTree::SwitchStatementNode *node) {
  newScope();
  visitESTreeChildren(*this, node);
  closeScope(node);
}

void DeclCollector::closeScope(ESTree::Node *node) {
  assert(!scopeStack_.empty() && "no scope to close");

  // Move the popped scope out so we can put it in the `scopes_` map.
  ScopeDecls decls = std::move(scopeStack_.back());
  scopeStack_.pop_back();

  // Only store to the map if there's something to store.
  if (!decls.empty()) {
    auto result = scopes_.try_emplace(node, decls);
    (void)result;
    assert(result.second && "Tried to collect same node twice");
  }
}

} // namespace sema
} // namespace hermes
