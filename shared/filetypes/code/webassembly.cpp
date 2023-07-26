#include "webassembly.h"
#include "comms/leb128.h"
#include "disassembler.h"

using namespace wabt;

#define WABT_PAGE_SIZE	0x10000		// 64k
#define WABT_MAX_PAGES	0x10000		// # of pages that fit in 32-bit address space

#define CHECK_RESULT(expr)	do { if (!expr) return false; } while (0)
/*

	*/

Opcode::Info Opcode::infos[] = {
#define WABT_OPCODE(feat, rtype, type1, type2, type3, mem_size, prefix_code, Name, text) {text, Type::rtype, Type::type1, Type::type2, Type::type3, mem_size, prefix_code, FeatureSet::feat},

/* *** NOTE *** This list must be kept sorted so it can be binary searched */

/*
 *	   fs: feature set
 *     tr: result type
 *     t1: type of the 1st parameter
 *     t2: type of the 2nd parameter
 *     t3: type of the 3rd parameter
 *      m: memory size of the operation, if any
 *   code: opcode
 *   Name: used to generate the opcode enum
 *   text: a string of the opcode name in the text format
 *
 *          fs    tr    t1    t2    t3    m  prefix_code  Name text
 * ==========================================================  */

WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0000, Unreachable, "unreachable")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0001, Nop, "nop")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0002, Block, "block")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0003, Loop, "loop")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0004, If, "if")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0005, Else, "else")

WABT_OPCODE(EXCP, ___,  ___,  ___,  ___,  0,  0x0006, Try, "try")
WABT_OPCODE(EXCP, ___,  ___,  ___,  ___,  0,  0x0007, Catch, "catch")
WABT_OPCODE(EXCP, ___,  ___,  ___,  ___,  0,  0x0008, Throw, "throw")
WABT_OPCODE(EXCP, ___,  ___,  ___,  ___,  0,  0x0009, Rethrow, "rethrow")
WABT_OPCODE(EXCP, ___,  ___,  ___,  ___,  0,  0x000a, IfExcept, "if_except")

WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x000b, End, "end")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x000c, Br, "br")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x000d, BrIf, "br_if")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x000e, BrTable, "br_table")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x000f, Return, "return")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0010, Call, "call")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0011, CallIndirect, "call_indirect")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x001a, Drop, "drop")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x001b, Select, "select")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0020, GetLocal, "get_local")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0021, SetLocal, "set_local")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0022, TeeLocal, "tee_local")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0023, GetGlobal, "get_global")
WABT_OPCODE(BASE, ___,  ___,  ___,  ___,  0,  0x0024, SetGlobal, "set_global")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  4,  0x0028, I32Load, "i32.load")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  8,  0x0029, I64Load, "i64.load")
WABT_OPCODE(BASE, F32,  I32,  ___,  ___,  4,  0x002a, F32Load, "f32.load")
WABT_OPCODE(BASE, F64,  I32,  ___,  ___,  8,  0x002b, F64Load, "f64.load")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  1,  0x002c, I32Load8S, "i32.load8_s")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  1,  0x002d, I32Load8U, "i32.load8_u")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  2,  0x002e, I32Load16S, "i32.load16_s")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  2,  0x002f, I32Load16U, "i32.load16_u")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  1,  0x0030, I64Load8S, "i64.load8_s")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  1,  0x0031, I64Load8U, "i64.load8_u")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  2,  0x0032, I64Load16S, "i64.load16_s")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  2,  0x0033, I64Load16U, "i64.load16_u")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  4,  0x0034, I64Load32S, "i64.load32_s")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  4,  0x0035, I64Load32U, "i64.load32_u")
WABT_OPCODE(BASE, ___,  I32,  I32,  ___,  4,  0x0036, I32Store, "i32.store")
WABT_OPCODE(BASE, ___,  I32,  I64,  ___,  8,  0x0037, I64Store, "i64.store")
WABT_OPCODE(BASE, ___,  I32,  F32,  ___,  4,  0x0038, F32Store, "f32.store")
WABT_OPCODE(BASE, ___,  I32,  F64,  ___,  8,  0x0039, F64Store, "f64.store")
WABT_OPCODE(BASE, ___,  I32,  I32,  ___,  1,  0x003a, I32Store8, "i32.store8")
WABT_OPCODE(BASE, ___,  I32,  I32,  ___,  2,  0x003b, I32Store16, "i32.store16")
WABT_OPCODE(BASE, ___,  I32,  I64,  ___,  1,  0x003c, I64Store8, "i64.store8")
WABT_OPCODE(BASE, ___,  I32,  I64,  ___,  2,  0x003d, I64Store16, "i64.store16")
WABT_OPCODE(BASE, ___,  I32,  I64,  ___,  4,  0x003e, I64Store32, "i64.store32")
WABT_OPCODE(BASE, I32,  ___,  ___,  ___,  0,  0x003f, MemorySize, "memory.size")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  0,  0x0040, MemoryGrow, "memory.grow")
WABT_OPCODE(BASE, I32,  ___,  ___,  ___,  0,  0x0041, I32Const, "i32.const")
WABT_OPCODE(BASE, I64,  ___,  ___,  ___,  0,  0x0042, I64Const, "i64.const")
WABT_OPCODE(BASE, F32,  ___,  ___,  ___,  0,  0x0043, F32Const, "f32.const")
WABT_OPCODE(BASE, F64,  ___,  ___,  ___,  0,  0x0044, F64Const, "f64.const")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  0,  0x0045, I32Eqz, "i32.eqz")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0046, I32Eq, "i32.eq")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0047, I32Ne, "i32.ne")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0048, I32LtS, "i32.lt_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0049, I32LtU, "i32.lt_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004a, I32GtS, "i32.gt_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004b, I32GtU, "i32.gt_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004c, I32LeS, "i32.le_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004d, I32LeU, "i32.le_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004e, I32GeS, "i32.ge_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x004f, I32GeU, "i32.ge_u")
WABT_OPCODE(BASE, I32,  I64,  ___,  ___,  0,  0x0050, I64Eqz, "i64.eqz")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0051, I64Eq, "i64.eq")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0052, I64Ne, "i64.ne")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0053, I64LtS, "i64.lt_s")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0054, I64LtU, "i64.lt_u")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0055, I64GtS, "i64.gt_s")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0056, I64GtU, "i64.gt_u")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0057, I64LeS, "i64.le_s")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0058, I64LeU, "i64.le_u")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x0059, I64GeS, "i64.ge_s")
WABT_OPCODE(BASE, I32,  I64,  I64,  ___,  0,  0x005a, I64GeU, "i64.ge_u")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x005b, F32Eq, "f32.eq")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x005c, F32Ne, "f32.ne")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x005d, F32Lt, "f32.lt")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x005e, F32Gt, "f32.gt")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x005f, F32Le, "f32.le")
WABT_OPCODE(BASE, I32,  F32,  F32,  ___,  0,  0x0060, F32Ge, "f32.ge")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0061, F64Eq, "f64.eq")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0062, F64Ne, "f64.ne")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0063, F64Lt, "f64.lt")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0064, F64Gt, "f64.gt")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0065, F64Le, "f64.le")
WABT_OPCODE(BASE, I32,  F64,  F64,  ___,  0,  0x0066, F64Ge, "f64.ge")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  0,  0x0067, I32Clz, "i32.clz")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  0,  0x0068, I32Ctz, "i32.ctz")
WABT_OPCODE(BASE, I32,  I32,  ___,  ___,  0,  0x0069, I32Popcnt, "i32.popcnt")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006a, I32Add, "i32.add")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006b, I32Sub, "i32.sub")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006c, I32Mul, "i32.mul")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006d, I32DivS, "i32.div_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006e, I32DivU, "i32.div_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x006f, I32RemS, "i32.rem_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0070, I32RemU, "i32.rem_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0071, I32And, "i32.and")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0072, I32Or, "i32.or")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0073, I32Xor, "i32.xor")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0074, I32Shl, "i32.shl")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0075, I32ShrS, "i32.shr_s")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0076, I32ShrU, "i32.shr_u")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0077, I32Rotl, "i32.rotl")
WABT_OPCODE(BASE, I32,  I32,  I32,  ___,  0,  0x0078, I32Rotr, "i32.rotr")
WABT_OPCODE(BASE, I64,  I64,  ___,  ___,  0,  0x0079, I64Clz, "i64.clz")
WABT_OPCODE(BASE, I64,  I64,  ___,  ___,  0,  0x007a, I64Ctz, "i64.ctz")
WABT_OPCODE(BASE, I64,  I64,  ___,  ___,  0,  0x007b, I64Popcnt, "i64.popcnt")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x007c, I64Add, "i64.add")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x007d, I64Sub, "i64.sub")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x007e, I64Mul, "i64.mul")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x007f, I64DivS, "i64.div_s")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0080, I64DivU, "i64.div_u")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0081, I64RemS, "i64.rem_s")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0082, I64RemU, "i64.rem_u")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0083, I64And, "i64.and")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0084, I64Or, "i64.or")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0085, I64Xor, "i64.xor")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0086, I64Shl, "i64.shl")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0087, I64ShrS, "i64.shr_s")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0088, I64ShrU, "i64.shr_u")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x0089, I64Rotl, "i64.rotl")
WABT_OPCODE(BASE, I64,  I64,  I64,  ___,  0,  0x008a, I64Rotr, "i64.rotr")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x008b, F32Abs, "f32.abs")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x008c, F32Neg, "f32.neg")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x008d, F32Ceil, "f32.ceil")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x008e, F32Floor, "f32.floor")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x008f, F32Trunc, "f32.trunc")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0090, F32Nearest, "f32.nearest")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0091, F32Sqrt, "f32.sqrt")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0092, F32Add, "f32.add")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0093, F32Sub, "f32.sub")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0094, F32Mul, "f32.mul")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0095, F32Div, "f32.div")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0096, F32Min, "f32.min")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0097, F32Max, "f32.max")
WABT_OPCODE(BASE, F32,  F32,  F32,  ___,  0,  0x0098, F32Copysign, "f32.copysign")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x0099, F64Abs, "f64.abs")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009a, F64Neg, "f64.neg")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009b, F64Ceil, "f64.ceil")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009c, F64Floor, "f64.floor")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009d, F64Trunc, "f64.trunc")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009e, F64Nearest, "f64.nearest")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x009f, F64Sqrt, "f64.sqrt")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a0, F64Add, "f64.add")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a1, F64Sub, "f64.sub")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a2, F64Mul, "f64.mul")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a3, F64Div, "f64.div")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a4, F64Min, "f64.min")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a5, F64Max, "f64.max")
WABT_OPCODE(BASE, F64,  F64,  F64,  ___,  0,  0x00a6, F64Copysign, "f64.copysign")
WABT_OPCODE(BASE, I32,  I64,  ___,  ___,  0,  0x00a7, I32WrapI64, "i32.wrap/i64")
WABT_OPCODE(BASE, I32,  F32,  ___,  ___,  0,  0x00a8, I32TruncSF32, "i32.trunc_s/f32")
WABT_OPCODE(BASE, I32,  F32,  ___,  ___,  0,  0x00a9, I32TruncUF32, "i32.trunc_u/f32")
WABT_OPCODE(BASE, I32,  F64,  ___,  ___,  0,  0x00aa, I32TruncSF64, "i32.trunc_s/f64")
WABT_OPCODE(BASE, I32,  F64,  ___,  ___,  0,  0x00ab, I32TruncUF64, "i32.trunc_u/f64")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  0,  0x00ac, I64ExtendSI32, "i64.extend_s/i32")
WABT_OPCODE(BASE, I64,  I32,  ___,  ___,  0,  0x00ad, I64ExtendUI32, "i64.extend_u/i32")
WABT_OPCODE(BASE, I64,  F32,  ___,  ___,  0,  0x00ae, I64TruncSF32, "i64.trunc_s/f32")
WABT_OPCODE(BASE, I64,  F32,  ___,  ___,  0,  0x00af, I64TruncUF32, "i64.trunc_u/f32")
WABT_OPCODE(BASE, I64,  F64,  ___,  ___,  0,  0x00b0, I64TruncSF64, "i64.trunc_s/f64")
WABT_OPCODE(BASE, I64,  F64,  ___,  ___,  0,  0x00b1, I64TruncUF64, "i64.trunc_u/f64")
WABT_OPCODE(BASE, F32,  I32,  ___,  ___,  0,  0x00b2, F32ConvertSI32, "f32.convert_s/i32")
WABT_OPCODE(BASE, F32,  I32,  ___,  ___,  0,  0x00b3, F32ConvertUI32, "f32.convert_u/i32")
WABT_OPCODE(BASE, F32,  I64,  ___,  ___,  0,  0x00b4, F32ConvertSI64, "f32.convert_s/i64")
WABT_OPCODE(BASE, F32,  I64,  ___,  ___,  0,  0x00b5, F32ConvertUI64, "f32.convert_u/i64")
WABT_OPCODE(BASE, F32,  F64,  ___,  ___,  0,  0x00b6, F32DemoteF64, "f32.demote/f64")
WABT_OPCODE(BASE, F64,  I32,  ___,  ___,  0,  0x00b7, F64ConvertSI32, "f64.convert_s/i32")
WABT_OPCODE(BASE, F64,  I32,  ___,  ___,  0,  0x00b8, F64ConvertUI32, "f64.convert_u/i32")
WABT_OPCODE(BASE, F64,  I64,  ___,  ___,  0,  0x00b9, F64ConvertSI64, "f64.convert_s/i64")
WABT_OPCODE(BASE, F64,  I64,  ___,  ___,  0,  0x00ba, F64ConvertUI64, "f64.convert_u/i64")
WABT_OPCODE(BASE, F64,  F32,  ___,  ___,  0,  0x00bb, F64PromoteF32, "f64.promote/f32")
WABT_OPCODE(BASE, I32,  F32,  ___,  ___,  0,  0x00bc, I32ReinterpretF32, "i32.reinterpret/f32")
WABT_OPCODE(BASE, I64,  F64,  ___,  ___,  0,  0x00bd, I64ReinterpretF64, "i64.reinterpret/f64")
WABT_OPCODE(BASE, F32,  I32,  ___,  ___,  0,  0x00be, F32ReinterpretI32, "f32.reinterpret/i32")
WABT_OPCODE(BASE, F64,  I64,  ___,  ___,  0,  0x00bf, F64ReinterpretI64, "f64.reinterpret/i64")

