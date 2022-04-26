#----------------------------------------------------------------
# Isopod custom summaries
#
# load into lldb/xcode with (eg)
# 	command script import /dev/shared/isopod.py
# or add the import command to ~/.lldbinit
#----------------------------------------------------------------

import lldb
import re

def maybe_summary(val):
	summary = val.GetSummary()
	return summary if summary else val.GetValue()

def SUMMARY_vec2(val, dict):
	return val.GetChildMemberWithName('xy')

def SUMMARY_vec3(val, dict):
	return val.GetChildMemberWithName('xyz')

def SUMMARY_vec4(val, dict):
	return val.GetChildMemberWithName('xyzw')

def SUMMARY_soft_vec2(val, dict):
	return "({},{})".format(
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("x")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("y"))
	)

def SUMMARY_soft_vec3(val, dict):
	return "({},{},{})".format(
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("x")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("y")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("z"))
	)

def SUMMARY_soft_vec4(val, dict):
	return "({},{},{})".format(
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("x")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("y")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("z")),
		maybe_summary(val.GetChildAtIndex(0).GetChildMemberWithName("w"))
	)

def SUMMARY_soft_pointer(val, dict):
	return val.target.EvaluateExpression(val.get_expr_path() + ".get()")

#----------------------------------------------------------------
#	ISO
#----------------------------------------------------------------

ISO_types = ["Unknown", "Int", "Float", "String", "Composite", "Array", "OpenArray", "Reference", "Virtual", "User", "Unknown10", "Unknown11", "Unknown12", "Unknown13", "Unknown14", "Unknown15"]

def SUMMARY_ISO_Type(val, dict):
	flags = val.GetChildMemberWithName("flags").GetValueAsUnsigned(1)
	return ISO_types[flags & 15];
	
def SUMMARY_ISO_TypeUser(val, dict):
	return val.target.EvaluateExpression(val.get_expr_path() + ".ID()")

class SYNTH_ISO_Type:
	def __init__(self, val, dict):
		self.val = val

	def update(self):
		pass

	def num_children(self):
		return 1

	def get_child_index(self, name):
		if name == "flags":
			return 1
		return None

	def get_child_at_index(self, index):
		if index == 1:
			return self.val.GetChildMemberWithName("flags")
		flags = self.val.GetChildMemberWithName("flags").GetValueAsUnsigned(0)
		type = ISO_types[flags & 15];
		return self.val.target.EvaluateExpression("(ISO::Type" + type + "*)" + self.val.get_expr_path())

#----------------------------------------------------------------
#	containers
#----------------------------------------------------------------

def synth_index(name):
	return int(name.lstrip('[').rstrip(']'))

#----------------------------------------------------------------
#	array
#----------------------------------------------------------------

class SYNTH_array:
	def has_children(self):
		return True

	def num_children(self):
		return self.size

	def get_child_at_index(self, index):
		if index < 0 or index >= self.size:
			return None
		return self.p.CreateChildAtOffset('[{}]'.format(index), index * self.stride, self.type)

	def get_child_index(self,name):
		return synth_index(name)

	def get_value(self):
		return self.size


class SYNTH_static_array(SYNTH_array):
	def __init__(self, val, dict):
		self.val = val

	def update(self):
		self.p		= self.val.GetChildMemberWithName('t')
		self.size	= self.val.GetChildMemberWithName('curr_size').GetValueAsUnsigned(0)
		self.type	= self.val.GetType().GetTemplateArgumentType(0)
		self.stride	= self.type.GetByteSize()
		return True

class SYNTH_dynamic_array(SYNTH_array):
	def __init__(self, val, dict):
		self.val = val

	def update(self):
		self.p		= self.val.GetChildMemberWithName('p')
		self.size	= self.val.GetChildMemberWithName('curr_size').GetValueAsUnsigned(0)
		self.type	= self.val.GetType().GetTemplateArgumentType(0)
		self.stride	= self.type.GetByteSize()
		return True

class SYNTH_compact_array(SYNTH_array):
	def __init__(self, val, dict):
		self.val = val

	def update(self):
		p			= self.val.GetChildMemberWithName('p')
		self.p		= p.GetChildMemberWithName('array')
		self.size	= p.GetChildMemberWithName('curr_size').GetValueAsUnsigned(0)
		self.type	= self.val.GetType().GetTemplateArgumentType(0)
		self.stride	= self.type.GetByteSize()
		return True
		
