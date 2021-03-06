#include "ua_types_generated.h"
#include "ua_types_encoding_xml.h"
#include "ua_xml.h"
#include "ua_namespace_xml.h"
#include <fcntl.h> // open, O_RDONLY

UA_Int32 UA_TypedArray_init(UA_TypedArray *p) {
	p->size     = -1;
	p->vt       = &UA_.types[UA_INVALIDTYPE];
	p->elements = UA_NULL;
	return UA_SUCCESS;
}

UA_Int32 UA_TypedArray_new(UA_TypedArray **p) {
	UA_alloc((void ** )p, sizeof(UA_TypedArray));
	UA_TypedArray_init(*p);
	return UA_SUCCESS;
}

UA_Int32 UA_TypedArray_setType(UA_TypedArray *p, UA_Int32 type) {
	UA_Int32 retval = UA_ERR_INVALID_VALUE;
	if(type >= UA_BOOLEAN && type <= UA_INVALIDTYPE) {
		p->vt  = &UA_.types[type];
		retval = UA_SUCCESS;
	}
	return retval;
}

// FIXME: We might want to have these classes and their methods
// defined in opcua.h via generate_builtin and the plugin-concept
// or in ua_basictypes.c

UA_Int32 UA_NodeSetAlias_init(UA_NodeSetAlias *p) {
	UA_String_init(&(p->alias));
	UA_String_init(&(p->value));
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAlias_new(UA_NodeSetAlias **p) {
	UA_alloc((void ** )p, sizeof(UA_NodeSetAlias));
	UA_NodeSetAlias_init(*p);
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAliases_init(UA_NodeSetAliases *p) {
	p->size    = -1;
	p->aliases = UA_NULL;
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAliases_new(UA_NodeSetAliases **p) {
	UA_alloc((void ** )p, sizeof(UA_NodeSetAliases));
	UA_NodeSetAliases_init(*p);
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAliases_println(cstring label, UA_NodeSetAliases *p) {
	UA_Int32 i;
	for(i = 0;i < p->size;i++) {
		UA_NodeSetAlias *a = p->aliases[i];
		printf("%s{addr=%p", label, (void *)a);
		if(a)
			printf(",alias='%.*s', value='%.*s'", a->alias.length, a->alias.data, a->value.length, a->value.data);
		printf("}\n");
	}
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSet_init(UA_NodeSet *p, UA_UInt32 nsid) {
	Namespace_new(&p->ns, nsid);
	p->aliases.size    = -1;
	p->aliases.aliases = UA_NULL;
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSet_new(UA_NodeSet **p, UA_UInt32 nsid) {
	UA_alloc((void ** )p, sizeof(UA_NodeSet));
	UA_NodeSet_init(*p, nsid);
	return UA_SUCCESS;
}

UA_Int32 _UA_NodeId_copycstring(cstring src, UA_NodeId* dst, UA_NodeSetAliases* aliases) {
	UA_Int32 retval = UA_SUCCESS;
	if (src != UA_NULL && dst != UA_NULL ) {
		if (src[0] == 'i' && src[1] == '=') { // namespace zero numeric identifier
			dst->identifier.numeric = atoi(&src[2]);
		} else if (src[0] == 'n' && src[1] == 's' && src[2] == '=') { // namespace
			dst->namespaceIndex = atoi(&src[3]);
			src = strchr(&src[3],';');
			if (src != UA_NULL)
				retval = _UA_NodeId_copycstring(src+1,dst,aliases);  // +1 to start beyond ;
			else
				retval = UA_ERR_INVALID_VALUE;
		} else if (aliases != UA_NULL ) { // try for aliases
			UA_Int32 i;
			for (i = 0; i < aliases->size && dst->identifier.numeric == 0; ++i) {
				if (0 == strncmp((char const*) src, (char const*) aliases->aliases[i]->alias.data,
								aliases->aliases[i]->alias.length)) {
					_UA_NodeId_copycstring((cstring)aliases->aliases[i]->alias.data,dst,UA_NULL); // substitute text of alias
				}
			}
		} else {
			retval = UA_ERR_NOT_IMPLEMENTED;
		}
	} else {
		retval = UA_ERR_INVALID_VALUE;
	}
	DBG_VERBOSE(printf("UA_NodeId_copycstring src=%s,id=%d\n", src, dst->identifier.numeric));
	return retval;
}

UA_Int32 UA_NodeId_copycstring(cstring src, UA_NodeId* dst, UA_NodeSetAliases* aliases) {
	dst->identifierType = UA_NODEIDTYPE_NUMERIC;
	dst->namespaceIndex = 0;
	dst->identifier.numeric = 0;
	return _UA_NodeId_copycstring(src,dst,aliases);
}

UA_Int32 UA_ReferenceNode_println(cstring label, UA_ReferenceNode *a) {
	printf("%s{referenceType=%d, target=%d, isInverse=%d}\n",
	       label,
	       a->referenceTypeId.identifier.numeric,
	       a->targetId.nodeId.identifier.numeric,
	       a->isInverse);
	return UA_SUCCESS;
}

UA_Int32 UA_ExpandedNodeId_copycstring(cstring src, UA_ExpandedNodeId *dst, UA_NodeSetAliases *aliases) {
	dst->nodeId.namespaceIndex     = 0;
	dst->nodeId.identifierType     = UA_NODEIDTYPE_NUMERIC;
	dst->nodeId.identifier.numeric = 0;
	UA_NodeId_copycstring(src, &(dst->nodeId), aliases);
	DBG_VERBOSE(printf("UA_ExpandedNodeId_copycstring src=%s,id=%d\n", src, dst->nodeId.identifier.numeric));
	return UA_SUCCESS;
}

void XML_Stack_init(XML_Stack* p, UA_UInt32 nsid, cstring name) {
	unsigned int i, j;
	p->depth = 0;
	for(i = 0;i < XML_STACK_MAX_DEPTH;i++) {
		p->parent[i].name = UA_NULL;
		p->parent[i].len  = 0;
		p->parent[i].activeChild   = -1;
		p->parent[i].textAttrib    = UA_NULL;
		p->parent[i].textAttribIdx = -1;
		/* p->parent[i].totalGatherLength = -1; */
		/* UA_list_init(&(p->parent[i].textGatherList)); */
		for (j = 0; j < XML_STACK_MAX_CHILDREN; j++) {
			p->parent[i].children[j].name = UA_NULL;
			p->parent[i].children[j].length         = -1;
			p->parent[i].children[j].elementHandler = UA_NULL;
			p->parent[i].children[j].type           = UA_INVALIDTYPE;
			p->parent[i].children[j].obj = UA_NULL;
		}
	}
	/* p->nsid = nsid; */
	p->parent[0].name = name;
}

char path_buffer[1024];
char *XML_Stack_path(XML_Stack *s) {
	UA_Int32 i;
	char    *p = &path_buffer[0];
	for(i = 0;i <= s->depth;i++) {
		strcpy(p, s->parent[i].name);
		p += strlen(s->parent[i].name);
		*p = '/';
		p++;
	}
	*p = 0;
	return &path_buffer[0];
}

void XML_Stack_print(XML_Stack *s) {
	printf("%s", XML_Stack_path(s));
}

// FIXME: we might want to calculate textAttribIdx from a string and the information given on the stack
void XML_Stack_handleTextAsElementOf(XML_Stack *p, cstring textAttrib, unsigned int textAttribIdx) {
	p->parent[p->depth].textAttrib    = textAttrib;
	p->parent[p->depth].textAttribIdx = textAttribIdx;
}

void XML_Stack_addChildHandler(XML_Stack *p, cstring name, UA_Int32 length, XML_decoder handler, UA_Int32 type,
                               void *dst) {
	unsigned int len = p->parent[p->depth].len;
	p->parent[p->depth].children[len].name = name;
	p->parent[p->depth].children[len].length         = length;
	p->parent[p->depth].children[len].elementHandler = handler;
	p->parent[p->depth].children[len].type           = type;
	p->parent[p->depth].children[len].obj = dst;
	p->parent[p->depth].len++;
}

UA_Int32 UA_Text_decodeXmlFromStack(XML_Stack *s, XML_Attr *attr, UA_Byte **dst, _Bool isStart) {
	DBG_VERBOSE(printf("UA_String entered with dst=%p,isStart=%d\n", (void * )dst, isStart));
	UA_UInt32 i;
	if(isStart) {
		if(dst == UA_NULL) {
			UA_alloc((void **)&dst, sizeof(void *));
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void *)dst;
		}
		// set attributes
		for(i = 0;attr[i];i += 2) {
			if(0 == strncmp("Data", attr[i], strlen("Data"))) {
				char *tmp;
				UA_alloc((void **)&tmp, strlen(attr[i+1])+1);
				strncpy(tmp, attr[i+1], strlen(attr[i+1]));
				tmp[strlen(attr[i+1])] = 0;
				*dst = (UA_Byte *)tmp;
			} else
				printf("UA_Text_decodeXml - Unknown attribute - name=%s, value=%s\n", attr[i], attr[i+1]);
		}
	}
	return UA_SUCCESS;
}

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_ReferenceNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_ReferenceNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_ReferenceNode_decodeXML entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	if (isStart) { */
/* 		// create if necessary */
/* 		if (dst == UA_NULL) { */
/* 			UA_ReferenceNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */
/* 		// set handlers */
/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "ReferenceType", strlen("ReferenceType"),(XML_decoder) UA_NodeId_decodeXmlFromStack, UA_NODEID, &(dst->referenceTypeId)); */
/* 		XML_Stack_addChildHandler(s, "IsForward", strlen("IsForward"), (XML_decoder) UA_Boolean_decodeXmlFromStack, UA_BOOLEAN, &(dst->isInverse)); */
/* 		XML_Stack_addChildHandler(s, "Target", strlen("Target"), (XML_decoder) UA_ExpandedNodeId_decodeXmlFromStack, UA_EXPANDEDNODEID, &(dst->targetId)); */
/* 		XML_Stack_handleTextAsElementOf(s, "NodeId", 2); */

/* 		// set attributes */
/* 		UA_Int32 i; */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("ReferenceType", attr[i], strlen("ReferenceType"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->referenceTypeId), s->aliases); */
/* 			} else if (0 == strncmp("IsForward", attr[i], strlen("IsForward"))) { */
/* 				UA_Boolean_copycstring(attr[i + 1], &(dst->isInverse)); */
/* 				dst->isInverse = !dst->isInverse; */
/* 			} else if (0 == strncmp("Target", attr[i], strlen("Target"))) { */
/* 				UA_ExpandedNodeId_copycstring(attr[i + 1], &(dst->targetId), s->aliases); */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s));DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

UA_Int32 UA_TypedArray_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_TypedArray* dst, _Bool isStart) {
	UA_Int32 type = s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].type;
	/* DBG_VERBOSE(printf("UA_TypedArray_decodeXML - entered with dst=%p,isStart=%d,type={%d,%s},name=%s\n", (void* ) dst, isStart,type,UA_[type].name, names)); */
	if (isStart) {
		if (dst == UA_NULL) {
			UA_TypedArray_new(&dst);
			UA_TypedArray_setType(dst, type);
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = dst;
		}
		// need to map from ArrayName to member name
		// References - Reference
		// Aliases - Alias
		// ListOfXX - XX
		DBG(UA_Int32 length = 0;
			cstring names  = s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].name;
			if (0 == strncmp("ListOf",names,strlen("ListOf"))) {
				names = &names[strlen("ListOf")];
				length = strlen(names);
			} else if ( 0 == strncmp("References",names,strlen("References"))){
				length = strlen(names) - 1;
			} else if ( 0 == strncmp("Aliases",names,strlen("Aliases"))){
				length = strlen(names) - 2;
			}
			printf("UA_TypedArray_decodeXML - add handler for {%.*s}\n", length, names));
		// this is problematic as the standard encoding functions in the vtable take a bytestring as the input
		/* XML_Stack_addChildHandler(s, names, length, (XML_decoder) UA_.types[type].encodings[1].decode, type, UA_NULL); */
	} else {
		// sub element is ready, add to array
		if (dst->size < 0 || dst->size == 0) {
			dst->size = 1;
			UA_alloc((void** )&(dst->elements), dst->size * sizeof(void*));
			DBG(printf("UA_TypedArray_decodeXML - allocate elements:dst=%p, aliases=%p, size=%d\n", (void* )dst, (void* )(dst->elements),dst->size));
		} else {
			dst->size++;
			dst->elements = realloc(dst->elements, dst->size * sizeof(void*));
			DBG(printf("UA_TypedArray_decodeXML - reallocate elements:dst=%p, aliases=%p, size=%d\n", (void* )dst,(void* )(dst->elements), dst->size));
		}
		// index starts with 0, therefore size-1
		DBG_VERBOSE(printf("UA_TypedArray_decodeXML - assign element[%d], src=%p\n", dst->size - 1, (void* )attr));
		/* dst->elements[dst->size - 1] = (void*) attr; */
		DBG_VERBOSE(printf("UA_TypedArray_decodeXML - clear %p\n",(void* ) (s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj)));
		s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_DataTypeNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_DataTypeNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_DataTypeNode entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_DataTypeNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "DisplayName", strlen("DisplayName"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->displayName)); */
/* 		XML_Stack_addChildHandler(s, "Description", strlen("Description"),(XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "BrowseName", strlen("BrowseName"),(XML_decoder) UA_QualifiedName_decodeXmlFromStack, UA_QUALIFIEDNAME, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "IsAbstract", strlen("IsAbstract"),(XML_decoder) UA_Boolean_decodeXmlFromStack, UA_BOOLEAN, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "References", strlen("References"),(XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_REFERENCENODE, UA_NULL); */

/* 		// set missing default attributes */
/* 		dst->nodeClass = UA_NODECLASS_DATATYPE; */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("NodeId", attr[i], strlen("NodeId"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->nodeId), s->aliases); */
/* 			} else if (0 == strncmp("BrowseName", attr[i], strlen("BrowseName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("DisplayName", attr[i], strlen("DisplayName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->displayName.text)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("IsAbstract", attr[i], strlen("IsAbstract"))) { */
/* 				UA_Boolean_copycstring(attr[i + 1], &(dst->isAbstract)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("Description", attr[i], strlen("Description"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->description.text)); */
/* 				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s));DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		switch (s->parent[s->depth - 1].activeChild) { */
/* 		case 4: // References */
/* 			if (attr != UA_NULL) { */
/* 				UA_TypedArray* array = (UA_TypedArray *) attr; */
/* 				DBG_VERBOSE(printf("finished aliases: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 				dst->referencesSize = array->size; */
/* 				/\* dst->references = (UA_ReferenceNode**) array->elements; *\/ */
/* 			} */
/* 		break; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_ObjectTypeNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_ObjectTypeNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_DataTypeNode entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_ObjectTypeNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "DisplayName", strlen("DisplayName"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->displayName)); */
/* 		XML_Stack_addChildHandler(s, "Description", strlen("Description"),(XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "BrowseName", strlen("BrowseName"),(XML_decoder) UA_QualifiedName_decodeXmlFromStack, UA_QUALIFIEDNAME, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "IsAbstract", strlen("IsAbstract"),(XML_decoder) UA_Boolean_decodeXmlFromStack, UA_BOOLEAN, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "References", strlen("References"),(XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_REFERENCENODE, UA_NULL); */

/* 		// set missing default attributes */
/* 		dst->nodeClass = UA_NODECLASS_OBJECTTYPE; */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("NodeId", attr[i], strlen("NodeId"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->nodeId), s->aliases); */
/* 			} else if (0 == strncmp("BrowseName", attr[i], strlen("BrowseName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("DisplayName", attr[i], strlen("DisplayName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->displayName.text)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("IsAbstract", attr[i], strlen("IsAbstract"))) { */
/* 				UA_Boolean_copycstring(attr[i + 1], &(dst->isAbstract)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("Description", attr[i], strlen("Description"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->description.text)); */
/* 				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s));DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		switch (s->parent[s->depth - 1].activeChild) { */
/* 		case 4: // References */
/* 			if (attr != UA_NULL) { */
/* 				UA_TypedArray* array = (UA_TypedArray *) attr; */
/* 				DBG_VERBOSE(printf("finished aliases: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 				dst->referencesSize = array->size; */
/* 				/\* dst->references = (UA_ReferenceNode**) array->elements; *\/ */
/* 			} */
/* 		break; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_VariableTypeNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_VariableTypeNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_DataTypeNode entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_VariableTypeNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "DisplayName", strlen("DisplayName"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->displayName)); */
/* 		XML_Stack_addChildHandler(s, "Description", strlen("Description"),(XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "BrowseName", strlen("BrowseName"),(XML_decoder) UA_QualifiedName_decodeXmlFromStack, UA_QUALIFIEDNAME, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "IsAbstract", strlen("IsAbstract"),(XML_decoder) UA_Boolean_decodeXmlFromStack, UA_BOOLEAN, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "References", strlen("References"),(XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_REFERENCENODE, UA_NULL); */

/* 		// set missing default attributes */
/* 		dst->nodeClass = UA_NODECLASS_VARIABLETYPE; */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("NodeId", attr[i], strlen("NodeId"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->nodeId), s->aliases); */
/* 			} else if (0 == strncmp("BrowseName", attr[i], strlen("BrowseName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("DisplayName", attr[i], strlen("DisplayName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->displayName.text)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("IsAbstract", attr[i], strlen("IsAbstract"))) { */
/* 				UA_Boolean_copycstring(attr[i + 1], &(dst->isAbstract)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("Description", attr[i], strlen("Description"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->description.text)); */
/* 				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s));DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		switch (s->parent[s->depth - 1].activeChild) { */
/* 		case 4: // References */
/* 			if (attr != UA_NULL) { */
/* 				UA_TypedArray* array = (UA_TypedArray *) attr; */
/* 				DBG_VERBOSE(printf("finished aliases: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 				dst->referencesSize = array->size; */
/* 				/\* dst->references = (UA_ReferenceNode**) array->elements; *\/ */
/* 			} */
/* 		break; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_ObjectNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_ObjectNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_ObjectNode entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_ObjectNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		XML_Stack_addChildHandler(s, "DisplayName", strlen("DisplayName"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->displayName)); */
/* 		XML_Stack_addChildHandler(s, "Description", strlen("Description"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, &(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "BrowseName", strlen("BrowseName"), (XML_decoder) UA_QualifiedName_decodeXmlFromStack, UA_QUALIFIEDNAME, &(dst->browseName)); */
/* 		XML_Stack_addChildHandler(s, "SymbolicName", strlen("SymbolicName"), (XML_decoder) UA_QualifiedName_decodeXmlFromStack, UA_QUALIFIEDNAME,&(dst->browseName)); */
/* 		XML_Stack_addChildHandler(s, "References", strlen("References"), (XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_REFERENCENODE, UA_NULL); */

/* 		// set missing default attributes */
/* 		dst->nodeClass = UA_NODECLASS_OBJECT; */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("NodeId", attr[i], strlen("NodeId"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->nodeId), s->aliases); */
/* 			} else if (0 == strncmp("SymbolicName", attr[i], strlen("SymbolicName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("BrowseName", attr[i], strlen("BrowseName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("DisplayName", attr[i], strlen("DisplayName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->displayName.text)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("Description", attr[i], strlen("Description"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->description.text)); */
/* 				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s));DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		if (s->parent[s->depth - 1].activeChild == 4 && attr != UA_NULL ) { // References Array */
/* 			UA_TypedArray* array = (UA_TypedArray*) attr; */
/* 			DBG(printf("UA_ObjectNode_decodeXML finished references: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 			dst->referencesSize = array->size; */
/* 			/\* dst->references = (UA_ReferenceNode**) array->elements; *\/ */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */


// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_Variant_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_Variant* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_Variant entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_Variant_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "ListOfExtensionObject", strlen("ListOfExtensionObject"), (XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_EXTENSIONOBJECT, UA_NULL); */
/* 		XML_Stack_addChildHandler(s, "ListOfLocalizedText", strlen("ListOfLocalizedText"), (XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_LOCALIZEDTEXT, UA_NULL); */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			{ */
/* 				DBG_ERR(XML_Stack_print(s)); */
/* 				DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		if (s->parent[s->depth - 1].activeChild == 0 && attr != UA_NULL ) { // ExtensionObject */
/* 			UA_TypedArray* array = (UA_TypedArray*) attr; */
/* 			DBG_VERBOSE(printf("UA_Variant_decodeXML - finished array: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 			dst->arrayLength = array->size; */
/* 			dst->data = array->elements; */
/* 			dst->vt = &UA_.types[UA_EXTENSIONOBJECT]; */
/* 			dst->encodingMask = UA_EXTENSIONOBJECT_NS0 & UA_VARIANT_ENCODINGMASKTYPE_ARRAY; */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL; */
/* 		} else if (s->parent[s->depth - 1].activeChild == 1 && attr != UA_NULL ) { // LocalizedText */
/* 			UA_TypedArray* array = (UA_TypedArray*) attr; */
/* 			DBG_VERBOSE(printf("UA_Variant_decodeXML - finished array: references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 			dst->arrayLength = array->size; */
/* 			dst->data = array->elements; */
/* 			dst->vt = &UA_.types[UA_LOCALIZEDTEXT]; */
/* 			dst->encodingMask = UA_LOCALIZEDTEXT_NS0 & UA_VARIANT_ENCODINGMASKTYPE_ARRAY; */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_ExtensionObject_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_ExtensionObject* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_ExtensionObject entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_ExtensionObject_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "TypeId", strlen("TypeId"), (XML_decoder) UA_NodeId_decodeXmlFromStack, UA_NODEID, &(dst->typeId)); */
/* 		// XML_Stack_addChildHandler(s, "Body", strlen("Body"), (XML_decoder) UA_Body_decodeXML, UA_LOCALIZEDTEXT, UA_NULL); */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			{ */
/* 				DBG_ERR(XML_Stack_print(s)); */
/* 				DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

_Bool UA_NodeId_isBuiltinType(UA_NodeId* nodeid) {
	return (nodeid->namespaceIndex == 0 && nodeid->identifier.numeric >= UA_BOOLEAN_NS0 &&
			nodeid->identifier.numeric <= UA_DIAGNOSTICINFO_NS0
			);
}

// Gets generated in ua_types_generated.h and .c. But currently only as stubs
/* UA_Int32 UA_VariableNode_decodeXmlFromStack(XML_Stack* s, XML_Attr* attr, UA_VariableNode* dst, _Bool isStart) { */
/* 	DBG_VERBOSE(printf("UA_VariableNode entered with dst=%p,isStart=%d\n", (void* ) dst, isStart)); */
/* 	UA_UInt32 i; */

/* 	if (isStart) { */
/* 		// create a new object if called with UA_NULL */
/* 		if (dst == UA_NULL) { */
/* 			UA_VariableNode_new(&dst); */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void*) dst; */
/* 		} */

/* 		s->parent[s->depth].len = 0; */
/* 		XML_Stack_addChildHandler(s, "DisplayName", strlen("DisplayName"), (XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, */
/* 				&(dst->displayName)); */
/* 		XML_Stack_addChildHandler(s, "Description", strlen("Description"),(XML_decoder) UA_LocalizedText_decodeXmlFromStack, UA_LOCALIZEDTEXT, */
/* 				&(dst->description)); */
/* 		XML_Stack_addChildHandler(s, "DataType", strlen("DataType"),(XML_decoder) UA_NodeId_decodeXmlFromStack, UA_NODEID, &(dst->dataType)); */
/* 		XML_Stack_addChildHandler(s, "ValueRank", strlen("ValueRank"),(XML_decoder) UA_Int32_decodeXmlFromStack, UA_INT32, &(dst->valueRank)); */
/* 		XML_Stack_addChildHandler(s, "Value", strlen("Value"),(XML_decoder) UA_Variant_decodeXmlFromStack, UA_VARIANT, &(dst->value)); */
/* 		XML_Stack_addChildHandler(s, "References", strlen("References"), (XML_decoder) UA_TypedArray_decodeXmlFromStack, UA_REFERENCENODE, */
/* 		UA_NULL); */

/* 		// set missing default attributes */
/* 		dst->nodeClass = UA_NODECLASS_VARIABLE; */

/* 		// set attributes */
/* 		for (i = 0; attr[i]; i += 2) { */
/* 			if (0 == strncmp("NodeId", attr[i], strlen("NodeId"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->nodeId), s->aliases); */
/* 			} else if (0 == strncmp("DataType", attr[i], strlen("DataType"))) { */
/* 				UA_NodeId_copycstring(attr[i + 1], &(dst->dataType), s->aliases); */
/* 				if (UA_NodeId_isBuiltinType(&(dst->dataType))) { */
/* 					dst->value.encodingMask = dst->dataType.identifier.numeric; */
/* 					dst->value.vt = &UA_.types[UA_ns0ToVTableIndex(&dst->dataType)]; */
/* 				} else { */
/* 					dst->value.encodingMask = UA_EXTENSIONOBJECT_NS0; */
/* 					dst->value.vt = &UA_.types[UA_EXTENSIONOBJECT]; */
/* 				} */
/* 			} else if (0 == strncmp("ValueRank", attr[i], strlen("ValueRank"))) { */
/* 				dst->valueRank = atoi(attr[i + 1]); */
/* 			} else if (0 == strncmp("ParentNodeId", attr[i], strlen("ParentNodeId"))) { */
/* 				// FIXME: this seems to be redundant to the hasProperty-reference */
/* 			} else if (0 == strncmp("BrowseName", attr[i], strlen("BrowseName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->browseName.name)); */
/* 				dst->browseName.namespaceIndex = 0; */
/* 			} else if (0 == strncmp("DisplayName", attr[i], strlen("DisplayName"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->displayName.text)); */
/* 				dst->displayName.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else if (0 == strncmp("Description", attr[i], strlen("Description"))) { */
/* 				UA_String_copycstring(attr[i + 1], &(dst->description.text)); */
/* 				dst->description.encodingMask = UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT; */
/* 			} else { */
/* 				DBG_ERR(XML_Stack_print(s)); */
/* 				DBG_ERR(printf("%s - unknown attribute\n", attr[i])); */
/* 			} */
/* 		} */
/* 	} else { */
/* 		if (s->parent[s->depth - 1].activeChild == 5 && attr != UA_NULL) { // References */
/* 			UA_TypedArray* array = (UA_TypedArray*) attr; */
/* 			DBG(printf("UA_VariableNode_decodeXML - finished references=%p, size=%d\n",(void*)array,(array==UA_NULL)?-1:array->size)); */
/* 			dst->referencesSize = array->size; */
/* 			/\* dst->references = (UA_ReferenceNode**) array->elements; *\/ */
/* 			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL; */
/* 		} */
/* 	} */
/* 	return UA_SUCCESS; */
/* } */

#ifdef DEBUG
void print_node(UA_Node const * node) {
	if (node != UA_NULL) {
		UA_NodeId_printf("node.nodeId=", &(node->nodeId));
		printf("\t.browseName='%.*s'\n", node->browseName.name.length, node->browseName.name.data);
		printf("\t.displayName='%.*s'\n", node->displayName.text.length, node->displayName.text.data);
		printf("\t.description='%.*s%s'\n", node->description.text.length > 40 ? 40 : node->description.text.length,
		       node->description.text.data, node->description.text.length > 40 ? "..." : "");
		printf("\t.nodeClass=%d\n", node->nodeClass);
		printf("\t.writeMask=%d\n", node->writeMask);
		printf("\t.userWriteMask=%d\n", node->userWriteMask);
		printf("\t.references.size=%d\n", node->referencesSize);
		UA_Int32 i;
		for(i = 0;i < node->referencesSize;i++) {
			printf("\t\t.element[%d]", i);
			UA_ReferenceNode_println("=", &node->references[i]);
		}
		switch(node->nodeClass) {
		case UA_NODECLASS_VARIABLE:
		{
			UA_VariableNode const *p = (UA_VariableNode const *)node;
			printf("\t----- UA_VariableNode ----- \n");
			UA_NodeId_printf("\t.dataType=", &(p->dataType));
			printf("\t.valueRank=%d\n", p->valueRank);
			printf("\t.accessLevel=%d\n", p->accessLevel);
			printf("\t.userAccessLevel=%d\n", p->userAccessLevel);
			printf("\t.arrayDimensionsSize=%d\n", p->arrayDimensionsSize);
			printf("\t.minimumSamplingInterval=%f\n", p->minimumSamplingInterval);
			printf("\t.historizing=%d\n", p->historizing);
			printf("\t----- UA_Variant ----- \n");
			printf("\t.value.type.name=%s\n", p->value.vt->name);
			printf("\t.value.array.length=%d\n", p->value.arrayLength);
			UA_Int32 i;
			UA_Byte *data = (UA_Byte *)p->value.data;
			for(i = 0;i < p->value.arrayLength || (p->value.arrayLength == -1 && i == 0);++i) {
				UA_Byte *currentData = data + (i*p->value.vt->memSize);
				printf("\t.value.array.element[%d]=%p", i, currentData);
				switch(p->value.vt->typeId.identifier.numeric) {
				case UA_LOCALIZEDTEXT_NS0:
				{
					if(p->value.data != UA_NULL) {
						UA_LocalizedText *ltp = (UA_LocalizedText *)currentData;
						printf(",locale={%d,{%.*s}},text={%d,{%.*s}}", ltp->locale.length,
							   ltp->locale.length, ltp->locale.data, ltp->text.length,
							   ltp->text.length, ltp->text.data);
					}
				}
				break;

				case UA_EXTENSIONOBJECT_NS0:
				{
					if(p->value.data != UA_NULL) {
						UA_ExtensionObject *eo = (UA_ExtensionObject *)currentData;
						if(eo == UA_NULL)
							printf(",(null)");
						else
							printf(",enc=%d,typeId={i=%d}", eo->encoding, eo->typeId.identifier.numeric);
					}
				}
				break;

				default:
					break;
				}
				printf("\n");
			}
		}
		break;

		// case UA_NODECLASS_DATATYPE:
		default:
			break;
		}
	}
}
#endif

UA_Int32 UA_NodeSetAlias_decodeXmlFromStack(XML_Stack *s, XML_Attr *attr, UA_NodeSetAlias *dst, _Bool isStart) {
	DBG_VERBOSE(printf("UA_NodeSetAlias entered with dst=%p,isStart=%d\n", (void * )dst, isStart));
	if(isStart) {
		// create if necessary
		if(dst == UA_NULL) {
			UA_NodeSetAlias_new(&dst);
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void *)dst;
		}
		// set handlers
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s, "Alias", strlen("Alias"), (XML_decoder)UA_String_decodeXmlFromStack, UA_STRING,
		                          &(dst->alias));
		XML_Stack_addChildHandler(s, "Value", strlen("Value"), (XML_decoder)UA_String_decodeXmlFromStack, UA_STRING,
		                          &(dst->value));
		XML_Stack_handleTextAsElementOf(s, "Data", 1);

		// set attributes
		UA_Int32 i;
		for(i = 0;attr[i];i += 2) {
			if(0 == strncmp("Alias", attr[i], strlen("Alias")))
				UA_String_copycstring(attr[i + 1], &(dst->alias));
			else if(0 == strncmp("Value", attr[i], strlen("Value")))
				UA_String_copycstring(attr[i + 1], &(dst->value));
			else {
				DBG_ERR(XML_Stack_print(s)); DBG_ERR(printf("%s - unknown attribute\n", attr[i]));
			}
		}
	}
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSetAliases_decodeXmlFromStack(XML_Stack *s, XML_Attr *attr, UA_NodeSetAliases *dst, _Bool isStart) {
	DBG_VERBOSE(printf("UA_NodeSetALiases entered with dst=%p,isStart=%d\n", (void * )dst, isStart));
	if(isStart) {
		if(dst == UA_NULL) {
			UA_NodeSetAliases_new(&dst);
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void *)dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s, "Alias", strlen(
		                              "Alias"), (XML_decoder)UA_NodeSetAlias_decodeXmlFromStack, UA_INVALIDTYPE,
		                          UA_NULL);
	} else {
		// sub element is ready, add to array
		if(dst->size < 0 || dst->size == 0) {
			dst->size = 1;
			UA_alloc((void ** )&(dst->aliases), dst->size * sizeof(UA_NodeSetAlias *));
			DBG_VERBOSE(
			    printf("allocate aliases:dst=%p, aliases=%p, size=%d\n", (void * )dst, (void * )(dst->aliases),
			           dst->size));
		} else {
			dst->size++;
			dst->aliases = realloc(dst->aliases, dst->size * sizeof(UA_NodeSetAlias *));
			DBG_VERBOSE(
			    printf("reallocate aliases:dst=%p, aliases=%p, size=%d\n", (void * )dst, (void * )(dst->aliases),
			           dst->size));
		}
		// index starts with 0, therefore size-1
		DBG_VERBOSE(printf("assign alias:dst=%p, src=%p\n", (void * )dst->aliases[dst->size - 1], (void * )attr));
		dst->aliases[dst->size - 1] = (UA_NodeSetAlias *)attr;
		DBG_VERBOSE(printf("UA_NodeSetAliases clears %p\n",
		                   (void * )(s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj)));
		s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL;
	}
	return UA_SUCCESS;
}

