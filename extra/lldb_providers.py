#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

import lldb

def inlinestring_summary(valobj, _) -> str:
    storage = valobj.GetChildMemberWithName("_M_elems")
    buffer = storage.Cast(valobj.target.GetBasicType(lldb.eBasicTypeChar).GetArrayType(storage.size))
    data = bytes(buffer.GetData().uint8s)
    return f'"{data[0:data.find(b'\0')].decode()}"'

def heapvector_summary(valobj, _) -> str:
    size = valobj.GetNonSyntheticValue().GetChildMemberWithName("offset").GetValueAsUnsigned()
    capacity = valobj.GetNonSyntheticValue().GetChildMemberWithName("cap").GetValueAsUnsigned()
    child_type = valobj.GetType().template_args[0].GetName()
    return f"size={size}, capacity={capacity} ({child_type})"

class HeapVectorSyntheticChildrenProvider:
    def __init__(self, valobj, _):
        self.valobj = valobj
        self.size = valobj.GetChildMemberWithName("offset").GetValueAsUnsigned()
        self.capacity = valobj.GetChildMemberWithName("cap").GetValueAsUnsigned()
        self.update()

    def update(self):
        self.size = self.valobj.GetNonSyntheticValue().GetChildMemberWithName("offset").GetValueAsUnsigned()
        self.capacity = self.valobj.GetNonSyntheticValue().GetChildMemberWithName("cap").GetValueAsUnsigned()

    def num_children(self):
        return self.size

    def get_child_index(self, name):
        sname = name.lstrip('[').rstrip(']')
        i = int(sname)
        if str(i) == sname:
            return i
        return None

    def get_child_at_index(self, index):
        try:
            child_type = self.valobj.GetType().template_args[0]
            void_storage = self.valobj.GetNonSyntheticValue().GetChildMemberWithName("storage")
            storage = void_storage.Cast(child_type.GetPointerType())
            return storage.GetChildAtIndex(index, False, True)
        except e:
            print(e)
            value = f"<python error>"

            child_type = self.valobj.target.GetBasicType(lldb.eBasicTypeChar)
            byte_order = self.valobj.GetData().GetByteOrder()
            data = lldb.SBData.CreateDataFromCString(byte_order, len(value), value)

            return self.valobj.CreateValueFromData(f'[{index}]', data, child_type.GetArrayType(data.GetByteSize()))


def entitymap_summary(valobj, _) -> str:
    size = valobj.GetNonSyntheticValue().GetChildMemberWithName("validEntities").GetChildMemberWithName("offset").GetValueAsUnsigned()
    capacity = valobj.GetNonSyntheticValue().GetChildMemberWithName("storage").GetChildMemberWithName("cap").GetValueAsUnsigned()
    child_type = valobj.GetType().template_args[0].GetName()
    return f"size={size}, capacity={capacity} (Entity, {child_type})"

class EntityMapSyntheticChildrenProvider:
    def __init__(self, valobj, _):
        self.valobj = valobj
        self.update()

    def update(self):
        validEntities = self.valobj.GetChildMemberWithName("validEntities")
        storage = self.valobj.GetChildMemberWithName("storage")
        self.size = validEntities.GetChildMemberWithName("offset").GetValueAsUnsigned()
        self.capacity = storage.GetChildMemberWithName("cap").GetValueAsUnsigned()
        void_pair_storage = storage.GetChildMemberWithName("storage")
        pair_type = storage.GetType().template_args[0]
        self.value_type = pair_type.template_args[1]
        pair_storage = void_pair_storage.Cast(pair_type.GetPointerType())
        void_valid_storage = validEntities.GetChildMemberWithName("storage")
        entity_type = validEntities.GetType().template_args[0]
        valid_storage = void_valid_storage.Cast(entity_type.GetPointerType())
        self.keys = []
        self.values = []
        for i in range(self.size):
            key = valid_storage.GetChildAtIndex(i, False, True)
            pair = pair_storage.GetChildAtIndex(key.GetValueAsUnsigned(), False, True)
            self.keys.append(key)
            value = pair.GetChildMemberWithName("second")
            self.values.append(self.valobj.CreateValueFromAddress(f'[{key.GetValueAsUnsigned()}]', value.addr.GetFileAddress(), self.value_type))

    def num_children(self):
        return self.size

    def get_child_index(self, name):
        sname = name.lstrip('[').rstrip(']')
        for i, key in self.keys:
            if key == sname:
                return i
        return None

    def get_child_at_index(self, index):
        return self.values[index]

def __lldb_init_module(debugger, _):
    print("Loading Stray Photons LLDB provider...")
    debugger.HandleCommand('type summary add sp::HeapString --summary-string "${var.storage%s}"')
    # debugger.HandleCommand('type summary add -x "^sp::InlineString<.*>$" --summary-string "${var._M_elems}"')
    debugger.HandleCommand('type summary add -x "^sp::InlineString<.*>$" -F lldb_providers.inlinestring_summary')
    debugger.HandleCommand('type summary add -x "^sp::HeapVector<.*>$" -F lldb_providers.heapvector_summary')
    debugger.HandleCommand('type synthetic add -x "^sp::HeapVector<.*>$" -l lldb_providers.HeapVectorSyntheticChildrenProvider')
    debugger.HandleCommand('type summary add -x "^sp::EntityMap<.*>$" -F lldb_providers.entitymap_summary')
    debugger.HandleCommand('type synthetic add -x "^sp::EntityMap<.*>$" -l lldb_providers.EntityMapSyntheticChildrenProvider')
