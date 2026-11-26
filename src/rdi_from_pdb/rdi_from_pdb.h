// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RDI_FROM_PDB_H
#define RDI_FROM_PDB_H

////////////////////////////////
//~ rjf: Conversion Stage Inputs

typedef struct P2R_ConvertParams P2R_ConvertParams;
struct P2R_ConvertParams
{
  String8 input_pdb_name;
  String8 input_pdb_data;
  String8 input_exe_name;
  String8 input_exe_data;
  RDIM_SubsetFlags subset_flags;
  B32 deterministic;
  B32 single_symbol_name; // keep linkage only when the primary name is empty
};

////////////////////////////////
//~ rjf: Conversion Helper Types

//- rjf: link name map (voff -> string)

typedef struct P2R_LinkNameNode P2R_LinkNameNode;
struct P2R_LinkNameNode
{
  P2R_LinkNameNode *next;
  U64 voff;
  String8 name;
  U64 referencing_symbol_count;
};

typedef struct P2R_LinkNameMap P2R_LinkNameMap;
struct P2R_LinkNameMap
{
  P2R_LinkNameNode **buckets;
  U64 buckets_count;
  U64 bucket_collision_count;
  U64 link_name_count;
};

//- rjf: deduplicated namespace map type

typedef struct P2R_NamespaceNode P2R_NamespaceNode;
struct P2R_NamespaceNode
{
  P2R_NamespaceNode *next;
  String8 string;
  B32 corresponds_to_scope;
  RDIM_Scope *scope;
  RDIM_Type *type;
  RDIM_Namespace *ns;
};

//- rjf: normalized file path -> source file map

typedef struct P2R_SrcFileStub P2R_SrcFileStub;
struct P2R_SrcFileStub
{
  String8 file_path;
  CV_C13ChecksumKind checksum_kind;
  String8 checksum;
};

typedef struct P2R_SrcFileStubArray P2R_SrcFileStubArray;
struct P2R_SrcFileStubArray
{
  P2R_SrcFileStub *v;
  U64 count;
};

typedef struct P2R_SrcFileStubNode P2R_SrcFileStubNode;
struct P2R_SrcFileStubNode
{
  P2R_SrcFileStubNode *next;
  P2R_SrcFileStub v;
};

typedef struct P2R_SrcFileNode P2R_SrcFileNode;
struct P2R_SrcFileNode
{
  P2R_SrcFileNode *next;
  RDIM_SrcFile *src_file;
};

typedef struct P2R_SrcFileMap P2R_SrcFileMap;
struct P2R_SrcFileMap
{
  P2R_SrcFileNode **slots;
  U64 slots_count;
};

//- rjf: itype chains

typedef struct P2R_TypeIdChain P2R_TypeIdChain;
struct P2R_TypeIdChain
{
  P2R_TypeIdChain *next;
  CV_TypeId itype;
};

////////////////////////////////
//~ rjf: PDB -> (Codeview -> RDI) TPI Hash Table Building

internal CV2R_TPIHash *p2r_cv2r_tpi_hash_from_data(Arena *arena, PDB_Strtbl *strtbl, PDB_TpiParsed *tpi, String8 data);

////////////////////////////////
//~ rjf: Top-Level Conversion Entry Point (Revised)

internal RDIM_BakeParams p2r_convert2(Arena *arena, P2R_ConvertParams *params);

#endif // RDI_FROM_PDB_H