UA_Int32 UA_NodeSet_decodeXmlFromStack(XML_Stack *s, XML_Attr *attr, UA_NodeSet *dst, _Bool isStart) {
	DBG_VERBOSE(printf("UA_NodeSet entered with dst=%p,isStart=%d\n", (void * )dst, isStart));
	if(isStart) {
		if(dst == UA_NULL) {
			UA_NodeSet_new(&dst, 99); // we don't really need the namespaceid for this..'
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = (void *)dst;
		}
		s->parent[s->depth].len = 0;
		XML_Stack_addChildHandler(s, "Aliases", strlen(
		                              "Aliases"), (XML_decoder)UA_NodeSetAliases_decodeXmlFromStack, UA_INVALIDTYPE,
		                          &(dst->aliases));
		XML_Stack_addChildHandler(s, "UADataType", strlen(
		                              "UADataType"), (XML_decoder)UA_DataTypeNode_decodeXmlFromStack, UA_DATATYPENODE,
		                          UA_NULL);
		XML_Stack_addChildHandler(s, "UAVariableType", strlen(
		                              "UAVariableType"), (XML_decoder)UA_VariableTypeNode_decodeXmlFromStack,
		                          UA_VARIABLETYPENODE, UA_NULL);
		XML_Stack_addChildHandler(s, "UAVariable", strlen(
		                              "UAVariable"), (XML_decoder)UA_VariableNode_decodeXmlFromStack, UA_VARIABLENODE,
		                          UA_NULL);
		XML_Stack_addChildHandler(s, "UAObjectType", strlen(
		                              "UAObjectType"), (XML_decoder)UA_ObjectTypeNode_decodeXmlFromStack,
		                          UA_OBJECTTYPENODE, UA_NULL);
		XML_Stack_addChildHandler(s, "UAObject", strlen(
		                              "UAObject"), (XML_decoder)UA_ObjectNode_decodeXmlFromStack, UA_OBJECTNODE,
		                          UA_NULL);
	} else {
		if(s->parent[s->depth - 1].activeChild == 0 && attr != UA_NULL) {
			UA_NodeSetAliases *aliases = (UA_NodeSetAliases *)attr;
			DBG(printf("UA_NodeSet_decodeXml - finished aliases: aliases=%p, size=%d\n", (void *)aliases,
			           (aliases == UA_NULL) ? -1 : aliases->size));
			s->aliases = aliases;
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL;
		} else {
			UA_Node* node = (UA_Node*) attr;
			DBG(printf("UA_NodeSet_decodeXML - finished node: node=%p\n", (void* )node));
			Namespace_insert(dst->ns, &node, 0);
			DBG(printf("UA_NodeSet_decodeXml - Inserting "));
			s->parent[s->depth - 1].children[s->parent[s->depth - 1].activeChild].obj = UA_NULL;
		}
	}
	return UA_SUCCESS;
}

