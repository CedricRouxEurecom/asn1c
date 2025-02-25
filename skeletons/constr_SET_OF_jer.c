/*
 * Copyright (c) 2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <constr_SET_OF.h>

/*
 * Return a standardized complex structure.
 */
#undef RETURN
#define RETURN(_code)                     \
    do {                                  \
        rval.code = _code;                \
        rval.consumed = consumed_myself;  \
        return rval;                      \
    } while(0)

#undef JER_ADVANCE
#define JER_ADVANCE(num_bytes)                    \
    do {                                          \
        size_t num = num_bytes;                   \
        buf_ptr = ((const char *)buf_ptr) + num;  \
        size -= num;                              \
        consumed_myself += num;                   \
    } while(0)

/*
 * Decode the JER (JSON) data.
 */
asn_dec_rval_t
SET_OF_decode_jer(const asn_codec_ctx_t *opt_codec_ctx,
                  const asn_TYPE_descriptor_t *td, void **struct_ptr,
                  const char *opt_mname, const void *buf_ptr, size_t size) {
    /*
     * Bring closer parts of structure description.
     */
    const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    const asn_TYPE_member_t *element = td->elements;
    const char *elm_tag;

    /*
     * ... and parts of the structure being constructed.
     */
    void *st = *struct_ptr;  /* Target structure. */
    asn_struct_ctx_t *ctx;   /* Decoder context */

    asn_dec_rval_t rval = {RC_OK, 0};  /* Return value from a decoder */
    ssize_t consumed_myself = 0;       /* Consumed bytes from ptr */

    /*
     * Create the target structure if it is not present already.
     */
    if(st == 0) {
        st = *struct_ptr = CALLOC(1, specs->struct_size);
        if(st == 0) RETURN(RC_FAIL);
    }

    /* Which tag is expected for the downstream */
    if(specs->as_XMLValueList) {
        elm_tag = (specs->as_XMLValueList == 1) ? 0 : "";
    } else {
        elm_tag = (*element->name)
                ? element->name : element->type->xml_tag;
    }

    /*
     * Restore parsing context.
     */
    ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);

    /*
     * Phases of JER/JSON processing:
     * Phase 0: Check that the opening tag matches our expectations.
     * Phase 1: Processing body and reacting on closing token.
     * Phase 2: Processing inner type.
     */
    for(; ctx->phase <= 2;) {
        pjer_chunk_type_e ch_type;  /* JER chunk type */
        ssize_t ch_size;            /* Chunk size */
        jer_check_sym_e scv;        /* Tag check value */

        /*
         * Go inside the inner member of a set.
         */
        if(ctx->phase == 2) {
            asn_dec_rval_t tmprval = {RC_OK, 0};

            /* Invoke the inner type decoder, m.b. multiple times */
            ASN_DEBUG("JER/SET OF element [%s]", elm_tag);
            tmprval = element->type->op->jer_decoder(opt_codec_ctx,
                                                     element->type,
                                                     &ctx->ptr, elm_tag,
                                                     buf_ptr, size);
            if(tmprval.code == RC_OK) {
                asn_anonymous_set_ *list = _A_SET_FROM_VOID(st);
                if(ASN_SET_ADD(list, ctx->ptr) != 0)
                    RETURN(RC_FAIL);
                ctx->ptr = 0;
                JER_ADVANCE(tmprval.consumed);
            } else {
                JER_ADVANCE(tmprval.consumed);
                RETURN(tmprval.code);
            }

            ctx->phase = 1;  /* Back to body processing */
            ASN_DEBUG("JER/SET OF phase => %d", ctx->phase);
            /* Fall through */
        }

        /*
         * Get the next part of the JSON stream.
         */
        ch_size = jer_next_token(&ctx->context,
                                 buf_ptr, size, &ch_type);
        if(ch_size == -1) {
            RETURN(RC_FAIL);
        } else {
            switch(ch_type) {
            case PJER_WMORE:
                RETURN(RC_WMORE);
            case PJER_TEXT:  
                JER_ADVANCE(ch_size);
                continue;

            case PJER_DLM:
            case PJER_VALUE:
            case PJER_KEY:
                break;  /* Check the rest down there */
            }
        }

        scv = jer_check_sym(buf_ptr, ch_size, NULL);
        ASN_DEBUG("JER/SET OF: scv = %d, ph=%d t=%s",
                  scv, ctx->phase, json_key);
        switch(scv) {
        case JCK_AEND:
            if(ctx->phase == 0) break;
            ctx->phase = 0;

            if(ctx->phase == 0) {
                /* No more things to decode */
                JER_ADVANCE(ch_size);
                ctx->phase = 3;  /* Phase out */
                RETURN(RC_OK);
            }
        case JCK_OEND:
            /* Fall through */
        case JCK_KEY:
        case JCK_COMMA:

        case JCK_ASTART:
            if(ctx->phase == 0) {
                JER_ADVANCE(ch_size);
                ctx->phase = 1;  /* Processing body phase */
                continue;
            }
            /* Fall through */
        case JCK_UNKNOWN:
        case JCK_OSTART:
            ASN_DEBUG("JER/SET OF: scv=%d, ph=%d", scv, ctx->phase);
            if(ctx->phase == 1) {
                /*
                 * Process a single possible member.
                 */
                ctx->phase = 2;
                continue;
            }
            /* Fall through */
        default:
            break;
        }

        ASN_DEBUG("Unexpected JSON key in SET OF");
        break;
    }

    ctx->phase = 3;  /* "Phase out" on hard failure */
    RETURN(RC_FAIL);
}