class SYNTH_range(SYNTH_array):
	def __init__(self, val, dict):
		self.val = val

	def update(self):
		a	= self.val.GetChildMemberWithName('a')
		b	= self.val.GetChildMemberWithName('b')
		self.p		= a
		self.type	= a.GetType().GetPointeeType()
		self.stride	= self.type.GetByteSize()
		self.size	= (b.GetValueAsUnsigned(0) - a.GetValueAsUnsigned(0)) // self.stride
		return True


#----------------------------------------------------------------
#	hash
#----------------------------------------------------------------

class SYNTH_hash_map:
	def __init__(self, val, dict):
		self.val 	= val

	def has_children(self):
		return True

	def num_children(self):
		return self.curr_size

	def get_child_at_index(self, index):
		if index < 0 or index >= self.num_children():
			return None
		return self.val.EvaluateExpression('entries[{}].val'.format(self.map[index]), lldb.SBExpressionOptions(), '[{}]'.format(index)).Cast(self.T)

	def get_child_index(self,name):
		return synth_index(name)

	def is_used(self, i):
		key = self.val.EvaluateExpression('entries[{}].key'.format(i)).GetValueAsUnsigned(0)
		return key != i + 1 and key != i + 2

	def update(self):
		self.entries	= self.val.GetChildMemberWithName('entries')
		self.curr_size	= self.val.GetChildMemberWithName('curr_size').GetValueAsUnsigned(0)
		self.T			= self.entries.GetType().GetPointeeType().GetTemplateArgumentType(1);
		
		max_size		= self.val.GetChildMemberWithName('max_size').GetValueAsUnsigned(0)
		self.map		= [i for i in range(0, max_size) if self.is_used(i)]
		
		lldb.formatters.Logger.Logger() >> '{}:{}'.format(self.val.get_expr_path(), self.map)
		return True

	def get_value(self):
		return self.curr_size

#----------------------------------------------------------------
#	list
#----------------------------------------------------------------

def node_next(node):
	return node.GetChildMemberWithName('next').Cast(node.GetType())

def node_value(node):
	return node.GetValueAsUnsigned(0)

class SYNTH_list:
	def __init__(self, val, dict):
		self.val 	= val
		self.offset	= 16
		self.T 		= val.GetType().GetTemplateArgumentType(0)

	def is_end(self, node):
		return node_value(node_next(node)) == self.head_address

	def num_children(self):
		if self.count is None:
			self.count = self.num_children_impl()
		return self.count

	def num_children_impl(self):
		size 	= 0
		fast	= self.head
		slow	= fast
		while not self.is_end(fast):
			size 	= size + 1
			fast1 	= node_next(fast)
			if self.is_end(fast1):
				break
				
			size 	= size + 1
			fast 	= node_next(fast1)
			
			slow_value = node_value(slow)
			if node_value(fast1) == slow_value or node_value(fast) == slow_value:
				break
				
			slow = node_next(slow)
			
		return size

	def get_child_index(self, name):
		return synth_index(name)

	def get_child_at_index(self, index):
		if index < 0 or index >= self.num_children():
			return None

		current = self.head
		for i in range(0, index + 1):
			current = node_next(current)
			
		return current.CreateChildAtOffset('[{}]'.format(index), self.offset, self.T)

	def update(self):
		self.count			= None
		self.head 			= self.val.GetChildMemberWithName('head').AddressOf()
		self.head_address 	= node_value(self.head)

	def has_children(self):
		return True
		
class SYNTH_elist(SYNTH_list):
	def __init__(self, val, dict):
		self.val 	= val
		self.offset	= 0
		self.T 		= val.GetType().GetTemplateArgumentType(0)

#----------------------------------------------------------------
#	tree
#----------------------------------------------------------------

def child(node, side):
	return node.GetChildMemberWithName("child").GetChildAtIndex(side).Cast(node.GetType())

def subtree_size(node):
	if node_value(node) == 0:
		return 0;
	return 1 + subtree_size(child(node, 0)) + subtree_size(child(node, 1))

def subtree_index(node, i):
	c0 	= child(node, 0)
	n0 	= subtree_size(c0)
	
	if i == n0:
		return node
	elif i < n0:
		return subtree_index(c0, i)
	else:
		return subtree_index(child(node, 1), i - n0 - 1)
	
class SYNTH_tree:
	def __init__(self, val, dict):
		self.val	= val
		self.T 		= val.GetType().GetTemplateArgumentType(0)
		self.offset	= 0

	def update(self):
		self.count 		= None
		self.root		= self.val.EvaluateExpression(self.val.get_expr_path() + ".b")

	def num_children(self):
		if self.count is None:
			self.count = subtree_size(self.root)
		return self.count

	def get_child_index(self, name):
		return synth_index(name)

	def get_child_at_index(self, index):
		if index < 0 or index >= self.num_children():
			return None

		node = subtree_index(self.root, index)
		return node.CreateChildAtOffset('[{}]'.format(index), self.offset, self.T)

	def has_children(self):
		return True
		
