// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RDI_FROM_DWARF_H
#define RDI_FROM_DWARF_H

////////////////////////////////
//~ rjf: Tag Hash Cache Types

typedef struct D2R_TagHashNode D2R_TagHashNode;
struct D2R_TagHashNode
{
  D2R_TagHashNode *next;
  U64 container_ancestor_info_off;
  U64 info_off;
  U64 hash;
};

////////////////////////////////
//~ rjf: Unique Tag Tree Deduplication Types

typedef struct D2R_TreeHash D2R_TreeHash;
struct D2R_TreeHash
{
  U64 info_off;
  U64 hash;
};

typedef struct D2R_TreeHashNode D2R_TreeHashNode;
struct D2R_TreeHashNode
{
  D2R_TreeHashNode *next;
  D2R_TreeHash v;
};

typedef enum D2R_UniqueTagKind
{
  D2R_UniqueTagKind_Type,
  D2R_UniqueTagKind_Namespace,
  D2R_UniqueTagKind_COUNT
}
D2R_UniqueTagKind;

typedef struct D2R_UniqueTagNode D2R_UniqueTagNode;
struct D2R_UniqueTagNode
{
  D2R_UniqueTagNode *next;
  D2R_UniqueTagKind kind;
  U64 hash;
  U64 info_off;
  U64 container_ancestor_info_off;
  U64 dependency_count;
  U64 order_idx;
};

typedef struct D2R_UnitDedupedTagNode D2R_UnitDedupedTagNode;
struct D2R_UnitDedupedTagNode
{
  D2R_UnitDedupedTagNode *next;
  U64 src_info_off;
  U64 dst_hash;
  U64 dependency_count;
};

typedef struct D2R_UnitDedupedTagMap D2R_UnitDedupedTagMap;
struct D2R_UnitDedupedTagMap
{
  D2R_UnitDedupedTagNode **slots;
  U64 slots_count;
};

////////////////////////////////
// Common Symbol Attributes

#define D2R_CommonAttrib_XList \
  X(name,            Name)           \
  X(linkname,        LinkageName)    \
  X(type,            Type)           \
  X(inline,          Inline)         \
  X(ranges,          Ranges)         \
  X(lopc,            LowPc)          \
  X(hipc,            HighPc)         \
  X(constval,        ConstValue)     \
  X(location,        Location)       \
  X(framebase,       FrameBase)      \
  X(external,        External)       \
  X(decl,            Declaration)    \
  X(abstract_origin, AbstractOrigin) \
  X(specification,   Specification)  \
  // end of list

typedef enum D2R_CommonAttrib
{
#define X(id, kind) D2R_CommonAttrib_##kind,
  D2R_CommonAttrib_XList
#undef X
  D2R_CommonAttrib_COUNT
} D2R_CommonAttrib;

typedef struct D2R_CommonAttribs
{
  DW2_Attrib *v[D2R_CommonAttrib_COUNT];
  DW2_Attrib inherited[D2R_CommonAttrib_COUNT];
} D2R_CommonAttribs;

////////////////////////////////
// Lightweight Name Space

typedef struct D2R_NameNode D2R_NameNode;
struct D2R_NameNode
{
  D2R_NameNode   *next;
  D2R_NameNode   *parent;
  U64             info_off;
  U64             ref_offs[3]; // origin/specification in attribute order, then namespace extension
  String8         name;
  RDIM_Namespace *ns;
  RDIM_Symbol    *symbol;
};

////////////////////////////////
//~ rjf: Conversion Stage Inputs (New)

typedef struct D2R_ConvertParams D2R_ConvertParams;
struct D2R_ConvertParams
{
  String8 exe_name;
  String8 exe_data;
  Arch arch;
  U64 base_vaddr;
  RDIM_BinarySectionList binary_sections;
  DW_Raw raw;
  PathStyle path_style;
  RDIM_SubsetFlags subset_flags;
  B32 deterministic;
  B32 skip_exe_hash;
};

////////////////////////////////
//~ rjf: Helpers

internal int d2r_tree_hash_is_less_than(D2R_TreeHash *a, D2R_TreeHash *b);
internal int d2r_unique_tag_node_is_less_than(D2R_UniqueTagNode **l, D2R_UniqueTagNode **r);

////////////////////////////////
//~ rjf: Main Conversion Entry Point (New)

internal RDIM_BakeParams d2r_convert(Arena *arena, D2R_ConvertParams *params);

#endif // RDI_FROM_DWARF_H