/* Sign-extension opcodes (--enable-sign-extension) */
WABT_OPCODE(SGNX, I32,  I32,  ___,  ___,  0,  0x00C0, I32Extend8S, "i32.extend8_s")
WABT_OPCODE(SGNX, I32,  I32,  ___,  ___,  0,  0x00C1, I32Extend16S, "i32.extend16_s")
WABT_OPCODE(SGNX, I64,  I64,  ___,  ___,  0,  0x00C2, I64Extend8S, "i64.extend8_s")
WABT_OPCODE(SGNX, I64,  I64,  ___,  ___,  0,  0x00C3, I64Extend16S, "i64.extend16_s")
WABT_OPCODE(SGNX, I64,  I64,  ___,  ___,  0,  0x00C4, I64Extend32S, "i64.extend32_s")

/* Interpreter-only opcodes */
WABT_OPCODE(INTP, ___,  ___,  ___,  ___,  0,  0x00e0, InterpAlloca, "alloca")
WABT_OPCODE(INTP, ___,  ___,  ___,  ___,  0,  0x00e1, InterpBrUnless, "br_unless")
WABT_OPCODE(INTP, ___,  ___,  ___,  ___,  0,  0x00e2, InterpCallHost, "call_host")
WABT_OPCODE(INTP, ___,  ___,  ___,  ___,  0,  0x00e3, InterpData, "data")
WABT_OPCODE(INTP, ___,  ___,  ___,  ___,  0,  0x00e4, InterpDropKeep, "drop_keep")

/* Saturating float-to-int opcodes (--enable-saturating-float-to-int) */
WABT_OPCODE(SATI, I32,  F32,  ___,  ___,  0,  0xfc00, I32TruncSSatF32, "i32.trunc_s:sat/f32")
WABT_OPCODE(SATI, I32,  F32,  ___,  ___,  0,  0xfc01, I32TruncUSatF32, "i32.trunc_u:sat/f32")
WABT_OPCODE(SATI, I32,  F64,  ___,  ___,  0,  0xfc02, I32TruncSSatF64, "i32.trunc_s:sat/f64")
WABT_OPCODE(SATI, I32,  F64,  ___,  ___,  0,  0xfc03, I32TruncUSatF64, "i32.trunc_u:sat/f64")
WABT_OPCODE(SATI, I64,  F32,  ___,  ___,  0,  0xfc04, I64TruncSSatF32, "i64.trunc_s:sat/f32")
WABT_OPCODE(SATI, I64,  F32,  ___,  ___,  0,  0xfc05, I64TruncUSatF32, "i64.trunc_u:sat/f32")
WABT_OPCODE(SATI, I64,  F64,  ___,  ___,  0,  0xfc06, I64TruncSSatF64, "i64.trunc_s:sat/f64")
WABT_OPCODE(SATI, I64,  F64,  ___,  ___,  0,  0xfc07, I64TruncUSatF64, "i64.trunc_u:sat/f64")

/* Simd opcodes (--enable-simd) */
WABT_OPCODE(SIMD, V128, ___,  ___,  ___,  0,  0xfd00, V128Const, "uint128.const")
WABT_OPCODE(SIMD, V128, I32,  ___,  ___,  16, 0xfd01, V128Load,  "uint128.load")
WABT_OPCODE(SIMD, ___,  I32,  V128, ___,  16, 0xfd02, V128Store, "uint128.store")
WABT_OPCODE(SIMD, V128, I32,  ___,  ___,  0,  0xfd03, I8X16Splat, "i8x16.splat")
WABT_OPCODE(SIMD, V128, I32,  ___,  ___,  0,  0xfd04, I16X8Splat, "i16x8.splat")
WABT_OPCODE(SIMD, V128, I32,  ___,  ___,  0,  0xfd05, I32X4Splat, "i32x4.splat")
WABT_OPCODE(SIMD, V128, I64,  ___,  ___,  0,  0xfd06, I64X2Splat, "i64x2.splat")
WABT_OPCODE(SIMD, V128, F32,  ___,  ___,  0,  0xfd07, F32X4Splat, "f32x4.splat")
WABT_OPCODE(SIMD, V128, F64,  ___,  ___,  0,  0xfd08, F64X2Splat, "f64x2.splat")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd09, I8X16ExtractLaneS, "i8x16.extract_lane_s")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd0a, I8X16ExtractLaneU, "i8x16.extract_lane_u")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd0b, I16X8ExtractLaneS, "i16x8.extract_lane_s")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd0c, I16X8ExtractLaneU, "i16x8.extract_lane_u")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd0d, I32X4ExtractLane, "i32x4.extract_lane")
WABT_OPCODE(SIMD, I64,  V128, ___,  ___,  0,  0xfd0e, I64X2ExtractLane, "i64x2.extract_lane")
WABT_OPCODE(SIMD, F32,  V128, ___,  ___,  0,  0xfd0f, F32X4ExtractLane, "f32x4.extract_lane")
WABT_OPCODE(SIMD, F64,  V128, ___,  ___,  0,  0xfd10, F64X2ExtractLane, "f64x2.extract_lane")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd11, I8X16ReplaceLane, "i8x16.replace_lane")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd12, I16X8ReplaceLane, "i16x8.replace_lane")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd13, I32X4ReplaceLane, "i32x4.replace_lane")
WABT_OPCODE(SIMD, V128, V128, I64,  ___,  0,  0xfd14, I64X2ReplaceLane, "i64x2.replace_lane")
WABT_OPCODE(SIMD, V128, V128, F32,  ___,  0,  0xfd15, F32X4ReplaceLane, "f32x4.replace_lane")
WABT_OPCODE(SIMD, V128, V128, F64,  ___,  0,  0xfd16, F64X2ReplaceLane, "f64x2.replace_lane")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd17, V8X16Shuffle, "v8x16.shuffle")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd18, I8X16Add, "i8x16.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd19, I16X8Add, "i16x8.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1a, I32X4Add, "i32x4.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1b, I64X2Add, "i64x2.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1c, I8X16Sub, "i8x16.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1d, I16X8Sub, "i16x8.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1e, I32X4Sub, "i32x4.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd1f, I64X2Sub, "i64x2.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd20, I8X16Mul, "i8x16.mul")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd21, I16X8Mul, "i16x8.mul")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd22, I32X4Mul, "i32x4.mul")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd23, I8X16Neg, "i8x16.neg")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd24, I16X8Neg, "i16x8.neg")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd25, I32X4Neg, "i32x4.neg")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd26, I64X2Neg, "i64x2.neg")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd27, I8X16AddSaturateS, "i8x16.add_saturate_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd28, I8X16AddSaturateU, "i8x16.add_saturate_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd29, I16X8AddSaturateS, "i16x8.add_saturate_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd2a, I16X8AddSaturateU, "i16x8.add_saturate_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd2b, I8X16SubSaturateS, "i8x16.sub_saturate_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd2c, I8X16SubSaturateU, "i8x16.sub_saturate_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd2d, I16X8SubSaturateS, "i16x8.sub_saturate_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd2e, I16X8SubSaturateU, "i16x8.sub_saturate_u")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd2f, I8X16Shl, "i8x16.shl")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd30, I16X8Shl, "i16x8.shl")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd31, I32X4Shl, "i32x4.shl")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd32, I64X2Shl, "i64x2.shl")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd33, I8X16ShrS, "i8x16.shr_s")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd34, I8X16ShrU, "i8x16.shr_u")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd35, I16X8ShrS, "i16x8.shr_s")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd36, I16X8ShrU, "i16x8.shr_u")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd37, I32X4ShrS, "i32x4.shr_s")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd38, I32X4ShrU, "i32x4.shr_u")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd39, I64X2ShrS, "i64x2.shr_s")
WABT_OPCODE(SIMD, V128, V128, I32,  ___,  0,  0xfd3a, I64X2ShrU, "i64x2.shr_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd3b, V128And, "uint128.and")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd3c, V128Or,  "uint128.or")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd3d, V128Xor, "uint128.xor")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd3e, V128Not, "uint128.not")
WABT_OPCODE(SIMD, V128, V128, V128, V128, 0,  0xfd3f, V128BitSelect, "uint128.bitselect")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd40, I8X16AnyTrue, "i8x16.any_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd41, I16X8AnyTrue, "i16x8.any_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd42, I32X4AnyTrue, "i32x4.any_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd43, I64X2AnyTrue, "i64x2.any_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd44, I8X16AllTrue, "i8x16.all_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd45, I16X8AllTrue, "i16x8.all_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd46, I32X4AllTrue, "i32x4.all_true")
WABT_OPCODE(SIMD, I32,  V128, ___,  ___,  0,  0xfd47, I64X2AllTrue, "i64x2.all_true")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd48, I8X16Eq, "i8x16.eq")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd49, I16X8Eq, "i16x8.eq")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4a, I32X4Eq, "i32x4.eq")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4b, F32X4Eq, "f32x4.eq")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4c, F64X2Eq, "f64x2.eq")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4d, I8X16Ne, "i8x16.ne")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4e, I16X8Ne, "i16x8.ne")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd4f, I32X4Ne, "i32x4.ne")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd50, F32X4Ne, "f32x4.ne")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd51, F64X2Ne, "f64x2.ne")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd52, I8X16LtS, "i8x16.lt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd53, I8X16LtU, "i8x16.lt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd54, I16X8LtS, "i16x8.lt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd55, I16X8LtU, "i16x8.lt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd56, I32X4LtS, "i32x4.lt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd57, I32X4LtU, "i32x4.lt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd58, F32X4Lt, "f32x4.lt")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd59, F64X2Lt, "f64x2.lt")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5a, I8X16LeS, "i8x16.le_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5b, I8X16LeU, "i8x16.le_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5c, I16X8LeS, "i16x8.le_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5d, I16X8LeU, "i16x8.le_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5e, I32X4LeS, "i32x4.le_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd5f, I32X4LeU, "i32x4.le_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd60, F32X4Le, "f32x4.le")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd61, F64X2Le, "f64x2.le")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd62, I8X16GtS, "i8x16.gt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd63, I8X16GtU, "i8x16.gt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd64, I16X8GtS, "i16x8.gt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd65, I16X8GtU, "i16x8.gt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd66, I32X4GtS, "i32x4.gt_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd67, I32X4GtU, "i32x4.gt_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd68, F32X4Gt, "f32x4.gt")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd69, F64X2Gt, "f64x2.gt")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6a, I8X16GeS, "i8x16.ge_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6b, I8X16GeU, "i8x16.ge_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6c, I16X8GeS, "i16x8.ge_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6d, I16X8GeU, "i16x8.ge_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6e, I32X4GeS, "i32x4.ge_s")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd6f, I32X4GeU, "i32x4.ge_u")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd70, F32X4Ge, "f32x4.ge")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd71, F64X2Ge, "f64x2.ge")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd72, F32X4Neg, "f32x4.neg")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd73, F64X2Neg, "f64x2.neg")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd74, F32X4Abs, "f32x4.abs")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd75, F64X2Abs, "f64x2.abs")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd76, F32X4Min, "f32x4.min")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd77, F64X2Min, "f64x2.min")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd78, F32X4Max, "f32x4.max")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd79, F64X2Max, "f64x2.max")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7a, F32X4Add, "f32x4.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7b, F64X2Add, "f64x2.add")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7c, F32X4Sub, "f32x4.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7d, F64X2Sub, "f64x2.sub")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7e, F32X4Div, "f32x4.div")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd7f, F64X2Div, "f64x2.div")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd80, F32X4Mul, "f32x4.mul")
WABT_OPCODE(SIMD, V128, V128, V128, ___,  0,  0xfd81, F64X2Mul, "f64x2.mul")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd82, F32X4Sqrt, "f32x4.sqrt")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd83, F64X2Sqrt, "f64x2.sqrt")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd84, F32X4ConvertSI32X4, "f32x4.convert_s/i32x4")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd85, F32X4ConvertUI32X4, "f32x4.convert_u/i32x4")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd86, F64X2ConvertSI64X2, "f64x2.convert_s/i64x2")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd87, F64X2ConvertUI64X2, "f64x2.convert_u/i64x2")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd88, I32X4TruncSF32X4Sat,"i32x4.trunc_s/f32x4:sat")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd89, I32X4TruncUF32X4Sat,"i32x4.trunc_u/f32x4:sat")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd8a, I64X2TruncSF64X2Sat,"i64x2.trunc_s/f64x2:sat")
WABT_OPCODE(SIMD, V128, V128, ___,  ___,  0,  0xfd8b, I64X2TruncUF64X2Sat,"i64x2.trunc_u/f64x2:sat")

