
#line 167 "isocmd\\separate.template.cpp"
case 1: 
#line 274 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.dummy = GlobalInitStatements(cg->current_scope, yystack.getl(0).sc_stmt); }
#line 274 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{YYVALID;}; break;
case 2: 
#line 276 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.dummy = NULL; }
#line 276 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{YYVALID;}; break;
case 3: 
#line 278 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ DefineTechnique(cg, yystack.getl(-3).sc_ident, yystack.getl(-1).sc_expr, NULL); }
#line 278 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{YYVALID;}; break;
case 4: 
#line 280 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ DefineTechnique(cg, yystack.getl(-4).sc_ident, yystack.getl(-1).sc_expr, yystack.getl(-3).sc_stmt); }
#line 280 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{YYVALID;}; break;
case 8: 
#line 290 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NULL; }; break;
case 9: 
#line 292 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(-1).sc_stmt; }; break;
case 10: 
#line 294 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ RecordErrorPos(cg->tokenLoc); yyval.sc_stmt = NULL; }; break;
case 11: 
#line 298 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = yystack.getl(0).sc_decl; }; break;
case 14: 
#line 304 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_TYPEDEF); }; break;
case 15: 
#line 309 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_type = yystack.getl(0).sc_type; }; break;
case 16: 
#line 311 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeQualifiers(yystack.getl(-1).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 17: 
#line 313 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetStorageClass(yystack.getl(-1).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 18: 
#line 315 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeDomain(yystack.getl(-1).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 19: 
#line 317 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeQualifiers(yystack.getl(-1).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 20: 
#line 319 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(yystack.getl(-1).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 21: 
#line 321 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); yyval.sc_type = cg->type_specs; }; break;
case 22: 
#line 323 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_ROWMAJOR); yyval.sc_type = cg->type_specs; }; break;
case 23: 
#line 325 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->ClearTypeMisc(TYPE_MISC_ROWMAJOR); yyval.sc_type = cg->type_specs; }; break;
case 24: 
#line 327 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_PRECISION*1); yyval.sc_type = cg->type_specs; }; break;
case 25: 
#line 329 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_PRECISION*2); yyval.sc_type = cg->type_specs; }; break;
case 26: 
#line 331 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_PRECISION*3); yyval.sc_type = cg->type_specs; }; break;
case 27: 
#line 336 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_type = *SetDType(&cg->type_specs, yystack.getl(0).sc_ptype); }; break;
case 28: 
#line 338 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeQualifiers(yystack.getl(0).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 29: 
#line 340 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetStorageClass(yystack.getl(0).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 30: 
#line 342 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeDomain(yystack.getl(0).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 31: 
#line 344 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeQualifiers(yystack.getl(0).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 32: 
#line 346 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(yystack.getl(0).sc_int); yyval.sc_type = cg->type_specs; }; break;
case 33: 
#line 348 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->SetTypeMisc(TYPE_MISC_PACKED | TYPE_MISC_PACKED_KW); yyval.sc_type = cg->type_specs; }; break;
case 34: 
#line 352 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(0).sc_stmt; }; break;
case 35: 
#line 354 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = AddStmt(yystack.getl(-2).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 36: 
#line 358 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = Init_Declarator(cg, yystack.getl(0).sc_decl, NULL); }; break;
case 37: 
#line 360 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = Init_Declarator(cg, yystack.getl(-2).sc_decl, yystack.getl(0).sc_expr); }; break;
case 39: 
#line 367 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NULL; }; break;
case 40: 
#line 369 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yystack.getl(-5).sc_expr = InitializerList(yystack.getl(-5).sc_expr, StateInitializer(cg, yystack.getl(-3).sc_ident, yystack.getl(-1).sc_expr)); }; break;
case 43: 
#line 386 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(0).sc_expr, NULL); }; break;
case 44: 
#line 388 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 45: 
#line 390 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(-1).sc_expr; }; break;
case 46: 
#line 394 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = StateInitializer(cg, yystack.getl(-2).sc_ident, yystack.getl(0).sc_expr); }; break;
case 48: 
#line 399 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = SymbolicConstant(cg, yystack.getl(-2).sc_ident, yystack.getl(-3).sc_ident); }; break;
case 49: 
#line 401 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = SymbolicConstant(cg, 0, 0); }; break;
case 50: 
#line 404 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = ParseAsm(cg, cg->tokenLoc); }; break;
case 51: 
#line 413 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, INT_SY); }; break;
case 52: 
#line 415 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, INT_SY); }; break;
case 53: 
#line 417 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, FLOAT_SY); }; break;
case 54: 
#line 419 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, VOID_SY); }; break;
case 55: 
#line 421 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, BOOLEAN_SY); }; break;
case 56: 
#line 423 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, TEXOBJ_SY); }; break;
case 57: 
#line 425 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = yystack.getl(0).sc_ptype; }; break;
case 58: 
#line 427 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = yystack.getl(0).sc_ptype; }; break;
case 59: 
#line 429 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = LookUpTypeSymbol(cg, yystack.getl(0).sc_ident); }; break;
case 60: 
#line 431 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = yystack.getl(0).sc_ptype; }; break;
case 61: 
#line 433 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								SemanticParseError(cg, cg->tokenLoc, ERROR_S_TYPE_NAME_EXPECTED, cg->GetAtomString(cg->last_token /* yychar */));
								yyval.sc_ptype = UndefinedType;
							}; break;
