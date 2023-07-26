#ifndef EXPRESSION_H
#define EXPRESSION_H

//-----------------------------------------------------------------------------
//	expression evaluator
//-----------------------------------------------------------------------------

struct expression {
	enum OP {
		OP_PLU,	//	+
		OP_NEG,	//	-
		OP_COM,	//	~
		OP_NOT,	//	!
		OP_MUL,	//	*
		OP_DIV,	//	/
		OP_MOD,	//	%
		OP_ADD,	//	+
		OP_SUB,	//	-
		OP_SL,	//	<<
		OP_SR,	//	>>
		OP_LT,	//	<
		OP_LE,	//	<=
		OP_GT,	//	>
		OP_GE,	//	>=
		OP_EQ,	//	==
		OP_NE,	//	!=
		OP_AND,	//	&
		OP_XOR,	//	^
		OP_OR,	//	|
		OP_ANA,	//	&&
		OP_ORO,	//	||
		OP_QUE,	//	?
		OP_COL,	//	:
		OP_LPA,	//	(
		OP_RPA,	//	)
		OP_END,	//	End of expression marker
		OP_FAIL,
	};

	static bool is_unary(OP op)		{ return op >= OP_PLU && op < OP_MUL; }
	static bool is_binary(OP op)	{ return op >= OP_MUL && op < OP_RPA; }

	template<class value, class C>	static value	evaluate(C &ctx);
};

// C must provide: void getc(), void backup()
template<class C> struct default_get_op {
	expression::OP get_op() {
		C &ctx = *static_cast<C*>(this);
		for (;;) switch (ctx.getc()) {
			case ' ': case '\t': case '\n':
				continue;
			case '(':	return expression::OP_LPA;
			case ')':	return expression::OP_RPA;
			case '~':	return expression::OP_COM;
			case '*':	return expression::OP_MUL;
			case '/':	return expression::OP_DIV;
			case '%':	return expression::OP_MOD;
			case '+':	return expression::OP_ADD;
			case '-':	return expression::OP_SUB;
			case '^':	return expression::OP_XOR;
			case '?':	return expression::OP_QUE;
			case ':':	return expression::OP_COL;
			case '!':
				if (ctx.getc() == '=')
					return expression::OP_NE;
				ctx.backup();
				return expression::OP_NOT;
			case '=':
				if (ctx.getc() == '=')
					return expression::OP_EQ;
				ctx.backup();
				return expression::OP_END;
			case '<':
				switch (ctx.getc()) {
					case '<': return expression::OP_SL; 
					case '=': return expression::OP_LE;
					default: ctx.backup(); return expression::OP_LT;
				}
			case '>':
				switch (ctx.getc()) {
					case '>': return expression::OP_SR;
					case '=': return expression::OP_GE;
					default: ctx.backup(); return expression::OP_GT;
				}
			case '&':
				if (ctx.getc() == '&')
					return expression::OP_ANA;
				ctx.backup();
				return expression::OP_AND;
			case '|':
				if (ctx.getc() == '|')
					return expression::OP_ORO;
				ctx.backup();
				return expression::OP_OR;
			default:
				ctx.backup();
				return expression::OP_END;
		}
	}
};