/* Thread opcodes (--enable-threads) */
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe00, AtomicWake, "atomic.wake")
WABT_OPCODE(THRD, I32,  I32,  I32,  I64,  4,  0xfe01, I32AtomicWait, "i32.atomic.wait")
WABT_OPCODE(THRD, I32,  I32,  I64,  I64,  8,  0xfe02, I64AtomicWait, "i64.atomic.wait")
WABT_OPCODE(THRD, I32,  I32,  ___,  ___,  4,  0xfe10, I32AtomicLoad, "i32.atomic.load")
WABT_OPCODE(THRD, I64,  I32,  ___,  ___,  8,  0xfe11, I64AtomicLoad, "i64.atomic.load")
WABT_OPCODE(THRD, I32,  I32,  ___,  ___,  1,  0xfe12, I32AtomicLoad8U, "i32.atomic.load8_u")
WABT_OPCODE(THRD, I32,  I32,  ___,  ___,  2,  0xfe13, I32AtomicLoad16U, "i32.atomic.load16_u")
WABT_OPCODE(THRD, I64,  I32,  ___,  ___,  1,  0xfe14, I64AtomicLoad8U, "i64.atomic.load8_u")
WABT_OPCODE(THRD, I64,  I32,  ___,  ___,  2,  0xfe15, I64AtomicLoad16U, "i64.atomic.load16_u")
WABT_OPCODE(THRD, I64,  I32,  ___,  ___,  4,  0xfe16, I64AtomicLoad32U, "i64.atomic.load32_u")
WABT_OPCODE(THRD, ___,  I32,  I32,  ___,  4,  0xfe17, I32AtomicStore, "i32.atomic.store")
WABT_OPCODE(THRD, ___,  I32,  I64,  ___,  8,  0xfe18, I64AtomicStore, "i64.atomic.store")
WABT_OPCODE(THRD, ___,  I32,  I32,  ___,  1,  0xfe19, I32AtomicStore8, "i32.atomic.store8")
WABT_OPCODE(THRD, ___,  I32,  I32,  ___,  2,  0xfe1a, I32AtomicStore16, "i32.atomic.store16")
WABT_OPCODE(THRD, ___,  I32,  I64,  ___,  1,  0xfe1b, I64AtomicStore8, "i64.atomic.store8")
WABT_OPCODE(THRD, ___,  I32,  I64,  ___,  2,  0xfe1c, I64AtomicStore16, "i64.atomic.store16")
WABT_OPCODE(THRD, ___,  I32,  I64,  ___,  4,  0xfe1d, I64AtomicStore32, "i64.atomic.store32")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe1e, I32AtomicRmwAdd, "i32.atomic.rmw.add")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe1f, I64AtomicRmwAdd, "i64.atomic.rmw.add")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe20, I32AtomicRmw8UAdd, "i32.atomic.rmw8_u.add")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe21, I32AtomicRmw16UAdd, "i32.atomic.rmw16_u.add")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe22, I64AtomicRmw8UAdd, "i64.atomic.rmw8_u.add")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe23, I64AtomicRmw16UAdd, "i64.atomic.rmw16_u.add")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe24, I64AtomicRmw32UAdd, "i64.atomic.rmw32_u.add")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe25, I32AtomicRmwSub, "i32.atomic.rmw.sub")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe26, I64AtomicRmwSub, "i64.atomic.rmw.sub")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe27, I32AtomicRmw8USub, "i32.atomic.rmw8_u.sub")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe28, I32AtomicRmw16USub, "i32.atomic.rmw16_u.sub")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe29, I64AtomicRmw8USub, "i64.atomic.rmw8_u.sub")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe2a, I64AtomicRmw16USub, "i64.atomic.rmw16_u.sub")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe2b, I64AtomicRmw32USub, "i64.atomic.rmw32_u.sub")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe2c, I32AtomicRmwAnd, "i32.atomic.rmw.and")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe2d, I64AtomicRmwAnd, "i64.atomic.rmw.and")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe2e, I32AtomicRmw8UAnd, "i32.atomic.rmw8_u.and")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe2f, I32AtomicRmw16UAnd, "i32.atomic.rmw16_u.and")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe30, I64AtomicRmw8UAnd, "i64.atomic.rmw8_u.and")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe31, I64AtomicRmw16UAnd, "i64.atomic.rmw16_u.and")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe32, I64AtomicRmw32UAnd, "i64.atomic.rmw32_u.and")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe33, I32AtomicRmwOr, "i32.atomic.rmw.or")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe34, I64AtomicRmwOr, "i64.atomic.rmw.or")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe35, I32AtomicRmw8UOr, "i32.atomic.rmw8_u.or")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe36, I32AtomicRmw16UOr, "i32.atomic.rmw16_u.or")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe37, I64AtomicRmw8UOr, "i64.atomic.rmw8_u.or")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe38, I64AtomicRmw16UOr, "i64.atomic.rmw16_u.or")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe39, I64AtomicRmw32UOr, "i64.atomic.rmw32_u.or")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe3a, I32AtomicRmwXor, "i32.atomic.rmw.xor")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe3b, I64AtomicRmwXor, "i64.atomic.rmw.xor")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe3c, I32AtomicRmw8UXor, "i32.atomic.rmw8_u.xor")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe3d, I32AtomicRmw16UXor, "i32.atomic.rmw16_u.xor")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe3e, I64AtomicRmw8UXor, "i64.atomic.rmw8_u.xor")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe3f, I64AtomicRmw16UXor, "i64.atomic.rmw16_u.xor")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe40, I64AtomicRmw32UXor, "i64.atomic.rmw32_u.xor")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  4,  0xfe41, I32AtomicRmwXchg, "i32.atomic.rmw.xchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  8,  0xfe42, I64AtomicRmwXchg, "i64.atomic.rmw.xchg")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  1,  0xfe43, I32AtomicRmw8UXchg, "i32.atomic.rmw8_u.xchg")
WABT_OPCODE(THRD, I32,  I32,  I32,  ___,  2,  0xfe44, I32AtomicRmw16UXchg, "i32.atomic.rmw16_u.xchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  1,  0xfe45, I64AtomicRmw8UXchg, "i64.atomic.rmw8_u.xchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  2,  0xfe46, I64AtomicRmw16UXchg, "i64.atomic.rmw16_u.xchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  ___,  4,  0xfe47, I64AtomicRmw32UXchg, "i64.atomic.rmw32_u.xchg")
WABT_OPCODE(THRD, I32,  I32,  I32,  I32,  4,  0xfe48, I32AtomicRmwCmpxchg, "i32.atomic.rmw.cmpxchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  I64,  8,  0xfe49, I64AtomicRmwCmpxchg, "i64.atomic.rmw.cmpxchg")
WABT_OPCODE(THRD, I32,  I32,  I32,  I32,  1,  0xfe4a, I32AtomicRmw8UCmpxchg, "i32.atomic.rmw8_u.cmpxchg")
WABT_OPCODE(THRD, I32,  I32,  I32,  I32,  2,  0xfe4b, I32AtomicRmw16UCmpxchg, "i32.atomic.rmw16_u.cmpxchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  I64,  1,  0xfe4c, I64AtomicRmw8UCmpxchg, "i64.atomic.rmw8_u.cmpxchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  I64,  2,  0xfe4d, I64AtomicRmw16UCmpxchg, "i64.atomic.rmw16_u.cmpxchg")
WABT_OPCODE(THRD, I64,  I32,  I64,  I64,  4,  0xfe4e, I64AtomicRmw32UCmpxchg, "i64.atomic.rmw32_u.cmpxchg")
#undef WABT_OPCODE
};

bool ExprVisitor::VisitExprList(ExprList& exprs) {
	for (Expr& expr : exprs)
		CHECK_RESULT(VisitExpr(&expr));
	return true;
}

bool ExprVisitor::VisitExpr(Expr* root_expr) {
	state_stack.clear();
	expr_stack.clear();
	expr_iter_stack.clear();

	PushDefault(root_expr);

	while (!state_stack.empty()) {
		State state = state_stack.back();
		auto* expr = expr_stack.back();

		switch (state) {
			case State::Default:
				PopDefault();
				CHECK_RESULT(HandleDefaultState(expr));
				break;

			case State::Block: {
				auto block_expr = cast<BlockExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != block_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndBlockExpr(block_expr));
					PopExprlist();
				}
				break;
			}
			case State::IfTrue: {
				auto if_expr = cast<IfExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_expr->true_.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.AfterIfTrueExpr(if_expr));
					PopExprlist();
					PushExprlist(State::IfFalse, expr, if_expr->false_);
				}
				break;
			}
			case State::IfFalse: {
				auto if_expr = cast<IfExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_expr->false_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndIfExpr(if_expr));
					PopExprlist();
				}
				break;
			}
			case State::IfExceptTrue: {
				auto if_except_expr = cast<IfExceptExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_except_expr->true_.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.AfterIfExceptTrueExpr(if_except_expr));
					PopExprlist();
					PushExprlist(State::IfExceptFalse, expr, if_except_expr->false_);
				}
				break;
			}
			case State::IfExceptFalse: {
				auto if_except_expr = cast<IfExceptExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != if_except_expr->false_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndIfExceptExpr(if_except_expr));
					PopExprlist();
				}
				break;
			}
			case State::Loop: {
				auto loop_expr = cast<LoopExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != loop_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndLoopExpr(loop_expr));
					PopExprlist();
				}
				break;
			}
			case State::Try: {
				auto try_expr = cast<TryExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != try_expr->block.exprs.end()) {
					PushDefault(&*iter++);
				} else {
					if (try_expr->catch_.empty()) {
						CHECK_RESULT(delegate.EndTryExpr(try_expr));
						PopExprlist();
					} else {
						CHECK_RESULT(delegate.OnCatchExpr(try_expr));
						PopExprlist();
						PushExprlist(State::Catch, expr, try_expr->catch_);
					}
				}
				break;
			}
			case State::Catch: {
				auto try_expr = cast<TryExpr>(expr);
				auto& iter = expr_iter_stack.back();
				if (iter != try_expr->catch_.end()) {
					PushDefault(&*iter++);
				} else {
					CHECK_RESULT(delegate.EndTryExpr(try_expr));
					PopExprlist();
				}
				break;
			}
		}
	}
	return true;
}

bool ExprVisitor::HandleDefaultState(Expr* expr) {
	switch (expr->type) {
		case Expr::Type::AtomicLoad:		return delegate.OnAtomicLoadExpr(cast<AtomicLoadExpr>(expr));
		case Expr::Type::AtomicStore:		return delegate.OnAtomicStoreExpr(cast<AtomicStoreExpr>(expr));
		case Expr::Type::AtomicRmw:			return delegate.OnAtomicRmwExpr(cast<AtomicRmwExpr>(expr));
		case Expr::Type::AtomicRmwCmpxchg:	return delegate.OnAtomicRmwCmpxchgExpr(cast<AtomicRmwCmpxchgExpr>(expr));
		case Expr::Type::AtomicWait:		return delegate.OnAtomicWaitExpr(cast<AtomicWaitExpr>(expr));
		case Expr::Type::AtomicWake:		return delegate.OnAtomicWakeExpr(cast<AtomicWakeExpr>(expr));
		case Expr::Type::Binary:			return delegate.OnBinaryExpr(cast<BinaryExpr>(expr));
		case Expr::Type::Block:			{	auto block_expr = cast<BlockExpr>(expr); return delegate.BeginBlockExpr(block_expr) && (PushExprlist(State::Block, expr, block_expr->block.exprs), true); }
		case Expr::Type::Br:				return delegate.OnBrExpr(cast<BrExpr>(expr));
		case Expr::Type::BrIf:				return delegate.OnBrIfExpr(cast<BrIfExpr>(expr));
		case Expr::Type::BrTable:			return delegate.OnBrTableExpr(cast<BrTableExpr>(expr));
		case Expr::Type::Call:				return delegate.OnCallExpr(cast<CallExpr>(expr));
		case Expr::Type::CallIndirect:		return delegate.OnCallIndirectExpr(cast<CallIndirectExpr>(expr));
		case Expr::Type::Compare:			return delegate.OnCompareExpr(cast<CompareExpr>(expr));
		case Expr::Type::Const:				return delegate.OnConstExpr(cast<ConstExpr>(expr));
		case Expr::Type::Convert:			return delegate.OnConvertExpr(cast<ConvertExpr>(expr));
		case Expr::Type::Drop:				return delegate.OnDropExpr(cast<DropExpr>(expr));
		case Expr::Type::GetGlobal:			return delegate.OnGetGlobalExpr(cast<GetGlobalExpr>(expr));
		case Expr::Type::GetLocal:			return delegate.OnGetLocalExpr(cast<GetLocalExpr>(expr));
		case Expr::Type::If:			{	auto if_expr = cast<IfExpr>(expr); return delegate.BeginIfExpr(if_expr) && (PushExprlist(State::IfTrue, expr, if_expr->true_.exprs), true); }
		case Expr::Type::IfExcept:		{	auto if_except_expr = cast<IfExceptExpr>(expr); return delegate.BeginIfExceptExpr(if_except_expr) && (PushExprlist(State::IfExceptTrue, expr, if_except_expr->true_.exprs), true); }
		case Expr::Type::Load:				return delegate.OnLoadExpr(cast<LoadExpr>(expr));
		case Expr::Type::Loop:			{	auto loop_expr = cast<LoopExpr>(expr); return delegate.BeginLoopExpr(loop_expr) && (PushExprlist(State::Loop, expr, loop_expr->block.exprs), true); }
		case Expr::Type::MemoryGrow:		return delegate.OnMemoryGrowExpr(cast<MemoryGrowExpr>(expr));
		case Expr::Type::MemorySize:		return delegate.OnMemorySizeExpr(cast<MemorySizeExpr>(expr));
		case Expr::Type::Nop:				return delegate.OnNopExpr(cast<NopExpr>(expr));
		case Expr::Type::Rethrow:			return delegate.OnRethrowExpr(cast<RethrowExpr>(expr));
		case Expr::Type::Return:			return delegate.OnReturnExpr(cast<ReturnExpr>(expr));
		case Expr::Type::Select:			return delegate.OnSelectExpr(cast<SelectExpr>(expr));
		case Expr::Type::SetGlobal:			return delegate.OnSetGlobalExpr(cast<SetGlobalExpr>(expr));
		case Expr::Type::SetLocal:			return delegate.OnSetLocalExpr(cast<SetLocalExpr>(expr));
		case Expr::Type::Store:				return delegate.OnStoreExpr(cast<StoreExpr>(expr));
		case Expr::Type::TeeLocal:			return delegate.OnTeeLocalExpr(cast<TeeLocalExpr>(expr));
		case Expr::Type::Throw:				return delegate.OnThrowExpr(cast<ThrowExpr>(expr));
		case Expr::Type::Try:			{	auto try_expr = cast<TryExpr>(expr); return delegate.BeginTryExpr(try_expr) && (PushExprlist(State::Try, expr, try_expr->block.exprs), true); }
		case Expr::Type::Unary:				return delegate.OnUnaryExpr(cast<UnaryExpr>(expr));
		case Expr::Type::Ternary:			return delegate.OnTernaryExpr(cast<TernaryExpr>(expr));
		case Expr::Type::SimdLaneOp:		return delegate.OnSimdLaneOpExpr(cast<SimdLaneOpExpr>(expr));
		case Expr::Type::SimdShuffleOp:		return delegate.OnSimdShuffleOpExpr(cast<SimdShuffleOpExpr>(expr));
		case Expr::Type::Unreachable:		return delegate.OnUnreachableExpr(cast<UnreachableExpr>(expr));
		default: return true;
	}
}