/** lookup if element is a known child of parent, if yes go for it otherwise ignore */
void XML_Stack_startElement(void *data, const char *el, const char **attr) {
	XML_Stack  *s = (XML_Stack *)data;
	int         i;

// scan expected children
	XML_Parent *cp = &s->parent[s->depth];
	for(i = 0;i < cp->len;i++) {
		if(0 == strncmp(cp->children[i].name, el, cp->children[i].length)) {
			DBG_VERBOSE(XML_Stack_print(s));
			DBG_VERBOSE(printf("%s - processing\n", el));

			cp->activeChild = i;

			s->depth++;
			s->parent[s->depth].name = el;
			s->parent[s->depth].len  = 0;
			s->parent[s->depth].textAttribIdx = -1;
			s->parent[s->depth].activeChild   = -1;

			// finally call the elementHandler and return
			cp->children[i].elementHandler(data, attr, cp->children[i].obj, TRUE);
			return;
		}
	}
// if we come here we rejected the processing of el
	DBG_VERBOSE(XML_Stack_print(s));
	DBG_VERBOSE(printf("%s - rejected\n", el));
	s->depth++;
	s->parent[s->depth].name = el;
// this should be sufficient to reject the children as well
	s->parent[s->depth].len  = 0;
}

UA_Int32 XML_isSpace(cstring s, int len) {
	int i;
	for(i = 0;i < len;i++) {
		if(!isspace(s[i]))
			return UA_FALSE;
	}
	return UA_TRUE;
}