#----------------------------------------------------------------
#	initialisation
#----------------------------------------------------------------

def __lldb_init_module(debugger, dict):
	lldb.formatters.Logger._lldb_formatters_debug_level = 1
	lldb.formatters.Logger._lldb_formatters_debug_filename = "/Users/Shared/lldb.py.log"
	
	#	ISO
	debugger.HandleCommand('type summary add -w isopod "ISO::Type" -F isopod.SUMMARY_ISO_Type')
	debugger.HandleCommand('type summary add -w isopod "ISO::TypeUser" -F isopod.SUMMARY_ISO_TypeUser')
	debugger.HandleCommand('type synthetic add -w isopod "ISO::Type" -l isopod.SYNTH_ISO_Type')

	#	containers
	debugger.HandleCommand('type synthetic add -w isopod -x "^iso::static_array<.+>(( )?&)?$" -l isopod.SYNTH_static_array')
	debugger.HandleCommand('type synthetic add -w isopod -x "^iso::dynamic_array<.+>(( )?&)?$" -l isopod.SYNTH_dynamic_array')
	debugger.HandleCommand('type synthetic add -w isopod -x "^iso::compact_array<.+>(( )?&)?$" -l isopod.SYNTH_compact_array')
	debugger.HandleCommand('type synthetic add -w isopod -xpr "^iso::hash_map_with_key<.+>(( )?&)?$" -l isopod.SYNTH_hash_map')
	debugger.HandleCommand('type synthetic add -w isopod -xpr "^iso::hash_map<.+>(( )?&)?$" -l isopod.SYNTH_hash_map')
	debugger.HandleCommand('type synthetic add -w isopod -x "^iso::list<.+>(( )?&)?$" -l isopod.SYNTH_list')
	debugger.HandleCommand('type synthetic add -w isopod -x "^iso::e_list<.+>(( )?&)?$" -l isopod.SYNTH_elist')
	debugger.HandleCommand('type synthetic add -w isopod -xpr "^iso::e_tree0<.+>$" -l isopod.SYNTH_tree')
	debugger.HandleCommand('type synthetic add -w isopod -xpr "^iso::range<.+\*>$" -l isopod.SYNTH_range')

	debugger.HandleCommand('type summary add -w isopod -x "^iso::static_array<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::dynamic_array<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::compact_array<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::hash_map_with_key<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::hash_map<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::list<.+>(( )?&)?$" -s "size = ${svar%#}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::e_list<.+>(( )?&)?$" -s "size = ${svar%#}"')
	
	#	vector
#	debugger.HandleCommand('type summary add -w isopod -x "^iso::vec<.+, 2>$" -F isopod.SUMMARY_vec2')
#	debugger.HandleCommand('type summary add -w isopod -x "^iso::vec<.+, 3>$" -F isopod.SUMMARY_vec3')
#	debugger.HandleCommand('type summary add -w isopod -x "^iso::vec<.+, 4>$" -F isopod.SUMMARY_vec4')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::soft_vec<.+, 2,.+>$" -F isopod.SUMMARY_soft_vec2')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::soft_vec<.+, 3,.+>$" -F isopod.SUMMARY_soft_vec3')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::soft_vec<.+, 4,.+>$" -F isopod.SUMMARY_soft_vec4')

	#	strings
	debugger.HandleCommand('type summary add -w isopod -x "^iso::string_base<.+>(( )?&)?$" -s "${var.p}"')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::fixed_string<.+>(( )?&)?$" -s "${var.p}"')
#	debugger.HandleCommand('type summary add -w isopod "iso::cstring" -s ${var.p}')
	debugger.HandleCommand('type summary add -w isopod "iso::alloc_string<char>" -s ${var.p}')
	debugger.HandleCommand('type summary add -w isopod "iso::filename" -s "${var.p}"')

	debugger.HandleCommand('type summary add -w isopod -x "iso::soft_pointer<" -F isopod.SUMMARY_soft_pointer')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::offset_type<.+>$" -s ${var.t}')
	debugger.HandleCommand('type summary add -w isopod -x "^iso::pair<.+>(( )?&)?$" -s "a=${var.a}, b=${var.b}"')
	

	debugger.HandleCommand('type summary add -w isopod "float" -s ${var}')

	debugger.HandleCommand('type category enable isopod')