//-----------------------------------------------------------------------------
//	BinaryReader
//-----------------------------------------------------------------------------
#define ERROR_UNLESS(expr, ...)				do { if (!(expr)) { PrintError(__VA_ARGS__); return false; } } while (0)

#define WABT_BINARY_MAGIC					0x6d736100
#define WABT_BINARY_VERSION					1
#define WABT_BINARY_LIMITS_HAS_MAX_FLAG		0x1
#define WABT_BINARY_LIMITS_IS_SHARED_FLAG	0x2

struct LabelNode {
	LabelType	label_type;
	ExprList*	exprs;
	Expr*		context;
	LabelNode(LabelType label_type, ExprList* exprs, Expr* context = nullptr) : label_type(label_type), exprs(exprs), context(context) {}
};

class BinaryReader {
	Module&			module;
	const char*		filename;

	istream_ref		r;//memory_reader	r;
	Features		features;
	size_t			read_end;
	TypeVector		param_types;
	TypeVector		result_types;
	dynamic_array<Index> target_depths;
	bool			did_read_names_section	= false;
	Index			num_signatures			= 0;
	Index			num_imports				= 0;
	Index			num_func_imports		= 0;
	Index			num_table_imports		= 0;
	Index			num_memory_imports		= 0;
	Index			num_global_imports		= 0;
	Index			num_exception_imports	= 0;
	Index			num_function_signatures	= 0;
	Index			num_tables				= 0;
	Index			num_memories			= 0;
	Index			num_globals				= 0;
	Index			num_exports				= 0;
	Index			num_function_bodies		= 0;
	Index			num_exceptions			= 0;

	Func*						current_func		= nullptr;
	dynamic_array<LabelNode>	label_stack;
	ExprList*					current_init_expr	= nullptr;

	static string MakeDollarName(const char *name) {
		return string("$") + name;
	}

	bool HandleError(ErrorLevel, Offset offset, const char* message) {
		//	return error_handler->OnError(error_level, offset, message);
		return true;
	}
	Location GetLocation() const {
		return Location(count_string(filename), r.tell());
	}
	void PushLabel(LabelType label_type, ExprList* first, Expr* context = nullptr) {
		label_stack.emplace_back(label_type, first, context);
	}
	bool PopLabel() {
		if (label_stack.size() == 0) {
			PrintError("popping empty label stack");
			return false;
		}
		label_stack.pop_back();
		return true;
	}
	bool GetLabelAt(LabelNode** label, Index depth) {
		if (depth >= label_stack.size()) {
			//PrintError("accessing stack depth: %" PRIindex " >= max: %" PRIzd, depth, label_stack.size());
			return false;
		}
		*label = &label_stack[label_stack.size() - depth - 1];
		return true;
	}
	bool TopLabel(LabelNode** label) {
		return GetLabelAt(label, 0);
	}
	bool TopLabelExpr(LabelNode** label, Expr** expr) {
		CHECK_RESULT(TopLabel(label));
		LabelNode* parent_label;
		CHECK_RESULT(GetLabelAt(&parent_label, 1));
		*expr = &parent_label->exprs->back();
		return true;
	}
	bool AppendExpr(Expr *expr) {
		expr->loc = GetLocation();
		LabelNode* label;
		CHECK_RESULT(TopLabel(&label));
		label->exprs->push_back(expr);
		return true;
	}
	void SetBlockDeclaration(BlockDeclaration* decl, Type sig_type) {
		if (IsTypeIndex(sig_type)) {
			Index type_index = GetTypeIndex(sig_type);
			decl->has_func_type = true;
			decl->type_var = Var(type_index);
			decl->sig = module.func_types[type_index]->sig;
		} else {
			decl->has_func_type = false;
			decl->sig.param_types.clear();
			decl->sig.result_types = GetInlineTypeVector(sig_type);
		}
	}

public:

//class BinaryReader {

	void	PrintError(const char* format, ...) {
		ErrorLevel error_level = false//reading_custom_section && !options->fail_on_custom_section_error
			? ErrorLevel::Warning
			: ErrorLevel::Error;

		//WABT_SNPRINTF_ALLOCA(buffer, length, format);
		bool handled = HandleError(error_level, r.tell(), format);

		if (!handled) {
			// Not great to just print, but we don't want to eat the error either.
			//fprintf(stderr, "%07" PRIzx ": %s: %s\n", r.offset, GetErrorLevelName(error_level), buffer);
		}
	}
	bool	ReadOpcode(Opcode &out_value) {
		uint8 value = 0;
		if (!r.read(value))
			return false;

		if (Opcode::IsPrefixByte(value)) {
			uint32 code;
			if (!read_leb128(r, code))
				return false;
			out_value = Opcode::FromCode(value, code);
		} else {
			out_value = Opcode::FromCode(value);
		}
		return out_value.IsEnabled(features);
	}
	bool	ReadType(Type* out_value) {
		return read_leb128(r, *out_value);
	}
	bool	ReadStr(string* out_str) {
		uint32 str_len = 0;
		return read_leb128(r, str_len) && out_str->read(r, str_len);
	}
	bool	ReadIndex(Index* index) {
		return read_leb128(r, *index);
	}
	bool	ReadOffset(Offset* offset) {
		uint32 value;
		if (!read_leb128(r, value))
			return false;
		*offset = value;
		return true;
	}
	bool	ReadCount(Index* count) {
		return ReadIndex(count) && *count <= read_end - r.tell();
	}
	bool	IsConcreteType(Type type) const {
		switch (type) {
			case Type::I32:	case Type::I64:	case Type::F32:	case Type::F64:
				return true;
			case Type::V128:
				return features.IsEnabled(FeatureSet::simd);
			default:
				return false;
		}
	}
	bool	IsBlockType(Type type) const {
		if (IsConcreteType(type) || type == Type::Void)
			return true;
		if (!(features.IsEnabled(FeatureSet::multi_value) && IsTypeIndex(type)))
			return false;
		return GetTypeIndex(type) < num_signatures;
	}

	Index	NumTotalFuncs()		const	{ return num_func_imports + num_function_signatures;}
	Index	NumTotalTables()	const	{ return num_table_imports + num_tables;}
	Index	NumTotalMemories()	const	{ return num_memory_imports + num_memories;}
	Index	NumTotalGlobals()	const	{ return num_global_imports + num_globals; }

	bool	ReadI32InitExpr(Index index)	{ return ReadInitExpr(index, true); }
	bool	ReadInitExpr(Index index, bool require_i32 = false);
	bool	ReadTable(Type* out_elem_type, Limits* out_elem_limits);
	bool	ReadMemory(Limits* out_page_limits);
	bool	ReadGlobalHeader(Type* out_type, bool* out_mutable);
	bool	ReadExceptionType(TypeVector& sig);
	bool	ReadFunctionBody(Offset end_offset);
	bool	ReadNameSection(Offset section_size);
	bool	ReadRelocSection(Offset section_size);
	bool	ReadLinkingSection(Offset section_size);
	bool	ReadCustomSection(Offset section_size);
	bool	ReadTypeSection(Offset section_size);
	bool	ReadImportSection(Offset section_size);
	bool	ReadFunctionSection(Offset section_size);
	bool	ReadTableSection(Offset section_size);
	bool	ReadMemorySection(Offset section_size);
	bool	ReadGlobalSection(Offset section_size);
	bool	ReadExportSection(Offset section_size);
	bool	ReadStartSection(Offset section_size);
	bool	ReadElemSection(Offset section_size);
	bool	ReadCodeSection(Offset section_size);
	bool	ReadDataSection(Offset section_size);
	bool	ReadExceptionSection(Offset section_size);
	bool	ReadSections();

public:
	BinaryReader(Module &module, istream_ref file, const char* filename) : module(module), filename(filename), r(file), read_end(r.length()) {
	}

	bool ReadModule() {
		uint32 magic = 0;
		CHECK_RESULT(r.read(magic));
		ERROR_UNLESS(magic == WABT_BINARY_MAGIC, "bad magic value");
		uint32 version = 0;
		CHECK_RESULT(r.read(version));
		ERROR_UNLESS(version == WABT_BINARY_VERSION, "bad wasm file version");
		return ReadSections();
	}
};

bool BinaryReader::ReadInitExpr(Index index, bool require_i32) {
	Opcode opcode;
	CHECK_RESULT(ReadOpcode(opcode));

	switch (opcode) {
		case Opcode::I32Const: {
			int32 value = 0;
			CHECK_RESULT(read_leb128(r, value));
			Location loc	= GetLocation();
			current_init_expr->push_back(new ConstExpr(Const::I32(value, loc), loc));
			break;
		}
		case Opcode::I64Const: {
			int64 value = 0;
			CHECK_RESULT(read_leb128(r, value));
			Location loc	= GetLocation();
			current_init_expr->push_back(new ConstExpr(Const::I64(value, loc), loc));
			break;
		}
		case Opcode::F32Const: {
			uint32 value_bits = 0;
			CHECK_RESULT(r.read(value_bits));
			Location loc	= GetLocation();
			current_init_expr->push_back(new ConstExpr(Const::F32(value_bits, loc), loc));
			break;
		}
		case Opcode::F64Const: {
			uint64 value_bits = 0;
			CHECK_RESULT(r.read(value_bits));
			Location loc	= GetLocation();
			current_init_expr->push_back(new ConstExpr(Const::F64(value_bits, loc), loc));
			break;
		}
		case Opcode::V128Const: {
			uint128 value_bits;
			clear(value_bits);
			CHECK_RESULT(r.read(value_bits));
			Location loc	= GetLocation();
			current_init_expr->push_back(new ConstExpr(Const::V128(value_bits, loc), loc));
			break;
		}
		case Opcode::GetGlobal: {
			Index global_index;
			CHECK_RESULT(ReadIndex(&global_index));
			Location loc	= GetLocation();
			current_init_expr->push_back(new GetGlobalExpr(Var(global_index, loc), loc));
			break;
		}
		case Opcode::End:
			return true;

		default:
			return false;//Unexpected Opcode in initializer expression
	}

	if (require_i32 && opcode != Opcode::I32Const && opcode != Opcode::GetGlobal) {
		PrintError("expected i32 init_expr");
		return false;
	}

	CHECK_RESULT(ReadOpcode(opcode));
	ERROR_UNLESS(opcode == Opcode::End, "expected END opcode after initializer expression");
	return true;
}

