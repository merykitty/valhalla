/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "opto/callnode.hpp"
#include "opto/inlinetypenode.hpp"
#include "opto/newobjectnode.hpp"
#include "opto/phaseX.hpp"
#include "opto/rootnode.hpp"

uint NewObjectNode::field_index(int offset) const {
  ciInstanceKlass* klass = this->klass();
  for (uint i = 0; i < field_count(); i++) {
    if (klass->nonstatic_field_at(i)->offset_in_bytes() == offset) {
      return i;
    }
  }
  fatal("field not found, type %s at offset %d", klass->name()->as_klass_external_name(), offset);
  return -1;
}

Node* NewObjectNode::field_value_by_offset(int offset) const {
  return field_value(field_index(offset));
}

void NewObjectNode::set_field_value_by_offset(int offset, Node* value) {
  set_req(field_index(offset) + Values, value);
}

void NewObjectNode::make_scalar_in_safepoint(PhaseIterGVN& igvn, SafePointNode* sfpt) const {
  JVMState* jvms = sfpt->jvms();
  assert(jvms != nullptr, "missing JVMS");

  // Iterate over the fields in order of increasing offset and add the field
  // values to the safepoint.
  uint first_ind = sfpt->req() - jvms->scloff();
  uint field_num = field_count();
  for (uint i = 0; i < field_num; ++i) {
    Node* value = field_value(i);
    sfpt->add_req(value);
  }
  jvms->set_endoff(sfpt->req());

  // Replace safepoint edge by SafePointScalarObjectNode
  auto sobj = new SafePointScalarObjectNode(type()->isa_instptr(), nullptr, first_ind,
                                            sfpt->jvms()->depth(), field_num);
  sobj->init_req(0, igvn.C->root());
  sobj = igvn.transform(sobj)->as_SafePointScalarObject();
  igvn.rehash_node_delayed(sfpt);
  for (uint i = jvms->debug_start(); i < jvms->debug_end(); i++) {
    Node* debug = sfpt->in(i);
    if (debug != nullptr && debug->uncast() == this) {
      sfpt->set_req(i, sobj);
    }
  }
}

void NewObjectNode::make_scalar_in_safepoints(PhaseIterGVN& igvn) {
  ResourceMark rm;
  Unique_Node_List safepoints;
  Unique_Node_List vt_worklist;
  Unique_Node_List worklist;
  worklist.push(this);

  // Find all safepoint uses of this node
  while (worklist.size() > 0) {
    Node* n = worklist.pop();
    for (DUIterator_Fast imax, i = n->fast_outs(imax); i < imax; i++) {
      Node* use = n->fast_out(i);
      if (use->is_SafePoint() && !use->is_CallLeaf() && (!use->is_Call() || use->as_Call()->has_debug_use(n))) {
        safepoints.push(use);
      } else if (use->is_ConstraintCast()) {
        worklist.push(use);
      }
    }
  }

  // Process all safepoint uses
  while (safepoints.size() > 0) {
    SafePointNode* sfpt = safepoints.pop()->as_SafePoint();
    make_scalar_in_safepoint(igvn, sfpt);
  }
}

NewObjectNode* NewObjectNode::clone_if_required(PhaseGVN& gvn, SafePointNode* map) {
  if (map == nullptr && outcnt() != 0) {
    return clone()->as_NewObject();
  }
  for (DUIterator_Fast imax, i = fast_outs(imax); i < imax; i++) {
    if (fast_out(i) != map) {
      return clone()->as_NewObject();
    }
  }
  gvn.hash_delete(this);
  return this;
}

NewObjectNode* NewObjectNode::make_default(PhaseGVN& gvn, ciInstanceKlass* klass) {
  NewObjectNode* node = new NewObjectNode(klass);
  gvn.C->add_new_object(node);
  for (uint i = 0; i < node->field_count(); i++) {
    ciField* field = klass->nonstatic_field_at(i);
    ciType* field_type = field->type();
    Node* value;
    if (field_type->is_inlinetype()) {
      ciInlineKlass* vk = field_type->as_inline_klass();
      if (field->is_null_free()) {
        value = InlineTypeNode::make_default(gvn, vk);
      } else {
        value = InlineTypeNode::make_null(gvn, vk);
      }
    } else {
      value = gvn.zerocon(field_type->basic_type());
    }
    node->set_field_value(i, value);
  }
  return gvn.transform(node)->as_NewObject();
}