case 62: 
#line 444 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_QUALIFIER_CONST; }; break;
case 63: 
#line 452 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_DOMAIN_UNIFORM; }; break;
case 64: 
#line 460 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_STATIC; }; break;
case 65: 
#line 462 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_EXTERN; }; break;
case 66: 
#line 464 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_NOINTERP; }; break;
case 67: 
#line 466 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_PRECISE; }; break;
case 68: 
#line 468 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_SHARED; }; break;
case 69: 
#line 470 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_GROUPSHARED; }; break;
case 70: 
#line 472 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = SC_UNKNOWN; }; break;
case 71: 
#line 480 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_MISC_INLINE; }; break;
case 72: 
#line 482 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_MISC_INTERNAL; }; break;
case 73: 
#line 490 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_QUALIFIER_IN; }; break;
case 74: 
#line 492 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_QUALIFIER_OUT; }; break;
case 75: 
#line 494 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = TYPE_QUALIFIER_INOUT; }; break;
case 76: 
#line 503 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = SetStructMembers(cg, yystack.getl(-3).sc_ptype, cg->PopScope()); }; break;
case 77: 
#line 505 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = SetStructMembers(cg, yystack.getl(-3).sc_ptype, cg->PopScope()); }; break;
case 78: 
#line 507 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = yystack.getl(0).sc_ptype; }; break;
case 79: 
#line 511 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->current_scope->flags |= Scope::is_struct; yyval.dummy = yystack.getl(0).dummy; }; break;
case 80: 
#line 516 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, yystack.getl(0).sc_ident); }; break;
case 81: 
#line 518 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = StructHeader(cg, cg->tokenLoc, cg->current_scope, yystack.getl(0).sc_ident, yystack.getl(-2).sc_ident); }; break;
case 82: 
#line 520 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = AddStructBase(StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, yystack.getl(-2).sc_ident), LookUpTypeSymbol(cg, yystack.getl(0).sc_ident)); }; break;
case 85: 
#line 528 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = StructHeader(cg, cg->tokenLoc, cg->current_scope, 0, 0); }; break;
case 90: 
#line 544 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ SetConstantBuffer(cg, cg->tokenLoc, yystack.getl(-3).sc_sym, cg->PopScope()); }; break;
case 91: 
#line 548 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->current_scope->flags |= Scope::is_struct | Scope::is_cbuffer; yyval.dummy = yystack.getl(0).dummy; }; break;
case 92: 
#line 552 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_sym = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, yystack.getl(0).sc_ident, 0); }; break;
case 93: 
#line 554 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_sym = ConstantBuffer(cg, cg->tokenLoc, cg->current_scope, yystack.getl(-2).sc_ident, yystack.getl(0).sc_ident); }; break;
case 94: 
#line 562 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->PopScope(); yyval.sc_ptype = yystack.getl(-4).sc_ptype; }; break;
case 95: 
#line 561 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->PushScope(yystack.getl(-1).sc_ptype->str.members); cg->current_scope->flags |= Scope::is_struct; }; break;
case 97: 
#line 569 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->current_scope->formal--; yyval.sc_ptype = TemplateHeader(cg, cg->tokenLoc, cg->current_scope, yystack.getl(0).sc_ident, yystack.getl(-3).sc_decl); }; break;
case 98: 
#line 567 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->current_scope->formal++; }; break;
case 99: 
#line 573 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = yystack.getl(0).sc_decl; }; break;
case 100: 
#line 575 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = AddDecl(yystack.getl(-2).sc_decl, yystack.getl(0).sc_decl); }; break;
case 101: 
#line 579 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = NewDeclNode(cg->tokenLoc, yystack.getl(0).sc_ident, 0); }; break;
case 102: 
#line 581 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = yystack.getl(0).sc_decl; }; break;
case 103: 
#line 589 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, yystack.getl(0).sc_ident), 0); }; break;
case 104: 
#line 591 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = InstantiateTemplate(cg, cg->tokenLoc, cg->current_scope, LookUpTypeSymbol(cg, yystack.getl(-3).sc_ident), yystack.getl(-1).sc_typelist); }; break;
case 105: 
#line 595 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_typelist = NULL; }; break;
case 107: 
#line 601 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_typelist = AddtoTypeList(cg, NULL, yystack.getl(0).sc_ptype); }; break;
case 108: 
#line 603 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_typelist = AddtoTypeList(cg, yystack.getl(-2).sc_typelist, yystack.getl(0).sc_ptype); }; break;
case 110: 
#line 608 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = IntToType(cg, yystack.getl(0).sc_int); }; break;
case 112: 
#line 614 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{SetDType(&cg->type_specs, yystack.getl(-1).sc_ptype);}; break;
case 114: 
#line 615 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{SetDType(&cg->type_specs, yystack.getl(-1).sc_ptype);}; break;
case 116: 
#line 620 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = EnumHeader(cg, cg->tokenLoc, cg->current_scope, yystack.getl(0).sc_ident); }; break;
case 117: 
#line 624 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ptype = EnumHeader(cg, cg->tokenLoc, cg->current_scope, 0); }; break;
case 120: 
#line 632 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, yystack.getl(0).sc_ident, 0); }; break;
case 121: 
#line 634 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ EnumAdd(cg, cg->tokenLoc, cg->current_scope, cg->type_specs.basetype, yystack.getl(-2).sc_ident, yystack.getl(0).sc_int); }; break;
case 122: 
#line 642 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(-1).sc_stmt; cg->PopScope(); }; break;
case 123: 
#line 641 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->PushScope(); }; break;
case 124: 
#line 646 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = 0; }; break;
case 126: 
#line 655 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_attr = Attribute(yystack.getl(-1).sc_ident, 0, 0, 0); }; break;
case 127: 
#line 657 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_attr = Attribute(yystack.getl(-4).sc_ident, 1, yystack.getl(-2).sc_int, 0); }; break;
case 128: 
#line 659 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_attr = Attribute(yystack.getl(-6).sc_ident, 2, yystack.getl(-4).sc_int, yystack.getl(-2).sc_int); }; break;
case 129: 
#line 661 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_attr = Attribute(yystack.getl(-8).sc_ident, 2, yystack.getl(-6).sc_int, yystack.getl(-4).sc_int); }; break;
case 132: 
#line 673 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Declarator(cg, yystack.getl(0).sc_decl, 0, 0); }; break;
case 133: 
#line 675 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Declarator(cg, yystack.getl(-2).sc_decl, yystack.getl(0).sc_ident, 0); }; break;
case 134: 
#line 677 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Declarator(cg, yystack.getl(-2).sc_decl, 0, yystack.getl(0).sc_ident); }; break;
case 135: 
#line 679 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Declarator(cg, yystack.getl(-4).sc_decl, yystack.getl(-2).sc_ident, yystack.getl(0).sc_ident); }; break;
case 136: 
#line 683 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = yystack.getl(-1).sc_ident; }; break;
case 137: 
#line 685 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = yystack.getl(-1).sc_ident; }; break;
case 138: 
#line 689 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = NewDeclNode(cg->tokenLoc, yystack.getl(0).sc_ident, &cg->type_specs); }; break;
case 139: 
#line 691 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Array_Declarator(cg, yystack.getl(-3).sc_decl, yystack.getl(-1).sc_int, 0); }; break;
case 140: 
#line 693 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Array_Declarator(cg, yystack.getl(-2).sc_decl, 0, 1); }; break;
case 141: 
#line 695 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = SetFunTypeParams(cg, yystack.getl(-2).sc_decl, yystack.getl(-1).sc_decl, yystack.getl(-1).sc_decl); }; break;
case 142: 
#line 697 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = SetFunTypeParams(cg, yystack.getl(-2).sc_decl, yystack.getl(-1).sc_decl, NULL); }; break;
case 143: 
#line 701 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = FunctionDeclHeader(cg, yystack.getl(-1).sc_decl->loc, cg->current_scope, yystack.getl(-1).sc_decl); }; break;
case 144: 
#line 703 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = FunctionDeclHeader(cg, cg->tokenLoc, cg->current_scope, NewDeclNode(cg->tokenLoc, GetOperatorName(cg, yystack.getl(-1).sc_ident), &cg->type_specs)); }; break;
case 145: 
#line 706 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = POS_OP;		}; break;
case 146: 
#line 707 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = NEG_OP;		}; break;
case 147: 
#line 708 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = BNOT_OP;		}; break;
case 148: 
#line 709 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = NOT_OP;		}; break;
case 149: 
#line 710 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = MUL_OP;		}; break;
case 150: 
#line 711 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = DIV_OP;		}; break;
case 151: 
#line 712 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = MOD_OP;		}; break;
case 152: 
#line 713 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = SHR_OP;		}; break;
case 153: 
#line 714 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = LT_OP;		}; break;
case 154: 
#line 715 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = GT_OP;		}; break;
case 155: 
#line 716 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = LE_OP;		}; break;
case 156: 
#line 717 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = GE_OP;		}; break;
case 157: 
#line 718 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = EQ_OP;		}; break;
case 158: 
#line 719 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = NE_OP;		}; break;
case 159: 
#line 720 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = AND_OP;		}; break;
case 160: 
#line 721 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = XOR_OP;		}; break;
case 161: 
#line 722 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = OR_OP;		}; break;
case 162: 
#line 723 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = BAND_OP;		}; break;
case 163: 
#line 724 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = BOR_OP;		}; break;
case 164: 
#line 725 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = FUN_CALL_OP;		}; break;
case 165: 
#line 726 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_ident = ARRAY_INDEX_OP;	}; break;
case 166: 
#line 730 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = NewDeclNode(cg->tokenLoc, 0, &cg->type_specs); }; break;
case 167: 
#line 732 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Array_Declarator(cg, yystack.getl(-3).sc_decl, yystack.getl(-1).sc_int, 0); }; break;
case 168: 
#line 734 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Array_Declarator(cg, yystack.getl(-2).sc_decl, 0 , 1); }; break;
case 169: 
#line 751 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = yystack.getl(0).sc_decl; }; break;
case 170: 
#line 753 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = AddDecl(yystack.getl(-2).sc_decl, yystack.getl(0).sc_decl); }; break;
case 171: 
#line 757 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = AddDeclAttribute(yystack.getl(0).sc_decl, yystack.getl(-1).sc_attr); }; break;
case 172: 
#line 759 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Param_Init_Declarator(cg, yystack.getl(0).sc_decl, NULL); }; break;
case 173: 
#line 761 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Param_Init_Declarator(cg, yystack.getl(-2).sc_decl, yystack.getl(0).sc_expr); }; break;
case 174: 
#line 765 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = NULL; }; break;
case 176: 
#line 771 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								if (IsVoid(&yystack.getl(0).sc_decl->type.type))
								cg->current_scope->flags |= Scope::has_void_param;
								yyval.sc_decl = yystack.getl(0).sc_decl;
							}; break;