bool BinaryReader::ReadTable(Type* out_elem_type, Limits* out_elem_limits) {
	CHECK_RESULT(ReadType(out_elem_type));
	ERROR_UNLESS(*out_elem_type == Type::Anyfunc, "table elem type must by anyfunc");

	uint32	flags, initial, max = 0;
	CHECK_RESULT(read_leb128(r, flags));
	CHECK_RESULT(read_leb128(r, initial));
	bool	has_max		= flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
	bool	is_shared	= flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
	ERROR_UNLESS(!is_shared, "tables may not be shared");
	if (has_max) {
		CHECK_RESULT(read_leb128(r, max));
		ERROR_UNLESS(initial <= max, "table initial elem count must be <= max elem count");
	}

	out_elem_limits->has_max	= has_max;
	out_elem_limits->initial	= initial;
	out_elem_limits->max		= max;
	return true;
}

bool BinaryReader::ReadMemory(Limits* out_page_limits) {
	uint32	flags;
	uint32	initial;
	uint32	max		= 0;
	CHECK_RESULT(read_leb128(r, flags));
	CHECK_RESULT(read_leb128(r, initial));
	ERROR_UNLESS(initial <= WABT_MAX_PAGES, "invalid memory initial size");
	bool	has_max		= flags & WABT_BINARY_LIMITS_HAS_MAX_FLAG;
	bool	is_shared	= flags & WABT_BINARY_LIMITS_IS_SHARED_FLAG;
	ERROR_UNLESS(!is_shared || has_max, "shared memory must have a max size");
	if (has_max) {
		CHECK_RESULT(read_leb128(r, max));
		ERROR_UNLESS(max <= WABT_MAX_PAGES, "invalid memory max size");
		ERROR_UNLESS(initial <= max, "memory initial size must be <= max size");
	}

	out_page_limits->has_max	= has_max;
	out_page_limits->is_shared	= is_shared;
	out_page_limits->initial	= initial;
	out_page_limits->max		= max;
	return true;
}

bool BinaryReader::ReadGlobalHeader(Type* out_type, bool* out_mutable) {
	Type	global_type = Type::Void;
	uint8	mut	= 0;
	CHECK_RESULT(ReadType(&global_type));
	ERROR_UNLESS(IsConcreteType(global_type), "invalid global type");

	CHECK_RESULT(r.read(mut));
	ERROR_UNLESS(mut <= 1, "global mutability must be 0 or 1");

	*out_type		= global_type;
	*out_mutable	= mut;
	return true;
}


