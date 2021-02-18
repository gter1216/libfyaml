/*
 * fy-doc.h - YAML document internal header file
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FY_DOC_H
#define FY_DOC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include <libfyaml.h>

#include "fy-ctype.h"
#include "fy-utf8.h"
#include "fy-list.h"
#include "fy-typelist.h"
#include "fy-types.h"
#include "fy-diag.h"
#include "fy-dump.h"
#include "fy-docstate.h"
#include "fy-accel.h"

struct fy_eventp;

FY_TYPE_FWD_DECL_LIST(document);

struct fy_node;

struct fy_node_pair {
	struct list_head node;
	struct fy_node *key;
	struct fy_node *value;
	struct fy_document *fyd;
	struct fy_node *parent;
};
FY_TYPE_FWD_DECL_LIST(node_pair);
FY_TYPE_DECL_LIST(node_pair);

FY_TYPE_FWD_DECL_LIST(node);
struct fy_node {
	struct list_head node;
	struct fy_token *tag;
	enum fy_node_style style;
	struct fy_node *parent;
	struct fy_document *fyd;
	unsigned int marks;
	enum fy_node_type type : 2;	/* 2 bits are enough for 3 types */
	bool has_meta : 1;
	bool attached : 1;		/* when it's attached somewhere */
	bool synthetic : 1;		/* node has been modified programmaticaly */
	void *meta;
	struct fy_accel *xl;		/* mapping access accelerator */
	union {
		struct fy_token *scalar;
		struct fy_node_list sequence;
		struct fy_node_pair_list mapping;
	};
	union {
		struct fy_token *sequence_start;
		struct fy_token *mapping_start;
	};
	union {
		struct fy_token *sequence_end;
		struct fy_token *mapping_end;
	};
};
FY_TYPE_DECL_LIST(node);

struct fy_node *fy_node_alloc(struct fy_document *fyd, enum fy_node_type type);
struct fy_node_pair *fy_node_pair_alloc(struct fy_document *fyd);
int fy_node_pair_free(struct fy_node_pair *fynp);

void fy_node_detach_and_free(struct fy_node *fyn);
void fy_node_pair_detach_and_free(struct fy_node_pair *fynp);

struct fy_anchor {
	struct list_head node;
	struct fy_node *fyn;
	struct fy_token *anchor;
	bool multiple : 1;
};
FY_TYPE_FWD_DECL_LIST(anchor);
FY_TYPE_DECL_LIST(anchor);

struct fy_document {
	struct list_head node;
	struct fy_anchor_list anchors;
	struct fy_accel *axl;		/* name -> anchor access accelerator */
	struct fy_accel *naxl;		/* node -> anchor access accelerator */
	struct fy_document_state *fyds;
	struct fy_diag *diag;
	struct fy_parse_cfg parse_cfg;
	struct fy_node *root;
	bool parse_error : 1;

	struct fy_document *parent;
	struct fy_document_list children;

	fy_node_meta_clear_fn meta_clear_fn;
	void *meta_user;
};
/* only the list declaration/methods */
FY_TYPE_DECL_LIST(document);

struct fy_document *fy_parse_document_create(struct fy_parser *fyp, struct fy_eventp *fyep);

struct fy_node_mapping_sort_ctx {
	fy_node_mapping_sort_fn key_cmp;
	void *arg;
	struct fy_node_pair **fynpp;
	int count;
};

void fy_node_mapping_perform_sort(struct fy_node *fyn_map,
		fy_node_mapping_sort_fn key_cmp, void *arg,
		struct fy_node_pair **fynpp, int count);

struct fy_node_pair **fy_node_mapping_sort_array(struct fy_node *fyn_map,
		fy_node_mapping_sort_fn key_cmp,
		void *arg, int *countp);

void fy_node_mapping_sort_release_array(struct fy_node *fyn_map, struct fy_node_pair **fynpp);

struct fy_node_walk_ctx {
	unsigned int max_depth;
	unsigned int next_slot;
	unsigned int mark;
	struct fy_node *marked[0];
};