typedef struct jer_tmp_enc_s {
    void *buffer;
    size_t offset;
    size_t size;
} jer_tmp_enc_t;

static int
SET_OF_encode_jer_callback(const void *buffer, size_t size, void *key) {
    jer_tmp_enc_t *t = (jer_tmp_enc_t *)key;
    if(t->offset + size >= t->size) {
        size_t newsize = (t->size << 2) + size;
        void *p = REALLOC(t->buffer, newsize);
        if(!p) return -1;
        t->buffer = p;
        t->size = newsize;
    }
    memcpy((char *)t->buffer + t->offset, buffer, size);
    t->offset += size;
    return 0;
}

static int
SET_OF_jer_order(const void *aptr, const void *bptr) {
    const jer_tmp_enc_t *a = (const jer_tmp_enc_t *)aptr;
    const jer_tmp_enc_t *b = (const jer_tmp_enc_t *)bptr;
    size_t minlen = a->offset;
    int ret;
    if(b->offset < minlen) minlen = b->offset;
    /* Well-formed UTF-8 has this nice lexicographical property... */
    ret = memcmp(a->buffer, b->buffer, minlen);
    if(ret != 0) return ret;
    if(a->offset == b->offset)
        return 0;
    if(a->offset == minlen)
        return -1;
    return 1;
}

asn_enc_rval_t
SET_OF_encode_jer(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
                  enum jer_encoder_flags_e flags, asn_app_consume_bytes_f *cb,
                  void *app_key) {
    asn_enc_rval_t er = {0,0,0};
    const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    const asn_TYPE_member_t *elm = td->elements;
    const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
    int xcan = 0;
    jer_tmp_enc_t *encs = 0;
    size_t encs_count = 0;
    void *original_app_key = app_key;
    asn_app_consume_bytes_f *original_cb = cb;
    int i;

    if(!sptr) ASN__ENCODE_FAILED;

    if(xcan) {
        encs = (jer_tmp_enc_t *)MALLOC(list->count * sizeof(encs[0]));
        if(!encs) ASN__ENCODE_FAILED;
        cb = SET_OF_encode_jer_callback;
    }

    er.encoded = 0;
    ASN__CALLBACK("[", 1);

    for(i = 0; i < list->count; i++) {
        asn_enc_rval_t tmper = {0,0,0};

        void *memb_ptr = list->array[i];
        if(!memb_ptr) continue;

        if(encs) {
            memset(&encs[encs_count], 0, sizeof(encs[0]));
            app_key = &encs[encs_count];
            encs_count++;
        }

        ASN__TEXT_INDENT(1, ilevel + 1);
        tmper = elm->type->op->jer_encoder(elm->type, memb_ptr,
                                           ilevel + (specs->as_XMLValueList != 2),
                                           flags, cb, app_key);
        if(tmper.encoded == -1) return tmper;
        er.encoded += tmper.encoded;
        if(tmper.encoded == 0 && specs->as_XMLValueList) {
            const char *name = elm->type->xml_tag;
            size_t len = strlen(name);
            ASN__CALLBACK3("\"", 1, name, len, "\"", 1);
        }
        if (i != list->count - 1) {
          ASN__CALLBACK(",", 1);
        }
    }

    if(!xcan) ASN__TEXT_INDENT(1, ilevel);
    ASN__CALLBACK("]", 1);

    if(encs) {
        jer_tmp_enc_t *enc = encs;
        jer_tmp_enc_t *end = encs + encs_count;
        ssize_t control_size = 0;

        er.encoded = 0;
        cb = original_cb;
        app_key = original_app_key;
        qsort(encs, encs_count, sizeof(encs[0]), SET_OF_jer_order);

        for(; enc < end; enc++) {
            ASN__CALLBACK(enc->buffer, enc->offset);
            FREEMEM(enc->buffer);
            enc->buffer = 0;
            control_size += enc->offset;
        }
        assert(control_size == er.encoded);
    }

    goto cleanup;
cb_failed:
    ASN__ENCODE_FAILED;
cleanup:
    if(encs) {
        size_t n;
        for(n = 0; n < encs_count; n++) {
            FREEMEM(encs[n].buffer);
        }
        FREEMEM(encs);
    }
    ASN__ENCODED_OK(er);
}