bool BinaryReader::ReadFunctionBody(Offset end_offset) {
	bool seen_end_opcode = false;
	while (r.tell() < end_offset) {
		Opcode opcode;
		CHECK_RESULT(ReadOpcode(opcode));

		switch (opcode) {
			case Opcode::Unreachable:
				AppendExpr(new UnreachableExpr());
				break;

			case Opcode::Block: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				auto expr			= new BlockExpr();
				SetBlockDeclaration(&expr->block.decl, sig_type);
				ExprList* expr_list	= &expr->block.exprs;
				CHECK_RESULT(AppendExpr(expr));
				PushLabel(LabelType::Block, expr_list);
				break;
			}
			case Opcode::Loop: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				auto expr			= new LoopExpr();
				SetBlockDeclaration(&expr->block.decl, sig_type);
				ExprList* expr_list = &expr->block.exprs;
				CHECK_RESULT(AppendExpr(expr));
				PushLabel(LabelType::Loop, expr_list);
				break;
			}
			case Opcode::If: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				auto expr			= new IfExpr();
				SetBlockDeclaration(&expr->true_.decl, sig_type);
				ExprList* expr_list = &expr->true_.exprs;
				CHECK_RESULT(AppendExpr(expr));
				PushLabel(LabelType::If, expr_list);
				break;
			}
			case Opcode::Else: {
				LabelNode*	label;
				Expr*		expr;
				CHECK_RESULT(TopLabelExpr(&label, &expr));

				if (label->label_type == LabelType::If) {
					auto* if_expr					= cast<IfExpr>(expr);
					if_expr->true_.end_loc			= GetLocation();
					label->exprs					= &if_expr->false_;
					label->label_type				= LabelType::Else;
				} else if (label->label_type == LabelType::IfExcept) {
					auto* if_except_expr			= cast<IfExceptExpr>(expr);
					if_except_expr->true_.end_loc	= GetLocation();
					label->exprs					= &if_except_expr->false_;
					label->label_type				= LabelType::IfExceptElse;
				} else {
					PrintError("else expression without matching if");
					return false;
				}
				break;
			}

			case Opcode::Select:
				AppendExpr(new SelectExpr());
				break;

			case Opcode::Br: {
				Index depth;
				CHECK_RESULT(ReadIndex(&depth));
				AppendExpr(new BrExpr(Var(depth)));
				break;
			}
			case Opcode::BrIf: {
				Index depth;
				CHECK_RESULT(ReadIndex(&depth));
				AppendExpr(new BrIfExpr(Var(depth)));
				break;
			}
			case Opcode::BrTable: {
				Index num_targets;
				CHECK_RESULT(ReadIndex(&num_targets));
				target_depths.resize(num_targets);

				for (Index i = 0; i < num_targets; ++i)
					CHECK_RESULT(ReadIndex(&target_depths[i]));

				Index default_target_depth;
				CHECK_RESULT(ReadIndex(&default_target_depth));
				auto expr			= new BrTableExpr();
				expr->default_target= Var(default_target_depth);
				expr->targets.resize(num_targets);
				for (Index i = 0; i < num_targets; ++i)
					expr->targets[i]= Var(target_depths[i]);
				AppendExpr(expr);
				break;
			}
			case Opcode::Return:
				AppendExpr(new ReturnExpr());
				break;

			case Opcode::Nop:
				AppendExpr(new NopExpr());
				break;

			case Opcode::Drop:
				AppendExpr(new DropExpr());
				break;

			case Opcode::End:
				if (r.tell() == end_offset) {
					seen_end_opcode = true;
				} else {
					LabelNode* label;
					Expr* expr;
					CHECK_RESULT(TopLabelExpr(&label, &expr));
					switch (label->label_type) {
						case LabelType::Block:			cast<BlockExpr>(expr)->block.end_loc	= GetLocation();	break;
						case LabelType::Loop:			cast<LoopExpr>(expr)->block.end_loc		= GetLocation();	break;
						case LabelType::If:				cast<IfExpr>(expr)->true_.end_loc		= GetLocation();	break;
						case LabelType::Else:			cast<IfExpr>(expr)->false_end_loc		= GetLocation();	break;
						case LabelType::IfExcept:		cast<IfExceptExpr>(expr)->true_.end_loc	= GetLocation();	break;
						case LabelType::IfExceptElse:	cast<IfExceptExpr>(expr)->false_end_loc	= GetLocation();	break;
						case LabelType::Try:			cast<TryExpr>(expr)->block.end_loc		= GetLocation();	break;
						case LabelType::Func:
						case LabelType::Catch:			break;
					}
					PopLabel();
				}
				break;

			case Opcode::I32Const: {
				int32 value;
				CHECK_RESULT(read_leb128(r, value));
				AppendExpr(new ConstExpr(Const::I32(value, GetLocation())));
				break;
			}
			case Opcode::I64Const: {
				int64 value;
				CHECK_RESULT(read_leb128(r, value));
				AppendExpr(new ConstExpr(Const::I64(value, GetLocation())));
				break;
			}
			case Opcode::F32Const: {
				uint32 value_bits = 0;
				CHECK_RESULT(r.read(value_bits));
				AppendExpr(new ConstExpr(Const::F32(value_bits, GetLocation())));
				break;
			}
			case Opcode::F64Const: {
				uint64 value_bits = 0;
				CHECK_RESULT(r.read(value_bits));
				AppendExpr(new ConstExpr(Const::F64(value_bits, GetLocation())));
				break;
			}
			case Opcode::V128Const: {
				uint128 value_bits;
				clear(value_bits);
				CHECK_RESULT(r.read(value_bits));
				AppendExpr(new ConstExpr(Const::V128(value_bits, GetLocation())));
				break;
			}
			case Opcode::GetGlobal: {
				Index global_index;
				CHECK_RESULT(ReadIndex(&global_index));
				AppendExpr(new GetGlobalExpr(Var(global_index, GetLocation())));
				break;
			}
			case Opcode::GetLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				AppendExpr(new GetLocalExpr(Var(local_index, GetLocation())));
				break;
			}
			case Opcode::SetGlobal: {
				Index global_index;
				CHECK_RESULT(ReadIndex(&global_index));
				AppendExpr(new SetGlobalExpr(Var(global_index, GetLocation())));
				break;
			}
			case Opcode::SetLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				AppendExpr(new SetLocalExpr(Var(local_index, GetLocation())));
				break;
			}
			case Opcode::Call: {
				Index func_index;
				CHECK_RESULT(ReadIndex(&func_index));
				ERROR_UNLESS(func_index < NumTotalFuncs(), "invalid call function index");
				ISO_ASSERT(func_index < module.funcs.size());
				AppendExpr(new CallExpr(Var(func_index)));
				break;
			}
			case Opcode::CallIndirect: {
				Index sig_index;
				CHECK_RESULT(ReadIndex(&sig_index));
				ERROR_UNLESS(sig_index < num_signatures, "invalid call_indirect signature index");
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "call_indirect reserved value must be 0");
				ISO_ASSERT(sig_index < module.func_types.size());
				auto expr					= new CallIndirectExpr();
				expr->decl.has_func_type	= true;
				expr->decl.type_var			= Var(sig_index, GetLocation());
				expr->decl.sig				= module.func_types[sig_index]->sig;
				AppendExpr(expr);
				break;
			}
			case Opcode::TeeLocal: {
				Index local_index;
				CHECK_RESULT(ReadIndex(&local_index));
				AppendExpr(new TeeLocalExpr(Var(local_index, GetLocation())));
				break;
			}
			case Opcode::I32Load8S:			case Opcode::I32Load8U:			case Opcode::I32Load16S:		case Opcode::I32Load16U:
			case Opcode::I64Load8S:			case Opcode::I64Load8U:			case Opcode::I64Load16S:		case Opcode::I64Load16U:
			case Opcode::I64Load32S:		case Opcode::I64Load32U:		case Opcode::I32Load:			case Opcode::I64Load:
			case Opcode::F32Load:			case Opcode::F64Load:			case Opcode::V128Load:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new LoadExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32Store8:			case Opcode::I32Store16:		case Opcode::I64Store8:			case Opcode::I64Store16:
			case Opcode::I64Store32:		case Opcode::I32Store:			case Opcode::I64Store:			case Opcode::F32Store:
			case Opcode::F64Store:			case Opcode::V128Store:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new StoreExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::MemorySize: {
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "memory.size reserved value must be 0");
				AppendExpr(new MemorySizeExpr());
				break;
			}
			case Opcode::MemoryGrow: {
				uint32 reserved;
				CHECK_RESULT(read_leb128(r, reserved));
				ERROR_UNLESS(reserved == 0, "memory.grow reserved value must be 0");
				AppendExpr(new MemoryGrowExpr());
				break;
			}
			case Opcode::I32Add:			case Opcode::I32Sub:			case Opcode::I32Mul:			case Opcode::I32DivS:	case Opcode::I32DivU:	case Opcode::I32RemS:	case Opcode::I32RemU:
			case Opcode::I32And:			case Opcode::I32Or:				case Opcode::I32Xor:			case Opcode::I32Shl:	case Opcode::I32ShrU:	case Opcode::I32ShrS:	case Opcode::I32Rotr:		case Opcode::I32Rotl:
			case Opcode::I64Add:			case Opcode::I64Sub:			case Opcode::I64Mul:			case Opcode::I64DivS:	case Opcode::I64DivU:	case Opcode::I64RemS:	case Opcode::I64RemU:
			case Opcode::I64And:			case Opcode::I64Or:				case Opcode::I64Xor:			case Opcode::I64Shl:	case Opcode::I64ShrU:	case Opcode::I64ShrS:	case Opcode::I64Rotr:		case Opcode::I64Rotl:
			case Opcode::F32Add:			case Opcode::F32Sub:			case Opcode::F32Mul:			case Opcode::F32Div:	case Opcode::F32Min:	case Opcode::F32Max:	case Opcode::F32Copysign:
			case Opcode::F64Add:			case Opcode::F64Sub:			case Opcode::F64Mul:			case Opcode::F64Div:	case Opcode::F64Min:	case Opcode::F64Max:	case Opcode::F64Copysign:
			case Opcode::I8X16Add:			case Opcode::I16X8Add:			case Opcode::I32X4Add:			case Opcode::I64X2Add:
			case Opcode::I8X16Sub:			case Opcode::I16X8Sub:			case Opcode::I32X4Sub:			case Opcode::I64X2Sub:
			case Opcode::I8X16Mul:			case Opcode::I16X8Mul:			case Opcode::I32X4Mul:
			case Opcode::I8X16AddSaturateS:	case Opcode::I8X16AddSaturateU:	case Opcode::I16X8AddSaturateS:	case Opcode::I16X8AddSaturateU:
			case Opcode::I8X16SubSaturateS:	case Opcode::I8X16SubSaturateU:	case Opcode::I16X8SubSaturateS:	case Opcode::I16X8SubSaturateU:
			case Opcode::I8X16Shl:			case Opcode::I16X8Shl:			case Opcode::I32X4Shl:			case Opcode::I64X2Shl:
			case Opcode::I8X16ShrS:			case Opcode::I8X16ShrU:			case Opcode::I16X8ShrS:			case Opcode::I16X8ShrU:	case Opcode::I32X4ShrS:	case Opcode::I32X4ShrU:	case Opcode::I64X2ShrS:		case Opcode::I64X2ShrU:
			case Opcode::V128And:			case Opcode::V128Or:			case Opcode::V128Xor:			case Opcode::F32X4Min:
			case Opcode::F64X2Min:			case Opcode::F32X4Max:			case Opcode::F64X2Max:
			case Opcode::F32X4Add:			case Opcode::F64X2Add:			case Opcode::F32X4Sub:			case Opcode::F64X2Sub:
			case Opcode::F32X4Div:			case Opcode::F64X2Div:			case Opcode::F32X4Mul:			case Opcode::F64X2Mul:
				AppendExpr(new BinaryExpr(opcode));
				break;

			case Opcode::I32Eq:		case Opcode::I32Ne:		case Opcode::I32LtS:	case Opcode::I32LeS:	case Opcode::I32LtU:	case Opcode::I32LeU:	case Opcode::I32GtS:	case Opcode::I32GeS:	case Opcode::I32GtU:	case Opcode::I32GeU:
			case Opcode::I64Eq:		case Opcode::I64Ne:		case Opcode::I64LtS:	case Opcode::I64LeS:	case Opcode::I64LtU:	case Opcode::I64LeU:	case Opcode::I64GtS:	case Opcode::I64GeS:	case Opcode::I64GtU:	case Opcode::I64GeU:
			case Opcode::F32Eq:		case Opcode::F32Ne:		case Opcode::F32Lt:		case Opcode::F32Le:		case Opcode::F32Gt:		case Opcode::F32Ge:
			case Opcode::F64Eq:		case Opcode::F64Ne:		case Opcode::F64Lt:		case Opcode::F64Le:		case Opcode::F64Gt:		case Opcode::F64Ge:
			case Opcode::I8X16Eq:	case Opcode::I16X8Eq:	case Opcode::I32X4Eq:	case Opcode::F32X4Eq:	case Opcode::F64X2Eq:
			case Opcode::I8X16Ne:	case Opcode::I16X8Ne:	case Opcode::I32X4Ne:	case Opcode::F32X4Ne:	case Opcode::F64X2Ne:
			case Opcode::I8X16LtS:	case Opcode::I8X16LtU:	case Opcode::I16X8LtS:	case Opcode::I16X8LtU:	case Opcode::I32X4LtS:	case Opcode::I32X4LtU:	case Opcode::F32X4Lt:	case Opcode::F64X2Lt:
			case Opcode::I8X16LeS:	case Opcode::I8X16LeU:	case Opcode::I16X8LeS:	case Opcode::I16X8LeU:	case Opcode::I32X4LeS:	case Opcode::I32X4LeU:	case Opcode::F32X4Le:	case Opcode::F64X2Le:
			case Opcode::I8X16GtS:	case Opcode::I8X16GtU:	case Opcode::I16X8GtS:	case Opcode::I16X8GtU:	case Opcode::I32X4GtS:	case Opcode::I32X4GtU:	case Opcode::F32X4Gt:	case Opcode::F64X2Gt:
			case Opcode::I8X16GeS:	case Opcode::I8X16GeU:	case Opcode::I16X8GeS:	case Opcode::I16X8GeU:	case Opcode::I32X4GeS:	case Opcode::I32X4GeU:	case Opcode::F32X4Ge:	case Opcode::F64X2Ge:
				AppendExpr(new CompareExpr(opcode));
				break;

			case Opcode::I32Clz:		case Opcode::I32Ctz:		case Opcode::I32Popcnt:
			case Opcode::I64Clz:		case Opcode::I64Ctz:		case Opcode::I64Popcnt:
			case Opcode::F32Abs:		case Opcode::F32Neg:		case Opcode::F32Ceil:		case Opcode::F32Floor:		case Opcode::F32Trunc:		case Opcode::F32Nearest:	case Opcode::F32Sqrt:
			case Opcode::F64Abs:		case Opcode::F64Neg:		case Opcode::F64Ceil:		case Opcode::F64Floor:		case Opcode::F64Trunc:		case Opcode::F64Nearest:	case Opcode::F64Sqrt:
			case Opcode::I8X16Splat:	case Opcode::I16X8Splat:	case Opcode::I32X4Splat:	case Opcode::I64X2Splat:	case Opcode::F32X4Splat:	case Opcode::F64X2Splat:
			case Opcode::I8X16Neg:		case Opcode::I16X8Neg:		case Opcode::I32X4Neg:		case Opcode::I64X2Neg:
			case Opcode::V128Not:
			case Opcode::I8X16AnyTrue:	case Opcode::I16X8AnyTrue:	case Opcode::I32X4AnyTrue:	case Opcode::I64X2AnyTrue:	case Opcode::I8X16AllTrue:	case Opcode::I16X8AllTrue:	case Opcode::I32X4AllTrue:	case Opcode::I64X2AllTrue:
			case Opcode::F32X4Neg:		case Opcode::F64X2Neg:		case Opcode::F32X4Abs:		case Opcode::F64X2Abs:		case Opcode::F32X4Sqrt:		case Opcode::F64X2Sqrt:
				AppendExpr(new UnaryExpr(opcode));
				break;

			case Opcode::V128BitSelect:
				AppendExpr(new TernaryExpr(opcode));
				break;

			case Opcode::I8X16ExtractLaneS:	case Opcode::I8X16ExtractLaneU:	case Opcode::I16X8ExtractLaneS:	case Opcode::I16X8ExtractLaneU:	case Opcode::I32X4ExtractLane:	case Opcode::I64X2ExtractLane:	case Opcode::F32X4ExtractLane:	case Opcode::F64X2ExtractLane:
			case Opcode::I8X16ReplaceLane:	case Opcode::I16X8ReplaceLane:	case Opcode::I32X4ReplaceLane:	case Opcode::I64X2ReplaceLane:	case Opcode::F32X4ReplaceLane:	case Opcode::F64X2ReplaceLane:
			{
				uint8 lane_val;
				CHECK_RESULT(r.read(lane_val));
				AppendExpr(new SimdLaneOpExpr(opcode, lane_val));
				break;
			}

			case Opcode::V8X16Shuffle: {
				uint128 value;
				CHECK_RESULT(r.read(value));
				AppendExpr(new SimdShuffleOpExpr(opcode, value));
				break;
			}

			case Opcode::I32TruncSF32:		case Opcode::I32TruncSF64:			case Opcode::I32TruncUF32:			case Opcode::I32TruncUF64:			case Opcode::I32WrapI64:
			case Opcode::I64TruncSF32:		case Opcode::I64TruncSF64:			case Opcode::I64TruncUF32:			case Opcode::I64TruncUF64:
			case Opcode::I64ExtendSI32:		case Opcode::I64ExtendUI32:
			case Opcode::F32ConvertSI32:	case Opcode::F32ConvertUI32:		case Opcode::F32ConvertSI64:		case Opcode::F32ConvertUI64:
			case Opcode::F32DemoteF64:		case Opcode::F32ReinterpretI32:
			case Opcode::F64ConvertSI32:	case Opcode::F64ConvertUI32:		case Opcode::F64ConvertSI64:		case Opcode::F64ConvertUI64:		case Opcode::F64PromoteF32:
			case Opcode::F64ReinterpretI64:	case Opcode::I32ReinterpretF32:		case Opcode::I64ReinterpretF64:
			case Opcode::I32Eqz:			case Opcode::I64Eqz:
			case Opcode::F32X4ConvertSI32X4:case Opcode::F32X4ConvertUI32X4:	case Opcode::F64X2ConvertSI64X2:	case Opcode::F64X2ConvertUI64X2:	case Opcode::I32X4TruncSF32X4Sat:	case Opcode::I32X4TruncUF32X4Sat:	case Opcode::I64X2TruncSF64X2Sat:	case Opcode::I64X2TruncUF64X2Sat:
				AppendExpr(new ConvertExpr(opcode));
				break;

			case Opcode::Try: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				auto		expr		= new TryExpr;
				ExprList*	expr_list	= &expr->block.exprs;
				SetBlockDeclaration(&expr->block.decl, sig_type);
				CHECK_RESULT(AppendExpr(expr));
				PushLabel(LabelType::Try, expr_list, expr);
				break;
			}
			case Opcode::Catch: {
				LabelNode* label;
				CHECK_RESULT(TopLabel(&label));
				if (label->label_type != LabelType::Try) {
					PrintError("catch expression without matching try");
					return false;
				}

				LabelNode* parent_label;
				CHECK_RESULT(GetLabelAt(&parent_label, 1));

				label->label_type	= LabelType::Catch;
				label->exprs		= &cast<TryExpr>(&parent_label->exprs->back())->catch_;
				break;
			}
			case Opcode::Rethrow: {
				AppendExpr(new RethrowExpr());
				break;
			}
			case Opcode::Throw: {
				Index index;
				CHECK_RESULT(ReadIndex(&index));
				AppendExpr(new ThrowExpr(Var(index, GetLocation())));
				break;
			}
			case Opcode::IfExcept: {
				Type sig_type;
				CHECK_RESULT(ReadType(&sig_type));
				ERROR_UNLESS(IsBlockType(sig_type), "expected valid block signature type");
				Index except_index;
				CHECK_RESULT(ReadIndex(&except_index));

				auto expr			= new IfExceptExpr();
				expr->except_var	= Var(except_index, GetLocation());
				SetBlockDeclaration(&expr->true_.decl, sig_type);
				ExprList* expr_list = &expr->true_.exprs;
				CHECK_RESULT(AppendExpr(expr));
				PushLabel(LabelType::IfExcept, expr_list);
				break;
			}
			case Opcode::I32Extend8S:		case Opcode::I32Extend16S:		case Opcode::I64Extend8S:		case Opcode::I64Extend16S:		case Opcode::I64Extend32S:
				AppendExpr(new UnaryExpr(opcode));
				break;

			case Opcode::I32TruncSSatF32:	case Opcode::I32TruncUSatF32:	case Opcode::I32TruncSSatF64:	case Opcode::I32TruncUSatF64:	case Opcode::I64TruncSSatF32:	case Opcode::I64TruncUSatF32:	case Opcode::I64TruncSSatF64:	case Opcode::I64TruncUSatF64:
				AppendExpr(new ConvertExpr(opcode));
				break;

			case Opcode::AtomicWake: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicWakeExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32AtomicWait:
			case Opcode::I64AtomicWait: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicWaitExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32AtomicLoad8U:	case Opcode::I32AtomicLoad16U:	case Opcode::I64AtomicLoad8U:	case Opcode::I64AtomicLoad16U:	case Opcode::I64AtomicLoad32U:	case Opcode::I32AtomicLoad:		case Opcode::I64AtomicLoad:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicLoadExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32AtomicStore8:	case Opcode::I32AtomicStore16:	case Opcode::I64AtomicStore8:	case Opcode::I64AtomicStore16:	case Opcode::I64AtomicStore32:	case Opcode::I32AtomicStore:	case Opcode::I64AtomicStore:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicStoreExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32AtomicRmwAdd:	case Opcode::I64AtomicRmwAdd:	case Opcode::I32AtomicRmw8UAdd:		case Opcode::I32AtomicRmw16UAdd:	case Opcode::I64AtomicRmw8UAdd:		case Opcode::I64AtomicRmw16UAdd:	case Opcode::I64AtomicRmw32UAdd:
			case Opcode::I32AtomicRmwSub:	case Opcode::I64AtomicRmwSub:	case Opcode::I32AtomicRmw8USub:		case Opcode::I32AtomicRmw16USub:	case Opcode::I64AtomicRmw8USub:		case Opcode::I64AtomicRmw16USub:	case Opcode::I64AtomicRmw32USub:
			case Opcode::I32AtomicRmwAnd:	case Opcode::I64AtomicRmwAnd:	case Opcode::I32AtomicRmw8UAnd:		case Opcode::I32AtomicRmw16UAnd:	case Opcode::I64AtomicRmw8UAnd:		case Opcode::I64AtomicRmw16UAnd:	case Opcode::I64AtomicRmw32UAnd:
			case Opcode::I32AtomicRmwOr:	case Opcode::I64AtomicRmwOr:	case Opcode::I32AtomicRmw8UOr:		case Opcode::I32AtomicRmw16UOr:		case Opcode::I64AtomicRmw8UOr:		case Opcode::I64AtomicRmw16UOr:		case Opcode::I64AtomicRmw32UOr:
			case Opcode::I32AtomicRmwXor:	case Opcode::I64AtomicRmwXor:	case Opcode::I32AtomicRmw8UXor:		case Opcode::I32AtomicRmw16UXor:	case Opcode::I64AtomicRmw8UXor:		case Opcode::I64AtomicRmw16UXor:	case Opcode::I64AtomicRmw32UXor:
			case Opcode::I32AtomicRmwXchg:	case Opcode::I64AtomicRmwXchg:	case Opcode::I32AtomicRmw8UXchg:	case Opcode::I32AtomicRmw16UXchg:	case Opcode::I64AtomicRmw8UXchg:	case Opcode::I64AtomicRmw16UXchg:	case Opcode::I64AtomicRmw32UXchg:
			{
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicRmwExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			case Opcode::I32AtomicRmwCmpxchg:
			case Opcode::I64AtomicRmwCmpxchg:
			case Opcode::I32AtomicRmw8UCmpxchg:
			case Opcode::I32AtomicRmw16UCmpxchg:
			case Opcode::I64AtomicRmw8UCmpxchg:
			case Opcode::I64AtomicRmw16UCmpxchg:
			case Opcode::I64AtomicRmw32UCmpxchg: {
				uint32 alignment_log2;
				CHECK_RESULT(read_leb128(r, alignment_log2));
				Address offset;
				CHECK_RESULT(read_leb128(r, offset));
				AppendExpr(new AtomicRmwCmpxchgExpr(opcode, 1 << alignment_log2, offset));
				break;
			}
			default:
				return false;//ReportUnexpectedOpcode(opcode);
		}
	}
	ERROR_UNLESS(r.tell() == end_offset, "function body longer than given size");
	ERROR_UNLESS(seen_end_opcode, "function body must end with END opcode");
	return true;
}