bool fy_check_ref_loop(struct fy_document *fyd, struct fy_node *fyn,
		       enum fy_node_walk_flags flags,
		       struct fy_node_walk_ctx *ctx);

#define FYNWF_VISIT_MARKER	(FYNWF_MAX_USER_MARKER + 1)
#define FYNWF_REF_MARKER	(FYNWF_MAX_USER_MARKER + 2)

#define FYNWF_SYSTEM_MARKS	(FY_BIT(FYNWF_VISIT_MARKER) | \
				 FY_BIT(FYNWF_REF_MARKER))

bool fy_node_uses_single_input_only(struct fy_node *fyn, struct fy_input *fyi);
struct fy_input *fy_node_get_first_input(struct fy_node *fyn);
bool fy_node_is_synthetic(struct fy_node *fyn);
void fy_node_mark_synthetic(struct fy_node *fyn);
struct fy_input *fy_node_get_input(struct fy_node *fyn);

struct fy_token *fy_node_non_synthesized_token(struct fy_node *fyn);
struct fy_token *fy_node_token(struct fy_node *fyn);

FILE *fy_document_get_error_fp(struct fy_document *fyd);
enum fy_parse_cfg_flags fy_document_get_cfg_flags(const struct fy_document *fyd);
bool fy_document_is_colorized(struct fy_document *fyd);
bool fy_document_is_accelerated(struct fy_document *fyd);
bool fy_document_can_be_accelerated(struct fy_document *fyd);

struct fy_walk_result {
	struct list_head node;
	struct fy_node *fyn;
};
FY_TYPE_FWD_DECL_LIST(walk_result);
FY_TYPE_DECL_LIST(walk_result);

enum fy_walk_component_type {
	/* none is analyzed and the others are found */
	fwct_none,
	/* start */
	fwct_start_root,
	fwct_start_alias,
	/* ypath */
	fwct_root,		/* /^ or / at the beginning of the expr */
	fwct_this,		/* /. */
	fwct_parent,		/* /.. */
	fwct_every_child,	// /* every immediate child
	fwct_every_child_r,	// /** every recursive child
	fwct_every_leaf,	// /**$ every leaf node
	fwct_assert_collection,	/* match only collection (at the end only) */
	fwct_simple_map_key,
	fwct_simple_seq_index,
	fwct_simple_sibling_map_key,
};

static inline bool
fy_walk_component_type_is_initial(enum fy_walk_component_type type)
{
	return type == fwct_start_root ||
	       type == fwct_start_alias;
}

static inline bool
fy_walk_component_type_is_terminating(enum fy_walk_component_type type)
{
	return type == fwct_every_child_r ||
	       type == fwct_every_leaf ||
	       type == fwct_assert_collection;
}

struct fy_walk_component {
	struct list_head node;
	const char *comp;
	size_t complen;
	enum fy_walk_component_type type;
	bool multi;
	union {
		int reljson_ptr_count;
		int seq_index;
		struct {
			char *key_alloc;
			const char *key;
			size_t keylen;
		} map_key;
		struct {
			const char *alias;
			size_t aliaslen;
		} alias;
	};
};
FY_TYPE_FWD_DECL_LIST(walk_component);
FY_TYPE_DECL_LIST(walk_component);

struct fy_walk_ctx {
	char *path;	/* work area */
	size_t pathlen;
	bool trailing_slash;
	enum fy_node_walk_flags flags;
	struct fy_walk_component_list components;
};

struct fy_walk_ctx *
fy_walk_create(const char *path, size_t len,
	       enum fy_node_walk_flags flags, struct fy_diag *diag);

void fy_walk_destroy(struct fy_walk_ctx *wc);

int fy_walk_perform(struct fy_walk_ctx *wc, struct fy_walk_result_list *results, struct fy_node *fyn);

void fy_walk_result_free(struct fy_walk_result *fwr);
void fy_walk_result_list_free(struct fy_walk_result_list *results);

#endif