/* gather text */
void XML_Stack_handleText(void * data, const char *txt, int len) {
	/* XML_Stack* s = (XML_Stack*) data; */

	if (len > 0 && !XML_isSpace(txt, len)) {
		/* XML_Parent* cp = &(s->parent[s->depth]); // determine current element */
		UA_ByteString src = { len+1, (UA_Byte*) txt };
		UA_ByteString *dst;
		UA_ByteString_new(&dst);	// alloc dst
		UA_ByteString_copy(&src,dst); // alloc dst->data and copy txt
		dst->data[len] = 0; // add terminating zero to handle single line efficiently
		/* UA_list_addPayloadToBack(&(cp->textGatherList), (void*) dst); */
		/* if (cp->totalGatherLength == -1) { */
		/* 	cp->totalGatherLength = len; */
		/* } else { */
		/* 	cp->totalGatherLength += len; */
		/* } */
	}
}

char* theGatherBuffer;
void textGatherListTotalLength(void* payload) {
	UA_ByteString* b = (UA_ByteString*) payload;
	#ifdef DEBUG
	UA_ByteString_printf("\t",b);
	#endif
	UA_memcpy(theGatherBuffer,b->data,b->length-1); // remove trailing zero
	theGatherBuffer += (b->length-1);
}
/** if we are an activeChild of a parent we call the child-handler */
void XML_Stack_endElement(void *data, const char *el) {
	/* XML_Stack *s = (XML_Stack *)data; */

	/* XML_Parent* ce = &(s->parent[s->depth]); */
	/* if (ce->textAttribIdx >= 0 && ce->totalGatherLength > 0 ) { */
	/* 	ce->activeChild = ce->textAttribIdx; */
	/* 	char* buf; */
	/* 	if (UA_list_getFirst(&(ce->textGatherList)) == UA_list_getLast(&(ce->textGatherList)) ) { */
	/* 		buf = (char*) ((UA_ByteString*) UA_list_getFirst(&(ce->textGatherList))->payload)->data; */
	/* 	} else { */
	/* 		printf("XML_Stack_endElement - more than one text snippet with total length=%d:\n",ce->totalGatherLength); */
	/* 		UA_alloc((void**)&theGatherBuffer,ce->totalGatherLength+1); */
	/* 		buf = theGatherBuffer; */
	/* 		UA_list_iteratePayload(&(ce->textGatherList), textGatherListTotalLength); */
	/* 		buf[ce->totalGatherLength] = 0; */
	/* 		printf("XML_Stack_endElement - gatherBuffer %s:\n",buf); */
	/* 	} */
	/* 	XML_Attr attr[3] = { ce->textAttrib, buf, UA_NULL }; */
	/* 	DBG(printf("handleText @ %s calls start elementHandler %s with dst=%p, buf={%s}\n", XML_Stack_path(s),ce->children[ce->activeChild].name, ce->children[ce->activeChild].obj, buf)); */
	/* 	ce->children[ce->activeChild].elementHandler(s, attr, ce->children[ce->activeChild].obj, TRUE); */
	/* 	if (s->parent[s->depth-1].activeChild > 0) { */
	/* 		XML_child* c = &(s->parent[s->depth-1].children[s->parent[s->depth-1].activeChild]); */
	/* 		c->elementHandler(s, UA_NULL, c->obj, FALSE); */
	/* 	} */
	/* 	if (UA_list_getFirst(&(ce->textGatherList)) != UA_list_getLast(&(ce->textGatherList)) ) { */
	/* 		UA_free(buf); */
	/* 	} */
	/* 	UA_list_destroy(&(ce->textGatherList),(UA_list_PayloadVisitor) UA_ByteString_delete); */
	/* 	UA_list_init(&(ce->textGatherList)); // don't know if destroy leaves the list in usable state... */
	/* 	ce->totalGatherLength = -1; */
	/* } */

	/* // the parent of the parent (pop) of the element knows the elementHandler, therefore depth-2! */
	/* if (s->depth > 1) { */
	/* 	// inform parents elementHandler that everything is done */
	/* 	XML_Parent *cp  = &(s->parent[s->depth - 1]); */
	/* 	XML_Parent *cpp = &(s->parent[s->depth - 2]); */
	/* 	if(cpp->activeChild >= 0 && cp->activeChild >= 0) { */
	/* 		DBG_VERBOSE(XML_Stack_print(s)); */
	/* 		DBG_VERBOSE( */
	/* 		    printf(" - inform pop %s, arg=%p\n", cpp->children[cpp->activeChild].name, */
	/* 		           (void * )cp->children[cp->activeChild].obj)); */
	/* 		cpp->children[cpp->activeChild].elementHandler(s, (XML_Attr *)cp->children[cp->activeChild].obj, */
	/* 		                                               cpp->children[cpp->activeChild].obj, FALSE); */
	/* 	} */
	/* 	// reset */
	/* 	cp->activeChild = -1; */
	/* } */
	/* s->depth--; */
}