bool BinaryReader::ReadNameSection(Offset section_size) {
	Index i = 0;
	uint32 previous_subsection_type = 0;
	while (r.tell() < read_end) {
		uint32 name_type;
		Offset subsection_size;
		CHECK_RESULT(read_leb128(r, name_type));
		if (i != 0) {
			ERROR_UNLESS(name_type != previous_subsection_type, "duplicate sub-section");
			ERROR_UNLESS(name_type >= previous_subsection_type, "out-of-order sub-section");
		}
		previous_subsection_type = name_type;
		CHECK_RESULT(ReadOffset(&subsection_size));
		size_t subsection_end = r.tell() + subsection_size;
		ERROR_UNLESS(subsection_end <= read_end, "invalid sub-section size: extends past end");
		auto	guard	= save(read_end, subsection_end);

		switch (static_cast<NameSectionSubsection>(name_type)) {
			case NameSectionSubsection::Module:
				if (subsection_size) {
					string name;
					CHECK_RESULT(ReadStr(&name));
					if (!name.empty())
						module.name	= MakeDollarName(name);
				}
				break;
			case NameSectionSubsection::Function:
				if (subsection_size) {
					Index num_names;
					CHECK_RESULT(ReadCount(&num_names));
					if (num_names > module.funcs.size()) {
						//PrintError("expected function name count (%" PRIindex ") <= function count (%" PRIzd ")", count, module.funcs.size());
						return false;
					}

					Index last_function_index = kInvalidIndex;

					for (Index j = 0; j < num_names; ++j) {
						Index	function_index;
						string	function_name;
						CHECK_RESULT(ReadIndex(&function_index));
						ERROR_UNLESS(function_index != last_function_index, "duplicate function name");
						ERROR_UNLESS(last_function_index == kInvalidIndex || function_index > last_function_index, "function index out of order");
						last_function_index = function_index;
						ERROR_UNLESS(function_index < NumTotalFuncs(), "invalid function index");
						CHECK_RESULT(ReadStr(&function_name));

						if (!function_name.empty()) {
							Func* func			= module.funcs[function_index];
							string dollar_name	= MakeDollarName(function_name);
							int counter			= 1;
							string orig_name	= dollar_name;
							while (module.func_bindings.count(dollar_name) != 0)
								dollar_name		= orig_name + "." + to_string(counter++);
							func->name			= dollar_name;
							module.func_bindings[dollar_name] = Binding(function_index);
						}
					}
				}
				break;
			case NameSectionSubsection::Local:
				if (subsection_size) {
					Index num_funcs;
					CHECK_RESULT(ReadCount(&num_funcs));
					Index last_function_index = kInvalidIndex;
					for (Index j = 0; j < num_funcs; ++j) {
						Index function_index;
						CHECK_RESULT(ReadIndex(&function_index));
						ERROR_UNLESS(function_index < NumTotalFuncs(), "invalid function index");
						ERROR_UNLESS(last_function_index == kInvalidIndex ||
							function_index > last_function_index, "locals function index out of order");
						last_function_index = function_index;
						Index num_locals;
						CHECK_RESULT(ReadCount(&num_locals));

						ISO_ASSERT(function_index < module.funcs.size());
						Func* func			= module.funcs[function_index];
						Index num_params_and_locals = func->GetNumParamsAndLocals();
						if (num_locals > num_params_and_locals) {
							//PrintError("expected local name count (%" PRIindex ") <= local count (%" PRIindex ")", count, num_params_and_locals);
							return false;
						}

						Index last_local_index = kInvalidIndex;
						for (Index k = 0; k < num_locals; ++k) {
							Index	local_index;
							string	local_name;

							CHECK_RESULT(ReadIndex(&local_index));
							ERROR_UNLESS(local_index != last_local_index, "duplicate local index");
							ERROR_UNLESS(last_local_index == kInvalidIndex ||
								local_index > last_local_index, "local index out of order");
							last_local_index = local_index;
							CHECK_RESULT(ReadStr(&local_name));

							if (!local_name.empty()) {
								Func* func			= module.funcs[function_index];
								Index num_params	= func->GetNumParams();
								BindingHash* bindings;
								Index index;
								if (local_index < num_params) {
									/* param name */
									bindings		= &func->param_bindings;
									index			= local_index;
								} else {
									/* local name */
									bindings		= &func->local_bindings;
									index			= local_index - num_params;
								}
								(*bindings)[MakeDollarName(local_name)]	= Binding(index);
							}
						}
					}
				}
				break;
			default:
				// Unknown subsection, skip it.
				r.seek(subsection_end);
				break;
		}
		++i;
		ERROR_UNLESS(r.tell() == subsection_end, "unfinished sub-section");
	}
	return true;
}

bool BinaryReader::ReadRelocSection(Offset section_size) {
	uint32 section_index;
	CHECK_RESULT(read_leb128(r, section_index));
	Index num_relocs;
	CHECK_RESULT(ReadCount(&num_relocs));
	for (Index i = 0; i < num_relocs; ++i) {
		Offset	offset;
		Index	index;
		uint32	reloc_type;
		int32	addend = 0;
		CHECK_RESULT(read_leb128(r, reloc_type));
		CHECK_RESULT(ReadOffset(&offset));
		CHECK_RESULT(ReadIndex(&index));
		Reloc::Type type = static_cast<Reloc::Type>(reloc_type);
		switch (type) {
			case Reloc::MemoryAddressLEB:
			case Reloc::MemoryAddressSLEB:
			case Reloc::MemoryAddressI32:
			case Reloc::FunctionOffsetI32:
			case Reloc::SectionOffsetI32:
				CHECK_RESULT(read_leb128(r, addend));
				break;
			default:
				break;
		}
	}
	return true;
}

bool BinaryReader::ReadLinkingSection(Offset section_size) {
	uint32 version;
	CHECK_RESULT(read_leb128(r, version));
	ERROR_UNLESS(version == 1, "invalid linking metadata version");
	while (r.tell() < read_end) {
		uint32 linking_type;
		Offset subsection_size;
		CHECK_RESULT(read_leb128(r, linking_type));
		CHECK_RESULT(ReadOffset(&subsection_size));
		size_t subsection_end = r.tell() + subsection_size;
		ERROR_UNLESS(subsection_end <= read_end, "invalid sub-section size: extends past end");
		auto	guard	= save(read_end, subsection_end);

		uint32 count;
		switch (static_cast<LinkingEntryType>(linking_type)) {
			case LinkingEntryType::SymbolTable:
				CHECK_RESULT(read_leb128(r, count));
				for (Index i = 0; i < count; ++i) {
					string name;
					uint32 flags	= 0;
					uint32 kind		= 0;
					CHECK_RESULT(read_leb128(r, kind));
					CHECK_RESULT(read_leb128(r, flags));

					Symbol::Type sym_type = static_cast<Symbol::Type>(kind);
					switch (sym_type) {
						case Symbol::Type::Function:
						case Symbol::Type::Global: {
							uint32 index = 0;
							CHECK_RESULT(read_leb128(r, index));
							if ((flags & Symbol::Undefined) == 0)
								CHECK_RESULT(ReadStr(&name));
							break;
						}
						case Symbol::Type::Data: {
							uint32 segment = 0;
							uint32 offset = 0;
							uint32 size = 0;
							CHECK_RESULT(ReadStr(&name));
							if ((flags & Symbol::Undefined) == 0) {
								CHECK_RESULT(read_leb128(r, segment));
								CHECK_RESULT(read_leb128(r, offset));
								CHECK_RESULT(read_leb128(r, size));
							}
							break;
						}
						case Symbol::Type::Section: {
							uint32 index = 0;
							CHECK_RESULT(read_leb128(r, index));
							break;
						}
					}
				}
				break;

			case LinkingEntryType::SegmentInfo:
				CHECK_RESULT(read_leb128(r, count));
				for (Index i = 0; i < count; i++) {
					string name;
					uint32 alignment;
					uint32 flags;
					CHECK_RESULT(ReadStr(&name));
					CHECK_RESULT(read_leb128(r, alignment));
					CHECK_RESULT(read_leb128(r, flags));
				}
				break;

			case LinkingEntryType::InitFunctions:
				CHECK_RESULT(read_leb128(r, count));
				while (count--) {
					uint32 priority;
					uint32 func;
					CHECK_RESULT(read_leb128(r, priority));
					CHECK_RESULT(read_leb128(r, func));
				}
				break;

			default:
				// Unknown subsection, skip it.
				r.seek(subsection_end);
				break;
		}
		ERROR_UNLESS(r.tell() == subsection_end, "unfinished sub-section");
	}
	return true;
}

bool BinaryReader::ReadExceptionType(TypeVector& sig) {
	Index num_values;
	CHECK_RESULT(ReadCount(&num_values));
	sig.resize(num_values);
	for (Index j = 0; j < num_values; ++j) {
		Type value_type;
		CHECK_RESULT(ReadType(&value_type));
		ERROR_UNLESS(IsConcreteType(value_type), "excepted valid exception value type");
		sig[j] = value_type;
	}
	return true;
}

bool BinaryReader::ReadExceptionSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_exceptions));

	for (Index i = 0; i < num_exceptions; ++i) {
		TypeVector sig;
		CHECK_RESULT(ReadExceptionType(sig));

		auto field			= new ExceptionModuleField(GetLocation());
		field->except.sig	= sig;
		module.AppendField(field);
	}

	return true;
}

bool BinaryReader::ReadCustomSection(Offset section_size) {
	string section_name;
	CHECK_RESULT(ReadStr(&section_name));

	if (section_name == "name") {
		CHECK_RESULT(ReadNameSection(section_size));
		did_read_names_section = true;
	} else if (section_name.rfind("reloc") == 0) {
		// Reloc sections always begin with "reloc."
		CHECK_RESULT(ReadRelocSection(section_size));
	} else if (section_name == "linking") {
		CHECK_RESULT(ReadLinkingSection(section_size));
	} else if (features.IsEnabled(FeatureSet::exceptions) && section_name == "exception") {
		CHECK_RESULT(ReadExceptionSection(section_size));
	} else {
		// This is an unknown custom section, skip it.
		r.seek(read_end);
	}
	return true;
}

bool BinaryReader::ReadTypeSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_signatures));

	module.func_types.reserve(num_signatures);

	for (Index i = 0; i < num_signatures; ++i) {
		Type form;
		CHECK_RESULT(ReadType(&form));
		ERROR_UNLESS(form == Type::Func, "unexpected type form");

		Index num_params;
		CHECK_RESULT(ReadCount(&num_params));

		param_types.resize(num_params);

		for (Index j = 0; j < num_params; ++j) {
			Type param_type;
			CHECK_RESULT(ReadType(&param_type));
			ERROR_UNLESS(IsConcreteType(param_type), "expected valid param type");
			param_types[j] = param_type;
		}

		Index num_results;
		CHECK_RESULT(ReadCount(&num_results));
		ERROR_UNLESS(num_results <= 1 || features.IsEnabled(FeatureSet::multi_value), "result count must be 0 or 1");

		result_types.resize(num_results);

		for (Index j = 0; j < num_results; ++j) {
			Type result_type;
			CHECK_RESULT(ReadType(&result_type));
			ERROR_UNLESS(IsConcreteType(result_type), "expected valid result type");
			result_types[j] = result_type;
		}

		auto field			= new FuncTypeModuleField(GetLocation());
		field->func_type.sig.param_types	= param_types;
		field->func_type.sig.result_types	= result_types;
		module.AppendField(field);
	}
	return true;
}