case 177: 
#line 777 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								if ((cg->current_scope->flags & Scope::has_void_param) || IsVoid(&yystack.getl(-2).sc_decl->type.type))
								SemanticError(cg, cg->tokenLoc, ERROR___VOID_NOT_ONLY_PARAM);
								yyval.sc_decl = AddDecl(yystack.getl(-2).sc_decl, yystack.getl(0).sc_decl);
							}; break;
case 178: 
#line 789 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = Initializer(cg, yystack.getl(0).sc_expr); }; break;
case 179: 
#line 791 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = Initializer(cg, yystack.getl(-1).sc_expr); }; break;
case 180: 
#line 793 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = Initializer(cg, yystack.getl(-2).sc_expr); }; break;
case 181: 
#line 795 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(-1).sc_expr;}; break;
case 182: 
#line 799 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(0).sc_expr, NULL); }; break;
case 183: 
#line 801 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 184: 
#line 805 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(-1).sc_expr, NULL); }; break;
case 185: 
#line 807 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = InitializerList(yystack.getl(-2).sc_expr, yystack.getl(-1).sc_expr); }; break;
case 186: 
#line 811 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = StateInitializer(cg, yystack.getl(-2).sc_ident, yystack.getl(0).sc_expr); }; break;
case 187: 
#line 813 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = StateInitializer(cg, yystack.getl(-2).sc_ident, yystack.getl(0).sc_expr); }; break;
case 188: 
#line 817 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = SymbolicConstant(cg, yystack.getl(0).sc_ident, 0); }; break;
case 190: 
#line 820 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(-1).sc_expr; }; break;
case 191: 
#line 831 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(0).sc_expr; }; break;
case 192: 
#line 833 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(0).sc_expr; }; break;
case 193: 
#line 837 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = BasicVariable(cg, yystack.getl(0).sc_ident); }; break;
case 196: 
#line 847 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = yystack.getl(-1).sc_expr; }; break;
case 197: 
#line 849 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewConstructor(cg, cg->tokenLoc, yystack.getl(-3).sc_ptype, yystack.getl(-1).sc_expr); }; break;
case 199: 
#line 858 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewUnopNode(cg, POSTINC_OP, yystack.getl(-1).sc_expr); }; break;
case 200: 
#line 860 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewUnopNode(cg, POSTDEC_OP, yystack.getl(-1).sc_expr); }; break;
case 201: 
#line 862 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewMemberSelectorOrSwizzleOrWriteMaskOperator(cg, cg->tokenLoc, yystack.getl(-2).sc_expr, yystack.getl(0).sc_ident); }; break;
case 202: 
#line 864 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewIndexOperator(cg, cg->tokenLoc, yystack.getl(-3).sc_expr, yystack.getl(-1).sc_expr); }; break;
case 203: 
#line 866 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewFunctionCallOperator(cg, cg->tokenLoc, yystack.getl(-3).sc_expr, yystack.getl(-1).sc_expr); }; break;
case 204: 
#line 870 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NULL; }; break;
case 206: 
#line 875 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = ArgumentList(cg, NULL, yystack.getl(0).sc_expr); }; break;
case 207: 
#line 877 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = ArgumentList(cg, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 208: 
#line 881 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = ExpressionList(cg, NULL, yystack.getl(0).sc_expr); }; break;
case 209: 
#line 883 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = ExpressionList(cg, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 211: 
#line 892 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewUnopNode(cg, PREINC_OP, yystack.getl(0).sc_expr); }; break;
case 212: 
#line 894 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewUnopNode(cg, PREDEC_OP, yystack.getl(0).sc_expr); }; break;
case 213: 
#line 896 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewUnaryOperator(cg, cg->tokenLoc, POS_OP, '+', yystack.getl(0).sc_expr, 0); }; break;
case 214: 
#line 898 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewUnaryOperator(cg, cg->tokenLoc, NEG_OP, '-', yystack.getl(0).sc_expr, 0); }; break;
case 215: 
#line 900 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewUnaryOperator(cg, cg->tokenLoc, BNOT_OP, '!', yystack.getl(0).sc_expr, 0); }; break;
case 216: 
#line 902 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewUnaryOperator(cg, cg->tokenLoc, NOT_OP, '~', yystack.getl(0).sc_expr, 1); }; break;
case 218: 
#line 914 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewCastOperator(cg, cg->tokenLoc, yystack.getl(0).sc_expr, GetTypePointer(cg, yystack.getl(-2).sc_decl->loc, &yystack.getl(-2).sc_decl->type)); }; break;
case 220: 
#line 923 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, MUL_OP, '*', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 0); }; break;
case 221: 
#line 925 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, DIV_OP, '/', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 0); }; break;
case 222: 
#line 927 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, MOD_OP, '%', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 224: 
#line 936 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, ADD_OP, '+', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 0); }; break;
case 225: 
#line 938 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, SUB_OP, '-', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 0); }; break;
case 227: 
#line 947 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, SHL_OP, LL_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 228: 
#line 949 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, SHR_OP, GG_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 230: 
#line 958 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, LT_OP, '<', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 231: 
#line 960 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, GT_OP, '>', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 232: 
#line 962 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, LE_OP, LE_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 233: 
#line 964 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, GE_OP, GE_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 235: 
#line 973 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, EQ_OP, EQ_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 236: 
#line 975 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryComparisonOperator(cg, cg->tokenLoc, NE_OP, NE_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 238: 
#line 984 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, AND_OP, '&', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 240: 
#line 993 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, XOR_OP, '^', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 242: 
#line 1002 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryOperator(cg, cg->tokenLoc, OR_OP, '|', yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 1); }; break;
case 244: 
#line 1011 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryBooleanOperator(cg, cg->tokenLoc, BAND_OP, AND_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 246: 
#line 1020 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewBinaryBooleanOperator(cg, cg->tokenLoc, BOR_OP, OR_SY, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 248: 
#line 1029 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NewConditionalOperator(cg, cg->tokenLoc, yystack.getl(-4).sc_expr, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 249: 
#line 1033 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{	int len; yyval.sc_expr = CheckBooleanExpr(cg, cg->tokenLoc, yystack.getl(0).sc_expr, &len); }; break;
case 251: 
#line 1052 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ DefineFunction(cg, yystack.getl(-2).sc_decl, yystack.getl(-1).sc_stmt); cg->PopScope(); }; break;
case 252: 
#line 1054 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ DefineFunction(cg, yystack.getl(-1).sc_decl, NULL); cg->PopScope(); }; break;
case 253: 
#line 1059 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = AddDeclAttribute(yystack.getl(0).sc_decl, yystack.getl(-1).sc_attr); }; break;
case 254: 
#line 1061 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_decl = Function_Definition_Header(cg, yystack.getl(-1).sc_decl); }; break;
case 255: 
#line 1069 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = AddStmtAttribute(yystack.getl(0).sc_stmt, yystack.getl(-1).sc_attr); }; break;
case 269: 
#line 1094 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(-1).sc_stmt; }; break;
case 270: 
#line 1101 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(0, TYPE_BASE_BOOLEAN), NULL)); }; break;
case 271: 
#line 1103 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								int	len;
								expr *e = CheckBooleanExpr(cg, cg->tokenLoc, yystack.getl(-1).sc_expr, &len);
								yyval.sc_stmt = NewDiscardStmt(cg->tokenLoc, NewUnopSubNode(cg, KILL_OP, SUBOP_V(len, TYPE_BASE_BOOLEAN), e));
							}; break;