// C must provide: void backup(), value get_value(bool), expression::OP get_op()
template<class value, class C> value expression::evaluate(C &ctx) {
	typedef unsigned char uint8;
	enum {
		STACK_SIZE		= 0x100,
		OP_END_PREC		= 0x13,
		OP_RPA_PREC     = 0x0b,
		OP_QUE_PREC     = 0x14,	// From right to left grouping
		OP_UNOP_PREC    = 0x6c,	// ditto
		S_ANDOR			= 2,
		S_QUEST			= 1,
	};

	//precedence, bit 0: binary, bit 1: next must be binary, bit 2: right-to-left
	static const uint8 op_prec[] = {
	// Unary op's
		0x70, 0x70, 0x70, 0x70,				// PLU, NEG, COM, NOT
	// Binary op's
		0x69, 0x69, 0x69,					// MUL, DIV, MOD,
		0x61, 0x61, 0x59, 0x59,				// ADD, SUB, SL, SR
		0x51, 0x51, 0x51, 0x51, 0x49, 0x49,	// LT, LE, GT, GE, EQ, NE
		0x41, 0x39, 0x31, 0x29, 0x21,		// AND, XOR, OR, ANA, ORO
		0x19, 0x19,							// QUE, COL
	// Parens
		0x78, 0x0b, 0x01					// LPA RPA, END
	};

	value	vals[STACK_SIZE], *vsp = vals;	// Value stack

	struct Operator	{
		uint8	op, prec, skip;
	} ops[STACK_SIZE], *opsp = ops;			// Operator stack

	bool	binop		= false;		// Set if binary op needed
	bool	skip_cur	= false;		// For short-circuit testing

	opsp->op	= OP_END;
	opsp->prec	= OP_END_PREC;
	opsp->skip	= 0;

	for (;;) {
		OP		op	= ctx.get_op();
		if (op == OP_END) {
			if (!binop) {
				*vsp++	= ctx.get_value(skip_cur);
				binop	= true;
				continue;
			}
		}

		int	prec = op_prec[op];
		if (binop != (prec & 1)) {
			if (op == OP_ADD)
				op = OP_PLU;
			else if (op == OP_SUB)
				op = OP_NEG;
			else
				iso_throw(op == OP_FAIL ? "Unterminated expression" : "Operator in incorrect context");
			prec = op_prec[op];
		}

		binop = (prec & 2) != 0;	// Binop should follow?

		while (prec <= opsp->prec) {
			skip_cur = opsp->skip != 0;
			OP	op1 = OP(opsp->op);
			switch (op1) {
				case OP_END:
					if (op == OP_RPA)
						ctx.backup();
					return vsp[-1];
				case OP_LPA:
					if (op != OP_RPA)
						iso_throw("Missing ')'");
					opsp--;
					break;
				case OP_QUE:
					break;
				case OP_COL:
					opsp--;
					if (opsp->op != OP_QUE)
						iso_throw("Misplaced ':'");
					//fallthrough
				default: {
					opsp--;
					value	v2;
					if (is_binary(op1))
						v2 = *--vsp;
					value	&v1 = vsp[-1];

					if (op1 == OP_COL || !skip_cur) {
						switch (op1) {
							case OP_PLU:							break;
							case OP_NEG:	v1 = -v1;				break;
							case OP_COM:	v1 = ~v1;				break;
							case OP_NOT:	v1 = value(!v1);		break;
							case OP_MUL:	v1 = v1 * v2;			break;
							case OP_DIV:	v1 = v1 / v2;			break;
							case OP_MOD:	v1 = v1 % v2;			break;
							case OP_ADD:	v1 = v1 + v2;			break;
							case OP_SUB:	v1 = v1 - v2;			break;
							case OP_SL:		v1 = v1 << v2;			break;
							case OP_SR:		v1 = v1 >> v2;			break;
							case OP_LT:		v1 = value(v1 < v2);	break;
							case OP_LE:		v1 = value(v1 <= v2);	break;
							case OP_GT:		v1 = value(v1 > v2);	break;
							case OP_GE:		v1 = value(v1 >= v2);	break;
							case OP_EQ:		v1 = value(v1 == v2);	break;
							case OP_NE:		v1 = value(v1 != v2);	break;
							case OP_AND:	v1 = v1 & v2;			break;
							case OP_XOR:	v1 = v1 ^ v2;			break;
							case OP_OR:		v1 = v1 | v2;			break;
							case OP_ANA:	v1 = value(v1 && v2);	break;
							case OP_ORO:	v1 = value(v1 || v2);	break;
							default: break;
						}
					}
				}
			}
			if (op1 == OP_END || op1 == OP_LPA || op1 == OP_QUE)
				break;
		}

		if (op == OP_COL && opsp == ops) {
			ctx.backup();
			return vsp[-1];
		}

		if (op == OP_RPA)
			continue;

		int	skip	= opsp->skip;
		opsp++;
		opsp->op	= op;
		opsp->prec	= op == OP_LPA ? OP_RPA_PREC : op == OP_QUE ? OP_QUE_PREC : is_unary(op) ? OP_UNOP_PREC : prec;

		// Short-circuit tester
		if (op == OP_ANA || op == OP_ORO || op == OP_QUE)
			skip_cur = !!vsp[-1];

		if ((op == OP_ANA && !skip_cur) || (op == OP_ORO && skip_cur)) {
			skip = S_ANDOR;
			vsp[-1] = value(skip_cur);

		} else if (op == OP_QUE) {
			skip = (skip & S_ANDOR) | (skip_cur ? 0 : S_QUEST);

		} else if (op == OP_COL) {
			skip ^= S_QUEST;
		}

		opsp->skip = skip;
		skip_cur = skip != 0;
	}
}

#endif // EXPRESSION_H