bool BinaryReader::ReadImportSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_imports));
	module.imports.reserve(num_imports);

	for (Index i = 0; i < num_imports; ++i) {
		string module_name;
		CHECK_RESULT(ReadStr(&module_name));
		string field_name;
		CHECK_RESULT(ReadStr(&field_name));

		uint8 kind;
		CHECK_RESULT(r.read(kind));
		switch (static_cast<ExternalKind>(kind)) {
			case ExternalKind::Func: {
				Index sig_index;
				CHECK_RESULT(ReadIndex(&sig_index));
				ERROR_UNLESS(sig_index < num_signatures, "invalid import signature index");

				auto import					= new FuncImport();
				import->module_name			= move(module_name);
				import->field_name			= move(field_name);
				import->func.decl.has_func_type	= true;
				import->func.decl.type_var	= Var(sig_index, GetLocation());
				import->func.decl.sig		= module.func_types[sig_index]->sig;
				module.AppendField(new ImportModuleField(import, GetLocation()));

				num_func_imports++;
				break;
			}

			case ExternalKind::Table: {
				Type elem_type;
				Limits elem_limits;
				CHECK_RESULT(ReadTable(&elem_type, &elem_limits));

				auto import					= new TableImport();
				import->module_name			= move(module_name);
				import->field_name			= move(field_name);
				import->table.elem_limits	= elem_limits;
				module.AppendField(new ImportModuleField(import, GetLocation()));

				num_table_imports++;
				break;
			}

			case ExternalKind::Memory: {
				Limits page_limits;
				CHECK_RESULT(ReadMemory(&page_limits));

				auto import					= new MemoryImport();
				import->module_name			= move(module_name);
				import->field_name			= move(field_name);
				import->memory.page_limits	= page_limits;
				module.AppendField(new ImportModuleField(import, GetLocation()));

				num_memory_imports++;
				break;
			}

			case ExternalKind::Global: {
				Type type;
				bool mut;
				CHECK_RESULT(ReadGlobalHeader(&type, &mut));

				auto import					= new GlobalImport();
				import->module_name			= move(module_name);
				import->field_name			= move(field_name);
				import->global.type			= type;
				import->global.mut			= mut;
				module.AppendField(new ImportModuleField(import, GetLocation()));

				num_global_imports++;
				break;
			}

			case ExternalKind::Except: {
				ERROR_UNLESS(features.IsEnabled(FeatureSet::exceptions), "invalid import exception kind: exceptions not allowed");
				TypeVector sig;
				CHECK_RESULT(ReadExceptionType(sig));

				auto import					= new ExceptionImport();
				import->module_name			= move(module_name);
				import->field_name			= move(field_name);
				import->except.sig			= sig;
				module.AppendField(new ImportModuleField(import, GetLocation()));

				num_exception_imports++;
				break;
			}
		}
	}
	return true;
}

bool BinaryReader::ReadFunctionSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_function_signatures));
	module.funcs.reserve(module.num_func_imports + num_function_signatures);

	for (Index i = 0; i < num_function_signatures; ++i) {
		Index sig_index;
		CHECK_RESULT(ReadIndex(&sig_index));
		ERROR_UNLESS(sig_index < num_signatures, "invalid function signature index");

		auto field					= new FuncModuleField(GetLocation());
		field->func.decl.has_func_type	= true;
		field->func.decl.type_var		= Var(sig_index, GetLocation());
		field->func.decl.sig			= module.func_types[sig_index]->sig;
		module.AppendField(field);
	}
	return true;
}

bool BinaryReader::ReadTableSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_tables));
	ERROR_UNLESS(num_tables <= 1, "table count must be 0 or 1");
	module.tables.reserve(module.num_table_imports + num_tables);

	for (Index i = 0; i < num_tables; ++i) {
		Type elem_type;
		Limits elem_limits;
		CHECK_RESULT(ReadTable(&elem_type, &elem_limits));

		auto field					= new TableModuleField(GetLocation());
		field->table.elem_limits	= elem_limits;
		module.AppendField(field);
	}
	return true;
}

bool BinaryReader::ReadMemorySection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_memories));
	ERROR_UNLESS(num_memories <= 1, "memory count must be 0 or 1");
	module.memories.reserve(module.num_memory_imports + num_memories);
	for (Index i = 0; i < num_memories; ++i) {
		Limits page_limits;
		CHECK_RESULT(ReadMemory(&page_limits));

		auto field					= new MemoryModuleField(GetLocation());
		field->memory.page_limits	= page_limits;
		module.AppendField(field);

	}
	return true;
}

bool BinaryReader::ReadGlobalSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_globals));
	module.globals.reserve(module.num_global_imports + num_globals);
	for (Index i = 0; i < num_globals; ++i) {
		Index	global_index = num_global_imports + i;
		Type	global_type;
		bool	mut;
		CHECK_RESULT(ReadGlobalHeader(&global_type, &mut));

		auto field				= new GlobalModuleField(GetLocation());
		field->global.type		= global_type;
		field->global.mut		= mut;
		module.AppendField(field);

		current_init_expr		= &field->global.init_expr;
		CHECK_RESULT(ReadInitExpr(global_index));
		current_init_expr		= nullptr;
	}
	return true;
}

bool BinaryReader::ReadExportSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_exports));
	module.exports.reserve(num_exports);
	for (Index i = 0; i < num_exports; ++i) {
		string name;
		CHECK_RESULT(ReadStr(&name));

		uint8 kind = 0;
		CHECK_RESULT(r.read(kind));
		ERROR_UNLESS(kind < (int)ExternalKind::size, "invalid export external kind");

		Index item_index;
		CHECK_RESULT(ReadIndex(&item_index));
		switch (static_cast<ExternalKind>(kind)) {
			case ExternalKind::Func:
				ERROR_UNLESS(item_index < NumTotalFuncs(), "invalid export func index");
				break;
			case ExternalKind::Table:
				ERROR_UNLESS(item_index < NumTotalTables(), "invalid export table index");
				break;
			case ExternalKind::Memory:
				ERROR_UNLESS(item_index < NumTotalMemories(), "invalid export memory index");
				break;
			case ExternalKind::Global:
				ERROR_UNLESS(item_index < NumTotalGlobals(), "invalid export global index");
				break;
			case ExternalKind::Except:
				// Can't check if index valid, exceptions section comes later
				ERROR_UNLESS(features.IsEnabled(FeatureSet::exceptions), "invalid export exception kind: exceptions not allowed");
				break;
		}

		auto field		= new ExportModuleField(GetLocation());
		Export& exp		= field->exp;
		exp.name		= name;
		switch (static_cast<ExternalKind>(kind)) {
			case ExternalKind::Func:	ISO_ASSERT(item_index < module.funcs.size());	break;
			case ExternalKind::Table:	ISO_ASSERT(item_index < module.tables.size());	break;
			case ExternalKind::Memory:	ISO_ASSERT(item_index < module.memories.size());break;
			case ExternalKind::Global:	ISO_ASSERT(item_index < module.globals.size());	break;
			case ExternalKind::Except:	/* Can't check if index valid, exceptions section comes later*/	break;
		}
		exp.var			= Var(item_index, GetLocation());
		exp.kind		= static_cast<ExternalKind>(kind);
		module.AppendField(field);
	}
	return true;
}

bool BinaryReader::ReadStartSection(Offset section_size) {
	Index func_index;
	CHECK_RESULT(ReadIndex(&func_index));
	ERROR_UNLESS(func_index < NumTotalFuncs(), "invalid start function index");
	ISO_ASSERT(func_index < module.funcs.size());
	Var start(func_index, GetLocation());
	module.AppendField(new StartModuleField(start, GetLocation()));
	return true;
}

bool BinaryReader::ReadElemSection(Offset section_size) {
	Index num_elem_segments;
	CHECK_RESULT(ReadCount(&num_elem_segments));
	module.elem_segments.reserve(num_elem_segments);
	ERROR_UNLESS(num_elem_segments == 0 || NumTotalTables() > 0, "elem section without table section");
	for (Index i = 0; i < num_elem_segments; ++i) {
		Index table_index;
		CHECK_RESULT(ReadIndex(&table_index));

		auto field					= new ElemSegmentModuleField(GetLocation());
		ElemSegment& elem_segment	= field->elem_segment;
		elem_segment.table_var		= Var(table_index, GetLocation());
		module.AppendField(field);

		current_init_expr	= &elem_segment.offset;

		CHECK_RESULT(ReadI32InitExpr(i));
		current_init_expr	= nullptr;

		Index num_function_indexes;
		CHECK_RESULT(ReadCount(&num_function_indexes));

		elem_segment.vars.reserve(num_function_indexes);
		for (Index j = 0; j < num_function_indexes; ++j) {
			Index func_index;
			CHECK_RESULT(ReadIndex(&func_index));
			elem_segment.vars.emplace_back(func_index, GetLocation());
		}
	}
	return true;
}

bool BinaryReader::ReadCodeSection(Offset section_size) {
	CHECK_RESULT(ReadCount(&num_function_bodies));
	ERROR_UNLESS(num_function_signatures == num_function_bodies, "function signature count != function body count");
	ISO_ASSERT(module.num_func_imports + num_function_bodies == module.funcs.size());

	for (Index i = 0; i < num_function_bodies; ++i) {
		Index func_index = num_func_imports + i;
		Offset func_offset = r.tell();
		r.seek(func_offset);

		current_func = module.funcs[func_index];
		PushLabel(LabelType::Func, &current_func->exprs);

		uint32 body_size;
		CHECK_RESULT(read_leb128(r, body_size));
		Offset body_start_offset = r.tell();
		Offset end_offset = body_start_offset + body_size;

		Index num_local_decls;
		CHECK_RESULT(ReadCount(&num_local_decls));
		for (Index k = 0; k < num_local_decls; ++k) {
			Index num_local_types;
			CHECK_RESULT(ReadIndex(&num_local_types));
			ERROR_UNLESS(num_local_types > 0, "local count must be > 0");
			Type local_type;
			CHECK_RESULT(ReadType(&local_type));
			ERROR_UNLESS(IsConcreteType(local_type), "expected valid local type");
			current_func->local_types.AppendDecl(local_type, num_local_types);
		}

		CHECK_RESULT(ReadFunctionBody(end_offset));
		CHECK_RESULT(PopLabel());
		current_func	= nullptr;
	}
	return true;
}

bool BinaryReader::ReadDataSection(Offset section_size) {
	Index num_data_segments;
	CHECK_RESULT(ReadCount(&num_data_segments));
	module.data_segments.reserve(num_data_segments);
	ERROR_UNLESS(num_data_segments == 0 || NumTotalMemories() > 0, "data section without memory section");
	for (Index i = 0; i < num_data_segments; ++i) {
		Index memory_index;
		CHECK_RESULT(ReadIndex(&memory_index));

		auto field					= new DataSegmentModuleField(GetLocation());
		DataSegment& data_segment	= field->data_segment;
		data_segment.memory_var		= Var(memory_index, GetLocation());
		module.AppendField(field);

		current_init_expr	= &data_segment.offset;
		CHECK_RESULT(ReadI32InitExpr(i));
		current_init_expr	= nullptr;

		uint32 data_size = 0;
		if (!read_leb128(r, data_size))
			return false;
		data_segment.data.read(r, data_size);
	}
	return true;
}

bool BinaryReader::ReadSections() {
	auto last_known_section	= BinarySection::Invalid;

	while (r.remaining()) {
		BinarySection	section;
		Offset			section_size;

		CHECK_RESULT(read_leb128(r, section));
		if (section >= BinarySection::size)
			return false;

		CHECK_RESULT(ReadOffset(&section_size));

		ERROR_UNLESS(read_end <= r.length(), "invalid section size: extends past end");
		ERROR_UNLESS(last_known_section == BinarySection::Invalid || section == BinarySection::Custom || section > last_known_section, "section out of order");
		ERROR_UNLESS(!did_read_names_section || section == BinarySection::Custom, "section can not occur after Name section");

		bool	stop_on_first_error	= true;
		bool	section_result		= false;
		auto	guard				= save(read_end, r.tell() + section_size);

		switch (section) {
			case BinarySection::Custom:		section_result = ReadCustomSection(section_size);	break;
			case BinarySection::Type:		section_result = ReadTypeSection(section_size);		break;
			case BinarySection::Import:		section_result = ReadImportSection(section_size);	break;
			case BinarySection::Function:	section_result = ReadFunctionSection(section_size);	break;
			case BinarySection::Table:		section_result = ReadTableSection(section_size);	break;
			case BinarySection::Memory:		section_result = ReadMemorySection(section_size);	break;
			case BinarySection::Global:		section_result = ReadGlobalSection(section_size);	break;
			case BinarySection::Export:		section_result = ReadExportSection(section_size);	break;
			case BinarySection::Start:		section_result = ReadStartSection(section_size);	break;
			case BinarySection::Elem:		section_result = ReadElemSection(section_size);		break;
			case BinarySection::Code:		section_result = ReadCodeSection(section_size);		break;
			case BinarySection::Data:		section_result = ReadDataSection(section_size);		break;
			case BinarySection::Invalid:	unreachable();
		}

		if (!section_result) {
			if (stop_on_first_error)
				return false;

			r.seek(read_end);
		}

		ERROR_UNLESS(r.tell() == read_end, "unfinished section");

		if (section != BinarySection::Custom)
			last_known_section = section;
	}

	return true;
}

bool Module::read(istream_ref file, const char* filename) {
	return BinaryReader(*this, file, filename).ReadModule();
}

//-----------------------------------------------------------------------------
//	FileHandlers
//-----------------------------------------------------------------------------
#include "iso/iso_files.h"

void write_c(Module& module, string_accum &c_acc, string_accum &h_acc, const char* header_name);

class WASMFileHandler : public FileHandler {
public:
	const char*		GetExt()			override { return "wasm"; }
	const char*		GetDescription()	override { return "WebAssembly file"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32>() == WABT_BINARY_MAGIC && file.get<uint32>() == WABT_BINARY_VERSION ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		using namespace wabt;
		Module			module;
		if (module.read(file)) {
			string_builder	s;
		#if 0
			string_builder	h;
			write_c(module, s, h, "out.h");
		#else
			if (auto	dis = Disassembler::Find("WebAssembly")) {
				auto state = dis->Disassemble(const_memory_block(&module), -1);
				for (int i = 0, n = state->Count(); i < n; i++) {
					state->GetLine(s, i);
					s << '\n';
				}
			}
		#endif
			return ISO_ptr<string>(id, move(s));
		}
		return ISO_NULL;
	}
} wasm;