case 272: 
#line 1113 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewBreakStmt(cg->tokenLoc); }; break;
case 273: 
#line 1121 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	SetThenElseStmts(yystack.getl(-3).sc_stmt, yystack.getl(-2).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 274: 
#line 1125 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	SetThenElseStmts(yystack.getl(-1).sc_stmt, yystack.getl(0).sc_stmt, NULL); }; break;
case 275: 
#line 1127 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	SetThenElseStmts(yystack.getl(-3).sc_stmt, yystack.getl(-2).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 276: 
#line 1131 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewIfStmt(cg->tokenLoc, yystack.getl(-1).sc_expr, NULL, NULL); ; }; break;
case 277: 
#line 1139 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewSwitchStmt(cg->tokenLoc, yystack.getl(-4).sc_expr, yystack.getl(-1).sc_stmt, cg->popped_scope); }; break;
case 278: 
#line 1143 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(0).sc_stmt; }; break;
case 279: 
#line 1145 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(0).sc_stmt; }; break;
case 281: 
#line 1150 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = AddStmt(yystack.getl(-1).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 282: 
#line 1158 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewBlockStmt(cg->tokenLoc, yystack.getl(-1).sc_stmt, cg->popped_scope); }; break;
case 283: 
#line 1160 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NULL; }; break;
case 284: 
#line 1164 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ cg->PushScope(); cg->current_scope->funindex = cg->func_index; }; break;
case 285: 
#line 1168 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								if (cg->opts & cgclib::DUMP_PARSETREE)
									PrintScopeDeclarations(cg);
								cg->PopScope();
							}; break;
