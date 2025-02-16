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

#ifndef SHARE_VM_OPTO_NEWOBJECTNODE_HPP
#define SHARE_VM_OPTO_NEWOBJECTNODE_HPP

#include "opto/node.hpp"

class NewObjectNode : public TypeNode {
private:
  friend class InlineTypeNode;

  enum { Control, // Control input.
         Oop,     // Oop to heap allocated buffer.
         Values   // Nodes corresponding to values of the object's fields.
                  // Nodes are connected in increasing order of the index of the field they correspond to.
  };

  NewObjectNode(ciInstanceKlass* klass)
    : TypeNode(TypeInstPtr::make_exact(TypePtr::NotNull, klass), Values + klass->nof_nonstatic_fields()) {
    init_class_id(Class_NewObject);
  }

  // These nodes have unique identities
  virtual uint hash() const { return NO_HASH; }

  ciInstanceKlass* klass() const { return type()->make_ptr()->is_instptr()->instance_klass(); }
  uint             field_count()           const { return req() - Values; }
  Node*            field_value(uint i) const { assert(i < field_count(), "%u >= %u", i, field_count()); return in(Values + i); }
  void             set_field_value(uint i, Node* value) { assert(i < field_count(), " %u >= %u", i, field_count()); set_req(Values + i, value); }
  uint             field_index(int offset) const;

  void make_scalar_in_safepoint(PhaseIterGVN& igvn, SafePointNode* sfpt) const;

public:
  virtual int Opcode() const;

  Node* field_value_by_offset(int offset) const;
  void set_field_value_by_offset(int offset, Node* value);

  NewObjectNode* clone_if_required(PhaseGVN& gvn, SafePointNode* map);
  void make_scalar_in_safepoints(PhaseIterGVN& igvn);

  static NewObjectNode* make_default(PhaseGVN& gvn, ciInstanceKlass* klass);
};

#endif // SHARE_VM_OPTO_NEWOBJECTNODE_HPP
