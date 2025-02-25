/*
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <constr_SET.h>

asn_TYPE_operation_t asn_OP_SET = {
    SET_free,
#if !defined(ASN_DISABLE_PRINT_SUPPORT)
    SET_print,
#else
    0,
#endif  /* !defined(ASN_DISABLE_PRINT_SUPPORT) */
    SET_compare,
#if !defined(ASN_DISABLE_BER_SUPPORT)
    SET_decode_ber,
    SET_encode_der,
#else
    0,
    0,
#endif  /* !defined(ASN_DISABLE_BER_SUPPORT) */
#if !defined(ASN_DISABLE_XER_SUPPORT)
    SET_decode_xer,
    SET_encode_xer,
#else
    0,
    0,
#endif  /* !defined(ASN_DISABLE_XER_SUPPORT) */
#if !defined(ASN_DISABLE_JER_SUPPORT)
    SET_decode_jer,
    SET_encode_jer,
#else
	0,
    0,
#endif  /* !defined(ASN_DISABLE_JER_SUPPORT) */
#if !defined(ASN_DISABLE_OER_SUPPORT)
    0,  /* SET_decode_oer */
    0,  /* SET_encode_oer */
#else
    0,
    0,
#endif  /* !defined(ASN_DISABLE_OER_SUPPORT) */
#if !defined(ASN_DISABLE_UPER_SUPPORT)
    0,  /* SET_decode_uper */
    0,  /* SET_encode_uper */
#else
    0,
    0,
#endif  /* !defined(ASN_DISABLE_UPER_SUPPORT) */
#if !defined(ASN_DISABLE_APER_SUPPORT)
    0,  /* SET_decode_aper */
    0,  /* SET_encode_aper */
#else
    0,
    0,
#endif  /* !defined(ASN_DISABLE_APER_SUPPORT) */
#if !defined(ASN_DISABLE_RFILL_SUPPORT)
    SET_random_fill,
#else
    0,
#endif  /* !defined(ASN_DISABLE_RFILL_SUPPORT) */
    0  /* Use generic outmost tag fetcher */
};

int
_SET_is_populated(const asn_TYPE_descriptor_t *td, const void *st) {
    const asn_SET_specifics_t *specs = (const asn_SET_specifics_t *)td->specifics;
	size_t edx;

	/*
	 * Check that all mandatory elements are present.
	 */
	for(edx = 0; edx < td->elements_count;
		edx += (8 * sizeof(specs->_mandatory_elements[0]))) {
		unsigned int midx, pres, must;

		midx = edx/(8 * sizeof(specs->_mandatory_elements[0]));
        pres = ((const unsigned int *)((const char *)st
                                       + specs->pres_offset))[midx];
        must = sys_ntohl(specs->_mandatory_elements[midx]);

		if((pres & must) == must) {
			/*
			 * Yes, everything seems to be in place.
			 */
		} else {
			ASN_DEBUG("One or more mandatory elements "
				"of a SET %s %d (%08x.%08x)=%08x "
				"are not present",
				td->name,
				midx,
				pres,
				must,
				(~(pres & must) & must)
			);
			return 0;
		}
	}

	return 1;
}

void
SET_free(const asn_TYPE_descriptor_t *td, void *ptr,
         enum asn_struct_free_method method) {
    size_t edx;

	if(!td || !ptr)
		return;

	ASN_DEBUG("Freeing %s as SET", td->name);

	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		void *memb_ptr;
		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void **)((char *)ptr + elm->memb_offset);
			if(memb_ptr)
				ASN_STRUCT_FREE(*elm->type, memb_ptr);
		} else {
			memb_ptr = (void *)((char *)ptr + elm->memb_offset);
			ASN_STRUCT_FREE_CONTENTS_ONLY(*elm->type, memb_ptr);
		}
	}

    switch(method) {
    case ASFM_FREE_EVERYTHING:
        FREEMEM(ptr);
        break;
    case ASFM_FREE_UNDERLYING:
        break;
    case ASFM_FREE_UNDERLYING_AND_RESET:
        memset(ptr, 0,
               ((const asn_SET_specifics_t *)(td->specifics))->struct_size);
        break;
    }
}

int
SET_constraint(const asn_TYPE_descriptor_t *td, const void *sptr,
               asn_app_constraint_failed_f *ctfailcb, void *app_key) {
    size_t edx;

	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}

	/*
	 * Iterate over structure members and check their validity.
	 */
	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				if(elm->optional)
					continue;
				ASN__CTFAIL(app_key, td, sptr,
				"%s: mandatory element %s absent (%s:%d)",
				td->name, elm->name, __FILE__, __LINE__);
				return -1;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}

		if(elm->encoding_constraints.general_constraints) {
			return elm->encoding_constraints.general_constraints(
					elm->type, memb_ptr, ctfailcb, app_key);
		} else {
			return elm->type->encoding_constraints.general_constraints(
					elm->type, memb_ptr, ctfailcb, app_key);
		}
	}

	return 0;
}

int
SET_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
            const void *bptr) {
    size_t edx;

	for(edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *amemb;
		const void *bmemb;
		int ret;

		if(elm->flags & ATF_POINTER) {
            amemb =
                *(const void *const *)((const char *)aptr + elm->memb_offset);
            bmemb =
                *(const void *const *)((const char *)bptr + elm->memb_offset);
            if(!amemb) {
                if(!bmemb) continue;
                return -1;
            } else if(!bmemb) {
                return 1;
            }
		} else {
            amemb = (const void *)((const char *)aptr + elm->memb_offset);
            bmemb = (const void *)((const char *)bptr + elm->memb_offset);
		}

        ret = elm->type->op->compare_struct(elm->type, amemb, bmemb);
        if(ret != 0) return ret;
    }

    return 0;
}