case 287: 
#line 1177 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = AddStmt(yystack.getl(-1).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 289: 
#line 1182 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = CheckStmt(yystack.getl(0).sc_stmt); }; break;
case 291: 
#line 1191 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NULL; }; break;
case 292: 
#line 1195 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewSimpleAssignmentStmt(cg, cg->tokenLoc, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr, 0); }; break;
case 293: 
#line 1197 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewExprStmt(cg->tokenLoc, yystack.getl(0).sc_expr); }; break;
case 294: 
#line 1199 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMINUS_OP, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 295: 
#line 1201 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNMOD_OP, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 296: 
#line 1203 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNPLUS_OP, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 297: 
#line 1205 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSLASH_OP, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 298: 
#line 1207 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NewCompoundAssignmentStmt(cg, cg->tokenLoc, ASSIGNSTAR_OP, yystack.getl(-2).sc_expr, yystack.getl(0).sc_expr); }; break;
case 299: 
#line 1215 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, yystack.getl(-2).sc_expr, yystack.getl(0).sc_stmt); }; break;
case 300: 
#line 1217 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewWhileStmt(cg->tokenLoc, DO_STMT, yystack.getl(-2).sc_expr, yystack.getl(-5).sc_stmt); }; break;
case 301: 
#line 1219 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewForStmt(cg->tokenLoc, yystack.getl(-6).sc_stmt, yystack.getl(-4).sc_expr, yystack.getl(-2).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 302: 
#line 1223 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewWhileStmt(cg->tokenLoc, WHILE_STMT, yystack.getl(-2).sc_expr, yystack.getl(0).sc_stmt); }; break;
case 303: 
#line 1225 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewForStmt(cg->tokenLoc, yystack.getl(-6).sc_stmt, yystack.getl(-4).sc_expr, yystack.getl(-2).sc_stmt, yystack.getl(0).sc_stmt); }; break;
case 304: 
#line 1230 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{	yyval.sc_expr = CheckBooleanExpr(cg, cg->tokenLoc, yystack.getl(0).sc_expr, NULL); }; break;
case 306: 
#line 1235 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = NULL; }; break;
case 308: 
#line 1240 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{
								if (stmt *lstmt = yystack.getl(-2).sc_stmt) {
									while (lstmt->next)
										lstmt = lstmt->next;
									lstmt->next = yystack.getl(0).sc_stmt;
									yyval.sc_stmt = yystack.getl(-2).sc_stmt;
								} else {
									yyval.sc_stmt = yystack.getl(0).sc_stmt;
								}
						}; break;
case 309: 
#line 1252 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt = yystack.getl(0).sc_stmt; }; break;
case 312: 
#line 1258 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr = NULL; }; break;
case 313: 
#line 1266 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, yystack.getl(-1).sc_expr); }; break;
case 314: 
#line 1268 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_stmt =	NewReturnStmt(cg, cg->tokenLoc, cg->current_scope, NULL); }; break;
case 320: 
#line 1291 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewIConstNode(cg, ICONST_OP, yystack.getl(0).sc_int, TYPE_BASE_CINT); }; break;
case 321: 
#line 1293 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewIConstNode(cg, ICONST_OP, yystack.getl(0).sc_int, TYPE_BASE_CINT); }; break;
case 322: 
#line 1295 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ int base = cg->GetFloatSuffixBase(' '); yyval.sc_expr = NewFConstNode(cg, FCONST_OP, yystack.getl(0).sc_fval, base); }; break;
case 323: 
#line 1297 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ int base = cg->GetFloatSuffixBase('f'); yyval.sc_expr = NewFConstNode(cg, FCONST_OP, yystack.getl(0).sc_fval, base); }; break;
case 324: 
#line 1299 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ int base = cg->GetFloatSuffixBase('h'); yyval.sc_expr = NewFConstNode(cg, FCONST_OP, yystack.getl(0).sc_fval, base); }; break;
case 325: 
#line 1301 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{int base = cg->GetFloatSuffixBase('x'); yyval.sc_expr = NewFConstNode(cg, FCONST_OP, yystack.getl(0).sc_fval, base); }; break;
case 326: 
#line 1303 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_expr =	NewIConstNode(cg, ICONST_OP, yystack.getl(0).sc_token, TYPE_BASE_STRING); }; break;
case 327: 
#line 1307 "D:\\dev\\orbiscrude\\shared\\cgc\\parser.y"
{ yyval.sc_int = GetConstant(cg, yystack.getl(0).sc_expr, 0); }; break;
